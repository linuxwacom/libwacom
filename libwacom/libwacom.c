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
#include <string.h>
#include <gudev/gudev.h>

WacomDeviceData*
libwacom_new_devicedata(void)
{
    WacomDeviceData *device;

    device = calloc(1, sizeof(*device));

    return device;
}

static int
libwacom_ref_device(WacomDevice *device, int fallback, int vendor_id, int product_id, WacomBusType bus)
{
    int i;

    for (i = 0; i < device->nentries; i++) {
        if (device->database[i]->vendor_id == vendor_id &&
            device->database[i]->product_id == product_id &&
            device->database[i]->bus == bus) {
            device->ref = device->database[i];
            break;
        }
    }

    if (device->ref == NULL && fallback) {
        for (i = 0; i < device->nentries; i++) {
            if (device->database[i]->vendor_id == 0 &&
                device->database[i]->product_id == 0 &&
                device->database[i]->bus == 0) {
                device->ref = device->database[i];
                break;
            }
        }
    }

    return !!device->ref;
}

static int
get_device_info (const char   *path,
		 int          *vendor_id,
		 int          *product_id,
		 WacomBusType *bus,
		 WacomError   *error)
{
	GUdevClient *client;
	GUdevDevice *device;
	const char * const subsystems[] = { "input", NULL };
	gboolean retval;
	const char *bus_str;

	retval = FALSE;
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
	/* Poke the parent device for Bluetooth models */
	if (bus_str == NULL) {
		GUdevDevice *parent;

		parent = g_udev_device_get_parent (device);

		g_object_unref (device);
		device = parent;
		bus_str = g_udev_device_get_property (device, "ID_BUS");
		if (bus_str == NULL) {
			libwacom_error_set(error, WERROR_INVALID_PATH, "Could not find bus property for '%s' in udev", path);
			goto bail;
		}
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

		product_str = g_udev_device_get_property (device, "PRODUCT");

		/* FIXME, parse that:
		 * E: PRODUCT=5/56a/81/100
		 * into:
		 * vendor 0x56a
		 * product 0x81
		 */
		*vendor_id = 0x56a;
		*product_id = 0x81;
	} else if (*bus == WBUSTYPE_SERIAL) {
		libwacom_error_set(error, WERROR_UNKNOWN_MODEL, "Unimplemented serial bus");
		/* FIXME implement */
	} else {
		libwacom_error_set(error, WERROR_UNKNOWN_MODEL, "Unsupported bus '%s'", bus_str);
	}

	if (*bus != WBUSTYPE_UNKNOWN &&
	    vendor_id != 0 &&
	    product_id != 0)
		retval = TRUE;

bail:
	if (device != NULL)
		g_object_unref (device);
	if (client != NULL)
		g_object_unref (client);
	return retval;
}

WacomDevice*
libwacom_new_from_path(const char *path, int fallback, WacomError *error)
{
    WacomDevice *device;
    int vendor_id, product_id;
    WacomBusType bus;

    if (!path) {
        libwacom_error_set(error, WERROR_INVALID_PATH, "path is NULL");
        return NULL;
    }

    device = calloc(1, sizeof(*device));
    if (!device) {
        libwacom_error_set(error, WERROR_BAD_ALLOC, NULL);
        return NULL;
    }

    if (!get_device_info (path, &vendor_id, &product_id, &bus, error))
        return NULL;

    if (!libwacom_load_database(device)) {
        libwacom_error_set(error, WERROR_BAD_ALLOC, "Could not load database");
        libwacom_destroy(&device);
    } else if (!libwacom_ref_device(device, fallback, vendor_id, product_id, bus)) {
        libwacom_error_set(error, WERROR_UNKNOWN_MODEL, NULL);
        libwacom_destroy(&device);
    }

    return device;
}

WacomDevice*
libwacom_new_from_usbid(int vendor_id, int product_id, WacomError *error)
{
    WacomDevice *device;
    device = calloc(1, sizeof(*device));
    if (!device) {
        libwacom_error_set(error, WERROR_BAD_ALLOC, NULL);
        return NULL;
    }

    if (!libwacom_load_database(device)) {
        libwacom_error_set(error, WERROR_BAD_ALLOC, "Could not load database");
        libwacom_destroy(&device);
    } else if (!libwacom_ref_device(device, 0, vendor_id, product_id, WBUSTYPE_USB)) {
        libwacom_error_set(error, WERROR_UNKNOWN_MODEL, NULL);
        libwacom_destroy(&device);
    }

    return device;
}

void
libwacom_destroy(WacomDevice **device)
{
    WacomDevice *d;

    if (!device || !*device)
        return;

    d = *device;

    while (d->nentries--)
        free(d->database[d->nentries]);

    free(d->database);
    free(d);

    *device = NULL;
}

const char* libwacom_get_vendor(WacomDevice *device)
{
    return device->ref->vendor;
}

int libwacom_get_vendor_id(WacomDevice *device)
{
    return device->ref->vendor_id;
}

const char* libwacom_get_product(WacomDevice *device)
{
    return device->ref->product;
}

int libwacom_get_product_id(WacomDevice *device)
{
    return device->ref->product_id;
}

int libwacom_get_width(WacomDevice *device)
{
    return device->ref->width;
}

int libwacom_get_height(WacomDevice *device)
{
    return device->ref->height;
}

WacomClass
libwacom_get_class(WacomDevice *device)
{
    return device->ref->cls;
}

int libwacom_has_stylus(WacomDevice *device)
{
    return !!(device->ref->features & FEATURE_STYLUS);
}

int libwacom_has_touch(WacomDevice *device)
{
    return !!(device->ref->features & FEATURE_TOUCH);
}

int libwacom_get_num_buttons(WacomDevice *device)
{
    return device->ref->num_buttons;
}

int libwacom_has_ring(WacomDevice *device)
{
    return !!(device->ref->features & FEATURE_RING);
}

int libwacom_has_ring2(WacomDevice *device)
{
    return !!(device->ref->features & FEATURE_RING2);
}

int libwacom_has_vstrip(WacomDevice *device)
{
    return !!(device->ref->features & FEATURE_VSTRIP);
}

int libwacom_has_hstrip(WacomDevice *device)
{
    return !!(device->ref->features & FEATURE_HSTRIP);
}

int libwacom_is_builtin(WacomDevice *device)
{
    return !!(device->ref->features & FEATURE_BUILTIN);
}

int libwacom_is_reversible(WacomDevice *device)
{
    return !!(device->ref->features & FEATURE_REVERSIBLE);
}

WacomBusType libwacom_get_bustype(WacomDevice *device)
{
    return device->ref->bus;
}

/* vim :noexpandtab shiftwidth=8: */
