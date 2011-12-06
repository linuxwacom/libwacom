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

WacomDeviceData*
libwacom_new_devicedata(void)
{
    WacomDeviceData *device;

    device = calloc(1, sizeof(*device));

    return device;
}

static int
libwacom_ref_device(WacomDevice *device, int vendor_id, int product_id, WacomBusType bus)
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

    return !!device->ref;
}

WacomDevice*
libwacom_new_from_path(const char *path, WacomError *error)
{
    WacomDevice *device;
    int vendor_id = 0, product_id = 0;
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

    /* FIXME: open path, read device information */
    bus = WBUSTYPE_UNKNOWN;

    if (!libwacom_load_database(device)) {
        libwacom_error_set(error, WERROR_BAD_ALLOC, "Could not load database");
        libwacom_destroy(&device);
    } else if (!libwacom_ref_device(device, vendor_id, product_id, bus)) {
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
    } else if (!libwacom_ref_device(device, vendor_id, product_id, WBUSTYPE_USB)) {
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

enum WacomClass
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

WacomBusType libwacom_get_bustype(WacomDevice *device)
{
    return device->ref->bus;
}

/* vim :noexpandtab shiftwidth=8: */
