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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "libwacom.h"
#include <assert.h>
#include <unistd.h>

typedef int (*NumModesFn) (const WacomDevice *device);

static int buttons_have_direction (WacomDevice *device)
{
	char               button;
	int                num_buttons;

	num_buttons = libwacom_get_num_buttons (device);
	if (num_buttons == 0)
		return 1;

	for (button = 'A'; button < 'A' + num_buttons; button++) {
		WacomButtonFlags  flags;
		flags = libwacom_get_button_flag(device, button);
		if (!(flags & WACOM_BUTTON_DIRECTION))
			return 0;
	}

	return 1;
}

static int match_mode_switch (WacomDevice *device, NumModesFn get_num_modes, WacomButtonFlags flag)
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
		return 0;

	/*
	 * If we have more than one mode, then we should find at least
	 * one mode-switch button.
	 */
	if (num_modes > 1 && num_switches == 0)
		return 0;

	return 1;
}

static int eraser_is_present(WacomDeviceDatabase *db, const int *styli, int nstyli, WacomStylusType type)
{
	int i;

	for (i = 0; i < nstyli; i++) {
		const WacomStylus *stylus;
		stylus = libwacom_stylus_get_for_id (db, styli[i]);
		if (libwacom_stylus_is_eraser (stylus) &&
		    libwacom_stylus_get_type (stylus) == type)
			return 1;
	}

	return 0;
}

static int tablet_has_lr_buttons(WacomDevice *device)
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
		return 1;

	return 0;
}

static void verify_tablet(WacomDeviceDatabase *db, WacomDevice *device)
{
	const char *name;
	const int *styli;
	int nstyli, i;

	name = libwacom_get_name(device);
	if (strcmp(name, "Generic") == 0)
		return;

	printf("Verifying tablet %s\n", name);
	assert(libwacom_get_class(device) != WCLASS_UNKNOWN);
	assert(name && strlen(name) > 0);
	if (libwacom_get_bustype(device) != WBUSTYPE_SERIAL) {
		assert(libwacom_get_vendor_id(device) > 0);
		assert(libwacom_get_product_id(device) > 0);
	}
	assert(libwacom_get_match(device) != NULL);
	assert(libwacom_get_matches(device) != NULL);

	/* ISDv4 are built-in, they may be of varying size */
	if (libwacom_get_class(device) != WCLASS_ISDV4 &&
	    libwacom_get_class(device) != WCLASS_REMOTE) {
		assert(libwacom_get_width(device) > 0);
		assert(libwacom_get_height(device) > 0);
	}
	assert(libwacom_get_num_buttons(device) >= 0);

	styli = libwacom_get_supported_styli(device, &nstyli);

	if (libwacom_has_stylus(device)) {
		assert(styli != NULL);
		assert(nstyli >= 1);
	}

	switch(libwacom_get_class(device)) {
		case WCLASS_BAMBOO:
		case WCLASS_ISDV4:
		case WCLASS_PEN_DISPLAYS:
		case WCLASS_GRAPHIRE:
		case WCLASS_REMOTE:
			break;
		case WCLASS_INTUOS:
		case WCLASS_INTUOS2:
		case WCLASS_INTUOS3:
		case WCLASS_INTUOS4:
		case WCLASS_INTUOS5:
		case WCLASS_CINTIQ:
			{
				int i;
				for (i = 0; i < nstyli; i++) {
					assert(styli[i] != WACOM_STYLUS_FALLBACK_ID);
					assert(styli[i] != WACOM_ERASER_FALLBACK_ID);
				}
			}
			break;
		default:
			abort(); /* don't get here */
	}

	for (i = 0; i < nstyli; i++) {
		const WacomStylus *stylus;
		const char *stylus_name;
		WacomAxisTypeFlags axes;

		stylus = libwacom_stylus_get_for_id (db, styli[i]);
		assert(stylus);
		stylus_name = libwacom_stylus_get_name (stylus);
		assert(stylus_name);
		if (libwacom_stylus_has_eraser (stylus)) {
			WacomStylusType type;
			type = libwacom_stylus_get_type (stylus);
			assert(eraser_is_present (db, styli, nstyli, type));
		}

		if (libwacom_stylus_get_type (stylus) == WSTYLUS_PUCK) {
			int has_wheel = libwacom_stylus_has_wheel (stylus);
			int has_lens = libwacom_stylus_has_lens (stylus);
			/* 4D mouse is the only one with neither, everything
			 * else has either wheel or lens */
			if (styli[i] == 0x94) {
				assert (!has_wheel);
				assert (!has_lens);
			} else {
				assert (has_wheel != has_lens);
			}
		}

		if (libwacom_stylus_is_eraser (stylus))
			assert (libwacom_stylus_get_num_buttons (stylus) > 0);

		axes = libwacom_stylus_get_axes (stylus);
		if (libwacom_stylus_get_type (stylus) == WSTYLUS_PUCK) {
			assert((axes & WACOM_AXIS_TYPE_PRESSURE) == 0);
		} else if ((styli[i] != 0xffffd) && (styli[i] != 0x8e2)) {
			assert(axes & WACOM_AXIS_TYPE_TILT);
			assert(axes & WACOM_AXIS_TYPE_PRESSURE);
			assert(axes & WACOM_AXIS_TYPE_DISTANCE);
		}
	}
	assert(libwacom_get_ring_num_modes(device) >= 0);
	assert(libwacom_get_ring2_num_modes(device) >= 0);
	assert(libwacom_get_num_strips(device) >= 0);
	assert(libwacom_get_strips_num_modes(device) >= 0);
	assert(libwacom_get_bustype(device) != WBUSTYPE_UNKNOWN);
	assert(buttons_have_direction(device) > 0);
	if (libwacom_has_ring(device))
		assert(match_mode_switch (device, libwacom_get_ring_num_modes, WACOM_BUTTON_RING_MODESWITCH));
	if (libwacom_has_ring2(device))
		assert(match_mode_switch (device, libwacom_get_ring2_num_modes, WACOM_BUTTON_RING2_MODESWITCH));
	if (libwacom_get_num_strips(device) > 1)
		assert(match_mode_switch (device, libwacom_get_strips_num_modes, WACOM_BUTTON_TOUCHSTRIP2_MODESWITCH));
	if (libwacom_get_num_strips(device) > 0)
		assert(match_mode_switch (device, libwacom_get_strips_num_modes, WACOM_BUTTON_TOUCHSTRIP_MODESWITCH));

	if (libwacom_is_reversible(device) && libwacom_get_num_buttons(device) > 0)
		assert(tablet_has_lr_buttons(device));
}

int main(int argc, char **argv)
{
	WacomDeviceDatabase *db;
	WacomDevice **device, **devices;

	db = libwacom_database_new_for_path(TOPSRCDIR"/data");
	if (!db)
		printf("Failed to load data from %s", TOPSRCDIR"/data");
	assert(db);

	devices = libwacom_list_devices_from_database(db, NULL);
	assert(devices);
	assert(*devices);

	for (device = devices; *device; device++)
		verify_tablet(db, *device);

	libwacom_database_destroy (db);

	free(devices);

	return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
