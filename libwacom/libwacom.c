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

static const WacomDevice *
libwacom_get_device(WacomDeviceDatabase *db, const char *match)
{
	return (WacomDevice *) g_hash_table_lookup (db->device_ht, match);
}

static gboolean
get_device_info (const char   *path,
		 int          *vendor_id,
		 int          *product_id,
		 char        **name,
		 WacomBusType *bus,
		 IsBuiltin    *builtin,
		 WacomError   *error)
{
	GUdevClient *client;
	GUdevDevice *device;
	const char * const subsystems[] = { "input", NULL };
	gboolean retval;
	const char *bus_str;
	const char *devname;

	retval = FALSE;
	*builtin = IS_BUILTIN_UNSET;
	*name = NULL;
	client = g_udev_client_new (subsystems);
	device = g_udev_client_query_by_device_file (client, path);
	if (device == NULL) {
		libwacom_error_set(error, WERROR_INVALID_PATH, "Could not find device '%s' in udev", path);
		goto bail;
	}

	if (g_udev_device_get_property_as_boolean (device, "ID_INPUT_TABLET") == FALSE) {
		libwacom_error_set(error, WERROR_INVALID_PATH, "Device '%s' is not a tablet", path);
		goto bail;
	}

	bus_str = g_udev_device_get_property (device, "ID_BUS");
	/* Serial devices are weird */
	if (bus_str == NULL) {
		if (g_strcmp0 (g_udev_device_get_subsystem (device), "tty") == 0)
			bus_str = "serial";
	}
	/* Poke the parent device for Bluetooth models */
	if (bus_str == NULL) {
		GUdevDevice *parent;

		parent = g_udev_device_get_parent (device);

		g_object_unref (device);
		device = parent;
		bus_str = "bluetooth";
	}

	/* Is the device builtin? */
	devname = g_udev_device_get_name (device);
	if (devname != NULL) {
		char *sysfs_path, *contents;

		sysfs_path = g_build_filename ("/sys/class/input", devname, "device/properties", NULL);
		if (g_file_get_contents (sysfs_path, &contents, NULL, NULL)) {
			int flag;
			/* 0x01: POINTER flag
			 * 0x02: DIRECT flag */
			flag = atoi(contents);
			*builtin = (flag & 0x02) == 0x02 ? IS_BUILTIN_TRUE : IS_BUILTIN_FALSE;
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

	*bus = bus_from_str (bus_str);
	if (*bus == WBUSTYPE_USB) {
		const char *vendor_str, *product_str;

		vendor_str = g_udev_device_get_property (device, "ID_VENDOR_ID");
		product_str = g_udev_device_get_property (device, "ID_MODEL_ID");

		*vendor_id = strtol (vendor_str, NULL, 16);
		*product_id = strtol (product_str, NULL, 16);
	} else if (*bus == WBUSTYPE_BLUETOOTH) {
		const char *product_str;
		int garbage;

		/* Parse that:
		 * E: PRODUCT=5/56a/81/100
		 * into:
		 * vendor 0x56a
		 * product 0x81 */
		product_str = g_udev_device_get_property (device, "PRODUCT");
		g_assert (product_str);
		if (sscanf(product_str, "%d/%x/%x/%d", &garbage, vendor_id, product_id, &garbage) != 4) {
			libwacom_error_set(error, WERROR_UNKNOWN_MODEL, "Unimplemented serial bus");
			goto bail;
		}
	} else if (*bus == WBUSTYPE_SERIAL) {
		/* FIXME This matches the declaration in serial-wacf004.tablet
		 * Might not be good enough though */
		vendor_id = 0;
		product_id = 0;
	} else {
		libwacom_error_set(error, WERROR_UNKNOWN_MODEL, "Unsupported bus '%s'", bus_str);
		goto bail;
	}

	if (*bus != WBUSTYPE_UNKNOWN &&
	    vendor_id != 0 &&
	    product_id != 0)
		retval = TRUE;
	/* The serial bus uses 0:0 as the vid/pid */
	if (*bus == WBUSTYPE_SERIAL)
		retval = TRUE;

bail:
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

	d = g_new0 (WacomDevice, 1);
	d->name = g_strdup (device->name);
	d->width = device->width;
	d->height = device->height;
	d->match = g_strdup (device->match);
	d->vendor_id = device->vendor_id;
	d->product_id = device->product_id;
	d->cls = device->cls;
	d->bus = device->bus;
	d->num_strips = device->num_strips;
	d->features = device->features;
	d->ring_num_modes = device->ring_num_modes;
	d->ring2_num_modes = device->ring2_num_modes;
	d->num_styli = device->num_styli;
	d->supported_styli = g_memdup (device->supported_styli, sizeof(int) * device->num_styli);
	d->num_buttons = device->num_buttons;
	d->buttons = g_memdup (device->buttons, sizeof(WacomButtonFlags) * device->num_buttons);
	return d;
}

static const WacomDevice *
libwacom_new (WacomDeviceDatabase *db, int vendor_id, int product_id, WacomBusType bus, WacomError *error)
{
    const WacomDevice *device;
    char *match;

    if (!db) {
        libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
        return NULL;
    }

    match = g_strdup_printf ("%s:%04x:%04x", bus_to_str (bus), vendor_id, product_id);
    device = libwacom_get_device(db, match);
    g_free (match);

    return device;
}

WacomDevice*
libwacom_new_from_path(WacomDeviceDatabase *db, const char *path, int fallback, WacomError *error)
{
    int vendor_id, product_id;
    WacomBusType bus;
    const WacomDevice *device;
    WacomDevice *ret;
    IsBuiltin builtin;
    char *name;

    if (!db) {
        libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
        return NULL;
    }

    if (!path) {
        libwacom_error_set(error, WERROR_INVALID_PATH, "path is NULL");
        return NULL;
    }

    if (!get_device_info (path, &vendor_id, &product_id, &name, &bus, &builtin, error))
        return NULL;

    device = libwacom_new (db, vendor_id, product_id, bus, error);
    if (device != NULL)
	    ret = libwacom_copy(device);
    else if (!fallback)
	    goto bail;

    if (device == NULL && fallback) {
	    device = libwacom_get_device(db, "generic");
	    if (device == NULL)
		    goto bail;

	    ret = libwacom_copy(device);

	    if (name != NULL) {
		    g_free (ret->name);
		    ret->name = name;
	    }
	    ret->vendor_id = vendor_id;
	    ret->product_id = product_id;
	    ret->bus = bus;
	    g_free (ret->match);
	    ret->match = g_strdup_printf ("%s:0x%x:0x%x", bus_to_str (bus), vendor_id, product_id);
    } else {
	    g_free (name);
    }

    if (device) {
	    if (builtin == IS_BUILTIN_TRUE)
	        ret->features |= FEATURE_BUILTIN;
	    else if (builtin == IS_BUILTIN_FALSE)
	        ret->features &= ~FEATURE_BUILTIN;

	    return ret;
    }

bail:
    g_free (name);
    libwacom_error_set(error, WERROR_UNKNOWN_MODEL, NULL);
    return NULL;
}

WacomDevice*
libwacom_new_from_usbid(WacomDeviceDatabase *db, int vendor_id, int product_id, WacomError *error)
{
    const WacomDevice *device;

    if (!db) {
        libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
        return NULL;
    }

    device = libwacom_new(db, vendor_id, product_id, WBUSTYPE_USB, error);

    if (device)
	    return libwacom_copy(device);

    libwacom_error_set(error, WERROR_UNKNOWN_MODEL, NULL);
    return NULL;
}

WacomDevice*
libwacom_new_from_name(WacomDeviceDatabase *db, const char *name, WacomError *error)
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

void
libwacom_destroy(WacomDevice *device)
{
	g_free (device->name);

	g_free (device->match);
	g_free (device->supported_styli);
	g_free (device->buttons);
	g_free (device);
}

int libwacom_get_vendor_id(WacomDevice *device)
{
    return device->vendor_id;
}

const char* libwacom_get_name(WacomDevice *device)
{
    return device->name;
}

int libwacom_get_product_id(WacomDevice *device)
{
    return device->product_id;
}

const char* libwacom_get_match(WacomDevice *device)
{
    /* FIXME make sure this only returns the first match
     * when we implement multiple matching */
    return device->match;
}

int libwacom_get_width(WacomDevice *device)
{
    return device->width;
}

int libwacom_get_height(WacomDevice *device)
{
    return device->height;
}

WacomClass
libwacom_get_class(WacomDevice *device)
{
    return device->cls;
}

int libwacom_has_stylus(WacomDevice *device)
{
    return !!(device->features & FEATURE_STYLUS);
}

int libwacom_has_touch(WacomDevice *device)
{
    return !!(device->features & FEATURE_TOUCH);
}

int libwacom_get_num_buttons(WacomDevice *device)
{
    return device->num_buttons;
}

int *libwacom_get_supported_styli(WacomDevice *device, int *num_styli)
{
    *num_styli = device->num_styli;
    return device->supported_styli;
}

int libwacom_has_ring(WacomDevice *device)
{
    return !!(device->features & FEATURE_RING);
}

int libwacom_has_ring2(WacomDevice *device)
{
    return !!(device->features & FEATURE_RING2);
}

int libwacom_get_ring_num_modes(WacomDevice *device)
{
    return device->ring_num_modes;
}

int libwacom_get_ring2_num_modes(WacomDevice *device)
{
    return device->ring2_num_modes;
}

int libwacom_get_num_strips(WacomDevice *device)
{
    return device->num_strips;
}

int libwacom_is_builtin(WacomDevice *device)
{
    return !!(device->features & FEATURE_BUILTIN);
}

int libwacom_is_reversible(WacomDevice *device)
{
    return !!(device->features & FEATURE_REVERSIBLE);
}

WacomBusType libwacom_get_bustype(WacomDevice *device)
{
    return device->bus;
}

WacomButtonFlags
libwacom_get_button_flag(WacomDevice *device,
			 char         button)
{
	int index;

	g_return_val_if_fail (device->num_buttons > 0, WACOM_BUTTON_NONE);
	g_return_val_if_fail (button >= 'A', WACOM_BUTTON_NONE);
	g_return_val_if_fail (button < 'A' + device->num_buttons, WACOM_BUTTON_NONE);

	index = button - 'A';

	return device->buttons[index];
}

const WacomStylus *libwacom_stylus_get_for_id (WacomDeviceDatabase *db, int id)
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

WacomStylusType libwacom_stylus_get_type (const WacomStylus *stylus)
{
	if (stylus->type == WSTYLUS_UNKNOWN) {
		g_warning ("Stylus '0x%x' has no type defined, falling back to 'General'", stylus->id);
		return WSTYLUS_GENERAL;
	}
	return stylus->type;
}

void libwacom_stylus_destroy(WacomStylus *stylus)
{
	g_free (stylus->name);
	g_free (stylus);
}

/* vim :noexpandtab shiftwidth=8: */
