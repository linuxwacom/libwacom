/*
 * Copyright © 2011 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Red Hat
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.  Red
 * Hat makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:
 *	Peter Hutterer (peter.hutterer@redhat.com)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "libwacomint.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gudev/gudev.h>

#ifdef HAVE_LINUX_INPUT_H
#include <linux/input.h>
#endif

/* Defined in linux/input.h but older versions may be missing these definitions */
#ifndef INPUT_PROP_POINTER
#define INPUT_PROP_POINTER		0x00	/* needs a pointer */
#endif

#ifndef INPUT_PROP_DIRECT
#define INPUT_PROP_DIRECT		0x01	/* direct input devices */
#endif

static const WacomDevice *
libwacom_get_device(const WacomDeviceDatabase *db, const char *match)
{
	return (WacomDevice *) g_hash_table_lookup (db->device_ht, match);
}

static gboolean
is_tablet_or_touchpad (GUdevDevice *device)
{
	return g_udev_device_get_property_as_boolean (device, "ID_INPUT_TABLET") ||
		g_udev_device_get_property_as_boolean (device, "ID_INPUT_TOUCHPAD");
}

/* Overriding SUBSYSTEM isn't allowed in udev (works sometimes, but not
 * always). For evemu devices we need to set custom properties to make them
 * detected by libwacom.
 */
static char *
get_uinput_subsystem (GUdevDevice *device)
{
	const char *bus_str;
	GUdevDevice *parent;


	bus_str = NULL;
	parent = g_object_ref (device);

	while (parent && !g_udev_device_get_property_as_boolean (parent, "UINPUT_DEVICE")) {
		GUdevDevice *old_parent = parent;
		parent = g_udev_device_get_parent (old_parent);
		g_object_unref (old_parent);
	}

	if (parent) {
		bus_str = g_udev_device_get_property (parent, "UINPUT_SUBSYSTEM");
		g_object_unref (parent);
	}

	return bus_str ? g_strdup (bus_str) : NULL;
}

static gboolean
get_bus_vid_pid (GUdevDevice  *device,
		 WacomBusType *bus,
		 int          *vendor_id,
		 int          *product_id,
		 WacomError   *error)
{
	GUdevDevice *parent;
	const char *product_str;
	gchar **splitted_product = NULL;
	unsigned int bus_id;
	gboolean retval = FALSE;

	/* Parse that:
	 * E: PRODUCT=5/56a/81/100
	 * into:
	 * vendor 0x56a
	 * product 0x81 */
	parent = g_object_ref (device);
	product_str = g_udev_device_get_property (device, "PRODUCT");

	while (!product_str && parent) {
		GUdevDevice *old_parent = parent;
		parent = g_udev_device_get_parent (old_parent);
		if (parent)
			product_str = g_udev_device_get_property (parent, "PRODUCT");
		g_object_unref (old_parent);
	}

	if (!product_str)
		/* PRODUCT not found, hoping the old method will work */
		goto out;

	splitted_product = g_strsplit (product_str, "/", 4);
	if (g_strv_length (splitted_product) != 4) {
		libwacom_error_set(error, WERROR_UNKNOWN_MODEL, "Unable to parse model identification");
		goto out;
	}

	bus_id = (int)strtoul (splitted_product[0], NULL, 16);
	*vendor_id = (int)strtol (splitted_product[1], NULL, 16);
	*product_id = (int)strtol (splitted_product[2], NULL, 16);

	switch (bus_id) {
	case 3:
		*bus = WBUSTYPE_USB;
		retval = TRUE;
		break;
	case 5:
		*bus = WBUSTYPE_BLUETOOTH;
		retval = TRUE;
		break;
	case 24:
		*bus = WBUSTYPE_I2C;
		retval = TRUE;
		break;
	}

out:
	if (splitted_product)
		g_strfreev (splitted_product);
	g_object_unref (parent);
	return retval;
}

static char *
get_bus (GUdevDevice *device)
{
	const char *subsystem;
	char *bus_str;
	GUdevDevice *parent;

	bus_str = get_uinput_subsystem (device);
	if (bus_str)
		return bus_str;

	subsystem = g_udev_device_get_subsystem (device);
	parent = g_object_ref (device);

	while (parent && ((g_strcmp0 (subsystem, "input") == 0) ||
			  (g_strcmp0 (subsystem, "hid") == 0)) ){
		GUdevDevice *old_parent = parent;
		parent = g_udev_device_get_parent (old_parent);
		if (parent)
			subsystem = g_udev_device_get_subsystem (parent);
		g_object_unref (old_parent);
	}

	if (parent) {
		if (g_strcmp0 (subsystem, "tty") == 0 || g_strcmp0 (subsystem, "serio") == 0)
			bus_str = g_strdup ("serial");
		else
			bus_str = g_strdup (subsystem);

		g_object_unref (parent);
	} else
		bus_str = strdup("unknown");

	return bus_str;
}

static gboolean
get_device_info (const char            *path,
		 int                   *vendor_id,
		 int                   *product_id,
		 char                 **name,
		 WacomBusType          *bus,
		 WacomIntegrationFlags *integration_flags,
		 WacomError            *error)
{
	GUdevClient *client;
	GUdevDevice *device;
	const char * const subsystems[] = { "input", NULL };
	gboolean retval;
	char *bus_str;
	const char *devname;

#if NEED_G_TYPE_INIT
	g_type_init();
#endif

	retval = FALSE;
	/* The integration flags from device info are unset by default */
	*integration_flags = WACOM_DEVICE_INTEGRATED_UNSET;
	*name = NULL;
	bus_str = NULL;
	client = g_udev_client_new (subsystems);
	device = g_udev_client_query_by_device_file (client, path);
	if (device == NULL) {
		libwacom_error_set(error, WERROR_INVALID_PATH, "Could not find device '%s' in udev", path);
		goto out;
	}

	/* Touchpads are only for the "Finger" part of Bamboo devices */
	if (!is_tablet_or_touchpad(device)) {
		GUdevDevice *parent;

		parent = g_udev_device_get_parent(device);
		if (!parent || !is_tablet_or_touchpad(parent)) {
			libwacom_error_set(error, WERROR_INVALID_PATH, "Device '%s' is not a tablet", path);
			g_object_unref (parent);
			goto out;
		}
		g_object_unref (parent);
	}

	/* Is the device integrated in display? */
	devname = g_udev_device_get_name (device);
	if (devname != NULL) {
		char *sysfs_path, *contents;

		sysfs_path = g_build_filename ("/sys/class/input", devname, "device/properties", NULL);
		if (g_file_get_contents (sysfs_path, &contents, NULL, NULL)) {
			int flag;

			flag = atoi(contents);
			flag &= (1 << INPUT_PROP_DIRECT) | (1 << INPUT_PROP_POINTER);
			/*
			 * To ensure we are dealing with a screen tablet, need
			 * to check that it has DIRECT and non-POINTER (DIRECT
			 * alone is not sufficient since it's set for drawing
			 * tablets as well)
			 */
			if (flag == (1 << INPUT_PROP_DIRECT))
				*integration_flags = WACOM_DEVICE_INTEGRATED_DISPLAY;
			else
				*integration_flags = WACOM_DEVICE_INTEGRATED_NONE;

			g_free (contents);
		}
		g_free (sysfs_path);
	}

	*name = g_strdup (g_udev_device_get_sysfs_attr (device, "name"));
	/* Try getting the name from the parent if that fails */
	if (*name == NULL) {
		GUdevDevice *parent;

		parent = g_udev_device_get_parent (device);
		*name = g_strdup (g_udev_device_get_sysfs_attr (parent, "name"));
		g_object_unref (parent);
	}

	/* Parse the PRODUCT attribute (for Bluetooth, USB, I2C) */
	retval = get_bus_vid_pid (device, bus, vendor_id, product_id, error);
	if (retval)
		goto out;

	bus_str = get_bus (device);
	*bus = bus_from_str (bus_str);

	if (*bus == WBUSTYPE_SERIAL) {
		/* The serial bus uses 0:0 as the vid/pid */
		*vendor_id = 0;
		*product_id = 0;
		retval = TRUE;
	} else {
		libwacom_error_set(error, WERROR_UNKNOWN_MODEL, "Unsupported bus '%s'", bus_str);
	}

out:
	if (bus_str != NULL)
		g_free (bus_str);
	if (retval == FALSE)
		g_free (*name);
	if (device != NULL)
		g_object_unref (device);
	if (client != NULL)
		g_object_unref (client);
	return retval;
}

static WacomMatch *libwacom_copy_match(const WacomMatch *src)
{
	WacomMatch *dst;

	dst = g_new0(WacomMatch, 1);
	dst->match = g_strdup(src->match);
	dst->name = g_strdup(src->name);
	dst->bus = src->bus;
	dst->vendor_id = src->vendor_id;
	dst->product_id = src->product_id;

	return dst;
}

static WacomDevice *
libwacom_copy(const WacomDevice *device)
{
	WacomDevice *d;
	int i;

	d = g_new0 (WacomDevice, 1);
	g_atomic_int_inc(&d->refcnt);
	d->name = g_strdup (device->name);
	d->width = device->width;
	d->height = device->height;
	d->integration_flags = device->integration_flags;
	d->layout = g_strdup(device->layout);
	d->nmatches = device->nmatches;
	d->matches = g_malloc((d->nmatches + 1) * sizeof(WacomMatch*));
	for (i = 0; i < d->nmatches; i++)
		d->matches[i] = libwacom_copy_match(device->matches[i]);
	d->matches[d->nmatches] = NULL;
	d->match = device->match;
	d->cls = device->cls;
	d->num_strips = device->num_strips;
	d->features = device->features;
	d->strips_num_modes = device->strips_num_modes;
	d->ring_num_modes = device->ring_num_modes;
	d->ring2_num_modes = device->ring2_num_modes;
	d->num_styli = device->num_styli;
	d->supported_styli = g_memdup (device->supported_styli, sizeof(int) * device->num_styli);
	d->num_leds = device->num_leds;
	d->status_leds = g_memdup (device->status_leds, sizeof(WacomStatusLEDs) * device->num_leds);
	d->num_buttons = device->num_buttons;
	d->buttons = g_memdup (device->buttons, sizeof(WacomButtonFlags) * device->num_buttons);
	return d;
}


static int
compare_matches(const WacomDevice *a, const WacomDevice *b)
{
	const WacomMatch **ma, **mb, **match_a, **match_b;

	ma = libwacom_get_matches(a);
	mb = libwacom_get_matches(b);

	for (match_a = ma; *match_a; match_a++) {
		int found = 0;
		for (match_b = mb; !found && *mb; mb++) {
			if (strcmp((*match_a)->match, (*match_b)->match) == 0)
				found = 1;
		}
		if (!found)
			return 1;
	}

	return 0;
}

/* Compare layouts based on file name, stripping the full path */
static gboolean
libwacom_same_layouts (const WacomDevice *a, const WacomDevice *b)
{
	gchar *file1, *file2;
	gboolean rc;

	/* Conveniently handle the null case */
	if (a->layout == b->layout)
		return TRUE;

	file1 = NULL;
	file2 = NULL;
	if (a->layout != NULL)
		file1 = g_path_get_basename (a->layout);
	if (b->layout != NULL)
		file2 = g_path_get_basename (b->layout);

	rc = (g_strcmp0 (file1, file2) == 0);

	g_free (file1);
	g_free (file2);

	return rc;
}

int
libwacom_compare(const WacomDevice *a, const WacomDevice *b, WacomCompareFlags flags)
{
	g_return_val_if_fail(a || b, 0);

	if (!a || !b)
		return 1;

	if (strcmp(a->name, b->name) != 0)
		return 1;

	if (a->width != b->width || a->height != b->height)
		return 1;

	if (!libwacom_same_layouts (a, b))
		return 1;

	if (a->integration_flags != b->integration_flags)
		return 1;

	if (a->cls != b->cls)
		return 1;

	if (a->num_strips != b->num_strips)
		return 1;

	if (a->features != b->features)
		return 1;

	if (a->strips_num_modes != b->strips_num_modes)
		return 1;

	if (a->ring_num_modes != b->ring_num_modes)
		return 1;

	if (a->ring2_num_modes != b->ring2_num_modes)
		return 1;

	if (a->num_buttons != b->num_buttons)
		return 1;

	if (a->num_styli != b->num_styli)
		return 1;

	if (memcmp(a->supported_styli, b->supported_styli, sizeof(int) * a->num_styli) != 0)
		return 1;

	if (a->num_leds != b->num_leds)
		return 1;

	if (memcmp(a->status_leds, b->status_leds, sizeof(WacomStatusLEDs) * a->num_leds) != 0)
		return 1;

	if (memcmp(a->buttons, b->buttons, sizeof(WacomButtonFlags) * a->num_buttons) != 0)
		return 1;

	if ((flags & WCOMPARE_MATCHES) && compare_matches(a, b) != 0)
		return 1;
	else if (strcmp(a->matches[a->match]->match, b->matches[b->match]->match) != 0)
		return 1;

	return 0;
}

static const WacomDevice *
libwacom_new (const WacomDeviceDatabase *db, const char *name, int vendor_id, int product_id, WacomBusType bus, WacomError *error)
{
	const WacomDevice *device;
	char *match;

	if (!db) {
		libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
		return NULL;
	}

	match = make_match_string(name, bus, vendor_id, product_id);
	device = libwacom_get_device(db, match);
	g_free (match);

	return device;
}

WacomDevice*
libwacom_new_from_path(const WacomDeviceDatabase *db, const char *path, WacomFallbackFlags fallback, WacomError *error)
{
	int vendor_id, product_id;
	WacomBusType bus;
	const WacomDevice *device;
	WacomDevice *ret = NULL;
	WacomIntegrationFlags integration_flags;
	char *name, *match_name;

	if (!db) {
		libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
		return NULL;
	}

	if (!path) {
		libwacom_error_set(error, WERROR_INVALID_PATH, "path is NULL");
		return NULL;
	}

	if (!get_device_info (path, &vendor_id, &product_id, &name, &bus, &integration_flags, error))
		return NULL;

	match_name = name;
	device = libwacom_new (db, match_name, vendor_id, product_id, bus, error);
	if (device == NULL) {
		match_name = NULL;
		device = libwacom_new (db, match_name, vendor_id, product_id, bus, error);
	}
	if (device != NULL)
		ret = libwacom_copy(device);
	else if (fallback == WFALLBACK_NONE)
		goto bail;

	if (device == NULL && fallback == WFALLBACK_GENERIC) {
		device = libwacom_get_device(db, "generic");
		if (device == NULL)
			goto bail;

		ret = libwacom_copy(device);

		if (name != NULL) {
			g_free (ret->name);
			ret->name = g_strdup(name);
		}
	}

	/* for multiple-match devices, set to the one we requested */
	libwacom_update_match(ret, match_name, bus, vendor_id, product_id);

	g_free (name);

	if (device) {
		/* if unset, use the kernel flags. Could be unset as well. */
		if (ret->integration_flags == WACOM_DEVICE_INTEGRATED_UNSET)
			ret->integration_flags = integration_flags;

		return ret;
	}

bail:
	g_free (name);
	libwacom_error_set(error, WERROR_UNKNOWN_MODEL, NULL);
	return NULL;
}

WacomDevice*
libwacom_new_from_usbid(const WacomDeviceDatabase *db, int vendor_id, int product_id, WacomError *error)
{
	const WacomDevice *device;

	if (!db) {
		libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
		return NULL;
	}

	device = libwacom_new(db, NULL, vendor_id, product_id, WBUSTYPE_USB, error);

	if (device)
		return libwacom_copy(device);

	libwacom_error_set(error, WERROR_UNKNOWN_MODEL, NULL);
	return NULL;
}

WacomDevice*
libwacom_new_from_name(const WacomDeviceDatabase *db, const char *name, WacomError *error)
{
	const WacomDevice *device;
	GList *keys, *l;

	if (!db) {
		libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
		return NULL;
	}

	device = NULL;
	keys = g_hash_table_get_values (db->device_ht);
	for (l = keys; l; l = l->next) {
		WacomDevice *d = l->data;

		if (g_strcmp0 (d->name, name) == 0) {
			device = d;
			break;
		}
	}
	g_list_free (keys);

	if (device)
		return libwacom_copy(device);

	libwacom_error_set(error, WERROR_UNKNOWN_MODEL, NULL);
	return NULL;
}

static void print_styli_for_device (int fd, const WacomDevice *device)
{
	int nstyli;
	const int *styli;
	int i;

	if (!libwacom_has_stylus(device))
		return;

	styli = libwacom_get_supported_styli(device, &nstyli);

	dprintf(fd, "Styli=");
	for (i = 0; i < nstyli; i++)
		dprintf(fd, "%#x;", styli[i]);
	dprintf(fd, "\n");
}

static void print_layout_for_device (int fd, const WacomDevice *device)
{
	const char *layout_filename;
	gchar      *base_name;

	layout_filename = libwacom_get_layout_filename(device);
	if (layout_filename) {
		base_name = g_path_get_basename (layout_filename);
		dprintf(fd, "Layout=%s\n", base_name);
		g_free (base_name);
	}
}

static void print_supported_leds (int fd, const WacomDevice *device)
{
	char *leds_name[] = {
		"Ring",
		"Ring2",
		"Touchstrip",
		"Touchstrip2"
	};
	int num_leds;
	const WacomStatusLEDs *status_leds;
	int i;

	status_leds = libwacom_get_status_leds(device, &num_leds);

	dprintf(fd, "StatusLEDs=");
	for (i = 0; i < num_leds; i++)
		dprintf(fd, "%s;", leds_name [status_leds[i]]);
	dprintf(fd, "\n");
}

static void print_button_flag_if(int fd, const WacomDevice *device, const char *label, int flag)
{
	int nbuttons = libwacom_get_num_buttons(device);
	char b;
	dprintf(fd, "%s=", label);
	for (b = 'A'; b < 'A' + nbuttons; b++)
		if (libwacom_get_button_flag(device, b) & flag)
			dprintf(fd, "%c;", b);
	dprintf(fd, "\n");
}

static void print_buttons_for_device (int fd, const WacomDevice *device)
{
	int nbuttons = libwacom_get_num_buttons(device);

	if (nbuttons == 0)
		return;

	dprintf(fd, "[Buttons]\n");

	print_button_flag_if(fd, device, "Left", WACOM_BUTTON_POSITION_LEFT);
	print_button_flag_if(fd, device, "Right", WACOM_BUTTON_POSITION_RIGHT);
	print_button_flag_if(fd, device, "Top", WACOM_BUTTON_POSITION_TOP);
	print_button_flag_if(fd, device, "Bottom", WACOM_BUTTON_POSITION_BOTTOM);
	print_button_flag_if(fd, device, "Touchstrip", WACOM_BUTTON_TOUCHSTRIP_MODESWITCH);
	print_button_flag_if(fd, device, "Touchstrip2", WACOM_BUTTON_TOUCHSTRIP2_MODESWITCH);
	print_button_flag_if(fd, device, "OLEDs", WACOM_BUTTON_OLED);
	print_button_flag_if(fd, device, "Ring", WACOM_BUTTON_RING_MODESWITCH);
	print_button_flag_if(fd, device, "Ring2", WACOM_BUTTON_RING2_MODESWITCH);
	dprintf(fd, "RingNumModes=%d\n", libwacom_get_ring_num_modes(device));
	dprintf(fd, "Ring2NumModes=%d\n", libwacom_get_ring2_num_modes(device));
	dprintf(fd, "StripsNumModes=%d\n", libwacom_get_strips_num_modes(device));

	dprintf(fd, "\n");
}

static void print_integrated_flags_for_device (int fd, const WacomDevice *device)
{
	/*
	 * If flag is WACOM_DEVICE_INTEGRATED_UNSET, the info is not provided
	 * by the tablet database but deduced otherwise (e.g. from sysfs device
	 * properties on Linux)
	 */
	if (device->integration_flags == WACOM_DEVICE_INTEGRATED_UNSET)
		return;
	dprintf(fd, "IntegratedIn=");
	if (device->integration_flags & WACOM_DEVICE_INTEGRATED_DISPLAY)
		dprintf(fd, "Display;");
	if (device->integration_flags & WACOM_DEVICE_INTEGRATED_SYSTEM)
		dprintf(fd, "System;");
	dprintf(fd, "\n");
}


void
libwacom_print_device_description(int fd, const WacomDevice *device)
{
	const WacomMatch **match;
	WacomClass class;
	const char *bus_name, *class_name;

	class  = libwacom_get_class(device);
	switch(class) {
		case WCLASS_UNKNOWN:	class_name = "Unknown";	break;
		case WCLASS_INTUOS3:	class_name = "Intuos3";	break;
		case WCLASS_INTUOS4:	class_name = "Intuos4";	break;
		case WCLASS_INTUOS5:	class_name = "Intuos5";	break;
		case WCLASS_CINTIQ:	class_name = "Cintiq";	break;
		case WCLASS_BAMBOO:	class_name = "Bamboo";	break;
		case WCLASS_GRAPHIRE:	class_name = "Graphire";break;
		case WCLASS_ISDV4:	class_name = "ISDV4";	break;
		case WCLASS_INTUOS:	class_name = "Intuos";	break;
		case WCLASS_INTUOS2:	class_name = "Intuos2";	break;
		case WCLASS_PEN_DISPLAYS:	class_name = "PenDisplay";	break;
		case WCLASS_REMOTE:	class_name = "Remote";	break;
		default:		g_assert_not_reached(); break;
	}

	dprintf(fd, "[Device]\n");
	dprintf(fd, "Name=%s\n", libwacom_get_name(device));
	dprintf(fd, "DeviceMatch=");
	for (match = libwacom_get_matches(device); *match; match++) {
		const char  *name       = libwacom_match_get_name(*match);
		WacomBusType type	= libwacom_match_get_bustype(*match);
		int          vendor     = libwacom_match_get_vendor_id(*match);
		int          product    = libwacom_match_get_product_id(*match);

		switch(type) {
			case WBUSTYPE_BLUETOOTH:	bus_name = "bluetooth";	break;
			case WBUSTYPE_USB:		bus_name = "usb";	break;
			case WBUSTYPE_SERIAL:		bus_name = "serial";	break;
			case WBUSTYPE_I2C:		bus_name = "i2c";	break;
			case WBUSTYPE_UNKNOWN:		bus_name = "unknown";	break;
			default:			g_assert_not_reached(); break;
		}
		dprintf(fd, "%s:%04x:%04x", bus_name, vendor, product);
		if (name)
			dprintf(fd, ":%s", name);
		dprintf(fd, ";");
	}
	dprintf(fd, "\n");

	dprintf(fd, "Class=%s\n",		class_name);
	dprintf(fd, "Width=%d\n",		libwacom_get_width(device));
	dprintf(fd, "Height=%d\n",		libwacom_get_height(device));
	print_integrated_flags_for_device(fd, device);
	print_layout_for_device(fd, device);
	print_styli_for_device(fd, device);
	dprintf(fd, "\n");

	dprintf(fd, "[Features]\n");
	dprintf(fd, "Reversible=%s\n", libwacom_is_reversible(device)	? "true" : "false");
	dprintf(fd, "Stylus=%s\n",	 libwacom_has_stylus(device)	? "true" : "false");
	dprintf(fd, "Ring=%s\n",	 libwacom_has_ring(device)	? "true" : "false");
	dprintf(fd, "Ring2=%s\n",	 libwacom_has_ring2(device)	? "true" : "false");
	dprintf(fd, "Touch=%s\n",	 libwacom_has_touch(device)	? "true" : "false");
	dprintf(fd, "TouchSwitch=%s\n",	libwacom_has_touchswitch(device)? "true" : "false");
	print_supported_leds(fd, device);

	dprintf(fd, "NumStrips=%d\n",	libwacom_get_num_strips(device));
	dprintf(fd, "Buttons=%d\n",		libwacom_get_num_buttons(device));

	print_buttons_for_device(fd, device);
}


void
libwacom_destroy(WacomDevice *device)
{
	int i;

	if (!g_atomic_int_dec_and_test(&device->refcnt))
		return;

	g_free (device->name);
	g_free (device->layout);

	for (i = 0; i < device->nmatches; i++) {
		g_free (device->matches[i]->match);
		g_free (device->matches[i]->name);
		g_free (device->matches[i]);
	}
	g_free (device->matches);
	g_free (device->supported_styli);
	g_free (device->status_leds);
	g_free (device->buttons);
	g_free (device);
}

void
libwacom_update_match(WacomDevice *device, const char *name, WacomBusType bus, int vendor_id, int product_id)
{
	char *newmatch;
	int i;
	WacomMatch match;

	if (name == NULL && bus == WBUSTYPE_UNKNOWN && vendor_id == 0 && product_id == 0)
		newmatch = g_strdup("generic");
	else
		newmatch = make_match_string(name, bus, vendor_id, product_id);

	match.match = newmatch;
	match.name = g_strdup(name);
	match.bus = bus;
	match.vendor_id = vendor_id;
	match.product_id = product_id;

	for (i = 0; i < device->nmatches; i++) {
		if (g_strcmp0(libwacom_match_get_match_string(device->matches[i]), newmatch) == 0) {
			device->match = i;
			goto out;
		}
	}

	device->nmatches++;

	device->matches = g_realloc_n(device->matches, device->nmatches + 1, sizeof(WacomMatch*));
	device->matches[device->nmatches] = NULL;
	device->matches[device->nmatches - 1] = libwacom_copy_match(&match);
	device->match = device->nmatches - 1;
out:
	g_free(newmatch);
	g_free(match.name);
}

int libwacom_get_vendor_id(const WacomDevice *device)
{
	g_return_val_if_fail(device->match >= 0, -1);
	g_return_val_if_fail(device->match < device->nmatches, -1);
	return device->matches[device->match]->vendor_id;
}

const char* libwacom_get_name(const WacomDevice *device)
{
	return device->name;
}

const char* libwacom_get_layout_filename(const WacomDevice *device)
{
	return device->layout;
}

int libwacom_get_product_id(const WacomDevice *device)
{
	g_return_val_if_fail(device->match >= 0, -1);
	g_return_val_if_fail(device->match < device->nmatches, -1);
	return device->matches[device->match]->product_id;
}

const char* libwacom_get_match(const WacomDevice *device)
{
	g_return_val_if_fail(device->match >= 0, NULL);
	g_return_val_if_fail(device->match < device->nmatches, NULL);
	return device->matches[device->match]->match;
}

const WacomMatch** libwacom_get_matches(const WacomDevice *device)
{
	return (const WacomMatch**)device->matches;
}

int libwacom_get_width(const WacomDevice *device)
{
	return device->width;
}

int libwacom_get_height(const WacomDevice *device)
{
	return device->height;
}

WacomClass libwacom_get_class(const WacomDevice *device)
{
	return device->cls;
}

int libwacom_has_stylus(const WacomDevice *device)
{
	return !!(device->features & FEATURE_STYLUS);
}

int libwacom_has_touch(const WacomDevice *device)
{
	return !!(device->features & FEATURE_TOUCH);
}

int libwacom_get_num_buttons(const WacomDevice *device)
{
	return device->num_buttons;
}

const int *libwacom_get_supported_styli(const WacomDevice *device, int *num_styli)
{
	*num_styli = device->num_styli;
	return device->supported_styli;
}

int libwacom_has_ring(const WacomDevice *device)
{
	return !!(device->features & FEATURE_RING);
}

int libwacom_has_ring2(const WacomDevice *device)
{
	return !!(device->features & FEATURE_RING2);
}

int libwacom_get_ring_num_modes(const WacomDevice *device)
{
	return device->ring_num_modes;
}

int libwacom_get_ring2_num_modes(const WacomDevice *device)
{
	return device->ring2_num_modes;
}

int libwacom_get_num_strips(const WacomDevice *device)
{
	return device->num_strips;
}

int libwacom_get_strips_num_modes(const WacomDevice *device)
{
	return device->strips_num_modes;
}

const WacomStatusLEDs *libwacom_get_status_leds(const WacomDevice *device, int *num_leds)
{
	*num_leds = device->num_leds;
	return device->status_leds;
}

struct {
	WacomButtonFlags button_flags;
	WacomStatusLEDs  status_leds;
} button_status_leds[] = {
	{ WACOM_BUTTON_RING_MODESWITCH,		WACOM_STATUS_LED_RING },
	{ WACOM_BUTTON_RING2_MODESWITCH,	WACOM_STATUS_LED_RING2 },
	{ WACOM_BUTTON_TOUCHSTRIP_MODESWITCH,	WACOM_STATUS_LED_TOUCHSTRIP },
	{ WACOM_BUTTON_TOUCHSTRIP2_MODESWITCH,	WACOM_STATUS_LED_TOUCHSTRIP2 }
};

int libwacom_get_button_led_group (const WacomDevice *device, char button)
{
	int button_index, led_index;
	WacomButtonFlags button_flags;

	g_return_val_if_fail (device->num_buttons > 0, -1);
	g_return_val_if_fail (button >= 'A', -1);
	g_return_val_if_fail (button < 'A' + device->num_buttons, -1);

	button_index = button - 'A';
	button_flags = device->buttons[button_index];

	if (!(button_flags & WACOM_BUTTON_MODESWITCH))
		return -1;

	for (led_index = 0; led_index < device->num_leds; led_index++) {
		guint n;

		for (n = 0; n < G_N_ELEMENTS (button_status_leds); n++) {
			if ((button_flags & button_status_leds[n].button_flags) &&
			    (device->status_leds[led_index] == button_status_leds[n].status_leds)) {
				return led_index;
			}
		}
	}

	return WACOM_STATUS_LED_UNAVAILABLE;
}

int libwacom_is_builtin(const WacomDevice *device)
{
	return !!(libwacom_get_integration_flags (device) & WACOM_DEVICE_INTEGRATED_DISPLAY);
}

int libwacom_is_reversible(const WacomDevice *device)
{
	return !!(device->features & FEATURE_REVERSIBLE);
}

int libwacom_has_touchswitch(const WacomDevice *device)
{
	return !!(device->features & FEATURE_TOUCHSWITCH);
}

WacomIntegrationFlags libwacom_get_integration_flags (const WacomDevice *device)
{
	/* "unset" is for internal use only */
	if (device->integration_flags == WACOM_DEVICE_INTEGRATED_UNSET)
		return WACOM_DEVICE_INTEGRATED_NONE;

	return device->integration_flags;
}

WacomBusType libwacom_get_bustype(const WacomDevice *device)
{
	g_return_val_if_fail(device->match >= 0, -1);
	g_return_val_if_fail(device->match < device->nmatches, -1);
	return device->matches[device->match]->bus;
}

WacomButtonFlags
libwacom_get_button_flag(const WacomDevice *device, char button)
{
	int index;

	g_return_val_if_fail (device->num_buttons > 0, WACOM_BUTTON_NONE);
	g_return_val_if_fail (button >= 'A', WACOM_BUTTON_NONE);
	g_return_val_if_fail (button < 'A' + device->num_buttons, WACOM_BUTTON_NONE);

	index = button - 'A';

	return device->buttons[index];
}

const WacomStylus *libwacom_stylus_get_for_id (const WacomDeviceDatabase *db, int id)
{
	return g_hash_table_lookup (db->stylus_ht, GINT_TO_POINTER(id));
}

int libwacom_stylus_get_id (const WacomStylus *stylus)
{
	return stylus->id;
}

const char *libwacom_stylus_get_name (const WacomStylus *stylus)
{
	return stylus->name;
}

int libwacom_stylus_get_num_buttons (const WacomStylus *stylus)
{
	if (stylus->num_buttons == -1) {
		g_warning ("Stylus '0x%x' has no number of buttons defined, falling back to 2", stylus->id);
		return 2;
	}
	return stylus->num_buttons;
}

int libwacom_stylus_has_eraser (const WacomStylus *stylus)
{
	return stylus->has_eraser;
}

int libwacom_stylus_is_eraser (const WacomStylus *stylus)
{
	return stylus->is_eraser;
}

int libwacom_stylus_has_lens (const WacomStylus *stylus)
{
	return stylus->has_lens;
}

int libwacom_stylus_has_wheel (const WacomStylus *stylus)
{
	return stylus->has_wheel;
}

WacomAxisTypeFlags libwacom_stylus_get_axes (const WacomStylus *stylus)
{
	return stylus->axes;
}

WacomStylusType libwacom_stylus_get_type (const WacomStylus *stylus)
{
	if (stylus->type == WSTYLUS_UNKNOWN) {
		g_warning ("Stylus '0x%x' has no type defined, falling back to 'General'", stylus->id);
		return WSTYLUS_GENERAL;
	}
	return stylus->type;
}

void
libwacom_print_stylus_description (int fd, const WacomStylus *stylus)
{
	const char *type;
	WacomAxisTypeFlags axes;

	dprintf(fd, "[%#x]\n",	libwacom_stylus_get_id(stylus));
	dprintf(fd, "Name=%s\n",	libwacom_stylus_get_name(stylus));
	dprintf(fd, "Buttons=%d\n",	libwacom_stylus_get_num_buttons(stylus));
	dprintf(fd, "HasEraser=%s\n", libwacom_stylus_has_eraser(stylus) ? "true" : "false");
	dprintf(fd, "IsEraser=%s\n",	libwacom_stylus_is_eraser(stylus) ? "true" : "false");
	dprintf(fd, "HasLens=%s\n",	libwacom_stylus_has_lens(stylus) ? "true" : "false");
	dprintf(fd, "HasWheel=%s\n",	libwacom_stylus_has_wheel(stylus) ? "true" : "false");
	axes = libwacom_stylus_get_axes(stylus);
	dprintf(fd, "Axes=");
	if (axes & WACOM_AXIS_TYPE_TILT)
		dprintf(fd, "Tilt;");
	if (axes & WACOM_AXIS_TYPE_ROTATION_Z)
		dprintf(fd, "RotationZ;");
	if (axes & WACOM_AXIS_TYPE_DISTANCE)
		dprintf(fd, "Distance;");
	if (axes & WACOM_AXIS_TYPE_PRESSURE)
		dprintf(fd, "Pressure;");
	if (axes & WACOM_AXIS_TYPE_SLIDER)
		dprintf(fd, "Slider;");
	dprintf(fd, "\n");

	switch(libwacom_stylus_get_type(stylus)) {
		case WSTYLUS_UNKNOWN:	type = "Unknown";	 break;
		case WSTYLUS_GENERAL:	type = "General";	 break;
		case WSTYLUS_INKING:	type = "Inking";	 break;
		case WSTYLUS_AIRBRUSH:	type = "Airbrush";	 break;
		case WSTYLUS_CLASSIC:	type = "Classic";	 break;
		case WSTYLUS_MARKER:	type = "Marker";	 break;
		case WSTYLUS_STROKE:	type = "Stroke";	 break;
		case WSTYLUS_PUCK:	type = "Puck";		break;
		default:		g_assert_not_reached();	break;
	}

	dprintf(fd, "Type=%s\n", type);
}

void libwacom_stylus_destroy(WacomStylus *stylus)
{
	g_free (stylus->name);
	g_free (stylus);
}


const char *libwacom_match_get_name(const WacomMatch *match)
{
	return match->name;
}

WacomBusType libwacom_match_get_bustype(const WacomMatch *match)
{
	return match->bus;
}

uint32_t libwacom_match_get_product_id(const WacomMatch *match)
{
	return match->product_id;
}

uint32_t libwacom_match_get_vendor_id(const WacomMatch *match)
{
	return match->vendor_id;
}

const char* libwacom_match_get_match_string(const WacomMatch *match)
{
	return match->match;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
