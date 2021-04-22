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

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <dirent.h>
#include <glib/gi18n.h>
#include <glib.h>
#include <gudev/gudev.h>
#include "libwacom.h"

static enum output_format {
	YAML,
	DATAFILE,
} output_format = YAML;

static char *database_path;

/* Most devices have 2-3 event nodes, let's have a wrapper struct to group
 * those together */
struct tablet {
	WacomDevice *dev;
	GList *nodes; /* list of "/dev/input/eventX" paths */
};

static void
tablet_destroy(gpointer data)
{
	struct tablet *d = data;

	libwacom_destroy(d->dev);
	g_list_free_full(d->nodes, free);
};

/* Note: users with two identical devices plugged in will see
 * as one device with twice the event nodes.
 * Too niche to worry about.
 */
static gint
tablet_compare(gconstpointer list_elem, gconstpointer dev)
{
	const struct tablet *t = list_elem;

	return libwacom_compare(t->dev, dev, WCOMPARE_MATCHES);
}

static void
print_node(gpointer data, gpointer user_data)
{
	printf("#  - %s\n", (char *)data);
}

static void
tablet_print(gpointer data, gpointer user_data)
{
	struct tablet *d = data;

	printf("# %s\n", libwacom_get_name(d->dev));
	g_list_foreach(d->nodes, print_node, NULL);
	libwacom_print_device_description(STDOUT_FILENO, d->dev);
	printf("---------------------------------------------------------------\n");
}

static void
print_str(gpointer data, gpointer user_data)
{
	printf("  - %s\n", (char *)data);
}

static void
tablet_print_yaml(gpointer data, gpointer user_data)
{
	struct tablet *d = data;
	const char *name = libwacom_get_name(d->dev);
	const char *bus = "unknown";
	int vid = libwacom_get_vendor_id(d->dev);
	int pid = libwacom_get_product_id(d->dev);
	WacomBusType bustype = libwacom_get_bustype(d->dev);

	switch (bustype) {
		case WBUSTYPE_USB:	bus = "usb"; break;
		case WBUSTYPE_SERIAL:	bus = "serial"; break;
		case WBUSTYPE_BLUETOOTH:bus = "bluetooth"; break;
		case WBUSTYPE_I2C:	bus = "i2c"; break;
		default:
			break;
	}

	printf("- name: '%s'\n", name);
	printf("  bus: '%s'\n", bus);
	printf("  vid: '0x%04x'\n", vid);
	printf("  pid: '0x%04x'\n", pid);
	printf("  nodes: \n");
	g_list_foreach(d->nodes, print_str, NULL);
}

static void
check_if_udev_tablet(const char *path)
{
	GUdevClient *client;
	GUdevDevice *device;
	const char * const subsystems[] = { "input", NULL };

	client = g_udev_client_new (subsystems);
	device = g_udev_client_query_by_device_file (client, path);
	if (device &&
	    g_udev_device_get_property_as_boolean (device, "ID_INPUT_TABLET")) {
		fprintf(stderr,
			"%s is a tablet but not supported by libwacom\n",
			path);
	}
	g_object_unref (device);
	g_object_unref (client);
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
        {"database", 0, 0, G_OPTION_ARG_FILENAME, &database_path, N_("Path to device database"), NULL },
	{ "format", 0, 0, G_OPTION_ARG_CALLBACK, check_format, N_("Output format, one of 'yaml', 'datafile'"), NULL },
	{ .long_name = NULL}
};

int main(int argc, char **argv)
{
	WacomDeviceDatabase *db;
	GOptionContext *context;
	GError *error;
	GList *tabletlist = NULL;
	GDir *dir = NULL;
	const char *filename;

	context = g_option_context_new (NULL);

	g_option_context_add_main_entries (context, opts, NULL);
	error = NULL;

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		if (error != NULL) {
			fprintf (stderr, "%s\n", error->message);
			g_error_free (error);
		}
		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	if (database_path) {
		db = libwacom_database_new_for_path(database_path);
		g_free (database_path);
	} else {
		db = libwacom_database_new();
	}

	if (!db) {
		fprintf(stderr, "Failed to initialize device database\n");
		return EXIT_FAILURE;
	}

	dir = g_dir_open("/dev/input", 0, &error);
	if (!dir) {
		fprintf(stderr, "%s\n", error->message);
		g_error_free(error);
		return EXIT_FAILURE;
	}

	while ((filename = g_dir_read_name(dir))) {
		WacomDevice *dev;
		char fname[PATH_MAX];
		GList *found;

		if (!g_str_has_prefix(filename, "event"))
			continue;

		snprintf(fname, sizeof(fname), "/dev/input/%s", filename);

		dev = libwacom_new_from_path(db, fname, WFALLBACK_NONE, NULL);
		if (!dev) {
			check_if_udev_tablet(fname);
			continue;
		}

		found = g_list_find_custom(tabletlist, dev, tablet_compare);
		if (found) {
			struct tablet *t = found->data;
			t->nodes = g_list_append(t->nodes, g_strdup(fname));
			libwacom_destroy(dev);
		} else {
			struct tablet *t = g_new0(struct tablet, 1);
			t->dev = dev;
			t->nodes = g_list_append(t->nodes, g_strdup(fname));
			tabletlist = g_list_append(tabletlist, t);
		}
	}

	if (!tabletlist) {
		fprintf(stderr, "Failed to find any devices known to libwacom.\n");
	} else {
		switch (output_format) {
		case DATAFILE:
			g_list_foreach(tabletlist, tablet_print, NULL);
			break;
		case YAML:
			printf("devices:\n");
			g_list_foreach(tabletlist, tablet_print_yaml, NULL);
			break;
		default:
			abort();
		}
	}

	g_list_free_full(tabletlist, tablet_destroy);
	g_dir_close(dir);
	libwacom_database_destroy (db);
	return 0;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
