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

#include "config.h"

#define _GNU_SOURCE 1
#include "libwacomint.h"
#include "util-strings.h"
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

	if (g_str_equal(class, "Intuos3"))
		return WCLASS_INTUOS3;
	if (g_str_equal(class, "Intuos4"))
		return WCLASS_INTUOS4;
	if (g_str_equal(class, "Intuos5"))
		return WCLASS_INTUOS5;
	if (g_str_equal(class, "Cintiq"))
		return WCLASS_CINTIQ;
	if (g_str_equal(class, "Bamboo"))
		return WCLASS_BAMBOO;
	if (g_str_equal(class, "Graphire"))
		return WCLASS_GRAPHIRE;
	if (g_str_equal(class, "Intuos"))
		return WCLASS_INTUOS;
	if (g_str_equal(class, "Intuos2"))
		return WCLASS_INTUOS2;
	if (g_str_equal(class, "ISDV4"))
		return WCLASS_ISDV4;
	if (g_str_equal(class, "PenDisplay"))
		return WCLASS_PEN_DISPLAYS;
	if (g_str_equal(class, "Remote"))
		return WCLASS_REMOTE;

	return WCLASS_UNKNOWN;
}

static WacomStylusType
type_from_str (const char *type)
{
	if (type == NULL)
		return WSTYLUS_UNKNOWN;
	if (g_str_equal(type, "General"))
		return WSTYLUS_GENERAL;
	if (g_str_equal(type, "Inking"))
		return WSTYLUS_INKING;
	if (g_str_equal(type, "Airbrush"))
		return WSTYLUS_AIRBRUSH;
	if (g_str_equal(type, "Classic"))
		return WSTYLUS_CLASSIC;
	if (g_str_equal(type, "Marker"))
		return WSTYLUS_MARKER;
	if (g_str_equal(type, "Stroke"))
		return WSTYLUS_STROKE;
	if (g_str_equal(type, "Puck"))
		return WSTYLUS_PUCK;
	if (g_str_equal(type, "3D"))
		return WSTYLUS_3D;
	if (g_str_equal(type, "Mobile"))
		return WSTYLUS_MOBILE;
	return WSTYLUS_UNKNOWN;
}

static WacomEraserType
eraser_type_from_str (const char *type)
{
	if (type == NULL)
		return WACOM_ERASER_NONE;
	if (g_str_equal(type, "None"))
		return WACOM_ERASER_NONE;
	if (g_str_equal(type, "Invert"))
		return WACOM_ERASER_INVERT;
	if (g_str_equal(type, "Button"))
		return WACOM_ERASER_BUTTON;
	return WACOM_ERASER_UNKNOWN;
}

WacomBusType
bus_from_str (const char *str)
{
	if (g_str_equal(str, "usb"))
		return WBUSTYPE_USB;
	if (g_str_equal(str, "serial"))
		return WBUSTYPE_SERIAL;
	if (g_str_equal(str, "bluetooth"))
		return WBUSTYPE_BLUETOOTH;
	if (g_str_equal(str, "i2c"))
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

static WacomMatch *
libwacom_match_from_string(WacomDevice *device, const char *matchstr)
{
	char *name = NULL;
	int vendor_id, product_id;
	WacomBusType bus;
	WacomMatch *match;

	if (matchstr == NULL)
		return NULL;

	if (g_str_equal(matchstr, GENERIC_DEVICE_MATCH)) {
		name = NULL;
		bus = WBUSTYPE_UNKNOWN;
		vendor_id = 0;
		product_id = 0;
	} else if (!match_from_string(matchstr, &bus, &vendor_id, &product_id, &name)) {
		DBG("failed to match '%s' for product/vendor IDs. Skipping.\n", matchstr);
		return NULL;
	}

	match = libwacom_match_new(name, bus, vendor_id, product_id);
	free(name);

	return match;
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

		if (!safe_atoi_base (groups[i], &id, 16)) {
			g_warning ("Failed to parse stylus ID '%s'", groups[i]);
			continue;
		}

		stylus = g_new0 (WacomStylus, 1);
		stylus->refcnt = 1;
		stylus->id = id;
		stylus->name = g_key_file_get_string(keyfile, groups[i], "Name", NULL);
		stylus->group = g_key_file_get_string(keyfile, groups[i], "Group", NULL);

		type = g_key_file_get_string(keyfile, groups[i], "EraserType", NULL);
		stylus->eraser_type = eraser_type_from_str (type);
		g_free (type);

		string_list = g_key_file_get_string_list (keyfile, groups[i], "PairedStylusIds", NULL, NULL);
		stylus->paired_ids = g_array_new (FALSE, FALSE, sizeof(int));
		if (string_list) {
			guint j;

			for (j = 0; string_list[j]; j++) {
				int val;

				if (safe_atoi_base (string_list[j], &val, 16)) {
					g_array_append_val (stylus->paired_ids, val);
				} else {
					g_warning ("Stylus %s (%s) Ignoring invalid PairedId value\n", stylus->name, groups[i]);
				}
			}

			g_strfreev (string_list);
		}

		stylus->has_lens = g_key_file_get_boolean(keyfile, groups[i], "HasLens", &error);
		if (error && error->code == G_KEY_FILE_ERROR_INVALID_VALUE)
			g_warning ("Stylus %s (%s) %s\n", stylus->name, groups[i], error->message);
		g_clear_error (&error);
		stylus->has_wheel = g_key_file_get_boolean(keyfile, groups[i], "HasWheel", &error);
		if (error && error->code == G_KEY_FILE_ERROR_INVALID_VALUE)
			g_warning ("Stylus %s (%s) %s\n", stylus->name, groups[i], error->message);
		g_clear_error (&error);

		stylus->num_buttons = g_key_file_get_integer(keyfile, groups[i], "Buttons", &error);
		if (stylus->num_buttons == 0 && error != NULL) {
			stylus->num_buttons = -1;
			g_clear_error (&error);
		}

		string_list = g_key_file_get_string_list (keyfile, groups[i], "Axes", NULL, NULL);
		if (string_list) {
			WacomAxisTypeFlags axes = WACOM_AXIS_TYPE_NONE;
			guint j;

			for (j = 0; string_list[j]; j++) {
				WacomAxisTypeFlags flag = WACOM_AXIS_TYPE_NONE;
				if (g_str_equal(string_list[j], "Tilt")) {
					flag = WACOM_AXIS_TYPE_TILT;
				} else if (g_str_equal(string_list[j], "RotationZ")) {
					flag = WACOM_AXIS_TYPE_ROTATION_Z;
				} else if (g_str_equal(string_list[j], "Distance")) {
					flag = WACOM_AXIS_TYPE_DISTANCE;
				} else if (g_str_equal(string_list[j], "Pressure")) {
					flag = WACOM_AXIS_TYPE_PRESSURE;
				} else if (g_str_equal(string_list[j], "Slider")) {
					flag = WACOM_AXIS_TYPE_SLIDER;
				} else {
					g_warning ("Invalid axis %s for stylus ID %s\n",
						   string_list[j], groups[i]);
				}
				if (axes & flag)
					g_warning ("Duplicate axis %s for stylus ID %s\n",
						   string_list[j], groups[i]);
				axes |= flag;
			}

			stylus->axes = axes;
			g_strfreev (string_list);
		}

		type = g_key_file_get_string(keyfile, groups[i], "Type", NULL);
		stylus->type = type_from_str (type);
		g_free (type);

		if (g_hash_table_lookup (db->stylus_ht, GINT_TO_POINTER (id)) != NULL)
			g_warning ("Duplicate definition for stylus ID '%#x'", id);

		g_hash_table_insert (db->stylus_ht, GINT_TO_POINTER (id), stylus);
	}
	g_strfreev (groups);
	if (keyfile)
		g_key_file_free (keyfile);
}

static void
libwacom_setup_paired_attributes(WacomDeviceDatabase *db)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init(&iter, db->stylus_ht);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		WacomStylus *stylus = value;
		const int* ids;
		int count;
		int i;

		ids = libwacom_stylus_get_paired_ids(stylus, &count);
		for (i = 0; i < count; i++) {
			const WacomStylus *pair;

			pair = libwacom_stylus_get_for_id(db, ids[i]);
			if (pair == NULL) {
				g_warning("Paired stylus '0x%x' not found, ignoring.", ids[i]);
				continue;
			}
			if (libwacom_stylus_is_eraser(pair)) {
				stylus->has_eraser = true;
			}
		}
	}
}

static const struct {
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

static const struct {
	const char       *key;
	WacomStatusLEDs   value;
} supported_leds[] = {
	{ "Ring",		WACOM_STATUS_LED_RING },
	{ "Ring2",		WACOM_STATUS_LED_RING2 },
	{ "Touchstrip",		WACOM_STATUS_LED_TOUCHSTRIP },
	{ "Touchstrip2",	WACOM_STATUS_LED_TOUCHSTRIP2 }
};

static const struct {
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
		WacomButton *button;

		val = *vals[i];
		if (strlen (vals[i]) > 1 ||
		    val < 'A' ||
		    val > 'Z') {
			g_warning ("Ignoring value '%s' in key '%s'", vals[i], key);
			continue;
		}

		button = g_hash_table_lookup(device->buttons, GINT_TO_POINTER(val));
		if (!button) {
			button = g_new0(WacomButton, 1);
			g_hash_table_insert(device->buttons, GINT_TO_POINTER(val), button);
		}

		button->flags |= flag;
	}

	g_strfreev (vals);
}

static void
reset_code(gpointer key, gpointer value, gpointer user_data)
{
	WacomButton *button = value;
	button->code = 0;
}

static inline bool
set_button_codes_from_string(WacomDevice *device, char **strvals)
{
	bool success = false;

	assert(strvals);

	for (guint i = 0; i < g_hash_table_size(device->buttons); i++) {
		char key = 'A' + i;
		int code;
		WacomButton *button = g_hash_table_lookup(device->buttons, GINT_TO_POINTER(key));

		if (!button) {
			g_error("%s: Button %c is not defined, ignoring all codes\n",
				device->name, key);
			goto out;
		}

		if (!strvals[i]) {
			g_error ("%s: Missing EvdevCode for button %d, ignoring all codes\n",
				 device->name, i);
			goto out;
		}

		if (!safe_atoi_base (strvals[i], &code, 16) || code < BTN_MISC || code >= BTN_DIGI) {
			g_warning ("%s: Invalid EvdevCode %s for button %c, ignoring all codes\n",
				   device->name, strvals[i], key);
			goto out;
		}
		button->code = code;
	}
	success = true;

out:
	if (!success)
		g_hash_table_foreach(device->buttons, reset_code, NULL);

	return success;
}

static inline void
set_button_codes_from_heuristics(WacomDevice *device)
{
	for (char key = 'A'; key <= 'Z'; key++) {
		int code = 0;
		WacomButton *button;

		button = g_hash_table_lookup(device->buttons, GINT_TO_POINTER(key));
		if (!button)
			continue;

		if (device->cls == WCLASS_BAMBOO ||
		    device->cls == WCLASS_GRAPHIRE) {
			switch (key) {
			case 'A': code = BTN_LEFT; break;
			case 'B': code = BTN_RIGHT; break;
			case 'C': code = BTN_FORWARD; break;
			case 'D': code = BTN_BACK; break;
			default:
				break;
			}
		} else {
			/* Assume traditional ExpressKey ordering */
			switch (key) {
			case 'A': code = BTN_0; break;
			case 'B': code = BTN_1; break;
			case 'C': code = BTN_2; break;
			case 'D': code = BTN_3; break;
			case 'E': code = BTN_4; break;
			case 'F': code = BTN_5; break;
			case 'G': code = BTN_6; break;
			case 'H': code = BTN_7; break;
			case 'I': code = BTN_8; break;
			case 'J': code = BTN_9; break;
			case 'K': code = BTN_A; break;
			case 'L': code = BTN_B; break;
			case 'M': code = BTN_C; break;
			case 'N': code = BTN_X; break;
			case 'O': code = BTN_Y; break;
			case 'P': code = BTN_Z; break;
			case 'Q': code = BTN_BASE; break;
			case 'R': code = BTN_BASE2; break;
			default:
				break;
			}
		}

		if (code == 0)
			g_warning ("Unable to determine evdev code for button %c (%s)",
				   key, device->name);

		button->code = code;
	}
}

static void
libwacom_parse_button_codes(WacomDevice *device,
			    GKeyFile    *keyfile)
{
	char **vals;

	vals = g_key_file_get_string_list(keyfile, BUTTONS_GROUP, "EvdevCodes", NULL, NULL);
	if (!vals || !set_button_codes_from_string(device, vals))
		set_button_codes_from_heuristics(device);

	g_strfreev (vals);
}

static int
libwacom_parse_num_modes (WacomDevice      *device,
			  GKeyFile         *keyfile,
			  const char       *key,
			  WacomButtonFlags  flag)
{
	GHashTableIter iter;
	int num;
	gpointer k, v;

	num = g_key_file_get_integer (keyfile, BUTTONS_GROUP, key, NULL);
	if (num > 0)
		return num;

	g_hash_table_iter_init(&iter, device->buttons);
	while (g_hash_table_iter_next(&iter, &k, &v)) {
		WacomButton *button = v;
		if (button->flags & flag)
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
	for (i = 0; ids[i]; i++) {
		const char *id = ids[i];

		if (g_str_has_prefix(id, "0x")) {
			int int_value;
			if (safe_atoi_base (ids[i], &int_value, 16)) {
				g_array_append_val (array, int_value);
			}
		} else if (g_str_has_prefix(id, "@")) {
			const char *group = &id[1];
			GHashTableIter iter;
			gpointer key, value;

			g_hash_table_iter_init(&iter, db->stylus_ht);
			while (g_hash_table_iter_next (&iter, &key, &value)) {
				WacomStylus *stylus = value;
				if (stylus->group && g_str_equal(group, stylus->group)) {
					g_array_append_val (array, stylus->id);
				}
			}
		} else {
			g_warning ("Invalid prefix for '%s'!", id);
		}
	}
	/* Using groups means we don't get the styli in ascending order.
	   Sort it so the output is predictable */
	g_array_sort(array, styli_id_sort);
	device->styli = array;
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
	int num_buttons;

	keyfile = g_key_file_new();

	path = g_build_filename (datadir, filename, NULL);
	rc = g_key_file_load_from_file(keyfile, path, G_KEY_FILE_NONE, &error);

	if (!rc) {
		DBG("%s: %s\n", path, error->message);
		goto out;
	}

	device = g_new0 (WacomDevice, 1);
	device->refcnt = 1;
	device->matches = g_array_new(TRUE, TRUE, sizeof(WacomMatch*));

	string_list = g_key_file_get_string_list(keyfile, DEVICE_GROUP, "DeviceMatch", NULL, NULL);
	if (!string_list) {
		DBG("Missing DeviceMatch= line in '%s'\n", path);
		goto out;
	} else {
		guint i;
		guint nmatches = 0;
		for (i = 0; string_list[i]; i++) {
			WacomMatch *m = libwacom_match_from_string(device,
								   string_list[i]);
			if (!m) {
				DBG("'%s' is an invalid DeviceMatch in '%s'\n",
				    string_list[i], path);
				continue;
			}
			libwacom_add_match(device, m);
			nmatches++;
			/* set default to first entry */
			if (nmatches == 1)
				libwacom_set_default_match(device, m);
			libwacom_match_unref(m);
		}
		g_strfreev (string_list);
		if (nmatches == 0)
			goto out;
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
				if (g_str_equal(string_list[i], integration_flags[n].key)) {
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
		int fallback_eraser = WACOM_ERASER_FALLBACK_ID;
		int fallback_stylus = WACOM_STYLUS_FALLBACK_ID;
		device->styli = g_array_new(FALSE, FALSE, sizeof(int));
		g_array_append_val(device->styli, fallback_eraser);
		g_array_append_val(device->styli, fallback_stylus);
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

	num_buttons = g_key_file_get_integer(keyfile, FEATURES_GROUP, "Buttons", &error);
	if (num_buttons == 0 &&
	    g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
		g_warning ("Tablet '%s' has no buttons defined, do something!", libwacom_get_match(device));
		g_clear_error (&error);
	}

	device->buttons = g_hash_table_new_full(g_direct_hash, g_direct_equal,
						NULL, g_free);
	if (num_buttons > 0)
		libwacom_parse_buttons(device, keyfile);

	device->status_leds = g_array_new (FALSE, FALSE, sizeof(WacomStatusLEDs));
	string_list = g_key_file_get_string_list(keyfile, FEATURES_GROUP, "StatusLEDs", NULL, NULL);
	if (string_list) {
		guint i, n;

		for (i = 0; string_list[i]; i++) {
			for (n = 0; n < G_N_ELEMENTS (supported_leds); n++) {
				if (g_str_equal(string_list[i], supported_leds[n].key)) {
					g_array_append_val (device->status_leds, supported_leds[n].value);
					break;
				}
			}
		}
		g_strfreev (string_list);
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

static bool
has_suffix(const char *name, const char *suffix)
{
	size_t len = strlen(name);
	size_t suffix_len = strlen(suffix);

	if (!name || name[0] == '.')
		return 0;

	if (len <= suffix_len)
		return false;

	return g_str_equal(&name[len - suffix_len], suffix);
}

static int
is_tablet_file(const struct dirent *entry)
{
	return has_suffix(entry->d_name, TABLET_SUFFIX);
}

static int
is_stylus_file(const struct dirent *entry)
{
	return has_suffix(entry->d_name, STYLUS_SUFFIX);
}

static bool
load_tablet_files(WacomDeviceDatabase *db, const char *datadir)
{
	DIR *dir;
	struct dirent *file;
	bool success = false;
	GHashTable *keyset = NULL;

	dir = opendir(datadir);
	if (!dir)
		return errno == ENOENT; /* non-existing directory is ok */

	/* A set of all matches for duplicate detection. We allow duplicates
	 * across data directories, but we don't allow for duplicates
	 * within the same data directory.
	 */
	keyset = g_hash_table_new_full (g_str_hash, g_str_equal, free, NULL);
	if (!keyset)
		goto out;

	while ((file = readdir(dir))) {
		WacomDevice *d;
		guint idx = 0;

		if (!is_tablet_file(file))
			continue;

		d = libwacom_parse_tablet_keyfile(db, datadir, file->d_name);
		if (!d)
			continue;

		if (d->matches->len == 0) {
			g_critical("Device '%s' has no matches defined\n",
				   libwacom_get_name(d));
			goto out;
		}

		/* Note: we may change the array while iterating over it */
		while (idx < d->matches->len) {
			WacomMatch *match = g_array_index(d->matches, WacomMatch*, idx);
			const char *matchstr;

			matchstr = libwacom_match_get_match_string(match);
			/* no duplicate matches allowed within the same
			 * directory */
			if (g_hash_table_contains(keyset, matchstr)) {
				g_critical("Duplicate match of '%s' on device '%s'.",
					   matchstr, libwacom_get_name(d));
				goto out;
			}
			g_hash_table_add(keyset, g_strdup(matchstr));

			/* We already have an entry for this match in the database,
			 * that takes precedence. Remove the current match
			 * from the new tablet - that's fine because we
			 * haven't exposed the tablet yet.
			 */
			if (g_hash_table_lookup(db->device_ht, matchstr)) {
				libwacom_remove_match(d, match);
				continue;
			}

			g_hash_table_insert(db->device_ht,
					    g_strdup (matchstr),
					    d);
			libwacom_ref(d);
			idx++;
		}
		libwacom_unref(d);
	}

	success = true;

out:
	if (keyset)
		g_hash_table_destroy(keyset);
	closedir(dir);
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
	DIR *dir;
	struct dirent *file;

	dir = opendir(datadir);
	if (!dir)
		return errno == ENOENT; /* non-existing directory is ok */

	while ((file = readdir(dir))) {
		char *path;

		if (!is_stylus_file(file))
			continue;

		path = g_build_filename (datadir, file->d_name, NULL);
		libwacom_parse_stylus_keyfile(db, path);
		g_free(path);
	}

	closedir(dir);

	return true;
}

static WacomDeviceDatabase *
database_new_for_paths (size_t npaths, const char **datadirs)
{
	WacomDeviceDatabase *db;
	size_t n;
	const char **datadir;

	db = g_new0 (WacomDeviceDatabase, 1);
	db->device_ht = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       g_free,
					       (GDestroyNotify) libwacom_destroy);
	db->stylus_ht = g_hash_table_new_full (g_direct_hash,
					       g_direct_equal,
					       NULL,
					       (GDestroyNotify) stylus_destroy);

	for (datadir = datadirs, n = npaths; n--; datadir++) {
		if (!load_stylus_files(db, *datadir))
			goto error;
	}

	for (datadir = datadirs, n = npaths; n--; datadir++) {
		if (!load_tablet_files(db, *datadir))
			goto error;
	}

	/* If we couldn't load _anything_ then something's wrong */
	if (g_hash_table_size (db->stylus_ht) == 0 ||
	    g_hash_table_size (db->device_ht) == 0)
		goto error;

	libwacom_setup_paired_attributes(db);

	return db;

error:
	libwacom_database_destroy(db);
	return NULL;
}

LIBWACOM_EXPORT WacomDeviceDatabase *
libwacom_database_new_for_path (const char *datadir)
{
	return database_new_for_paths(1, &datadir);
}

LIBWACOM_EXPORT WacomDeviceDatabase *
libwacom_database_new (void)
{
	const char *datadir[] = {
		ETCDIR,
		DATADIR,
	};

	return database_new_for_paths (2, datadir);
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
	if (cmp == 0)
		cmp = g_strcmp0(libwacom_get_name(a), libwacom_get_name(b));
	return cmp;
}

static void
ht_copy_key(gpointer key, gpointer value, gpointer user_data)
{
	g_hash_table_add((GHashTable*)user_data, value);
}

LIBWACOM_EXPORT WacomDevice**
libwacom_list_devices_from_database(const WacomDeviceDatabase *db, WacomError *error)
{
	GList *cur, *devices = NULL;
	WacomDevice **list, **p;
	GHashTable *ht = NULL;

	if (!db) {
		libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
		return NULL;
	}

	/* Devices may be present more than one in the device_ht, so let's
	 * use a temporary hashtable like a set to filter duplicates */
	ht = g_hash_table_new (g_direct_hash, g_direct_equal);
	if (!ht)
		goto error;
	g_hash_table_foreach (db->device_ht, ht_copy_key, ht);

	devices = g_hash_table_get_keys (ht);
	list = calloc (g_list_length (devices) + 1, sizeof (WacomDevice *));
	if (!list)
		goto error;

	devices = g_list_sort (devices, device_compare);
	for (p = list, cur = devices; cur; cur = g_list_next (cur))
		*p++ = (WacomDevice *) cur->data;
	g_list_free (devices);
	g_hash_table_destroy (ht);

	return list;

error:
	libwacom_error_set(error, WERROR_BAD_ALLOC, "Memory allocation failed");
	if (ht)
		g_hash_table_destroy (ht);
	if (devices)
		g_list_free (devices);
	return NULL;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
