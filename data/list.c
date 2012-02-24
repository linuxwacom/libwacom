/*
 * Copyright © 2012 Red Hat, Inc.
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
 *        Olivier Fourdan (ofourdan@redhat.com)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include "libwacom.h"

static void print_udev_header (void)
{
    printf ("# udev rules for libwacom supported devices\n");
    printf ("\n");
    printf ("ACTION!=\"add|change\", GOTO=\"libwacom_end\"\n");
    printf ("KERNEL!=\"event[0-9]*\", GOTO=\"libwacom_end\"\n");
    printf ("\n");
}

static void print_udev_entry (WacomDevice *device)
{
    WacomBusType type       = libwacom_get_bustype (device);
    int          vendor     = libwacom_get_vendor_id (device);
    int          product    = libwacom_get_product_id (device);
    int          has_touch  = libwacom_has_touch (device);
    static char *touchpad;

    if (has_touch)
        touchpad = ", ID_INPUT_TOUCHPAD=\"1\"";
    else
        touchpad = "";

    switch (type) {
        case WBUSTYPE_USB:
            printf ("ENV{ID_BUS}==\"usb\", ENV{ID_VENDOR_ID}==\"%04x\", ENV{ID_MODEL_ID}==\"%04x\", ENV{ID_INPUT}=\"1\", ENV{ID_INPUT_TABLET}=\"1\"%s\n", vendor, product, touchpad);
            break;
        case WBUSTYPE_BLUETOOTH:
            printf ("ENV{ID_BUS}==\"bluetooth\", ENV{ID_VENDOR_ID}==\"%04x\", ENV{ID_MODEL_ID}==\"%04x\", ENV{ID_INPUT}=\"1\", ENV{ID_INPUT_TABLET}=\"1\"%s\n", vendor, product, touchpad);
            break;
        default:
            /* Not sure how to deal with serials  */
            break;
    }
}

static void print_udev_trailer (void)
{
    printf ("\n");
    printf ("# Match all serial wacom tablets with a serial ID starting with WACf\n");
    printf ("ENV{ID_BUS}==\"tty|pnp\", ATTRS{id}==\"WACf*\", ENV{ID_INPUT}=\"1\", ENV{ID_INPUT_TABLET}=\"1\"\n");
    printf ("ENV{ID_BUS}==\"tty|pnp\", ATTRS{id}==\"FUJ*\", ENV{ID_INPUT}=\"1\", ENV{ID_INPUT_TABLET}=\"1\"\n");
    printf ("\n");
    printf ("LABEL=\"libwacom_end\"\n");
}


int main(int argc, char **argv)
{
    WacomDeviceDatabase *db;
    WacomDevice **list, **p;

    db = libwacom_database_new_for_path(TOPSRCDIR"/data");

    list = libwacom_list_devices_from_database(db, NULL);

    print_udev_header ();
    for (p = list; *p; p++)
        print_udev_entry ((WacomDevice *) *p);
    print_udev_trailer ();

    libwacom_database_destroy (db);

    return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
