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

static int
scandir_filter(const struct dirent *entry)
{
	return strncmp(entry->d_name, ".", 1);
}

static void
rmtmpdir(const char *dir)
{
	int nfiles;
	struct dirent **files;
	char *path = NULL;

	nfiles = scandir(dir, &files, scandir_filter, alphasort);
	while(nfiles--)
	{
		assert(asprintf(&path, "%s/%s", dir, files[nfiles]->d_name) != -1);
		assert(path);
		remove(path);
		free(files[nfiles]);
		free(path);
		path = NULL;
	}

	free(files);
	remove(dir);
}


static void
compare_databases(WacomDeviceDatabase *orig, WacomDeviceDatabase *new)
{
	int ndevices = 0, i;
	WacomDevice **oldall, **o;
	WacomDevice **newall, **n;
	char *old_matched;

	oldall = libwacom_list_devices_from_database(orig, NULL);
	newall = libwacom_list_devices_from_database(new, NULL);

	for (o = oldall; *o; o++)
		ndevices++;

	old_matched = calloc(ndevices, sizeof(char));
	assert(old_matched);

	for (n = newall; *n; n++)
	{
		int found = 0;
		printf("Matching %s\n", libwacom_get_name(*n));
		for (o = oldall, i = 0; *o && !found; o++, i++)
			/* devices with multiple matches will have multiple
			 * devices in the list */
			if (old_matched[i] == 0 &&
			    libwacom_compare(*n, *o, WCOMPARE_MATCHES) == 0) {
				found = 1;
				old_matched[i] = 1;
			}

		if (!found)
			printf("Failed to match '%s'\n", libwacom_get_name(*n));
		assert(found);
	}

	for (i = 0; i < ndevices; i++) {
		if (!old_matched[i])
			printf("No match for %s\n",
					libwacom_get_name(oldall[i]));
		assert(old_matched[i]);
	}

	free(old_matched);
	free(oldall);
	free(newall);
}

/* write out the current db, read it back in, compare */
static void
compare_written_database(WacomDeviceDatabase *db)
{
	char *dirname;
	WacomDeviceDatabase *db_new;
        WacomDevice **device, **devices;
	int i;

	devices = libwacom_list_devices_from_database(db, NULL);
	assert(devices);
	assert(*devices);

	dirname = strdup("tmp.dbverify.XXXXXX");
	assert(mkdtemp(dirname)); /* just check for non-null to avoid
				     Coverity complaints */

	for (device = devices, i = 0; *device; device++, i++) {
		int i;
		int fd;
		char *path = NULL;
		int nstyli;
		const int *styli;

		assert(asprintf(&path, "%s/%d-%04x-%04x.tablet", dirname,
				libwacom_get_bustype(*device),
				libwacom_get_vendor_id(*device),
				libwacom_get_product_id(*device)) != -1);
		assert(path);
		fd = open(path, O_WRONLY|O_CREAT, S_IRWXU);
		assert(fd >= 0);
		libwacom_print_device_description(fd, *device);
		close(fd);
		free(path);

		if (!libwacom_has_stylus(*device))
			continue;

		styli = libwacom_get_supported_styli(*device, &nstyli);
		for (i = 0; i < nstyli; i++) {
			int fd_stylus;
			const WacomStylus *stylus;

			assert(asprintf(&path, "%s/%#x.stylus", dirname, styli[i]) != -1);
			stylus = libwacom_stylus_get_for_id(db, styli[i]);
			assert(stylus);
			fd_stylus = open(path, O_WRONLY|O_CREAT, S_IRWXU);
			assert(fd_stylus >= 0);
			libwacom_print_stylus_description(fd_stylus, stylus);
			close(fd_stylus);
			free(path);
		}
	}

	db_new = libwacom_database_new_for_path(dirname);
	assert(db_new);
	compare_databases(db, db_new);
	libwacom_database_destroy(db_new);

	rmtmpdir(dirname);
	free(dirname);
	free(devices);
}


int main(int argc, char **argv)
{
	WacomDeviceDatabase *db;

	db = libwacom_database_new_for_path(TOPSRCDIR"/data");
	if (!db)
		printf("Failed to load data from %s", TOPSRCDIR"/data");
	assert(db);


	compare_written_database(db);
	libwacom_database_destroy (db);

	return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
