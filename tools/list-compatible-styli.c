/*
 * Copyright © 2019 Red Hat, Inc.
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

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "libwacom.h"

static void
print_device_info(const WacomDeviceDatabase *db, const WacomDevice *device)
{
    const int *styli;
    int nstyli;

    printf("%s", libwacom_get_name(device));
    if (libwacom_get_model_name(device)) {
	    printf(" (%s)", libwacom_get_model_name(device));
    }
    printf(":\n");

    if (!libwacom_has_stylus(device)) {
        printf("\tno styli defined\n");
        return;
    }

    styli = libwacom_get_supported_styli(device, &nstyli);
    for (int i = 0; i < nstyli; i++) {
        const WacomStylus *s;

        s = libwacom_stylus_get_for_id(db, styli[i]);
        printf("\t%#8x:\t%s\n",
               libwacom_stylus_get_id(s),
               libwacom_stylus_get_name(s));
    }
}

int main(int argc, char **argv)
{
	WacomDeviceDatabase *db;
	WacomDevice **list, **p;

	if (argc > 1) {
		printf("Usage: %s [--help] - list all supported devices\n",
		       basename(argv[0]));
	       return !!(strcmp(argv[1], "--help"));
	}

	db = libwacom_database_new_for_path(TOPSRCDIR"/data");

	list = libwacom_list_devices_from_database(db, NULL);
	if (!list) {
		fprintf(stderr, "Failed to load device database.\n");
		return 1;
	}

	for (p = list; *p; p++)
		print_device_info(db, (WacomDevice *)*p);

	libwacom_database_destroy(db);

        return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
