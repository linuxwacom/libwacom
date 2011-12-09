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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "libwacomint.h"

#include <glib.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define SUFFIX ".tablet"
#define FEATURE_GROUP "Features"
#define DEVICE_GROUP "Device"

static WacomClass
libwacom_model_string_to_enum(const char *model)
{
	if (model == NULL || *model == '\0')
		return WCLASS_UNKNOWN;

	if (strcmp(model, "Intuos3") == 0)
		return WCLASS_INTUOS3;
	if (strcmp(model, "Intuos4") == 0)
		return WCLASS_INTUOS4;
	if (strcmp(model, "Cintiq") == 0)
		return WCLASS_CINTIQ;
	if (strcmp(model, "Bamboo") == 0)
		return WCLASS_BAMBOO;
	if (strcmp(model, "Graphire") == 0)
		return WCLASS_GRAPHIRE;

	return WCLASS_UNKNOWN;
}

WacomBusType
bus_from_str (const char *str)
{
	if (strcmp (str, "usb") == 0)
		return WBUSTYPE_USB;
	if (strcmp (str, "serial") == 0)
		return WBUSTYPE_SERIAL;
	if (strcmp (str, "bluetooth") == 0)
		return WBUSTYPE_BLUETOOTH;
	return WBUSTYPE_UNKNOWN;
}

static int
libwacom_matchstr_to_ints(const char *match, uint32_t *vendor_id, uint32_t *product_id, WacomBusType *bus)
{
	char busstr[64];
	int rc;
	rc = sscanf(match, "%63[^:]:%x:%x", busstr, vendor_id, product_id);
	if (rc != 3)
		return 0;

	*bus = bus_from_str (busstr);

	return 1;
}

static WacomDeviceData*
libwacom_parse_keyfile(const char *path)
{
	WacomDeviceData *device = NULL;
	GKeyFile *keyfile;
	GError *error = NULL;
	gboolean rc;
	char *class;
	char *match;

	keyfile = g_key_file_new();

	rc = g_key_file_load_from_file(keyfile, path, G_KEY_FILE_NONE, &error);

	if (!rc) {
		DBG("%s: %s\n", path, error->message);
		goto out;
	}

	device = libwacom_new_devicedata();
	if (!device) {
		DBG("Cannot allocate memory\n");
		goto out;
	}

	device->vendor = g_key_file_get_string(keyfile, DEVICE_GROUP, "Vendor", NULL);
	device->product = g_key_file_get_string(keyfile, DEVICE_GROUP, "Product", NULL);
	device->width = g_key_file_get_integer(keyfile, DEVICE_GROUP, "Width", NULL);
	device->height = g_key_file_get_integer(keyfile, DEVICE_GROUP, "Height", NULL);

	class = g_key_file_get_string(keyfile, DEVICE_GROUP, "Class", NULL);
	device->cls = libwacom_model_string_to_enum(class);
	g_free(class);

	match = g_key_file_get_string(keyfile, DEVICE_GROUP, "DeviceMatch", NULL);
	if (!libwacom_matchstr_to_ints(match, &device->vendor_id, &device->product_id, &device->bus))
		DBG("failed to match %s for product/vendor IDs\n", match);
	g_free(match);

	/* Features */
	if (g_key_file_get_boolean(keyfile, FEATURE_GROUP, "Stylus", NULL))
		device->features |= FEATURE_STYLUS;

	if (g_key_file_get_boolean(keyfile, FEATURE_GROUP, "Touch", NULL))
		device->features |= FEATURE_TOUCH;

	if (g_key_file_get_boolean(keyfile, FEATURE_GROUP, "Ring", NULL))
		device->features |= FEATURE_RING;

	if (g_key_file_get_boolean(keyfile, FEATURE_GROUP, "Ring2", NULL))
		device->features |= FEATURE_RING2;

	if (g_key_file_get_boolean(keyfile, FEATURE_GROUP, "VStrip", NULL))
		device->features |= FEATURE_VSTRIP;

	if (g_key_file_get_boolean(keyfile, FEATURE_GROUP, "HStrip", NULL))
		device->features |= FEATURE_HSTRIP;

	if (g_key_file_get_boolean(keyfile, FEATURE_GROUP, "BuiltIn", NULL))
		device->features |= FEATURE_BUILTIN;

	if (g_key_file_get_boolean(keyfile, FEATURE_GROUP, "Reversible", NULL))
		device->features |= FEATURE_REVERSIBLE;

	device->num_buttons = g_key_file_get_integer(keyfile, FEATURE_GROUP, "Buttons", NULL);

out:
	if (keyfile)
		g_key_file_free(keyfile);
	if (error)
		g_error_free(error);

	return device;
}

static int
scandir_filter(const struct dirent *entry)
{
	const char *name = entry->d_name;
	int len, suffix_len;

	if (!name || name[0] == '.')
		return 0;

	len = strlen(name);
	suffix_len = strlen(SUFFIX);
	if (len <= suffix_len)
		return 0;

	return !strcmp(&name[len - suffix_len], SUFFIX);
}

int
libwacom_load_database(WacomDevice* device)
{
    int n, nfiles;
    struct dirent **files;
    WacomDeviceData **tmp;
    int ndevices = 0;

    n = scandir(DATADIR, &files, scandir_filter, alphasort);

    if (n <= 0)
	    return 0;

    device->database = calloc(n, sizeof(device));
    if (!device->database)
	    goto out;

    nfiles = n;
    while(n--) {
	    WacomDeviceData *d;
	    char *path = malloc(strlen(DATADIR) + strlen(files[n]->d_name) + 2);

	    if (!path)
		    break;

	    strcpy(path, DATADIR);
	    strcat(path, "/");
	    strcat(path, files[n]->d_name);
	    d = libwacom_parse_keyfile(path);
	    free(path);

	    if (d)
		    device->database[ndevices++] = d;
    }

    tmp = realloc(device->database, ndevices * sizeof(*device));
    if (tmp)
	    device->database = tmp;
    device->nentries = ndevices;

out:
    while(nfiles--)
	    free(files[nfiles]);
    free(files);

    return !!ndevices;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
