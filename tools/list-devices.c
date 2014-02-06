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
 *        Peter Hutterer <peter.hutterer@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "libwacom.h"

static void print_device_info (WacomDevice *device, WacomBusType bus_type_filter)
{
	const WacomMatch **match;

	for (match = libwacom_get_matches(device); *match; match++) {
		WacomBusType type = libwacom_match_get_bustype(*match);
		if (type == bus_type_filter) {
			libwacom_print_device_description(STDOUT_FILENO, device);
			dprintf(STDOUT_FILENO, "---------------------------------------------------------------\n");
		}
	}
}

int main(int argc, char **argv)
{
	WacomDeviceDatabase *db;
	WacomDevice **list, **p;

	db = libwacom_database_new_for_path(TOPSRCDIR"/data");

	list = libwacom_list_devices_from_database(db, NULL);
	if (!list) {
		fprintf(stderr, "Failed to load device database.\n");
		return 1;
	}

	for (p = list; *p; p++)
		print_device_info ((WacomDevice *) *p, WBUSTYPE_USB);

	for (p = list; *p; p++)
		print_device_info ((WacomDevice *) *p, WBUSTYPE_BLUETOOTH);

	for (p = list; *p; p++)
		print_device_info ((WacomDevice *) *p, WBUSTYPE_SERIAL);

	for (p = list; *p; p++)
		print_device_info ((WacomDevice *) *p, WBUSTYPE_UNKNOWN);

	libwacom_database_destroy (db);

	return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
