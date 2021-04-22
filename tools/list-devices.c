/*
 * Copyright © 2012-2021 Red Hat, Inc.
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

#include "config.h"

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <libgen.h>
#include <unistd.h>
#include <glib/gi18n.h>
#include <glib.h>
#include "libwacom.h"

static enum output_format {
	YAML,
	DATAFILE,
} output_format = YAML;

static void print_device_info (WacomDevice *device, WacomBusType bus_type_filter,
			       enum output_format format)
{
	const WacomMatch **match;

	for (match = libwacom_get_matches(device); *match; match++) {
		WacomBusType type = libwacom_match_get_bustype(*match);

		if (type != bus_type_filter)
			continue;

		if (format == DATAFILE) {
			libwacom_print_device_description(STDOUT_FILENO, device);
			dprintf(STDOUT_FILENO, "---------------------------------------------------------------\n");
		} else {
			const char *name = libwacom_get_name(device);
			const char *bus = "unknown";
			int vid = libwacom_match_get_vendor_id(*match);
			int pid = libwacom_match_get_product_id(*match);

			switch (type) {
				case WBUSTYPE_USB:	bus = "usb"; break;
				case WBUSTYPE_SERIAL:	bus = "serial"; break;
				case WBUSTYPE_BLUETOOTH:bus = "bluetooth"; break;
				case WBUSTYPE_I2C:	bus = "i2c"; break;
				default:
				   break;
			}

			/* We don't need to print the generic device */
			if (vid != 0 || pid != 0 || bus != 0)
				printf("- { bus: '%s',%*svid: '0x%04x', pid: '0x%04x', name: '%s' }\n",
				       bus, (int)(10 - strlen(bus)), " ",
				       vid, pid, name);
		}
	}
}

static gboolean
check_format(const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
	if (g_str_equal(value, "datafile"))
		output_format = DATAFILE;
	else if (g_str_equal(value, "yaml"))
		output_format = YAML;
	else
		return FALSE;
	return TRUE;
}

static GOptionEntry opts[] = {
	{ "format", 0, 0, G_OPTION_ARG_CALLBACK, check_format, N_("Output format, one of 'yaml', 'datafile'"), NULL },
	{ .long_name = NULL}
};

int main(int argc, char **argv)
{
	WacomDeviceDatabase *db;
	WacomDevice **list, **p;
	GOptionContext *context;
	GError *error = NULL;

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, opts, NULL);
	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		if (error != NULL) {
			fprintf (stderr, "%s\n", error->message);
			g_error_free (error);
		}
		return EXIT_FAILURE;
	}
	g_option_context_free (context);

#ifdef DATABASEPATH
	db = libwacom_database_new_for_path(DATABASEPATH);
#else
	db = libwacom_database_new();
#endif

	list = libwacom_list_devices_from_database(db, NULL);
	if (!list) {
		fprintf(stderr, "Failed to load device database.\n");
		return 1;
	}

	if (output_format == YAML)
		printf("devices:\n");

	for (p = list; *p; p++)
		print_device_info ((WacomDevice *) *p, WBUSTYPE_USB, output_format);

	for (p = list; *p; p++)
		print_device_info ((WacomDevice *) *p, WBUSTYPE_BLUETOOTH, output_format);

	for (p = list; *p; p++)
		print_device_info ((WacomDevice *) *p, WBUSTYPE_I2C, output_format);

	for (p = list; *p; p++)
		print_device_info ((WacomDevice *) *p, WBUSTYPE_SERIAL, output_format);

	for (p = list; *p; p++)
		print_device_info ((WacomDevice *) *p, WBUSTYPE_UNKNOWN, output_format);

	libwacom_database_destroy (db);
	free(list);

	return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
