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

#include <linux/input-event-codes.h>
#include <glib.h>
#include <stdlib.h>

#include "libwacom.h"

struct fixture {
	WacomDeviceDatabase *db;
};

static WacomDeviceDatabase *
load_database(void)
{
	WacomDeviceDatabase *db;
	const char *datadir;

	datadir = getenv("LIBWACOM_DATA_DIR");
	if (!datadir)
		datadir = TOPSRCDIR"/data";

	db = libwacom_database_new_for_path(datadir);
	if (!db)
		printf("Failed to load data from %s", datadir);

	g_assert(db);
	return db;
}

static void
fixture_setup(struct fixture *f, gconstpointer user_data)
{
	f->db = load_database();
}

static void
fixture_teardown(struct fixture *f, gconstpointer user_data)
{
	libwacom_database_destroy(f->db);
}


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
		if ((int)libwacom_match_get_vendor_id(*match) == libwacom_get_vendor_id(device))
			found_vendor_id = 1;
		if ((int)libwacom_match_get_product_id(*match) == libwacom_get_product_id(device))
			found_product_id = 1;
	}

	g_assert_cmpint(nmatches, ==, 2);
	g_assert_true(found_bus);
	g_assert_true(found_vendor_id);
	g_assert_true(found_product_id);
}


static void
test_invalid_device(struct fixture *f, gconstpointer user_data)
{
	WacomDevice *device = libwacom_new_from_usbid(f->db, 0, 0, NULL);
	g_assert_null(device);
}

static void
test_intuos4(struct fixture *f, gconstpointer user_data)
{
	WacomDevice *device = libwacom_new_from_usbid(f->db, 0x56a, 0x00bc, NULL);
	g_assert_nonnull(device);

	g_assert_cmpstr(libwacom_get_name(device), ==, "Wacom Intuos4 WL");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	g_assert_cmpint(libwacom_get_class(device), ==, WCLASS_INTUOS4);
#pragma GCC diagnostic pop
	g_assert_cmpint(libwacom_get_vendor_id(device), ==, 0x56a);
	g_assert_cmpint(libwacom_get_product_id(device), ==, 0xbc);
	g_assert_cmpint(libwacom_get_bustype(device), ==, WBUSTYPE_USB);
	g_assert_cmpint(libwacom_get_num_buttons(device), ==, 9);
	g_assert_true(libwacom_has_stylus(device));
	g_assert_true(libwacom_is_reversible(device));
	g_assert_false(libwacom_has_touch(device));
	g_assert_true(libwacom_has_ring(device));
	g_assert_false(libwacom_has_ring2(device));
	g_assert_false(libwacom_has_touchswitch(device));
	g_assert_cmpint(libwacom_get_num_strips(device), ==, 0);
	g_assert_cmpint(libwacom_get_integration_flags (device), ==, WACOM_DEVICE_INTEGRATED_NONE);
	g_assert_cmpint(libwacom_get_width(device), ==, 8);
	g_assert_cmpint(libwacom_get_height(device), ==, 5);

	check_multiple_match(device);

	libwacom_destroy(device);
}

static void
test_intuos4_wl(struct fixture *f, gconstpointer user_data)
{
	WacomDevice *device = libwacom_new_from_usbid(f->db, 0x56a, 0x00b9, NULL);
	g_assert_nonnull(device);

	g_assert_true(libwacom_get_button_flag(device, 'A') & WACOM_BUTTON_RING_MODESWITCH);
	g_assert_true(libwacom_get_button_flag(device, 'I') & WACOM_BUTTON_OLED);
#if 0
	/* disabled - needs subprocesses testing but invalid data handling
	   is better handled in a separate test suite */
	/*
	 * I4 WL has only 9 buttons, asking for a 10th button will raise a warning
	 * in libwacom_get_button_flag() which is expected.
	 */
	printf("Following critical warning in libwacom_get_button_flag() is expected\n");
	g_assert_cmpint(libwacom_get_button_flag(device, 'J'), ==, WACOM_BUTTON_NONE);
#endif
	g_assert_cmpint(libwacom_get_ring_num_modes(device), ==, 4);

	libwacom_destroy(device);
}

static void
test_cintiq24hd(struct fixture *f, gconstpointer user_data)
{
	WacomDevice *device = libwacom_new_from_usbid(f->db, 0x56a, 0x00f4, NULL);
	g_assert_nonnull(device);

	g_assert_cmpint(libwacom_get_ring_num_modes(device), ==, 3);
	g_assert_cmpint(libwacom_get_ring2_num_modes(device), ==, 3);

	libwacom_destroy(device);
}

static void
test_cintiq21ux(struct fixture *f, gconstpointer user_data)
{
	WacomDevice *device = libwacom_new_from_usbid(f->db, 0x56a, 0x00cc, NULL);
	g_assert_nonnull(device);

	g_assert_cmpint(libwacom_get_num_strips(device), ==, 2);
	libwacom_destroy(device);
}

static void
test_wacf004(struct fixture *f, gconstpointer user_data)
{
	WacomDevice *device = libwacom_new_from_name(f->db, "Wacom Serial Tablet WACf004", NULL);
	g_assert_nonnull(device);

	g_assert_true(libwacom_get_integration_flags(device) & WACOM_DEVICE_INTEGRATED_DISPLAY);
	g_assert_true(libwacom_get_integration_flags(device) & WACOM_DEVICE_INTEGRATED_SYSTEM);
	g_assert_null(libwacom_get_model_name(device));

	libwacom_destroy(device);
}

static void
test_cintiq24hdt(struct fixture *f, gconstpointer user_data)
{
	WacomDevice *device = libwacom_new_from_usbid(f->db, 0x56a, 0x00f8, NULL);
	const WacomMatch *match;

	g_assert_nonnull(device);

	/* 24HDT has one paired device */
	match = libwacom_get_paired_device(device);
	g_assert_nonnull(match);
	g_assert_cmpint(libwacom_match_get_vendor_id(match), ==, 0x56a);
	g_assert_cmpint(libwacom_match_get_product_id(match), ==, 0xf6);
	g_assert_cmpint(libwacom_match_get_bustype(match), ==, WBUSTYPE_USB);

	libwacom_destroy(device);
}

static void
test_cintiq13hd(struct fixture *f, gconstpointer user_data)
{
	WacomDevice *device = libwacom_new_from_name(f->db, "Wacom Cintiq 13HD", NULL);
	g_assert_nonnull(device);
	g_assert_cmpint(libwacom_get_button_evdev_code(device, 'A'), ==, BTN_0);
	g_assert_cmpint(libwacom_get_button_evdev_code(device, 'B'), ==, BTN_1);
	g_assert_cmpint(libwacom_get_button_evdev_code(device, 'C'), ==, BTN_2);
	g_assert_cmpint(libwacom_get_button_evdev_code(device, 'D'), ==, BTN_3);
	g_assert_cmpint(libwacom_get_button_evdev_code(device, 'E'), ==, BTN_4);
	g_assert_cmpint(libwacom_get_button_evdev_code(device, 'F'), ==, BTN_5);
	g_assert_cmpint(libwacom_get_button_evdev_code(device, 'G'), ==, BTN_6);
	g_assert_cmpint(libwacom_get_button_evdev_code(device, 'H'), ==, BTN_7);
	g_assert_cmpint(libwacom_get_button_evdev_code(device, 'I'), ==, BTN_8);
	g_assert_cmpstr(libwacom_get_model_name(device), ==, "DTK-1300");

	libwacom_destroy(device);
}

static void
test_bamboopen(struct fixture *f, gconstpointer user_data)
{
	WacomDevice *device = libwacom_new_from_name(f->db, "Wacom Bamboo Pen", NULL);
	g_assert_nonnull(device);
	g_assert_cmpint(libwacom_get_button_evdev_code(device, 'A'), ==, BTN_BACK);
	g_assert_cmpint(libwacom_get_button_evdev_code(device, 'B'), ==, BTN_FORWARD);
	g_assert_cmpint(libwacom_get_button_evdev_code(device, 'C'), ==, BTN_LEFT);
	g_assert_cmpint(libwacom_get_button_evdev_code(device, 'D'), ==, BTN_RIGHT);
	g_assert_cmpstr(libwacom_get_model_name(device), ==, "MTE-450");

	libwacom_destroy(device);
}

static void
test_dellcanvas(struct fixture *f, gconstpointer user_data)
{
	WacomDevice *device = libwacom_new_from_name(f->db, "Dell Canvas 27", NULL);

	g_assert_nonnull(device);
	g_assert_true(libwacom_get_integration_flags(device) & WACOM_DEVICE_INTEGRATED_DISPLAY);
	g_assert_false(libwacom_get_integration_flags(device) & WACOM_DEVICE_INTEGRATED_SYSTEM);

	libwacom_destroy(device);
}

static void
test_isdv4_4800(struct fixture *f, gconstpointer user_data)
{
	WacomDevice *device = libwacom_new_from_usbid(f->db, 0x56a, 0x4800, NULL);
	g_assert_nonnull(device);

	g_assert_true(libwacom_get_integration_flags(device) & WACOM_DEVICE_INTEGRATED_DISPLAY);
	g_assert_true(libwacom_get_integration_flags(device) & WACOM_DEVICE_INTEGRATED_SYSTEM);
	g_assert_null(libwacom_get_model_name(device));
	g_assert_cmpint(libwacom_get_vendor_id(device), ==, 0x56a);
	g_assert_cmpint(libwacom_get_product_id(device), ==, 0x4800);
	g_assert_cmpint(libwacom_get_num_buttons(device), ==, 0);

	libwacom_destroy(device);
}

int main(int argc, char **argv)
{
	g_test_init(&argc, &argv, NULL);
	g_test_set_nonfatal_assertions();

	g_test_add("/load/0000:0000", struct fixture, NULL,
		   fixture_setup, test_invalid_device,
		   fixture_teardown);
	g_test_add("/load/056a:00bc", struct fixture, NULL,
		   fixture_setup, test_intuos4,
		   fixture_teardown);
	g_test_add("/load/056a:00b8", struct fixture, NULL,
		   fixture_setup, test_intuos4_wl,
		   fixture_teardown);
	g_test_add("/load/056a:00f4", struct fixture, NULL,
		   fixture_setup, test_cintiq24hd,
		   fixture_teardown);
	g_test_add("/load/056a:00cc", struct fixture, NULL,
		   fixture_setup, test_cintiq21ux,
		   fixture_teardown);
	g_test_add("/load/056a:00f8", struct fixture, NULL,
		   fixture_setup, test_cintiq24hdt,
		   fixture_teardown);
	g_test_add("/load/056a:0304", struct fixture, NULL,
		   fixture_setup, test_cintiq13hd,
		   fixture_teardown);
	g_test_add("/load/056a:0065", struct fixture, NULL,
		   fixture_setup, test_bamboopen,
		   fixture_teardown);
	g_test_add("/load/056a:4200", struct fixture, NULL,
		   fixture_setup, test_dellcanvas,
		   fixture_teardown);
	g_test_add("/load/056a:WACf004", struct fixture, NULL,
		   fixture_setup, test_wacf004,
		   fixture_teardown);
	g_test_add("/load/056a:4800", struct fixture, NULL,
		   fixture_setup, test_isdv4_4800,
		   fixture_teardown);

	return g_test_run();
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
