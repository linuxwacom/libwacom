/*
 * Copyright © 2019 Red Hat, Inc.
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

static const WacomStylus **all_styli;

static void
test_type(gconstpointer data)
{
	const WacomStylus *stylus = data;

	switch (libwacom_stylus_get_type(stylus)) {
	case WSTYLUS_GENERAL:
	case WSTYLUS_INKING:
	case WSTYLUS_AIRBRUSH:
	case WSTYLUS_CLASSIC:
	case WSTYLUS_MARKER:
	case WSTYLUS_STROKE:
	case WSTYLUS_PUCK:
	case WSTYLUS_3D:
	case WSTYLUS_MOBILE:
		break;
	case WSTYLUS_UNKNOWN:
	default:
		g_test_fail();
	}
}

static void
test_mobile(gconstpointer data)
{
	const WacomStylus *stylus = data;

	g_assert_cmpint(libwacom_stylus_get_type(stylus), ==, WSTYLUS_MOBILE);
}

static void
test_eraser_type(gconstpointer data)
{
	const WacomStylus *stylus = data;

	switch (libwacom_stylus_get_eraser_type(stylus)) {
	case WACOM_ERASER_NONE:
	case WACOM_ERASER_INVERT:
	case WACOM_ERASER_BUTTON:
		break;
	case WACOM_ERASER_UNKNOWN:
	default:
		g_test_fail();
	}
}

static void
test_has_eraser(gconstpointer data)
{
	const WacomStylus *stylus = data;
	gboolean matching_eraser_found = FALSE;
	const WacomStylus **paired;
	int count;
	int i;

	/* A stylus cannot be an eraser and have an eraser at the same time */
	g_assert_true(libwacom_stylus_has_eraser(stylus));
	g_assert_false(libwacom_stylus_is_eraser(stylus));

	/* Search for the linked eraser */
	paired = libwacom_stylus_get_paired_styli(stylus, &count);
	g_assert_cmpint(count, >, 0);

	for (i = 0; i < count; i++) {
		const WacomStylus *s = paired[i];
		if (libwacom_stylus_is_eraser(s)) {
			matching_eraser_found = TRUE;
			break;
		}
	}
	g_free(paired);

	g_assert_true(matching_eraser_found);
}

static void
test_eraser_link(const WacomStylus *stylus, gboolean linked)
{
	gboolean matching_stylus_found = FALSE;
	const WacomStylus **paired;
	int count;
	int i;

	/* A stylus cannot be an eraser and have an eraser at the same time */
	g_assert_false(libwacom_stylus_has_eraser(stylus));
	g_assert_true(libwacom_stylus_is_eraser(stylus));

	/* Verify the link count */
	paired = libwacom_stylus_get_paired_styli(stylus, &count);
	if (!linked) {
		g_assert_cmpint(count, ==, 0);
		g_free(paired);
		return;
	}

	/* If we're supposed to be linked, ensure its to a non-eraser */
	g_assert_cmpint(count, >, 0);
	for (i = 0; i < count; i++) {
		const WacomStylus *s = paired[i];
		if (libwacom_stylus_has_eraser(s)) {
			matching_stylus_found = TRUE;
			break;
		}
	}
	g_free(paired);

	g_assert_true(matching_stylus_found);
}

static void
test_is_eraser_unlinked(gconstpointer data)
{
	const WacomStylus *stylus = data;

	test_eraser_link(stylus, FALSE);
}

static void
test_is_eraser_linked(gconstpointer data)
{
	const WacomStylus *stylus = data;

	test_eraser_link(stylus, TRUE);
}

static void
test_eraser_inverted(gconstpointer data)
{
	const WacomStylus *stylus = data;
	WacomEraserType eraser_type = libwacom_stylus_get_eraser_type (stylus);

	g_assert_cmpint(eraser_type, ==, WACOM_ERASER_INVERT);
}

static void
test_eraser_button(gconstpointer data)
{
	const WacomStylus *stylus = data;
	WacomEraserType eraser_type = libwacom_stylus_get_eraser_type (stylus);

	g_assert_cmpint(eraser_type, ==, WACOM_ERASER_BUTTON);
}

static void
test_puck(gconstpointer data)
{
	const WacomStylus *stylus = data;
	int has_wheel = libwacom_stylus_has_wheel(stylus);
	int has_lens = libwacom_stylus_has_lens(stylus);

	/* 4D mouse is the only one with neither, everything
	 * else has either wheel or lens */
	if (libwacom_stylus_get_id(stylus) == 0x94) {
		g_assert_false(has_wheel);
		g_assert_false(has_lens);
	} else {
		g_assert_true(has_wheel != has_lens);
	}
}

static void
test_tilt(gconstpointer data)
{
	const WacomStylus *stylus = data;
	gboolean has_tilt = libwacom_stylus_get_axes(stylus) & WACOM_AXIS_TYPE_TILT;
	g_assert_true(has_tilt);
}

static void
test_no_tilt(gconstpointer data)
{
	const WacomStylus *stylus = data;
	gboolean has_tilt = libwacom_stylus_get_axes(stylus) & WACOM_AXIS_TYPE_TILT;
	g_assert_false(has_tilt);
}

static void
test_pressure(gconstpointer data)
{
	const WacomStylus *stylus = data;
	gboolean has_pressure = libwacom_stylus_get_axes(stylus) & WACOM_AXIS_TYPE_PRESSURE;
	g_assert_true(has_pressure);
}

static void
test_no_pressure(gconstpointer data)
{
	const WacomStylus *stylus = data;
	gboolean has_pressure = libwacom_stylus_get_axes(stylus) & WACOM_AXIS_TYPE_PRESSURE;
	g_assert_false(has_pressure);
}

static void
test_distance(gconstpointer data)
{
	const WacomStylus *stylus = data;
	gboolean has_distance = libwacom_stylus_get_axes(stylus) & WACOM_AXIS_TYPE_DISTANCE;
	g_assert_true(has_distance);
}

static void
test_no_distance(gconstpointer data)
{
	const WacomStylus *stylus = data;
	gboolean has_distance = libwacom_stylus_get_axes(stylus) & WACOM_AXIS_TYPE_DISTANCE;
	g_assert_false(has_distance);
}

static void
test_name(gconstpointer data)
{
	const WacomStylus *stylus = data;

	g_assert_nonnull(libwacom_stylus_get_name(stylus));
}

static void
test_buttons(gconstpointer data)
{
	const WacomStylus *stylus = data;

	g_assert_cmpint(libwacom_stylus_get_num_buttons(stylus), >, 0);
}

static void
test_no_buttons(gconstpointer data)
{
	const WacomStylus *stylus = data;

	g_assert_cmpint(libwacom_stylus_get_num_buttons(stylus), ==, 0);
}

static void
test_mutually_paired(gconstpointer data)
{
	const WacomStylus *stylus = data;
	const WacomStylus **stylus_pairings;
	int count;

	stylus_pairings = libwacom_stylus_get_paired_styli(stylus, &count);

	for (int i = 0; i < count; i++) {
		const WacomStylus *paired = stylus_pairings[i];
		const WacomStylus **counter_paired;
		gboolean match_found = FALSE;
		int pair_count;

		g_assert_true(paired != stylus); /* can't be paired with itself */

		counter_paired = libwacom_stylus_get_paired_styli(paired, &pair_count);
		for (int j = 0; j < pair_count; j++) {
			const WacomStylus *back_paired = counter_paired[j];
			if (back_paired == stylus) {
				match_found = TRUE;
				break;
			}
		}
		g_free(counter_paired);

		g_assert_true(match_found);
	}
	g_free(stylus_pairings);
}

/* Wrapper function to make adding tests simpler. g_test requires
 * a unique test case name so we assemble that from the test function and
 * the stylus data.
 */
static inline void
_add_test(const WacomStylus *stylus, GTestDataFunc func, const char *funcname)
{
	char buf[128];
	const char *prefix;

	/* tests must be test_foobar */
	g_assert(strncmp(funcname, "test_", 5) == 0);
	prefix = &funcname[5];

	snprintf(buf, 128, "/stylus/%s/%03x-%s",
		 prefix,
		 libwacom_stylus_get_id(stylus),
		 libwacom_stylus_get_name(stylus));
	g_test_add_data_func(buf, stylus, func);
}
#define add_test(stylus, func_) \
	_add_test(stylus, func_, #func_)

static void
setup_aes_tests(const WacomStylus *stylus)
{
	add_test(stylus, test_mobile);

	add_test(stylus, test_pressure);
	add_test(stylus, test_no_distance);

	if (libwacom_stylus_get_id(stylus) < 0x8000) {
		add_test(stylus, test_no_tilt);
	} else {
		add_test(stylus, test_tilt);
	}

	if (libwacom_stylus_is_eraser(stylus)) {
		add_test(stylus, test_is_eraser_unlinked);
		add_test(stylus, test_eraser_button);
	}
}

static void
setup_emr_tests(const WacomStylus *stylus)
{
	switch (libwacom_stylus_get_id(stylus)) {
		case 0xffffd:
			add_test(stylus, test_pressure);
			add_test(stylus, test_no_distance);
			add_test(stylus, test_no_tilt);
			break;
		case 0x006:
		case 0x096:
		case 0x097:
			add_test(stylus, test_no_pressure);
			add_test(stylus, test_distance);
			add_test(stylus, test_no_tilt);
			break;
		case 0x007:
		case 0x017:
		case 0x094:
		case 0x806:
			add_test(stylus, test_no_pressure);
			add_test(stylus, test_distance);
			add_test(stylus, test_tilt);
			break;
		case 0x021:
		case 0x8e2:
		case 0x862:
			add_test(stylus, test_pressure);
			add_test(stylus, test_distance);
			add_test(stylus, test_no_tilt);
			break;
		default:
			add_test(stylus, test_pressure);
			add_test(stylus, test_tilt);
			add_test(stylus, test_distance);
			break;
	}

	if (libwacom_stylus_is_eraser(stylus)) {
		add_test(stylus, test_is_eraser_linked);
		add_test(stylus, test_eraser_inverted);
	}
}

static void
setup_tests(const WacomStylus *stylus)
{
	add_test(stylus, test_name);
	add_test(stylus, test_type);

	/* Button checks */
	switch (libwacom_stylus_get_type(stylus)) {
		case WSTYLUS_PUCK:
			add_test(stylus, test_puck);
			add_test(stylus, test_buttons);
			break;
		case WSTYLUS_INKING:
		case WSTYLUS_STROKE:
			add_test(stylus, test_no_buttons);
			break;
		default:
			switch (libwacom_stylus_get_id(stylus)) {
				case 0x885:
				case 0x8051:
					add_test(stylus, test_no_buttons);
					break;
				default:
					add_test(stylus, test_buttons);
			}
	}

	/* Technology-specific tests */
	if (libwacom_stylus_get_type(stylus) == WSTYLUS_MOBILE) {
		setup_aes_tests(stylus);
	} else {
		setup_emr_tests(stylus);
	}

	if (libwacom_stylus_has_eraser(stylus))
		add_test(stylus, test_has_eraser);

	if (libwacom_stylus_is_eraser(stylus))
		add_test(stylus, test_eraser_type);

	add_test(stylus, test_mutually_paired);
}

/**
 * Return a NULL-terminated list of all styli.
 *
 * libwacom only gives us the styli per-device so this is a bit more
 * complicated than it should be.
 */
static const WacomStylus **
assemble_styli(WacomDeviceDatabase *db)
{
	WacomDevice **devices = libwacom_list_devices_from_database(db, NULL);
	GHashTable *all = g_hash_table_new(g_direct_hash, g_direct_equal);
	const WacomStylus **all_styli = NULL;

	g_assert(devices);

	for (WacomDevice **d = devices; *d; d++) {
		const WacomStylus **styli;
		int nstyli;

		styli = libwacom_get_styli(*d, &nstyli);
		for (int i = 0; i < nstyli; i++) {
			g_hash_table_add(all, (gpointer)styli[i]);
		}
		g_free(styli);
	}

	all_styli = (const WacomStylus**)g_hash_table_get_keys_as_array(all, NULL);
	g_hash_table_steal_all(all);
	g_clear_pointer(&all, g_hash_table_unref);
	g_clear_pointer(&devices, g_free);

	return all_styli;
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
	int rc;

	g_test_init(&argc, &argv, NULL);
	g_test_set_nonfatal_assertions();

	db = load_database();
	all_styli = assemble_styli(db);

	for (const WacomStylus **s = all_styli; *s; s++)
		setup_tests(*s);

	rc = g_test_run();

	free(all_styli);
	libwacom_database_destroy (db);

	return rc;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
