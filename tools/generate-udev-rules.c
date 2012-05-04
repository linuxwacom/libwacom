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
#include <stdlib.h>
#include <string.h>
#include "libwacom.h"
#include <glib/gi18n.h>
#include <glib.h>

static gboolean need_uinput_rules = FALSE;

static GOptionEntry opts[] = {
        {"with-uinput-rules", 0, 0, G_OPTION_ARG_NONE, &need_uinput_rules, N_("Print udev rules for uinput devices"), NULL },
	{NULL}
};

static void print_udev_header (void)
{
	printf ("# udev rules for libwacom supported devices\n");
	printf ("\n");
	printf ("ACTION!=\"add|change\", GOTO=\"libwacom_end\"\n");
	printf ("KERNEL!=\"event[0-9]*\", GOTO=\"libwacom_end\"\n");
	printf ("\n");
}

static void print_udev_entry_for_match (WacomDevice *device, const WacomMatch *match,
					WacomBusType bus_type_filter)
{
	WacomBusType type       = libwacom_match_get_bustype (match);
	int          vendor     = libwacom_match_get_vendor_id (match);
	int          product    = libwacom_match_get_product_id (match);
	int          has_touch  = libwacom_has_touch (device);
	char         *touchpad;

	if (bus_type_filter != type)
		return;

	if (has_touch)
		touchpad = ", ENV{ID_INPUT_TOUCHPAD}=\"1\"";
	else
		touchpad = "";

	switch (type) {
		case WBUSTYPE_USB:
			printf ("ENV{ID_BUS}==\"usb\", ENV{ID_VENDOR_ID}==\"%04x\", ENV{ID_MODEL_ID}==\"%04x\", ENV{ID_INPUT}=\"1\", ENV{ID_INPUT_TABLET}=\"1\"%s\n", vendor, product, touchpad);
			break;
		case WBUSTYPE_BLUETOOTH:
			/* Bluetooth tablets do not have ID_VENDOR_ID/ID_MODEL_ID etc set correctly. They
			 * do have the PRODUCT set though. */
			printf ("ENV{PRODUCT}==\"5/%x/%x/*\", ENV{ID_INPUT}=\"1\", ENV{ID_INPUT_TABLET}=\"1\"%s\n", vendor, product, touchpad);
			break;
		default:
			/* Not sure how to deal with serials  */
			break;
	}
}

static void print_uinput_entry_for_match (WacomDevice *device, const WacomMatch *match,
					  WacomBusType bus_type_filter)
{
	WacomBusType type       = libwacom_match_get_bustype (match);
	int          vendor     = libwacom_match_get_vendor_id (match);
	int          product    = libwacom_match_get_product_id (match);
	const char *subsystem;

	if (bus_type_filter != type)
		return;

	switch(type) {
		case WBUSTYPE_USB: subsystem = "usb"; break;
		case WBUSTYPE_BLUETOOTH: subsystem = "bluetooth"; break;
		case WBUSTYPE_SERIAL: subsystem = "tty"; break;
		default:
				      return;
	}

	printf("ENV{DEVPATH}==\"/devices/virtual/*\", "
			"ENV{PRODUCT}==\"*/%x/%x/*\", "
			"ENV{UINPUT_DEVICE}=\"1\", "
			"ENV{UINPUT_SUBSYSTEM}=\"%s\", "
			"ENV{ID_VENDOR_ID}=\"%04x\", "
			"ENV{ID_MODEL_ID}=\"%04x\", "
			"\n", vendor, product,
			subsystem, vendor, product);
}

static void print_uinput_entry (WacomDevice *device, WacomBusType bus_type_filter)
{
	const WacomMatch **matches, **match;

	matches = libwacom_get_matches(device);
	for (match = matches; *match; match++)
		print_uinput_entry_for_match(device, *match, bus_type_filter);
}


static void print_udev_entry (WacomDevice *device, WacomBusType bus_type_filter)
{
	const WacomMatch **matches, **match;

	matches = libwacom_get_matches(device);
	for (match = matches; *match; match++)
		print_udev_entry_for_match(device, *match, bus_type_filter);
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
	GOptionContext *context;
	GError *error;

	context = g_option_context_new (NULL);

	g_option_context_add_main_entries (context, opts, NULL);
	error = NULL;

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		if (error != NULL) {
			fprintf (stderr, "%s\n", error->message);
			g_error_free (error);
		}
		return EXIT_FAILURE;
	}


	db = libwacom_database_new_for_path(TOPSRCDIR"/data");

	list = libwacom_list_devices_from_database(db, NULL);
	if (!list) {
		fprintf(stderr, "Failed to load device database.\n");
		return 1;
	}

	print_udev_header ();
	for (p = list; *p; p++)
		print_udev_entry ((WacomDevice *) *p, WBUSTYPE_USB);

	print_udev_trailer ();

	for (p = list; *p; p++)
		print_udev_entry ((WacomDevice *) *p, WBUSTYPE_BLUETOOTH);

	if (need_uinput_rules) {
		for (p = list; *p; p++)
			print_uinput_entry ((WacomDevice *) *p, WBUSTYPE_USB);
		for (p = list; *p; p++)
			print_uinput_entry ((WacomDevice *) *p, WBUSTYPE_BLUETOOTH);
	}

	libwacom_database_destroy (db);

	return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
