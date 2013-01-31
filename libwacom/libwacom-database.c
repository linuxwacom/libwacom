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

#define TABLET_SUFFIX ".tablet"
#define STYLUS_SUFFIX ".stylus"
#define FEATURES_GROUP "Features"
#define DEVICE_GROUP "Device"
#define BUTTONS_GROUP "Buttons"

static WacomClass
libwacom_class_string_to_enum(const char *class)
{
	if (class == NULL || *class == '\0')
		return WCLASS_UNKNOWN;

	if (strcmp(class, "Intuos3") == 0)
		return WCLASS_INTUOS3;
	if (strcmp(class, "Intuos4") == 0)
		return WCLASS_INTUOS4;
	if (strcmp(class, "Intuos5") == 0)
		return WCLASS_INTUOS5;
	if (strcmp(class, "Cintiq") == 0)
		return WCLASS_CINTIQ;
	if (strcmp(class, "Bamboo") == 0)
		return WCLASS_BAMBOO;
	if (strcmp(class, "Graphire") == 0)
		return WCLASS_GRAPHIRE;
	if (strcmp(class, "Intuos") == 0)
		return WCLASS_INTUOS;
	if (strcmp(class, "Intuos2") == 0)
		return WCLASS_INTUOS2;
	if (strcmp(class, "ISDV4") == 0)
		return WCLASS_ISDV4;
	if (strcmp(class, "PenDisplay") == 0)
		return WCLASS_PEN_DISPLAYS;

	return WCLASS_UNKNOWN;
}

WacomStylusType
type_from_str (const char *type)
{
	if (type == NULL)
		return WSTYLUS_UNKNOWN;
	if (strcmp (type, "General") == 0)
		return WSTYLUS_GENERAL;
	if (strcmp (type, "Inking") == 0)
		return WSTYLUS_INKING;
	if (strcmp (type, "Airbrush") == 0)
		return WSTYLUS_AIRBRUSH;
	if (strcmp (type, "Classic") == 0)
		return WSTYLUS_CLASSIC;
	if (strcmp (type, "Marker") == 0)
		return WSTYLUS_MARKER;
	if (strcmp (type, "Stroke") == 0)
		return WSTYLUS_STROKE;
	if (strcmp (type, "Puck") == 0)
		return WSTYLUS_PUCK;
	return WSTYLUS_UNKNOWN;
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

const char *
bus_to_str (WacomBusType bus)
{
	switch (bus) {
	case WBUSTYPE_UNKNOWN:
		g_assert_not_reached();
		break;
	case WBUSTYPE_USB:
		return "usb";
	case WBUSTYPE_SERIAL:
		return "serial";
	case WBUSTYPE_BLUETOOTH:
		return "bluetooth";
	}
	g_assert_not_reached ();
}

char *
make_match_string (WacomBusType bus, int vendor_id, int product_id)
{
	return g_strdup_printf("%s:%04x:%04x", bus_to_str (bus), vendor_id, product_id);
}

static int
libwacom_matchstr_to_matches(WacomDevice *device, const char *match)
{
	int rc = 1;
	char **strs;
	int i, nmatches = 0;
	WacomBusType first_bus;
	int first_vendor_id, first_product_id;

	if (match == NULL)
		return 0;

	strs = g_strsplit(match, ";", 0);
	for (i = 0; strs[i] != NULL && *strs[i] != '\0'; i++) {
		char busstr[64];
		int vendor_id, product_id;
		WacomBusType bus;
		rc = sscanf(strs[i], "%63[^:]:%x:%x", busstr, &vendor_id, &product_id);
		if (rc != 3) {
			DBG("failed to match '%s' for product/vendor IDs. Skipping.\n", strs[i]);
			continue;
		}
		bus = bus_from_str (busstr);

		libwacom_update_match(device, bus, vendor_id, product_id);

		if (nmatches == 0) {
			first_bus = bus;
			first_vendor_id = vendor_id;
			first_product_id = product_id;
		}
		nmatches++;
	}

	/* set default to first entry */
	if (nmatches > 1)
		libwacom_update_match(device, first_bus, first_vendor_id, first_product_id);

	g_strfreev(strs);
	return i;
}

static void
libwacom_parse_stylus_keyfile(WacomDeviceDatabase *db, const char *path)
{
	GKeyFile *keyfile;
	GError *error = NULL;
	char **groups;
	gboolean rc;
	guint i;

	keyfile = g_key_file_new();

	rc = g_key_file_load_from_file(keyfile, path, G_KEY_FILE_NONE, &error);
	g_assert (rc);
	groups = g_key_file_get_groups (keyfile, NULL);
	for (i = 0; groups[i]; i++) {
		WacomStylus *stylus;
		GError *error = NULL;
		char *type;
		int id;

		id = strtol (groups[i], NULL, 16);
		if (id == 0) {
			g_warning ("Failed to parse stylus ID '%s'", groups[i]);
			continue;
		}

		stylus = g_new0 (WacomStylus, 1);
		stylus->id = id;
		stylus->name = g_key_file_get_string(keyfile, groups[i], "Name", NULL);

		stylus->is_eraser = g_key_file_get_boolean(keyfile, groups[i], "IsEraser", NULL);

		if (stylus->is_eraser == FALSE) {
			stylus->has_eraser = g_key_file_get_boolean(keyfile, groups[i], "HasEraser", NULL);
			stylus->num_buttons = g_key_file_get_integer(keyfile, groups[i], "Buttons", &error);
			if (stylus->num_buttons == 0 && error != NULL) {
				stylus->num_buttons = -1;
				g_clear_error (&error);
			}
			stylus->has_lens = g_key_file_get_boolean(keyfile, groups[i], "HasLens", NULL);
		} else {
			stylus->num_buttons = 0;
			stylus->has_eraser = FALSE;
			stylus->has_lens = FALSE;
		}

		type = g_key_file_get_string(keyfile, groups[i], "Type", NULL);
		stylus->type = type_from_str (type);
		g_free (type);

		if (g_hash_table_lookup (db->stylus_ht, GINT_TO_POINTER (id)) != NULL)
			g_warning ("Duplicate definition for stylus ID '0x%x'", id);

		g_hash_table_insert (db->stylus_ht, GINT_TO_POINTER (id), stylus);
	}
	g_strfreev (groups);
	if (keyfile)
		g_key_file_free (keyfile);
}

struct {
	const char       *key;
	WacomButtonFlags  flag;
} options[] = {
	{ "Left", WACOM_BUTTON_POSITION_LEFT },
	{ "Right", WACOM_BUTTON_POSITION_RIGHT },
	{ "Top", WACOM_BUTTON_POSITION_TOP },
	{ "Bottom", WACOM_BUTTON_POSITION_BOTTOM },
	{ "Ring", WACOM_BUTTON_RING_MODESWITCH },
	{ "Ring2", WACOM_BUTTON_RING2_MODESWITCH },
	{ "Touchstrip", WACOM_BUTTON_TOUCHSTRIP_MODESWITCH },
	{ "Touchstrip2", WACOM_BUTTON_TOUCHSTRIP2_MODESWITCH },
	{ "OLEDs", WACOM_BUTTON_OLED }
};

struct {
	const char       *key;
	WacomStatusLEDs   value;
} supported_leds[] = {
	{ "Ring",		WACOM_STATUS_LED_RING },
	{ "Ring2",		WACOM_STATUS_LED_RING2 },
	{ "Touchstrip",		WACOM_STATUS_LED_TOUCHSTRIP },
	{ "Touchstrip2",	WACOM_STATUS_LED_TOUCHSTRIP2 }
};

struct {
	const char             *key;
	WacomIntegrationFlags   value;
} integration_flags[] = {
	{ "Display",		WACOM_DEVICE_INTEGRATED_DISPLAY },
	{ "System",		WACOM_DEVICE_INTEGRATED_SYSTEM }
};

static void
libwacom_parse_buttons_key(WacomDevice      *device,
			   GKeyFile         *keyfile,
			   const char       *key,
			   WacomButtonFlags  flag)
{
	guint i;
	char **vals;

	vals = g_key_file_get_string_list (keyfile, BUTTONS_GROUP, key, NULL, NULL);
	if (vals == NULL)
		return;
	for (i = 0; vals[i] != NULL; i++) {
		char val;

		val = *vals[i];
		if (strlen (vals[i]) > 1 ||
		    val < 'A' ||
		    val > 'Z') {
			g_warning ("Ignoring value '%s' in key '%s'", vals[i], key);
			continue;
		}
		val -= 'A';
		device->buttons[(int) val] |= flag;
	}

	g_strfreev (vals);
}

static int
libwacom_parse_num_modes (WacomDevice      *device,
			  GKeyFile         *keyfile,
			  const char       *key,
			  WacomButtonFlags  flag)
{
	int num;
	guint i;

	num = g_key_file_get_integer (keyfile, BUTTONS_GROUP, key, NULL);
	if (num > 0)
		return num;
	for (i = 0; i < device->num_buttons; i++) {
		if (device->buttons[i] & flag)
			num++;
	}
	return num;
}

static void
libwacom_parse_buttons(WacomDevice *device,
		       GKeyFile    *keyfile)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS (options); i++)
		libwacom_parse_buttons_key(device, keyfile, options[i].key, options[i].flag);

	device->ring_num_modes = libwacom_parse_num_modes(device, keyfile, "RingNumModes", WACOM_BUTTON_RING_MODESWITCH);
	device->ring2_num_modes = libwacom_parse_num_modes(device, keyfile, "Ring2NumModes", WACOM_BUTTON_RING2_MODESWITCH);
	device->strips_num_modes = libwacom_parse_num_modes(device, keyfile, "StripsNumModes", WACOM_BUTTON_TOUCHSTRIP_MODESWITCH);
}

static WacomDevice*
libwacom_parse_tablet_keyfile(const char *datadir, const char *filename)
{
	WacomDevice *device = NULL;
	GKeyFile *keyfile;
	GError *error = NULL;
	gboolean rc;
	char *path;
	char *layout;
	char *class;
	char *match;
	char **string_list;

	keyfile = g_key_file_new();

	path = g_build_filename (datadir, filename, NULL);
	rc = g_key_file_load_from_file(keyfile, path, G_KEY_FILE_NONE, &error);

	if (!rc) {
		DBG("%s: %s\n", path, error->message);
		goto out;
	}

	device = g_new0 (WacomDevice, 1);

	match = g_key_file_get_string(keyfile, DEVICE_GROUP, "DeviceMatch", NULL);
	if (g_strcmp0 (match, GENERIC_DEVICE_MATCH) == 0) {
		libwacom_update_match(device, WBUSTYPE_UNKNOWN, 0, 0);
	} else {
		if (libwacom_matchstr_to_matches(device, match) == 0) {
			DBG("failed to match '%s' for product/vendor IDs in '%s'\n", match, path);
			g_free (match);
			g_free (device);
			device = NULL;
			goto out;
		}
	}
	g_free (match);

	device->name = g_key_file_get_string(keyfile, DEVICE_GROUP, "Name", NULL);
	device->width = g_key_file_get_integer(keyfile, DEVICE_GROUP, "Width", NULL);
	device->height = g_key_file_get_integer(keyfile, DEVICE_GROUP, "Height", NULL);

	device->integration_flags = WACOM_DEVICE_INTEGRATED_UNSET;
	string_list = g_key_file_get_string_list(keyfile, DEVICE_GROUP, "IntegratedIn", NULL, NULL);
	if (string_list) {
		guint i, n;
		gboolean found;

		device->integration_flags = WACOM_DEVICE_INTEGRATED_NONE;
		for (i = 0; string_list[i]; i++) {
			found = FALSE;
			for (n = 0; n < G_N_ELEMENTS (integration_flags); n++) {
				if (!strcmp(string_list[i], integration_flags[n].key)) {
					device->integration_flags |= integration_flags[n].value;
					found = TRUE;
					break;
				}
			}
			if (!found)
				g_warning ("Unrecognized integration flag '%s'", string_list[i]);
		}
		g_strfreev (string_list);
	}

	layout = g_key_file_get_string(keyfile, DEVICE_GROUP, "Layout", NULL);
	if (layout) {
		/* For the layout, we store the full path to the SVG layout */
		device->layout = g_build_filename (datadir, "layouts", layout, NULL);
		g_free (layout);
	}

	class = g_key_file_get_string(keyfile, DEVICE_GROUP, "Class", NULL);
	device->cls = libwacom_class_string_to_enum(class);
	g_free(class);

	string_list = g_key_file_get_string_list(keyfile, DEVICE_GROUP, "Styli", NULL, NULL);
	if (string_list) {
		GArray *array;
		guint i;

		array = g_array_new (FALSE, FALSE, sizeof(int));
		device->num_styli = 0;
		for (i = 0; string_list[i]; i++) {
			glong long_value = strtol (string_list[i], NULL, 0);
			int int_value = long_value;

			g_array_append_val (array, int_value);
			device->num_styli++;
		}
		g_strfreev (string_list);
		device->supported_styli = (int *) g_array_free (array, FALSE);
	} else {
		device->supported_styli = g_new (int, 2);
		device->supported_styli[0] = WACOM_STYLUS_FALLBACK_ID;
		device->supported_styli[1] = WACOM_ERASER_FALLBACK_ID;
		device->num_styli = 2;
	}

	/* Features */
	if (g_key_file_get_boolean(keyfile, FEATURES_GROUP, "Stylus", NULL))
		device->features |= FEATURE_STYLUS;

	if (g_key_file_get_boolean(keyfile, FEATURES_GROUP, "Touch", NULL))
		device->features |= FEATURE_TOUCH;

	if (g_key_file_get_boolean(keyfile, FEATURES_GROUP, "Ring", NULL))
		device->features |= FEATURE_RING;

	if (g_key_file_get_boolean(keyfile, FEATURES_GROUP, "Ring2", NULL))
		device->features |= FEATURE_RING2;

	if (g_key_file_get_boolean(keyfile, FEATURES_GROUP, "Reversible", NULL))
		device->features |= FEATURE_REVERSIBLE;

	if (device->integration_flags != WACOM_DEVICE_INTEGRATED_UNSET &&
	    device->integration_flags & WACOM_DEVICE_INTEGRATED_DISPLAY &&
	    device->features & FEATURE_REVERSIBLE)
		g_warning ("Tablet '%s' is both reversible and integrated in screen. This is impossible", libwacom_get_match(device));

	if (!(device->features & FEATURE_RING) &&
	    (device->features & FEATURE_RING2))
		g_warning ("Table '%s' has Ring2 but no Ring. This is impossible", libwacom_get_match(device));

	device->num_strips = g_key_file_get_integer(keyfile, FEATURES_GROUP, "NumStrips", NULL);
	device->num_buttons = g_key_file_get_integer(keyfile, FEATURES_GROUP, "Buttons", &error);
	if (device->num_buttons == 0 &&
	    g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
		g_warning ("Tablet '%s' has no buttons defined, do something!", libwacom_get_match(device));
		g_clear_error (&error);
	}
	if (device->num_buttons > 0) {
		device->buttons = g_new0 (WacomButtonFlags, device->num_buttons);
		libwacom_parse_buttons(device, keyfile);
	}

	string_list = g_key_file_get_string_list(keyfile, FEATURES_GROUP, "StatusLEDs", NULL, NULL);
	if (string_list) {
		GArray *array;
		guint i, n;

		array = g_array_new (FALSE, FALSE, sizeof(WacomStatusLEDs));
		device->num_leds = 0;
		for (i = 0; string_list[i]; i++) {
			for (n = 0; n < G_N_ELEMENTS (supported_leds); n++) {
				if (!strcmp(string_list[i], supported_leds[n].key)) {
					g_array_append_val (array, supported_leds[n].value);
					device->num_leds++;
					break;
				}
			}
		}
		g_strfreev (string_list);
		device->status_leds = (WacomStatusLEDs *) g_array_free (array, FALSE);
	}

out:
	if (path)
		g_free(path);
	if (keyfile)
		g_key_file_free(keyfile);
	if (error)
		g_error_free(error);

	return device;
}

static int
scandir_tablet_filter(const struct dirent *entry)
{
	const char *name = entry->d_name;
	int len, suffix_len;

	if (!name || name[0] == '.')
		return 0;

	len = strlen(name);
	suffix_len = strlen(TABLET_SUFFIX);
	if (len <= suffix_len)
		return 0;

	return !strcmp(&name[len - suffix_len], TABLET_SUFFIX);
}

static int
scandir_stylus_filter(const struct dirent *entry)
{
	const char *name = entry->d_name;
	int len, suffix_len;

	if (!name || name[0] == '.')
		return 0;

	len = strlen(name);
	suffix_len = strlen(STYLUS_SUFFIX);
	if (len <= suffix_len)
		return 0;

	return !strcmp(&name[len - suffix_len], STYLUS_SUFFIX);
}


WacomDeviceDatabase *
libwacom_database_new_for_path (const char *datadir)
{
    int n, nfiles;
    struct dirent **files;
    WacomDeviceDatabase *db;
    char *path;

    db = g_new0 (WacomDeviceDatabase, 1);

    /* Load tablets */
    db->device_ht = g_hash_table_new_full (g_str_hash,
					   g_str_equal,
					   g_free,
					   (GDestroyNotify) libwacom_destroy);

    n = scandir(datadir, &files, scandir_tablet_filter, alphasort);
    if (n <= 0) {
	    libwacom_database_destroy(db);
	    return NULL;
    }

    nfiles = n;
    while(n--) {
	    WacomDevice *d;
	    const WacomMatch **matches, **match;

	    d = libwacom_parse_tablet_keyfile(datadir, files[n]->d_name);

	    if (!d)
		    continue;

	    matches = libwacom_get_matches(d);
	    for (match = matches; *match; match++) {
		    const char *matchstr;
		    matchstr = libwacom_match_get_match_string(*match);
		    /* no duplicate matches allowed */
		    if (g_hash_table_lookup(db->device_ht, matchstr) != NULL) {
			    g_critical("Duplicate match of '%s' on device '%s'.",
					matchstr, libwacom_get_name(d));
			    libwacom_database_destroy(db);
			    db = NULL;
			    goto end;
		    }
		    g_hash_table_insert (db->device_ht, g_strdup (matchstr), d);
		    d->refcnt++;
	    }
    }

    while(nfiles--)
	    free(files[nfiles]);
    free(files);

    /* Load styli */
    n = scandir(datadir, &files, scandir_stylus_filter, alphasort);
    if (n <= 0) {
	    libwacom_database_destroy(db);
	    return NULL;
    }

    db->stylus_ht = g_hash_table_new_full (g_direct_hash,
					   g_direct_equal,
					   NULL,
					   (GDestroyNotify) libwacom_stylus_destroy);
    nfiles = n;
    while(n--) {
	    path = g_build_filename (datadir, files[n]->d_name, NULL);
	    libwacom_parse_stylus_keyfile(db, path);
	    g_free(path);
    }

    /* If we couldn't load _anything_ then something's wrong */
    if (g_hash_table_size (db->device_ht) == 0 &&
	g_hash_table_size (db->stylus_ht) == 0) {
	    libwacom_database_destroy(db);
	    db = NULL;
    }

end:
    while(nfiles--)
	    free(files[nfiles]);
    free(files);

    return db;
}

WacomDeviceDatabase *
libwacom_database_new (void)
{
	return libwacom_database_new_for_path (DATADIR);
}

void
libwacom_database_destroy(WacomDeviceDatabase *db)
{
	if (db->device_ht)
		g_hash_table_destroy(db->device_ht);
	if (db->stylus_ht)
		g_hash_table_destroy(db->stylus_ht);
	g_free (db);
}

WacomDevice**
libwacom_list_devices_from_database(const WacomDeviceDatabase *db, WacomError *error)
{
	GList *cur, *devices;
	WacomDevice **list, **p;

	if (!db) {
		libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
		return NULL;
	}

	devices =  g_hash_table_get_values (db->device_ht);
	list = calloc (g_list_length (devices) + 1, sizeof (WacomDevice *));
	if (!list) {
		libwacom_error_set(error, WERROR_BAD_ALLOC, "Memory allocation failed");
		return NULL;
	}

	for (p = list, cur = devices; cur; cur = g_list_next (cur))
		*p++ = (WacomDevice *) cur->data;
	g_list_free (devices);

	return list;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
