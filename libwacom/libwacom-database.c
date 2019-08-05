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
#include <linux/input-event-codes.h>

#include <assert.h>
#include <glib.h>
#include <dirent.h>
#include <string.h>
#include <stdbool.h>
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

	if (streq(class, "Intuos3"))
		return WCLASS_INTUOS3;
	if (streq(class, "Intuos4"))
		return WCLASS_INTUOS4;
	if (streq(class, "Intuos5"))
		return WCLASS_INTUOS5;
	if (streq(class, "Cintiq"))
		return WCLASS_CINTIQ;
	if (streq(class, "Bamboo"))
		return WCLASS_BAMBOO;
	if (streq(class, "Graphire"))
		return WCLASS_GRAPHIRE;
	if (streq(class, "Intuos"))
		return WCLASS_INTUOS;
	if (streq(class, "Intuos2"))
		return WCLASS_INTUOS2;
	if (streq(class, "ISDV4"))
		return WCLASS_ISDV4;
	if (streq(class, "PenDisplay"))
		return WCLASS_PEN_DISPLAYS;
	if (streq(class, "Remote"))
		return WCLASS_REMOTE;

	return WCLASS_UNKNOWN;
}

static WacomStylusType
type_from_str (const char *type)
{
	if (type == NULL)
		return WSTYLUS_UNKNOWN;
	if (streq(type, "General"))
		return WSTYLUS_GENERAL;
	if (streq(type, "Inking"))
		return WSTYLUS_INKING;
	if (streq(type, "Airbrush"))
		return WSTYLUS_AIRBRUSH;
	if (streq(type, "Classic"))
		return WSTYLUS_CLASSIC;
	if (streq(type, "Marker"))
		return WSTYLUS_MARKER;
	if (streq(type, "Stroke"))
		return WSTYLUS_STROKE;
	if (streq(type, "Puck"))
		return WSTYLUS_PUCK;
	if (streq(type, "3D"))
		return WSTYLUS_3D;
	return WSTYLUS_UNKNOWN;
}

WacomBusType
bus_from_str (const char *str)
{
	if (streq(str, "usb"))
		return WBUSTYPE_USB;
	if (streq(str, "serial"))
		return WBUSTYPE_SERIAL;
	if (streq(str, "bluetooth"))
		return WBUSTYPE_BLUETOOTH;
	if (streq(str, "i2c"))
		return WBUSTYPE_I2C;
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
	case WBUSTYPE_I2C:
		return "i2c";
	}
	g_assert_not_reached ();
}

char *
make_match_string (const char *name, WacomBusType bus, int vendor_id, int product_id)
{
	return g_strdup_printf("%s:%04x:%04x%s%s",
				bus_to_str (bus),
				vendor_id, product_id,
				name ? ":" : "",
				name ? name : "");
}

static gboolean
match_from_string(const char *str, WacomBusType *bus, int *vendor_id, int *product_id, char **name)
{
	int rc = 1, len = 0;
	char busstr[64];

	rc = sscanf(str, "%63[^:]:%x:%x:%n", busstr, vendor_id, product_id, &len);
	if (len > 0) {
		/* Grumble grumble scanf handling of %n */
		*name = g_strdup(str+len);
	} else if (rc == 3) {
		*name = NULL;
	} else {
		return FALSE;
	}
	*bus = bus_from_str (busstr);

	return TRUE;
}

static gboolean
libwacom_matchstr_to_match(WacomDevice *device, const char *matchstr)
{
	char *name = NULL;
	int vendor_id, product_id;
	WacomBusType bus;
	WacomMatch *match;

	if (matchstr == NULL)
		return FALSE;

	if (streq(matchstr, GENERIC_DEVICE_MATCH)) {
		name = NULL;
		bus = WBUSTYPE_UNKNOWN;
		vendor_id = 0;
		product_id = 0;
	} else if (!match_from_string(matchstr, &bus, &vendor_id, &product_id, &name)) {
		DBG("failed to match '%s' for product/vendor IDs. Skipping.\n", matchstr);
		return FALSE;
	}

	match = libwacom_match_new(name, bus, vendor_id, product_id);
	libwacom_add_match(device, match);
	libwacom_match_unref(match);

	free(name);
	return TRUE;
}

static gboolean
libwacom_matchstr_to_paired(WacomDevice *device, const char *matchstr)
{
	char *name = NULL;
	int vendor_id, product_id;
	WacomBusType bus;

	g_return_val_if_fail(device->paired == NULL, FALSE);

	if (!match_from_string(matchstr, &bus, &vendor_id, &product_id, &name)) {
		DBG("failed to match '%s' for product/vendor IDs. Ignoring.\n", matchstr);
		return FALSE;
	}

	device->paired = libwacom_match_new(name, bus, vendor_id, product_id);

	free(name);
	return TRUE;
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
		char **string_list;

		id = strtol (groups[i], NULL, 16);
		if (id == 0) {
			g_warning ("Failed to parse stylus ID '%s'", groups[i]);
			continue;
		}

		stylus = g_new0 (WacomStylus, 1);
		stylus->refcnt = 1;
		stylus->id = id;
		stylus->name = g_key_file_get_string(keyfile, groups[i], "Name", NULL);
		stylus->group = g_key_file_get_string(keyfile, groups[i], "Group", NULL);

		stylus->is_eraser = g_key_file_get_boolean(keyfile, groups[i], "IsEraser", &error);
		if (error && error->code == G_KEY_FILE_ERROR_INVALID_VALUE)
			g_warning ("Stylus %s (%s) %s\n", stylus->name, groups[i], error->message);
		g_clear_error (&error);

		if (stylus->is_eraser == FALSE) {
			stylus->has_eraser = g_key_file_get_boolean(keyfile, groups[i], "HasEraser", &error);
			if (error && error->code == G_KEY_FILE_ERROR_INVALID_VALUE)
				g_warning ("Stylus %s (%s) %s\n", stylus->name, groups[i], error->message);
			g_clear_error (&error);
			stylus->has_lens = g_key_file_get_boolean(keyfile, groups[i], "HasLens", &error);
			if (error && error->code == G_KEY_FILE_ERROR_INVALID_VALUE)
				g_warning ("Stylus %s (%s) %s\n", stylus->name, groups[i], error->message);
			g_clear_error (&error);
			stylus->has_wheel = g_key_file_get_boolean(keyfile, groups[i], "HasWheel", &error);
			if (error && error->code == G_KEY_FILE_ERROR_INVALID_VALUE)
				g_warning ("Stylus %s (%s) %s\n", stylus->name, groups[i], error->message);
			g_clear_error (&error);
		} else {
			stylus->has_eraser = FALSE;
			stylus->has_lens = FALSE;
			stylus->has_wheel = FALSE;
		}

		stylus->num_buttons = g_key_file_get_integer(keyfile, groups[i], "Buttons", &error);
		if (stylus->num_buttons == 0 && error != NULL) {
			stylus->num_buttons = -1;
			g_clear_error (&error);
		}

		string_list = g_key_file_get_string_list (keyfile, groups[i], "Axes", NULL, NULL);
		if (string_list) {
			WacomAxisTypeFlags axes = WACOM_AXIS_TYPE_NONE;
			guint i;

			for (i = 0; string_list[i]; i++) {
				WacomAxisTypeFlags flag = WACOM_AXIS_TYPE_NONE;
				if (streq(string_list[i], "Tilt")) {
					flag = WACOM_AXIS_TYPE_TILT;
				} else if (streq(string_list[i], "RotationZ")) {
					flag = WACOM_AXIS_TYPE_ROTATION_Z;
				} else if (streq(string_list[i], "Distance")) {
					flag = WACOM_AXIS_TYPE_DISTANCE;
				} else if (streq(string_list[i], "Pressure")) {
					flag = WACOM_AXIS_TYPE_PRESSURE;
				} else if (streq(string_list[i], "Slider")) {
					flag = WACOM_AXIS_TYPE_SLIDER;
				} else {
					g_warning ("Invalid axis %s for stylus ID %s\n",
						   string_list[i], groups[i]);
				}
				if (axes & flag)
					g_warning ("Duplicate axis %s for stylus ID %s\n",
						   string_list[i], groups[i]);
				axes |= flag;
			}

			stylus->axes = axes;
			g_strfreev (string_list);
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

static inline bool
set_button_codes_from_string(WacomDevice *device, char **strvals)
{
	gint i;

	assert(strvals);

	for (i = 0; i < device->num_buttons; i++) {
		glong val;

		if (!strvals[i]) {
			g_error ("%s: Missing EvdevCode for button %d, ignoring all codes\n",
				 device->name, i);
			return false;
		}

		val = strtol (strvals[i], NULL, 0);
		if (val < BTN_MISC || val >= BTN_DIGI) {
			g_warning ("%s: Invalid EvdevCode %ld for button %d, ignoring all codes\n",
				   device->name, val, i);
			return false;
		}
		device->button_codes[i] = (int)val;
	}

	return true;
}

static inline void
set_button_codes_from_heuristics(WacomDevice *device)
{
	gint i;
	for (i = 0; i < device->num_buttons; i++) {
		if (device->cls == WCLASS_BAMBOO ||
		    device->cls == WCLASS_GRAPHIRE) {
			switch (i) {
			case 0:
				device->button_codes[i] = BTN_LEFT;
				break;
			case 1:
				device->button_codes[i] = BTN_RIGHT;
				break;
			case 2:
				device->button_codes[i] = BTN_FORWARD;
				break;
			case 3:
				device->button_codes[i] = BTN_BACK;
				break;
			default:
				device->button_codes[i] = 0;
				break;
			}
		} else {
			/* Assume traditional ExpressKey ordering */
			switch (i) {
			case 0 ... 9:
				device->button_codes[i] = BTN_0 + i;
				break;
			case 10 ... 15:
				device->button_codes[i] = BTN_A + (i-10);
				break;
			case 16:
			case 17:
				device->button_codes[i] = BTN_BASE + (i-16);
				break;
			default:
				device->button_codes[i] = 0;
				break;
			}
		}

		if (device->button_codes[i] == 0)
			g_warning ("Unable to determine evdev code for button %d (%s)", i, device->name);
	}
}

static void
libwacom_parse_button_codes(WacomDevice *device,
			    GKeyFile    *keyfile)
{
	char **vals;

	vals = g_key_file_get_string_list(keyfile, BUTTONS_GROUP, "EvdevCodes", NULL, NULL);
	if (!vals ||
	    !set_button_codes_from_string(device, vals))
		set_button_codes_from_heuristics(device);

	g_strfreev (vals);
}

static int
libwacom_parse_num_modes (WacomDevice      *device,
			  GKeyFile         *keyfile,
			  const char       *key,
			  WacomButtonFlags  flag)
{
	int num;
	int i;

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

	libwacom_parse_button_codes(device, keyfile);

	device->ring_num_modes = libwacom_parse_num_modes(device, keyfile, "RingNumModes", WACOM_BUTTON_RING_MODESWITCH);
	device->ring2_num_modes = libwacom_parse_num_modes(device, keyfile, "Ring2NumModes", WACOM_BUTTON_RING2_MODESWITCH);
	device->strips_num_modes = libwacom_parse_num_modes(device, keyfile, "StripsNumModes", WACOM_BUTTON_TOUCHSTRIP_MODESWITCH);
}

static int
styli_id_sort(gconstpointer pa, gconstpointer pb)
{
	const int *a = pa, *b = pb;
	return *a > *b ? 1 : *a == *b ? 0 : -1;
}

static void
libwacom_parse_styli_list(WacomDeviceDatabase *db, WacomDevice *device,
			  char **ids)
{
	GArray *array;
	guint i;

	array = g_array_new (FALSE, FALSE, sizeof(int));
	device->num_styli = 0;
	for (i = 0; ids[i]; i++) {
		const char *id = ids[i];

		if (strneq(id, "0x", 2)) {
			glong long_value = strtol (ids[i], NULL, 0);
			int int_value = long_value;
			g_array_append_val (array, int_value);
			device->num_styli++;
		} else if (strneq(id, "@", 1)) {
			const char *group = &id[1];
			GHashTableIter iter;
			gpointer key, value;

			g_hash_table_iter_init(&iter, db->stylus_ht);
			while (g_hash_table_iter_next (&iter, &key, &value)) {
				WacomStylus *stylus = value;
				if (stylus->group && streq(group, stylus->group)) {
					g_array_append_val (array, stylus->id);
					device->num_styli++;
				}
			}
		} else {
			g_warning ("Invalid prefix for '%s'!", id);
		}
	}
	/* Using groups means we don't get the styli in ascending order.
	   Sort it so the output is predictable */
	g_array_sort(array, styli_id_sort);
	device->supported_styli = (int *) g_array_free (array, FALSE);
}

static WacomDevice*
libwacom_parse_tablet_keyfile(WacomDeviceDatabase *db,
			      const char *datadir,
			      const char *filename)
{
	WacomDevice *device = NULL;
	GKeyFile *keyfile;
	GError *error = NULL;
	gboolean rc;
	char *path;
	char *layout;
	char *class;
	char *paired;
	char **string_list;
	bool success = FALSE;

	keyfile = g_key_file_new();

	path = g_build_filename (datadir, filename, NULL);
	rc = g_key_file_load_from_file(keyfile, path, G_KEY_FILE_NONE, &error);

	if (!rc) {
		DBG("%s: %s\n", path, error->message);
		goto out;
	}

	device = g_new0 (WacomDevice, 1);
	device->refcnt = 1;

	string_list = g_key_file_get_string_list(keyfile, DEVICE_GROUP, "DeviceMatch", NULL, NULL);
	if (!string_list) {
		DBG("Missing DeviceMatch= line in '%s'\n", path);
		goto out;
	} else {
		guint i;
		guint nmatches = 0;
		guint first_valid_match = 0;
		for (i = 0; string_list[i]; i++) {
			if (libwacom_matchstr_to_match (device, string_list[i]))
				nmatches++;
			if (nmatches == 1)
				first_valid_match = i;
		}
		if (nmatches == 0) {
			DBG("failed to match '%s' for product/vendor IDs in '%s'\n", string_list[i], path);
			g_strfreev (string_list);
			goto out;
		}
		if (nmatches > 1) {
			/* set default to first entry */
			libwacom_matchstr_to_match(device, string_list[first_valid_match]);
		}
		g_strfreev (string_list);
	}

	paired = g_key_file_get_string(keyfile, DEVICE_GROUP, "PairedID", NULL);
	if (paired) {
		libwacom_matchstr_to_paired(device, paired);
		g_free(paired);
	}

	device->name = g_key_file_get_string(keyfile, DEVICE_GROUP, "Name", NULL);
	device->model_name = g_key_file_get_string(keyfile, DEVICE_GROUP, "ModelName", NULL);
	/* ModelName= would give us the empty string, let's make it NULL
	 * instead */
	if (device->model_name && strlen(device->model_name) == 0) {
		free(device->model_name);
		device->model_name = NULL;
	}
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
				if (streq(string_list[i], integration_flags[n].key)) {
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
		libwacom_parse_styli_list(db, device, string_list);
		g_strfreev (string_list);
	} else {
		device->supported_styli = g_new (int, 2);
		device->supported_styli[0] = WACOM_ERASER_FALLBACK_ID;
		device->supported_styli[1] = WACOM_STYLUS_FALLBACK_ID;
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

	if (g_key_file_get_boolean(keyfile, FEATURES_GROUP, "TouchSwitch", NULL))
		device->features |= FEATURE_TOUCHSWITCH;

	if (device->integration_flags != WACOM_DEVICE_INTEGRATED_UNSET &&
	    device->integration_flags & WACOM_DEVICE_INTEGRATED_DISPLAY &&
	    device->features & FEATURE_REVERSIBLE)
		g_warning ("Tablet '%s' is both reversible and integrated in screen. This is impossible", libwacom_get_match(device));

	if (!(device->features & FEATURE_RING) &&
	    (device->features & FEATURE_RING2))
		g_warning ("Tablet '%s' has Ring2 but no Ring. This is impossible", libwacom_get_match(device));

	if (!(device->features & FEATURE_TOUCH) &&
	    (device->features & FEATURE_TOUCHSWITCH))
		g_warning ("Tablet '%s' has touch switch but no touch tool. This is impossible", libwacom_get_match(device));

	device->num_strips = g_key_file_get_integer(keyfile, FEATURES_GROUP, "NumStrips", NULL);
	device->num_buttons = g_key_file_get_integer(keyfile, FEATURES_GROUP, "Buttons", &error);
	if (device->num_buttons == 0 &&
	    g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
		g_warning ("Tablet '%s' has no buttons defined, do something!", libwacom_get_match(device));
		g_clear_error (&error);
	}
	if (device->num_buttons > 0) {
		device->buttons = g_new0 (WacomButtonFlags, device->num_buttons);
		device->button_codes = g_new0 (gint, device->num_buttons);
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
				if (streq(string_list[i], supported_leds[n].key)) {
					g_array_append_val (array, supported_leds[n].value);
					device->num_leds++;
					break;
				}
			}
		}
		g_strfreev (string_list);
		device->status_leds = (WacomStatusLEDs *) g_array_free (array, FALSE);
	}

	success = TRUE;

out:
	if (path)
		g_free(path);
	if (keyfile)
		g_key_file_free(keyfile);
	if (error)
		g_error_free(error);
	if (!success)
		device = libwacom_unref(device);

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

	return streq(&name[len - suffix_len], TABLET_SUFFIX);
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

	return streq(&name[len - suffix_len], STYLUS_SUFFIX);
}

static bool
load_tablet_files(WacomDeviceDatabase *db, const char *datadir)
{
    int n, nfiles;
    struct dirent **files;
    bool success = false;

    db->device_ht = g_hash_table_new_full (g_str_hash,
					   g_str_equal,
					   g_free,
					   (GDestroyNotify) libwacom_destroy);

    n = scandir(datadir, &files, scandir_tablet_filter, alphasort);
    if (n <= 0)
	    return false;

    nfiles = n;
    while(n--) {
	    WacomDevice *d;
	    const WacomMatch **matches, **match;

	    d = libwacom_parse_tablet_keyfile(db, datadir, files[n]->d_name);

	    if (!d)
		    continue;

	    matches = libwacom_get_matches(d);
	    if (!matches || !*matches) {
		    g_critical("Device '%s' has no matches defined\n",
			       libwacom_get_name(d));
		    libwacom_destroy(d);
		    goto out;
	    }

	    for (match = matches; *match; match++) {
		    const char *matchstr;
		    matchstr = libwacom_match_get_match_string(*match);
		    /* no duplicate matches allowed */
		    if (g_hash_table_lookup(db->device_ht, matchstr) != NULL) {
			    g_critical("Duplicate match of '%s' on device '%s'.",
					matchstr, libwacom_get_name(d));
			    goto out;
		    }
		    g_hash_table_insert (db->device_ht, g_strdup (matchstr), d);
		    libwacom_ref(d);
	    }
	    libwacom_unref(d);
    }

    if (g_hash_table_size (db->device_ht) == 0)
	    goto out;

    success = true;

out:
    while(nfiles--)
	    free(files[nfiles]);
    free(files);

    return success;
}

static void
stylus_destroy(void *data)
{
	libwacom_stylus_unref((WacomStylus*)data);
}

static bool
load_stylus_files(WacomDeviceDatabase *db, const char *datadir)
{
    int n, nfiles;
    struct dirent **files;
    bool success = false;

    n = scandir(datadir, &files, scandir_stylus_filter, alphasort);
    if (n <= 0)
	    return false;

    db->stylus_ht = g_hash_table_new_full (g_direct_hash,
					   g_direct_equal,
					   NULL,
					   (GDestroyNotify) stylus_destroy);
    nfiles = n;
    while(n--) {
	    char *path;

	    path = g_build_filename (datadir, files[n]->d_name, NULL);
	    libwacom_parse_stylus_keyfile(db, path);
	    g_free(path);
    }

    /* If we couldn't load _anything_ then something's wrong */
    if (g_hash_table_size (db->stylus_ht) == 0)
	    goto end;

    success = true;

end:
    while(nfiles--)
	    free(files[nfiles]);
    free(files);


    return success;
}


LIBWACOM_EXPORT WacomDeviceDatabase *
libwacom_database_new_for_path (const char *datadir)
{
    WacomDeviceDatabase *db;

    db = g_new0 (WacomDeviceDatabase, 1);

    if (!load_stylus_files(db, datadir) || !load_tablet_files(db, datadir)) {
	    libwacom_database_destroy(db);
	    db = NULL;
    }

    return db;
}

LIBWACOM_EXPORT WacomDeviceDatabase *
libwacom_database_new (void)
{
	return libwacom_database_new_for_path (DATADIR);
}

LIBWACOM_EXPORT void
libwacom_database_destroy(WacomDeviceDatabase *db)
{
	if (db->device_ht)
		g_hash_table_destroy(db->device_ht);
	if (db->stylus_ht)
		g_hash_table_destroy(db->stylus_ht);
	g_free (db);
}

static gint
device_compare(gconstpointer pa, gconstpointer pb)
{
	const WacomDevice *a = pa,
		          *b = pb;
	int cmp;

	cmp = libwacom_get_vendor_id(a) - libwacom_get_vendor_id(b);
	if (cmp == 0)
		cmp = libwacom_get_product_id(a) - libwacom_get_product_id(b);
	return cmp;
}

LIBWACOM_EXPORT WacomDevice**
libwacom_list_devices_from_database(const WacomDeviceDatabase *db, WacomError *error)
{
	GList *cur, *devices;
	WacomDevice **list, **p;

	if (!db) {
		libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
		return NULL;
	}

	devices = g_hash_table_get_values (db->device_ht);
	list = calloc (g_list_length (devices) + 1, sizeof (WacomDevice *));
	if (!list) {
		libwacom_error_set(error, WERROR_BAD_ALLOC, "Memory allocation failed");
		g_list_free (devices);
		return NULL;
	}

	devices = g_list_sort (devices, device_compare);

	for (p = list, cur = devices; cur; cur = g_list_next (cur))
		*p++ = (WacomDevice *) cur->data;
	g_list_free (devices);

	return list;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
