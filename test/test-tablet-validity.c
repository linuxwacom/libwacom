/*
 * Copyright © 2012 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 udo y and its documentation for any purpose is hereby granted without
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

#define _GNU_SOURCE
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libwacom.h"
#include <unistd.h>

typedef int (*NumModesFn) (const WacomDevice *device);

static gboolean
buttons_have_direction(WacomDevice *device)
{
	char               button;
	int                num_buttons;

	num_buttons = libwacom_get_num_buttons (device);
	if (num_buttons == 0)
		return TRUE;

	for (button = 'A'; button < 'A' + num_buttons; button++) {
		WacomButtonFlags  flags;
		flags = libwacom_get_button_flag(device, button);
		if (!(flags & WACOM_BUTTON_DIRECTION))
			return FALSE;
	}

	return TRUE;
}

static gboolean
match_mode_switch(WacomDevice *device,
		  NumModesFn get_num_modes,
		  WacomButtonFlags flag)
{
	char               button;
	int                num_buttons;
	int                num_switches;
	int                num_modes;

	num_buttons  = libwacom_get_num_buttons (device);
	num_modes    = get_num_modes (device);
	num_switches = 0;

	for (button = 'A'; button < 'A' + num_buttons; button++) {
		WacomButtonFlags  flags;
		flags = libwacom_get_button_flag(device, button);

		if (flags & flag)
			num_switches++;
	}

	/*
	 * If we have more than one mode-switch button, then the
	 * number of modes must match the number of mode-switch buttons.
	 */
	if (num_switches > 1 && num_modes != num_switches)
		return FALSE;

	/*
	 * If we have more than one mode, then we should find at least
	 * one mode-switch button.
	 */
	if (num_modes > 1 && num_switches == 0)
		return FALSE;

	return TRUE;
}

static gboolean
tablet_has_lr_buttons(WacomDevice *device)
{
	int nleft = 0;
	int nright = 0;
	int num_buttons;
	char button;

	num_buttons  = libwacom_get_num_buttons (device);

	for (button = 'A'; button < 'A' + num_buttons; button++) {
		WacomButtonFlags f = libwacom_get_button_flag(device, button);
		if (f & WACOM_BUTTON_POSITION_LEFT)
			nleft++;
		if (f & WACOM_BUTTON_POSITION_RIGHT)
			nright++;
	}

	if (nleft > 0 || nright > 0)
		return TRUE;

	return FALSE;
}

static void
test_class(gconstpointer data)
{
	WacomDevice *device = (WacomDevice*)data;
	WacomClass cls;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	cls = libwacom_get_class(device);
#pragma GCC diagnostic pop
	switch (cls) {
		case WCLASS_BAMBOO:
		case WCLASS_ISDV4:
		case WCLASS_PEN_DISPLAYS:
		case WCLASS_GRAPHIRE:
		case WCLASS_REMOTE:
		case WCLASS_INTUOS:
		case WCLASS_INTUOS2:
		case WCLASS_INTUOS3:
		case WCLASS_INTUOS4:
		case WCLASS_INTUOS5:
		case WCLASS_CINTIQ:
			break;
		default:
			g_test_fail();
			break;
	}
}

static void
test_name(gconstpointer data)
{
	WacomDevice *device = (WacomDevice*)data;

	g_assert_nonnull(libwacom_get_name(device));
	g_assert_cmpstr(libwacom_get_name(device), !=, "");
}

static void
assert_vidpid(WacomBusType bus, int vid, int pid)
{
	switch (bus) {
		case WBUSTYPE_SERIAL:
			g_assert_cmpint(vid, >=, 0);
			g_assert_cmpint(pid, >=, 0);
			break;
		case WBUSTYPE_USB:
			if (vid == 0x056A)
				g_assert_cmpint(pid, !=, 0x84); /* wireless dongle */
			g_assert_cmpint(vid, >, 0);
			g_assert_cmpint(pid, >, 0);
			break;
		case WBUSTYPE_BLUETOOTH:
		case WBUSTYPE_I2C:
			g_assert_cmpint(vid, >, 0);
			g_assert_cmpint(pid, >, 0);
			break;
		case WBUSTYPE_UNKNOWN:
		default:
			g_test_fail();
			break;
	}
}

static void
test_vidpid(gconstpointer data)
{
	WacomDevice *device = (WacomDevice*)data;
	WacomBusType bus = libwacom_get_bustype(device);

	assert_vidpid(bus, libwacom_get_vendor_id(device), libwacom_get_product_id(device));
}

static void
test_matches(gconstpointer data)
{
	WacomDevice *device = (WacomDevice*)data;

	g_assert_nonnull(libwacom_get_match(device));
	g_assert_nonnull(libwacom_get_matches(device));
}

static void
test_matches_vidpid(gconstpointer data)
{
	WacomDevice *device = (WacomDevice*)data;
	const WacomMatch **match = libwacom_get_matches(device);

	while (*match) {
		WacomBusType bus = libwacom_match_get_bustype(*match);
		assert_vidpid(bus, libwacom_match_get_vendor_id(*match), libwacom_match_get_product_id(*match));
		match++;
	}
}

static void
test_dimensions(gconstpointer data)
{
	WacomDevice *device = (WacomDevice*)data;

	g_assert_cmpint(libwacom_get_width(device), >, 0);
	g_assert_cmpint(libwacom_get_height(device), >, 0);
}

static void
test_buttons(gconstpointer data)
{
	WacomDevice *device = (WacomDevice*)data;

	g_assert_cmpint(libwacom_get_num_buttons(device), >=, 0);
	g_assert_true(buttons_have_direction(device));

	if (libwacom_is_reversible(device) && libwacom_get_num_buttons(device) > 0)
		g_assert_true(tablet_has_lr_buttons(device));
}

static void
test_styli(gconstpointer data)
{
	WacomDevice *device = (WacomDevice*)data;
	int nstyli;
	const int *styli = libwacom_get_supported_styli(device, &nstyli);

	g_assert_cmpint(nstyli, >, 0);
	g_assert_nonnull(styli);
}

static void
test_realstylus(gconstpointer data)
{
	WacomDevice *device = (WacomDevice*)data;
	int nstyli;
	const int *styli = libwacom_get_supported_styli(device, &nstyli);

	for (int i = 0; i < nstyli; i++) {
		g_assert_cmpint(styli[i], !=, WACOM_STYLUS_FALLBACK_ID);
		g_assert_cmpint(styli[i], !=, WACOM_ERASER_FALLBACK_ID);
	}
}

static void
test_rings(gconstpointer data)
{
	WacomDevice *device = (WacomDevice*)data;

	g_assert_cmpint(libwacom_get_ring_num_modes(device), >=, 0);
	g_assert_cmpint(libwacom_get_ring2_num_modes(device), >=, 0);

	if (libwacom_has_ring(device))
		g_assert_true(match_mode_switch(device,
						libwacom_get_ring_num_modes,
						WACOM_BUTTON_RING_MODESWITCH));
	if (libwacom_has_ring2(device))
		g_assert_true(match_mode_switch(device,
						libwacom_get_ring2_num_modes,
						WACOM_BUTTON_RING2_MODESWITCH));
}

static void
test_strips(gconstpointer data)
{
	WacomDevice *device = (WacomDevice*)data;

	g_assert_cmpint(libwacom_get_num_strips(device), >=, 0);
	g_assert_cmpint(libwacom_get_strips_num_modes(device), >=, 0);

	if (libwacom_get_num_strips(device) > 0)
		g_assert_true(match_mode_switch(device,
						libwacom_get_strips_num_modes,
						WACOM_BUTTON_TOUCHSTRIP_MODESWITCH));
}

/* Wrapper function to make adding tests simpler. g_test requires
 * a unique test case name so we assemble that from the test function and
 * the tablet data.
 */
static inline void
_add_test(WacomDevice *device, GTestDataFunc func, const char *funcname)
{
	char buf[128];
	static int count; /* guarantee unique test case names */
	const char *prefix;

	/* tests must be test_foobar */
	g_assert(strncmp(funcname, "test_", 5) == 0);
	prefix = &funcname[5];

	snprintf(buf, 128, "/tablet/%s/%03d/%04x:%04x-%s",
		 prefix,
		 ++count,
		 libwacom_get_vendor_id(device),
		 libwacom_get_product_id(device),
		 libwacom_get_name(device));
	g_test_add_data_func(buf, device, func);
}
#define add_test(device_, func_) \
	_add_test(device_, func_, #func_)

static void setup_tests(WacomDevice *device)
{
	const char *name;
	WacomClass cls;

	name = libwacom_get_name(device);
	if (strcmp(name, "Generic") == 0)
		return;

	add_test(device, test_class);
	add_test(device, test_name);
	add_test(device, test_vidpid);
	add_test(device, test_matches);
	add_test(device, test_matches_vidpid);
	add_test(device, test_buttons);
	add_test(device, test_styli);
	add_test(device, test_rings);
	add_test(device, test_strips);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	cls = libwacom_get_class(device);
#pragma GCC diagnostic pop

	/* ISDv4 are built-in, they may be of varying size */
	if (cls != WCLASS_ISDV4 && cls != WCLASS_REMOTE)
		add_test(device, test_dimensions);

	/* FIXME: we force the generic pen for these, should add a test */
	if (libwacom_has_stylus(device))
		add_test(device, test_styli);

	switch (cls) {
		case WCLASS_INTUOS:
		case WCLASS_INTUOS2:
		case WCLASS_INTUOS3:
		case WCLASS_INTUOS4:
		case WCLASS_INTUOS5:
		case WCLASS_CINTIQ:
			add_test(device, test_realstylus);
			break;
		default:
			break;
	}
}

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

int main(int argc, char **argv)
{
	WacomDeviceDatabase *db;
	WacomDevice **devices;
	int rc;

	g_test_init(&argc, &argv, NULL);
	g_test_set_nonfatal_assertions();

	db = load_database();

	devices = libwacom_list_devices_from_database(db, NULL);
	g_assert(devices);
	g_assert(*devices);

	for (WacomDevice **device = devices; *device; device++)
		setup_tests(*device);

	rc = g_test_run();

	libwacom_database_destroy(db);
	free(devices);

	return rc;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
