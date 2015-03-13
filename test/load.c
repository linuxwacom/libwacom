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

#include <stdio.h>
#include <string.h>
#include "libwacom.h"
#include <assert.h>

static void check_multiple_match(WacomDevice *device)
{
	const WacomMatch **match;
	int nmatches = 0;
	int found_bus = 0,
	    found_vendor_id = 0,
	    found_product_id = 0;

	for (match = libwacom_get_matches(device); *match; match++)
	{
		nmatches++;
		if (libwacom_match_get_bustype(*match) == libwacom_get_bustype(device))
			found_bus = 1;
		if (libwacom_match_get_vendor_id(*match) == libwacom_get_vendor_id(device))
			found_vendor_id = 1;
		if (libwacom_match_get_product_id(*match) == libwacom_get_product_id(device))
			found_product_id = 1;
	}

	assert(nmatches == 2);
	assert(found_bus && found_vendor_id && found_product_id);
}

int main(int argc, char **argv)
{
	WacomDeviceDatabase *db;
	WacomDevice *device;
	const char *str;

	db = libwacom_database_new_for_path(TOPSRCDIR"/data");
	if (!db)
		printf("Failed to load data from %s", TOPSRCDIR"/data");
	assert(db);

	device = libwacom_new_from_usbid(db, 0, 0, NULL);
	assert(!device);

	device = libwacom_new_from_usbid(db, 0x56a, 0x00bc, NULL);
	assert(device);

	str = libwacom_get_name(device);
	assert(strcmp(str, "Wacom Intuos4 WL") == 0);
	assert(libwacom_get_class(device) == WCLASS_INTUOS4);
	assert(libwacom_get_vendor_id(device) == 0x56a);
	assert(libwacom_get_product_id(device) == 0xbc);
	assert(libwacom_get_bustype(device) == WBUSTYPE_USB);
	assert(libwacom_get_num_buttons(device) == 9);
	assert(libwacom_has_stylus(device));
	assert(libwacom_is_reversible(device));
	assert(!libwacom_has_touch(device));
	assert(libwacom_has_ring(device));
	assert(!libwacom_has_ring2(device));
	assert(!libwacom_has_touchswitch(device));
	assert(libwacom_get_num_strips(device) == 0);
	assert(libwacom_get_integration_flags (device) == WACOM_DEVICE_INTEGRATED_NONE);
	assert(libwacom_get_width(device) == 8);
	assert(libwacom_get_height(device) == 5);

	/* I4 WL has two matches */
	check_multiple_match(device);

	libwacom_destroy(device);

	device = libwacom_new_from_usbid(db, 0x56a, 0x00b9, NULL);
	assert(device);

	assert(libwacom_get_button_flag(device, 'A') & WACOM_BUTTON_RING_MODESWITCH);
	assert(libwacom_get_button_flag(device, 'I') & WACOM_BUTTON_OLED);
	/*
	 * I4 WL has only 9 buttons, asking for a 10th button will raise a warning
	 * in libwacom_get_button_flag() which is expected.
	 */
	printf("Following critical warning in libwacom_get_button_flag() is expected\n");
	assert(libwacom_get_button_flag(device, 'J') == WACOM_BUTTON_NONE);
	assert(libwacom_get_ring_num_modes(device) == 4);

	libwacom_destroy(device);

	device = libwacom_new_from_usbid(db, 0x56a, 0x00f4, NULL);
	assert(device);

	assert(libwacom_get_ring_num_modes(device) == 3);
	assert(libwacom_get_ring2_num_modes(device) == 3);

	libwacom_destroy(device);

	device = libwacom_new_from_usbid(db, 0x056a, 0x00cc, NULL);
	assert(libwacom_get_num_strips(device) == 2);
	libwacom_destroy(device);

	device = libwacom_new_from_name(db, "Wacom Serial Tablet WACf004", NULL);
	assert(device);
	assert(libwacom_get_integration_flags (device) & WACOM_DEVICE_INTEGRATED_DISPLAY);
	assert(libwacom_get_integration_flags (device) & WACOM_DEVICE_INTEGRATED_SYSTEM);
	libwacom_destroy(device);
	libwacom_database_destroy (db);

	return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
