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

static void verify_tablet(WacomDevice *device)
{
	const char *name;
	int nstyli;

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
	if (libwacom_get_class(device) != WCLASS_ISDV4) {
		assert(libwacom_get_width(device) > 0);
		assert(libwacom_get_height(device) > 0);
	}
	assert(libwacom_get_num_buttons(device) >= 0);
	assert(libwacom_get_supported_styli(device, &nstyli) != NULL);
	assert(nstyli > 0);
	assert(libwacom_get_ring_num_modes(device) >= 0);
	assert(libwacom_get_ring2_num_modes(device) >= 0);
	assert(libwacom_get_num_strips(device) >= 0);
	assert(libwacom_get_strips_num_modes(device) >= 0);
	assert(libwacom_get_bustype(device) != WBUSTYPE_UNKNOWN);
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
		verify_tablet(*device);

	libwacom_database_destroy (db);

	return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
