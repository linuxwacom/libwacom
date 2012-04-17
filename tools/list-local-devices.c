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
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include "libwacom.h"

static int event_devices_only(const struct dirent *dir) {
	return strncmp("event", dir->d_name, 5) == 0;
}

int main(int argc, char **argv)
{
	WacomDeviceDatabase *db;
	WacomDevice *dev;
	int i;
	struct dirent **namelist = NULL;

	db = libwacom_database_new_for_path(TOPSRCDIR"/data");

	i = scandir("/dev/input", &namelist, event_devices_only, alphasort);

	if (i < 0 || i == 0) {
		fprintf(stderr, "Failed to find any devices.\n");
		goto out;
	}

	while (i--) {
		char fname[64];

		snprintf(fname, 63, "/dev/input/%s", namelist[i]->d_name);
		dev = libwacom_new_from_path(db, fname, WFALLBACK_NONE, NULL);
		if (!dev)
			continue;
		libwacom_print_device_description(0, dev);
		libwacom_destroy(dev);

		fprintf(stdout, "---------------------------------------------------------------\n");
	}

out:
	if (namelist)
		free(namelist);

	libwacom_database_destroy (db);
	return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
