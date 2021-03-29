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

#include "config.h"

#include "libwacomint.h"
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <gudev/gudev.h>

#include <linux/input-event-codes.h>

#if !HAVE_G_MEMDUP2
#define g_memdup2 g_memdup
#endif

static const WacomDevice *
libwacom_get_device(const WacomDeviceDatabase *db, const char *match)
{
	return (WacomDevice *) g_hash_table_lookup (db->device_ht, match);
}

static gboolean
is_tablet (GUdevDevice *device)
{
	return g_udev_device_get_property_as_boolean (device, "ID_INPUT_TABLET");
}

static gboolean
is_touchpad (GUdevDevice *device)
{
	return g_udev_device_get_property_as_boolean (device, "ID_INPUT_TOUCHPAD");
}


static gboolean
is_tablet_or_touchpad (GUdevDevice *device)
{
	return is_touchpad (device) || is_tablet (device);
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
	if (parent)
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

	while (parent && subsystem &&
	       (g_str_equal(subsystem, "input") || g_str_equal (subsystem, "hid"))) {
		GUdevDevice *old_parent = parent;
		parent = g_udev_device_get_parent (old_parent);
		if (parent)
			subsystem = g_udev_device_get_subsystem (parent);
		g_object_unref (old_parent);
	}

	if (parent) {
		if (subsystem && (g_str_equal(subsystem, "tty") || g_str_equal(subsystem, "serio")))
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
		if (!parent)
			goto out;
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
		if (is_touchpad (device))
			goto out;

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

static WacomDevice *
libwacom_copy(const WacomDevice *device)
{
	WacomDevice *d;
	GHashTableIter iter;
	gpointer k, v;

	d = g_new0 (WacomDevice, 1);
	g_atomic_int_inc(&d->refcnt);
	d->name = g_strdup (device->name);
	d->model_name = g_strdup (device->model_name);
	d->width = device->width;
	d->height = device->height;
	d->integration_flags = device->integration_flags;
	d->layout = g_strdup(device->layout);
	d->matches = g_array_sized_new(TRUE, TRUE, sizeof(WacomDevice*),
				       device->matches->len);
	for (guint i = 0; i < device->matches->len; i++) {
		WacomMatch *m = g_array_index(device->matches, WacomMatch*, i);
		libwacom_match_ref(m);
		g_array_append_val(d->matches, m);
	}
	d->match = libwacom_match_ref(device->match);
	if (device->paired)
		d->paired = libwacom_match_ref(device->paired);
	d->cls = device->cls;
	d->num_strips = device->num_strips;
	d->features = device->features;
	d->strips_num_modes = device->strips_num_modes;
	d->ring_num_modes = device->ring_num_modes;
	d->ring2_num_modes = device->ring2_num_modes;
	d->styli = g_array_sized_new(FALSE, FALSE, sizeof(int),
				     device->styli->len);
	for (guint i = 0; i < device->styli->len; i++) {
		int id = g_array_index(device->styli, int, i);
		g_array_append_val(d->styli, id);
	}
	d->status_leds = g_array_sized_new(FALSE, FALSE,
					   sizeof(WacomStatusLEDs),
					   device->status_leds->len);
	for (guint i = 0; i < device->status_leds->len; i++) {
		g_array_append_val(d->status_leds,
				   g_array_index(device->status_leds, WacomStatusLEDs, i));
	}

	d->buttons = g_hash_table_new_full(g_direct_hash, g_direct_equal,
					   NULL, g_free);
	g_hash_table_iter_init(&iter, device->buttons);
	while (g_hash_table_iter_next(&iter, &k, &v)) {
		WacomButton *a = v;
		WacomButton *b = g_memdup2(a, sizeof(WacomButton));
		g_hash_table_insert(d->buttons, k, b);
	}
	return d;
}

static bool
match_is_equal(const WacomMatch *a, const WacomMatch *b)
{
	return g_str_equal(a->match, b->match);
}

static bool
matches_are_equal(const WacomDevice *a, const WacomDevice *b)
{
	const WacomMatch **ma, **mb, **match_a, **match_b;

	ma = libwacom_get_matches(a);
	mb = libwacom_get_matches(b);

	for (match_a = ma; *match_a; match_a++) {
		int found = 0;
		for (match_b = mb; !found && *match_b; match_b++) {
			if (match_is_equal(*match_a, *match_b))
				found = 1;
		}
		if (!found)
			return false;
	}

	return true;
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

LIBWACOM_EXPORT int
libwacom_compare(const WacomDevice *a, const WacomDevice *b, WacomCompareFlags flags)
{
	GHashTableIter iter;
	gpointer k, v;

	g_return_val_if_fail(a || b, 0);

	if (!a || !b)
		return 1;

	if (a == b)
		return 0;

	if (!g_str_equal(a->name, b->name))
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

	if (g_hash_table_size(a->buttons) != g_hash_table_size(b->buttons))
		return 1;

	if (a->styli->len != b->styli->len)
		return 1;

	if (memcmp(a->styli->data, b->styli->data, sizeof(int) * a->styli->len) != 0)
		return 1;

	if (a->status_leds->len != b->status_leds->len)
		return 1;

	if (memcmp(a->status_leds->data, b->status_leds->data,
		   sizeof(WacomStatusLEDs) * a->status_leds->len) != 0)
		return 1;

	g_hash_table_iter_init(&iter, a->buttons);
	while (g_hash_table_iter_next(&iter, &k, &v)) {
		WacomButton *ba = v;
		WacomButton *bb = g_hash_table_lookup(b->buttons, k);

		if (!bb || ba->flags != bb->flags || ba->code != bb->code)
			return 1;
	}

	if ((a->paired == NULL && b->paired != NULL) ||
	    (a->paired != NULL && b->paired == NULL) ||
	    (a->paired && b->paired && !match_is_equal(a->paired, b->paired)))
		return 1;

	if ((flags & WCOMPARE_MATCHES) && !matches_are_equal(a, b))
		return 1;
	else if (!match_is_equal(a->match, b->match))
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

LIBWACOM_EXPORT WacomDevice*
libwacom_new_from_path(const WacomDeviceDatabase *db, const char *path, WacomFallbackFlags fallback, WacomError *error)
{
	int vendor_id, product_id;
	WacomBusType bus;
	const WacomDevice *device;
	WacomDevice *ret = NULL;
	WacomIntegrationFlags integration_flags;
	char *name, *match_name;
	WacomMatch *match;

	switch (fallback) {
		case WFALLBACK_NONE:
		case WFALLBACK_GENERIC:
			break;
		default:
			libwacom_error_set(error, WERROR_BUG_CALLER, "invalid fallback flags");
			return NULL;
	}

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

	if (device == NULL) {
		if (fallback == WFALLBACK_NONE)
			goto out;

		/* WFALLBACK_GENERIC */
		device = libwacom_get_device(db, "generic");
		if (device == NULL)
			goto out;

		ret = libwacom_copy(device);

		if (name != NULL) {
			g_free (ret->name);
			ret->name = g_strdup(name);
		}
	} else {
		ret = libwacom_copy(device);
	}

	/* for multiple-match devices, set to the one we requested */
	match = libwacom_match_new(match_name, bus, vendor_id, product_id);
	libwacom_set_default_match(ret, match);
	libwacom_match_unref(match);

	/* if unset, use the kernel flags. Could be unset as well. */
	if (device && ret->integration_flags == WACOM_DEVICE_INTEGRATED_UNSET)
		ret->integration_flags = integration_flags;

out:
	g_free (name);
	if (ret == NULL)
		libwacom_error_set(error, WERROR_UNKNOWN_MODEL, NULL);
	return ret;
}

LIBWACOM_EXPORT WacomDevice*
libwacom_new_from_usbid(const WacomDeviceDatabase *db, int vendor_id, int product_id, WacomError *error)
{
	const WacomDevice *device;

	if (!db) {
		libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
		return NULL;
	}

	device = libwacom_new(db, NULL, vendor_id, product_id, WBUSTYPE_USB, error);
	if (!device)
		device = libwacom_new(db, NULL, vendor_id, product_id, WBUSTYPE_I2C, error);
	if (!device)
		device = libwacom_new(db, NULL, vendor_id, product_id, WBUSTYPE_BLUETOOTH, error);

	if (device)
		return libwacom_copy(device);

	libwacom_error_set(error, WERROR_UNKNOWN_MODEL, NULL);
	return NULL;
}

LIBWACOM_EXPORT WacomDevice*
libwacom_new_from_name(const WacomDeviceDatabase *db, const char *name, WacomError *error)
{
	const WacomDevice *device;
	GList *keys, *l;

	if (!db) {
		libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
		return NULL;
	}

	g_return_val_if_fail(name != NULL, NULL);

	device = NULL;
	keys = g_hash_table_get_values (db->device_ht);
	for (l = keys; l; l = l->next) {
		WacomDevice *d = l->data;

		if (g_str_equal(d->name, name)) {
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
	unsigned idx = 0;
	char buf[1024] = {0};

	if (!libwacom_has_stylus(device))
		return;

	styli = libwacom_get_supported_styli(device, &nstyli);

	for (i = 0; i < nstyli; i++) {
		/* 20 digits for a stylus are enough, right */
		assert(idx < sizeof(buf) - 20);

		idx += snprintf(buf + idx, 20, "%#x;", styli[i]);
	}

	dprintf(fd, "Styli=%s\n", buf);
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
		"Ring;",
		"Ring2;",
		"Touchstrip;",
		"Touchstrip2;"
	};
	int num_leds;
	const WacomStatusLEDs *status_leds;
	char buf[256] = {0};
	bool have_led = false;

	status_leds = libwacom_get_status_leds(device, &num_leds);

	snprintf(buf, sizeof(buf), "%s%s%s%s",
		 num_leds > 0 ? leds_name[status_leds[0]] : "",
		 num_leds > 1 ? leds_name[status_leds[1]] : "",
		 num_leds > 2 ? leds_name[status_leds[2]] : "",
		 num_leds > 3 ? leds_name[status_leds[3]] : "");
	have_led = num_leds > 0;

	dprintf(fd, "%sStatusLEDs=%s\n", have_led ? "" : "# ", buf);
}

static void print_button_flag_if(int fd, const WacomDevice *device, const char *label, int flag)
{
	int nbuttons = libwacom_get_num_buttons(device);
	char buf[nbuttons * 2 + 1];
	int idx = 0;
	char b;
	bool have_flag = false;

	for (b = 'A'; b < 'A' + nbuttons; b++) {
		if (libwacom_get_button_flag(device, b) & flag) {
			buf[idx++] = b;
			buf[idx++] = ';';
			have_flag = true;
		}
	}
	buf[idx] = '\0';
	dprintf(fd, "%s%s=%s\n", have_flag ? "" : "# ", label, buf);
}

static void print_button_evdev_codes(int fd, const WacomDevice *device)
{
	int nbuttons = libwacom_get_num_buttons(device);
	char b;
	char buf[1024] = {0};
	unsigned idx = 0;

	for (b = 'A'; b < 'A' + nbuttons; b++) {
		assert(idx < sizeof(buf) - 30);
		idx += snprintf(buf + idx, 30, "0x%x;",
				libwacom_get_button_evdev_code(device, b));
	}
	dprintf(fd, "EvdevCodes=%s\n", buf);
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
	print_button_evdev_codes(fd, device);
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

static void print_match(int fd, const WacomMatch *match)
{
	const char  *name       = libwacom_match_get_name(match);
	WacomBusType type	= libwacom_match_get_bustype(match);
	int          vendor     = libwacom_match_get_vendor_id(match);
	int          product    = libwacom_match_get_product_id(match);
	const char  *bus_name;

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

LIBWACOM_EXPORT void
libwacom_print_device_description(int fd, const WacomDevice *device)
{
	const WacomMatch **match;
	WacomClass class;
	const char *class_name;

	class  = device->cls;
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
	dprintf(fd, "ModelName=%s\n", libwacom_get_model_name(device) ? libwacom_get_model_name(device) : "");
	dprintf(fd, "DeviceMatch=");
	for (match = libwacom_get_matches(device); *match; match++)
		print_match(fd, *match);
	dprintf(fd, "\n");

	if (libwacom_get_paired_device(device)) {
		dprintf(fd, "PairedID=");
		print_match(fd, libwacom_get_paired_device(device));
		dprintf(fd, "\n");
	}

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
	dprintf(fd, "\n");

	print_buttons_for_device(fd, device);
}

WacomDevice *
libwacom_ref(WacomDevice *device)
{
	assert(device->refcnt >= 1);

	g_atomic_int_inc(&device->refcnt);
	return device;
}

WacomDevice *
libwacom_unref(WacomDevice *device)
{
	if (device == NULL)
		return NULL;

	assert(device->refcnt >= 1);

	if (!g_atomic_int_dec_and_test(&device->refcnt))
		return NULL;

	g_free (device->name);
	g_free (device->model_name);
	g_free (device->layout);
	if (device->paired)
		libwacom_match_unref(device->paired);
	for (guint i = 0; i < device->matches->len; i++)
		libwacom_match_unref(g_array_index(device->matches, WacomMatch*, i));
	g_array_free (device->matches, TRUE);
	libwacom_match_unref(device->match);
	g_array_free (device->styli, TRUE);
	g_array_free (device->status_leds, TRUE);
	if (device->buttons)
		g_hash_table_destroy (device->buttons);
	g_free (device);

	return NULL;
}

LIBWACOM_EXPORT void
libwacom_destroy(WacomDevice *device)
{
	libwacom_unref(device);
}

WacomMatch*
libwacom_match_ref(WacomMatch *match)
{
	g_atomic_int_inc(&match->refcnt);
	return match;
}

WacomMatch*
libwacom_match_unref(WacomMatch *match)
{
	if (match == NULL ||
	    !g_atomic_int_dec_and_test(&match->refcnt))
		return NULL;

	g_free (match->match);
	g_free (match->name);
	g_free (match);

	return NULL;
}

WacomMatch*
libwacom_match_new(const char *name, WacomBusType bus, int vendor_id, int product_id)
{
	WacomMatch *match;
	char *newmatch;

	match = g_malloc(sizeof(*match));
	match->refcnt = 1;
	if (name == NULL && bus == WBUSTYPE_UNKNOWN && vendor_id == 0 && product_id == 0)
		newmatch = g_strdup("generic");
	else
		newmatch = make_match_string(name, bus, vendor_id, product_id);

	match->match = newmatch;
	match->name = g_strdup(name);
	match->bus = bus;
	match->vendor_id = vendor_id;
	match->product_id = product_id;

	return match;
}

void
libwacom_add_match(WacomDevice *device, WacomMatch *newmatch)
{
	for (guint i = 0; i < device->matches->len; i++) {
		WacomMatch *m = g_array_index(device->matches, WacomMatch *, i);
		const char *matchstr = libwacom_match_get_match_string(m);

		if (g_str_equal(matchstr, newmatch->match)) {
			return;
		}
	}
	libwacom_match_ref(newmatch);
	g_array_append_val(device->matches, newmatch);
}

void
libwacom_set_default_match(WacomDevice *device, WacomMatch *newmatch)
{
	for (guint i = 0; i < device->matches->len; i++) {
		WacomMatch *m = g_array_index(device->matches, WacomMatch *, i);

		if (match_is_equal(m, newmatch)) {
			libwacom_match_unref(device->match);
			device->match = libwacom_match_ref(m);
			return;
		}
	}
	g_return_if_reached();
}

void
libwacom_remove_match(WacomDevice *device, WacomMatch *to_remove)
{
	for (guint i= 0; i < device->matches->len; i++) {
		WacomMatch *m = g_array_index(device->matches, WacomMatch*, i);
		if (match_is_equal(m, to_remove)) {
			WacomMatch *dflt = device->match;

			/* remove from list */
			g_array_remove_index(device->matches, i);

			/* now reset the default match if needed */
			if (match_is_equal(dflt, to_remove)) {
				WacomMatch *first = g_array_index(device->matches,
							      WacomMatch*,
							      0);
				libwacom_set_default_match(device, first);
			}

			libwacom_match_unref(to_remove);
			break;
		}
	}
}

LIBWACOM_EXPORT int
libwacom_get_vendor_id(const WacomDevice *device)
{
	g_return_val_if_fail(device->match, -1);
	return device->match->vendor_id;
}

LIBWACOM_EXPORT const char*
libwacom_get_name(const WacomDevice *device)
{
	return device->name;
}

LIBWACOM_EXPORT const char*
libwacom_get_model_name(const WacomDevice *device)
{
	return device->model_name;
}

LIBWACOM_EXPORT const char*
libwacom_get_layout_filename(const WacomDevice *device)
{
	return device->layout;
}

LIBWACOM_EXPORT int
libwacom_get_product_id(const WacomDevice *device)
{
	g_return_val_if_fail(device->match, -1);
	return device->match->product_id;
}

LIBWACOM_EXPORT const char*
libwacom_get_match(const WacomDevice *device)
{
	g_return_val_if_fail(device->match, NULL);
	return device->match->match;
}

LIBWACOM_EXPORT const WacomMatch**
libwacom_get_matches(const WacomDevice *device)
{
	return (const WacomMatch**)device->matches->data;
}

LIBWACOM_EXPORT const WacomMatch*
libwacom_get_paired_device(const WacomDevice *device)
{
	return (const WacomMatch*)device->paired;
}

LIBWACOM_EXPORT int
libwacom_get_width(const WacomDevice *device)
{
	return device->width;
}

LIBWACOM_EXPORT int
libwacom_get_height(const WacomDevice *device)
{
	return device->height;
}

LIBWACOM_EXPORT WacomClass
libwacom_get_class(const WacomDevice *device)
{
	return device->cls;
}

LIBWACOM_EXPORT int
libwacom_has_stylus(const WacomDevice *device)
{
	return !!(device->features & FEATURE_STYLUS);
}

LIBWACOM_EXPORT int
libwacom_has_touch(const WacomDevice *device)
{
	return !!(device->features & FEATURE_TOUCH);
}

LIBWACOM_EXPORT int
libwacom_get_num_buttons(const WacomDevice *device)
{
	return g_hash_table_size(device->buttons);
}

LIBWACOM_EXPORT const int *
libwacom_get_supported_styli(const WacomDevice *device, int *num_styli)
{
	*num_styli = device->styli->len;
	return (const int *)device->styli->data;
}

LIBWACOM_EXPORT int
libwacom_has_ring(const WacomDevice *device)
{
	return !!(device->features & FEATURE_RING);
}

LIBWACOM_EXPORT int
libwacom_has_ring2(const WacomDevice *device)
{
	return !!(device->features & FEATURE_RING2);
}

LIBWACOM_EXPORT int
libwacom_get_ring_num_modes(const WacomDevice *device)
{
	return device->ring_num_modes;
}

LIBWACOM_EXPORT int
libwacom_get_ring2_num_modes(const WacomDevice *device)
{
	return device->ring2_num_modes;
}

LIBWACOM_EXPORT int
libwacom_get_num_strips(const WacomDevice *device)
{
	return device->num_strips;
}

LIBWACOM_EXPORT int
libwacom_get_strips_num_modes(const WacomDevice *device)
{
	return device->strips_num_modes;
}

LIBWACOM_EXPORT const WacomStatusLEDs *
libwacom_get_status_leds(const WacomDevice *device, int *num_leds)
{
	*num_leds = device->status_leds->len;
	return (const WacomStatusLEDs*)device->status_leds->data;
}

static const struct {
	WacomButtonFlags button_flags;
	WacomStatusLEDs  status_leds;
} button_status_leds[] = {
	{ WACOM_BUTTON_RING_MODESWITCH,		WACOM_STATUS_LED_RING },
	{ WACOM_BUTTON_RING2_MODESWITCH,	WACOM_STATUS_LED_RING2 },
	{ WACOM_BUTTON_TOUCHSTRIP_MODESWITCH,	WACOM_STATUS_LED_TOUCHSTRIP },
	{ WACOM_BUTTON_TOUCHSTRIP2_MODESWITCH,	WACOM_STATUS_LED_TOUCHSTRIP2 }
};

LIBWACOM_EXPORT int
libwacom_get_button_led_group (const WacomDevice *device, char button)
{
	WacomButton *b = g_hash_table_lookup(device->buttons,
					     GINT_TO_POINTER(button));

	if (!(b->flags & WACOM_BUTTON_MODESWITCH))
		return -1;

	for (guint led_index = 0; led_index < device->status_leds->len; led_index++) {
		guint n;

		for (n = 0; n < G_N_ELEMENTS (button_status_leds); n++) {
			WacomStatusLEDs led = g_array_index(device->status_leds,
							    WacomStatusLEDs,
							    led_index);
			if ((b->flags & button_status_leds[n].button_flags) &&
			    (led == button_status_leds[n].status_leds)) {
				return led_index;
			}
		}
	}

	return WACOM_STATUS_LED_UNAVAILABLE;
}

LIBWACOM_EXPORT int
libwacom_is_builtin(const WacomDevice *device)
{
	return !!(libwacom_get_integration_flags (device) & WACOM_DEVICE_INTEGRATED_DISPLAY);
}

LIBWACOM_EXPORT int
libwacom_is_reversible(const WacomDevice *device)
{
	return !!(device->features & FEATURE_REVERSIBLE);
}

LIBWACOM_EXPORT int
libwacom_has_touchswitch(const WacomDevice *device)
{
	return !!(device->features & FEATURE_TOUCHSWITCH);
}

LIBWACOM_EXPORT WacomIntegrationFlags
libwacom_get_integration_flags (const WacomDevice *device)
{
	/* "unset" is for internal use only */
	if (device->integration_flags == WACOM_DEVICE_INTEGRATED_UNSET)
		return WACOM_DEVICE_INTEGRATED_NONE;

	return device->integration_flags;
}

LIBWACOM_EXPORT WacomBusType
libwacom_get_bustype(const WacomDevice *device)
{
	g_return_val_if_fail(device->match, -1);
	return device->match->bus;
}

LIBWACOM_EXPORT WacomButtonFlags
libwacom_get_button_flag(const WacomDevice *device, char button)
{
	WacomButton *b = g_hash_table_lookup(device->buttons,
					     GINT_TO_POINTER(button));


	return b ? b->flags : WACOM_BUTTON_NONE;
}

LIBWACOM_EXPORT int
libwacom_get_button_evdev_code(const WacomDevice *device, char button)
{
	WacomButton *b = g_hash_table_lookup(device->buttons,
					     GINT_TO_POINTER(button));

	return b ? b->code : 0;
}

LIBWACOM_EXPORT const
WacomStylus *libwacom_stylus_get_for_id (const WacomDeviceDatabase *db, int id)
{
	return g_hash_table_lookup (db->stylus_ht, GINT_TO_POINTER(id));
}

LIBWACOM_EXPORT int
libwacom_stylus_get_id (const WacomStylus *stylus)
{
	return stylus->id;
}

LIBWACOM_EXPORT const char *
libwacom_stylus_get_name (const WacomStylus *stylus)
{
	return stylus->name;
}

LIBWACOM_EXPORT const int *
libwacom_stylus_get_paired_ids(const WacomStylus *stylus, int *num_paired_ids)
{
	if (num_paired_ids)
		*num_paired_ids = stylus->paired_ids->len;
	return (const int*)stylus->paired_ids->data;
}

LIBWACOM_EXPORT int
libwacom_stylus_get_num_buttons (const WacomStylus *stylus)
{
	if (stylus->num_buttons == -1) {
		g_warning ("Stylus '0x%x' has no number of buttons defined, falling back to 2", stylus->id);
		return 2;
	}
	return stylus->num_buttons;
}

LIBWACOM_EXPORT int
libwacom_stylus_has_eraser (const WacomStylus *stylus)
{
	return stylus->has_eraser;
}

LIBWACOM_EXPORT int
libwacom_stylus_is_eraser (const WacomStylus *stylus)
{
	return libwacom_stylus_get_eraser_type(stylus) != WACOM_ERASER_NONE;
}

LIBWACOM_EXPORT int
libwacom_stylus_has_lens (const WacomStylus *stylus)
{
	return stylus->has_lens;
}

LIBWACOM_EXPORT int
libwacom_stylus_has_wheel (const WacomStylus *stylus)
{
	return stylus->has_wheel;
}

LIBWACOM_EXPORT WacomAxisTypeFlags
libwacom_stylus_get_axes (const WacomStylus *stylus)
{
	return stylus->axes;
}

LIBWACOM_EXPORT WacomStylusType
libwacom_stylus_get_type (const WacomStylus *stylus)
{
	if (stylus->type == WSTYLUS_UNKNOWN) {
		g_warning ("Stylus '0x%x' has no type defined, falling back to 'General'", stylus->id);
		return WSTYLUS_GENERAL;
	}
	return stylus->type;
}

LIBWACOM_EXPORT WacomEraserType
libwacom_stylus_get_eraser_type (const WacomStylus *stylus)
{
	return stylus->eraser_type;
}

LIBWACOM_EXPORT void
libwacom_print_stylus_description (int fd, const WacomStylus *stylus)
{
	const char *type;
	WacomAxisTypeFlags axes;
	const int *paired_ids;
	int count;
	int i;

	dprintf(fd, "[%#x]\n",		libwacom_stylus_get_id(stylus));
	dprintf(fd, "Name=%s\n",	libwacom_stylus_get_name(stylus));
	dprintf(fd, "Buttons=%d\n",	libwacom_stylus_get_num_buttons(stylus));
	dprintf(fd, "PairedIds=");
	paired_ids = libwacom_stylus_get_paired_ids(stylus, &count);
	for (i = 0; i < count; i++) {
		dprintf(fd, "%#x;", paired_ids[i]);
	}
	dprintf(fd, "\n");
	switch (libwacom_stylus_get_eraser_type(stylus)) {
		case WACOM_ERASER_UNKNOWN: type = "Unknown";       break;
		case WACOM_ERASER_NONE:    type = "None";          break;
		case WACOM_ERASER_INVERT:  type = "Invert";        break;
		case WACOM_ERASER_BUTTON:  type = "Button";        break;
		default:                   g_assert_not_reached(); break;
	}
	dprintf(fd, "EraserType=%s\n", type);
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
		case WSTYLUS_3D:	type = "3D";		break;
		case WSTYLUS_MOBILE:	type = "Mobile";	break;
		default:		g_assert_not_reached();	break;
	}

	dprintf(fd, "Type=%s\n", type);
}

WacomStylus*
libwacom_stylus_ref(WacomStylus *stylus)
{
	g_atomic_int_inc(&stylus->refcnt);

	return stylus;
}

WacomStylus*
libwacom_stylus_unref(WacomStylus *stylus)
{
	if (!g_atomic_int_dec_and_test(&stylus->refcnt))
		return NULL;

	g_free (stylus->name);
	g_free (stylus->group);
	if (stylus->paired_ids)
		g_array_free (stylus->paired_ids, TRUE);
	g_free (stylus);

	return NULL;
}

LIBWACOM_EXPORT const char *
libwacom_match_get_name(const WacomMatch *match)
{
	return match->name;
}

LIBWACOM_EXPORT WacomBusType
libwacom_match_get_bustype(const WacomMatch *match)
{
	return match->bus;
}

LIBWACOM_EXPORT uint32_t
libwacom_match_get_product_id(const WacomMatch *match)
{
	return match->product_id;
}

LIBWACOM_EXPORT uint32_t
libwacom_match_get_vendor_id(const WacomMatch *match)
{
	return match->vendor_id;
}

LIBWACOM_EXPORT const char*
libwacom_match_get_match_string(const WacomMatch *match)
{
	return match->match;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
