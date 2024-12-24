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
#include <libevdev/libevdev.h>

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
#define KEYS_GROUP "Keys"

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

static const char *
str_from_type (WacomStylusType type)
{
	switch (type) {
	case WSTYLUS_UNKNOWN:
		return NULL;
	case WSTYLUS_GENERAL:
		return "General";
	case WSTYLUS_INKING:
		return "Inking";
	case WSTYLUS_AIRBRUSH:
		return "Airbrush";
	case WSTYLUS_CLASSIC:
		return "Classic";
	case WSTYLUS_MARKER:
		return "Marker";
	case WSTYLUS_STROKE:
		return "Stroke";
	case WSTYLUS_PUCK:
		return "Pick";
	case WSTYLUS_3D:
		return "3D";
	case WSTYLUS_MOBILE:
		return "Mobile";
	}
	return NULL;
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

static const char *
eraser_str_from_type (WacomEraserType type)
{
	switch (type) {
	case WACOM_ERASER_NONE:
		return "None";
	case WACOM_ERASER_INVERT:
		return "Invert";
	case WACOM_ERASER_BUTTON:
		return "Button";
	case WACOM_ERASER_UNKNOWN:
		return NULL;
	}
	return NULL;
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
make_match_string (const char *name, const char *uniq, WacomBusType bus, int vendor_id, int product_id)
{
	return g_strdup_printf("%s|%04x|%04x%s%s%s%s",
				bus_to_str (bus),
				vendor_id, product_id,
				(name || uniq) ? "|" : "",
				name ? name : "",
			        uniq ? "|" : "",
			        uniq ? uniq : "");
}

static gboolean
match_from_string(const char *str_in, WacomBusType *bus, int *vendor_id, int *product_id, char **name, char **uniq)
{
	gboolean rc = FALSE;
	guint64 num;
	char *str = g_strdup(str_in);
	char **components = NULL;

	if (g_str_has_suffix(str_in, ";"))
		str[strlen(str) - 1] = '\0';

	components = g_strsplit(str, "|", 16);
	if (!components[0] || !components[1] || !components[2])
		goto out;

	*bus = bus_from_str (components[0]);
	if (!g_ascii_string_to_unsigned(components[1], 16, 0, 0xffff, &num, NULL))
		goto out;
	*vendor_id = (int)num;

	if (!g_ascii_string_to_unsigned(components[2], 16, 0, 0xffff, &num, NULL))
		goto out;
	*product_id = (int)num;

	if (components[3]) {
		*name = g_strdup(components[3]);
		if (components[4])
			*uniq = g_strdup(components[4]);
	}

	rc = TRUE;
out:
	free(str);
	g_strfreev(components);
	return rc;
}

static WacomMatch *
libwacom_match_from_string(const char *matchstr)
{
	char *name = NULL;
	char *uniq = NULL;
	int vendor_id, product_id;
	WacomBusType bus;
	WacomMatch *match;

	if (matchstr == NULL)
		return NULL;

	if (g_str_equal(matchstr, GENERIC_DEVICE_MATCH)) {
		name = NULL;
		uniq = NULL;
		bus = WBUSTYPE_UNKNOWN;
		vendor_id = 0;
		product_id = 0;
	} else if (!match_from_string(matchstr, &bus, &vendor_id, &product_id, &name, &uniq)) {
		DBG("failed to match '%s' for product/vendor IDs. Skipping.\n", matchstr);
		return NULL;
	}

	match = libwacom_match_new(name, uniq, bus, vendor_id, product_id);
	free(name);
	free(uniq);

	return match;
}

static gboolean
libwacom_matchstr_to_paired(WacomDevice *device, const char *matchstr)
{
	char *name = NULL;
	char *uniq = NULL;
	int vendor_id, product_id;
	WacomBusType bus;

	g_return_val_if_fail(device->paired == NULL, FALSE);

	if (!match_from_string(matchstr, &bus, &vendor_id, &product_id, &name, &uniq)) {
		DBG("failed to match '%s' for product/vendor IDs. Ignoring.\n", matchstr);
		return FALSE;
	}

	device->paired = libwacom_match_new(name, uniq, bus, vendor_id, product_id);

	free(name);
	free(uniq);
	return TRUE;
}

static bool
parse_stylus_id(const char *str, WacomStylusId *id)
{
	char **tokens = g_strsplit(str, ":", 2);
	const char *vidstr, *tidstr;
	int vid, tool_id;
	bool ret = false;

	if (tokens[1] == NULL) {
		vidstr = G_STRINGIFY(WACOM_VENDOR_ID);
		tidstr = tokens[0];
	} else {
		vidstr = tokens[0];
		tidstr = tokens[1];
	}

	if (safe_atoi_base (vidstr, &vid, 16) && safe_atoi_base (tidstr, &tool_id, 16)) {
		id->vid = vid;
		id->tool_id = tool_id;
		ret = true;
	}
	g_clear_pointer(&tokens, g_strfreev);

	return ret;
}

static char *
string_or_fallback (GKeyFile *keyfile, const char *group,
		    const char *key, const char *fallback) {
	g_autoptr(GError) local_error = NULL;
	if (g_key_file_has_key(keyfile, group, key, &local_error))
		return g_key_file_get_string(keyfile, group, key, NULL);

	return g_strdup(fallback);
}

static int
int_or_fallback (GKeyFile *keyfile, const char *group,
		const char *key, int fallback) {
	g_autoptr(GError) local_error = NULL;
	if (g_key_file_has_key(keyfile, group, key, &local_error))
		return g_key_file_get_integer(keyfile, group, key, NULL);

	return fallback;
}

static bool
boolean_or_fallback (GKeyFile *keyfile, const char *group,
		 const char *key, bool fallback, WacomStylus *stylus) {
	g_autoptr(GError) error = NULL;
	if (g_key_file_has_key(keyfile, group, key, &error)) {
		return g_key_file_get_boolean(keyfile, group, key, NULL);
	}
	if (error && error->code == G_KEY_FILE_ERROR_INVALID_VALUE)
		g_warning ("Stylus %s (%s) %s\n", stylus->name, group, error->message);

	return fallback;
}

gchar **
garray_to_string_list(GArray *array)
{
	gchar **values;
	WacomStylusId *id;
	char buffer[33];
	bool ret;

	if (array == NULL)
		return NULL;

	values = g_new (gchar *, array->len + 1);
	for (guint i = 0; i < array->len; i++) {
		id = &g_array_index(array, WacomStylusId, i);
		ret = safe_itoa_base(id->tool_id, buffer, sizeof(buffer), 16);
		values[i] = strdup(buffer);
	}
	values[array->len] = NULL;

	return values;
}

static bool
create_stylus_id_from_aliased(const char* str, const char *alias_value, WacomStylusId *id)
{
	char **tokens = g_strsplit(str, ":", 2);
	const char *vidstr;
	int vid;
	bool ret = false;
	int aliased_int;

        if (tokens[1] == NULL) {
                vidstr = G_STRINGIFY(WACOM_VENDOR_ID);
        } else {
                vidstr = tokens[0];
        }

        if (safe_atoi_base (vidstr, &vid, 16) && safe_atoi_base (alias_value, &aliased_int, 16)) {
                id->vid = vid;
                id->tool_id = aliased_int;
                ret = true;
        }
        g_clear_pointer(&tokens, g_strfreev);

        return ret;
}

static void
libwacom_parse_stylus_keyfile(WacomDeviceDatabase *db, const char *path, AliasStatus alias)
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
		WacomStylus *stylus, *other;
		WacomStylusId id, lookup;
		GError *error = NULL;
		char *eraser_type, *type;
		char **string_list;
		char *alias_value;

		if (!parse_stylus_id(groups[i], &id)) {
			g_warning ("Failed to parse stylus ID '%s', ignoring entry", groups[i]);
			continue;
		}

		stylus = g_new0 (WacomStylus, 1);
		stylus->refcnt = 1;
		stylus->id = id;

		if (alias == IGNORE_ALIASES && g_key_file_get_string(keyfile, groups[i], "AliasOf", NULL) != NULL) {
			continue;
		}

		if (alias == ONLY_ALIASES && g_key_file_get_string(keyfile, groups[i], "AliasOf", NULL) == NULL) {
			continue;
		}

		if (alias == ONLY_ALIASES) {
			alias_value = g_key_file_get_string(keyfile, groups[i], "AliasOf", NULL);

			if (!create_stylus_id_from_aliased(groups[i], alias_value, &lookup)) {
				g_warning ("Failed to parse Aliased ID '%s', ignoring entry", groups[i]);
	                        continue;
			}
			other = g_hash_table_lookup(db->stylus_ht, &lookup);
			if (!other) {
				g_warning ("Unkown Aliased ID 0x%08x %s\n", lookup.tool_id, alias_value);
				continue;
			}
		}

		stylus->name = string_or_fallback(keyfile, groups[i], "Name", other ? other->name : NULL);
		stylus->group = string_or_fallback(keyfile, groups[i], "Group", other ? other->group : NULL);
		stylus->paired_stylus_ids = g_array_new (FALSE, FALSE, sizeof(WacomStylusId));

		eraser_type = string_or_fallback(keyfile, groups[i], "EraserType", other ? eraser_str_from_type(other->eraser_type) : NULL);
		stylus->eraser_type = eraser_type_from_str (eraser_type);
		g_clear_pointer(&eraser_type, g_free);

		/* We have to keep the integer array for libwacom_get_supported_styli() */
		stylus->deprecated_paired_ids = g_array_new (FALSE, FALSE, sizeof(int));
		stylus->paired_styli = g_array_new (FALSE, FALSE, sizeof(WacomStylus*));

		if (alias == IGNORE_ALIASES) {
			string_list = g_key_file_get_string_list(keyfile, groups[i], "PairedStylusIds", NULL, NULL);
		} else {
			string_list = g_key_file_get_string_list(keyfile, groups[i], "PairedStylusIds", NULL, NULL);
			if (string_list == NULL) {
				string_list = garray_to_string_list (other ? other->paired_stylus_ids: NULL);
			}
		}

		for (guint j = 0; string_list && string_list[j]; j++) {
			WacomStylusId paired_id;
			if (parse_stylus_id(string_list[j], &paired_id)) {
				g_array_append_val (stylus->paired_stylus_ids, paired_id);
				if (paired_id.vid == WACOM_VENDOR_ID)
					g_array_append_val (stylus->deprecated_paired_ids, paired_id.tool_id);
			} else {
				g_warning ("Stylus %s (%s) Ignoring invalid PairedStylusIds value\n", stylus->name, groups[i]);
			}
		}
		g_clear_pointer(&string_list, g_strfreev);

		stylus->has_lens = boolean_or_fallback(keyfile, groups[i], "HasLens", other ? other->has_lens : FALSE, stylus);

		stylus->has_wheel = boolean_or_fallback(keyfile, groups[i], "HasWheel", other ? other->has_wheel : FALSE, stylus);

		stylus->num_buttons = int_or_fallback(keyfile, groups[i], "Buttons", other ? other->num_buttons : 0);
		if (stylus->num_buttons == 0 && error != NULL) {
			stylus->num_buttons = -1;
			g_clear_error (&error);
		}

		stylus->axes = WACOM_AXIS_TYPE_NONE;
		string_list = g_key_file_get_string_list (keyfile, groups[i], "Axes", NULL, NULL);

		if (alias == ONLY_ALIASES && (string_list == NULL)) {
			stylus->axes = other->axes;
		} else {
			for (guint j = 0; string_list && string_list[j]; j++) {
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
				if (stylus->axes & flag)
					g_warning ("Duplicate axis %s for stylus ID %s\n",
						   string_list[j], groups[i]);
				stylus->axes |= flag;
			}
		}
		g_clear_pointer(&string_list, g_strfreev);

		type = string_or_fallback(keyfile, groups[i], "Type", other ? str_from_type(other->type) : NULL);
		stylus->type = type_from_str (type);
		g_clear_pointer(&type, g_free);

		if (g_hash_table_lookup (db->stylus_ht, &id) != NULL)
			g_warning ("Duplicate definition for stylus ID '%s'", groups[i]);

		g_hash_table_insert (db->stylus_ht, g_memdup2(&id, sizeof(id)), stylus);
	}
	g_clear_pointer(&groups, g_strfreev);
	g_clear_pointer(&keyfile, g_key_file_free);
}

static void
libwacom_setup_paired_attributes(WacomDeviceDatabase *db)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init(&iter, db->stylus_ht);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		WacomStylus *stylus = value;
		GArray *paired_ids = g_steal_pointer(&stylus->paired_stylus_ids);

		for (guint i = 0; i < paired_ids->len; i++) {
			WacomStylusId *id = &g_array_index(paired_ids, WacomStylusId, i);
			WacomStylus *paired = g_hash_table_lookup(db->stylus_ht, id);

			if (paired == NULL) {
				g_warning ("Ignoring paired stylus %04x:%x", id->vid, id->tool_id);
				continue;
			}

			g_array_append_val(stylus->paired_styli, paired);

			if (libwacom_stylus_is_eraser(paired)) {
				stylus->has_eraser = true;
			}
		}
		g_array_unref(paired_ids);
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
	{ "OLEDs", WACOM_BUTTON_OLED },
	{ "Dial", WACOM_BUTTON_DIAL_MODESWITCH },
	{ "Dial2", WACOM_BUTTON_DIAL2_MODESWITCH },
};

static const struct {
	const char       *key;
	WacomStatusLEDs   value;
} supported_leds[] = {
	{ "Ring",		WACOM_STATUS_LED_RING },
	{ "Ring2",		WACOM_STATUS_LED_RING2 },
	{ "Touchstrip",		WACOM_STATUS_LED_TOUCHSTRIP },
	{ "Touchstrip2",	WACOM_STATUS_LED_TOUCHSTRIP2 },
	{ "Dial",		WACOM_STATUS_LED_DIAL },
	{ "Dial2",		WACOM_STATUS_LED_DIAL2 },
};

static const struct {
	const char             *key;
	WacomIntegrationFlags   value;
} integration_flags[] = {
	{ "Display",		WACOM_DEVICE_INTEGRATED_DISPLAY },
	{ "System",		WACOM_DEVICE_INTEGRATED_SYSTEM },
	{ "Remote",		WACOM_DEVICE_INTEGRATED_REMOTE }
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
		int code = -1;
		WacomButton *button = g_hash_table_lookup(device->buttons, GINT_TO_POINTER(key));
		const char *str = strvals[i];

		if (!button) {
			g_error("%s: Button %c is not defined, ignoring all codes\n",
				device->name, key);
			goto out;
		}

		if (!str) {
			g_error ("%s: Missing EvdevCode for button %d, ignoring all codes\n",
				 device->name, i);
			goto out;
		} else if (g_str_has_prefix(str, "BTN")) {
			code = libevdev_event_code_from_code_name(str);
		} else if (!safe_atoi_base(str, &code, 16)) {
			code = -1;
		}

		if (code < BTN_MISC || code >= BTN_DIGI) {
			g_warning ("%s: Invalid EvdevCode %s for button %c, ignoring all codes\n",
				   device->name, str, key);
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

static inline bool
set_key_codes_from_string(WacomDevice *device, char **strvals)
{
	bool success = false;
	assert(strvals);

	for (unsigned int idx = 0; strvals[idx]; idx++) {
		const char *str = strvals[idx];
		int code = -1;
		int type = -1;

		if (!str) {
			g_error("%s: Missing KeyCode for key %d, ignoring all codes\n",
				device->name, idx);
			goto out;
		} else if (g_str_has_prefix(str, "KEY")) {
			type = EV_KEY;
			code = libevdev_event_code_from_code_name(str);
		} else if (g_str_has_prefix(str, "SW")) {
			type = EV_SW;
			code = libevdev_event_code_from_code_name(str);
		} else {
			if (safe_atoi_base (strvals[idx], &code, 16))
				type = EV_KEY;
		}

		if (code == -1 || type == -1) {
			g_warning ("%s: Invalid KeyCode %s, ignoring all codes\n", device->name, str);
			goto out;
		}

		device->keycodes[idx].type = type;
		device->keycodes[idx].code = code;
		device->num_keycodes = idx + 1;
	}

	success = true;
out:
	if (!success) {
		memset(device->keycodes, 0, sizeof(device->keycodes));
	}
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

	if (!g_key_file_has_group(keyfile, BUTTONS_GROUP))
		return;

	for (i = 0; i < G_N_ELEMENTS (options); i++)
		libwacom_parse_buttons_key(device, keyfile, options[i].key, options[i].flag);

	libwacom_parse_button_codes(device, keyfile);

	device->ring_num_modes = libwacom_parse_num_modes(device, keyfile, "RingNumModes", WACOM_BUTTON_RING_MODESWITCH);
	device->ring2_num_modes = libwacom_parse_num_modes(device, keyfile, "Ring2NumModes", WACOM_BUTTON_RING2_MODESWITCH);
	device->strips_num_modes = libwacom_parse_num_modes(device, keyfile, "StripsNumModes", WACOM_BUTTON_TOUCHSTRIP_MODESWITCH);
	device->dial_num_modes = libwacom_parse_num_modes(device, keyfile, "DialNumModes", WACOM_BUTTON_DIAL_MODESWITCH);
	device->dial2_num_modes = libwacom_parse_num_modes(device, keyfile, "Dial2NumModes", WACOM_BUTTON_DIAL2_MODESWITCH);
}

static void
libwacom_parse_key_codes(WacomDevice *device,
			 GKeyFile    *keyfile)
{
	char **vals;

	vals = g_key_file_get_string_list(keyfile, KEYS_GROUP, "KeyCodes", NULL, NULL);
	if (vals)
		set_key_codes_from_string(device, vals);

	g_strfreev (vals);
}

static void
libwacom_parse_keys(WacomDevice *device,
		    GKeyFile    *keyfile)
{
	if (!g_key_file_has_group(keyfile, KEYS_GROUP))
		return;

	libwacom_parse_key_codes(device, keyfile);
}

static int
wacom_stylus_id_sort(const WacomStylusId *a, const WacomStylusId *b)
{
	if (a->vid == b->vid)
		return a->tool_id - b->tool_id;

	return a->vid - b->vid;
}

static int
styli_id_sort(gconstpointer pa, gconstpointer pb)
{
	const WacomStylus *a = *(WacomStylus**)pa, *b = *(WacomStylus**)pb;

	return wacom_stylus_id_sort(&a->id, &b->id);
}

static void
libwacom_parse_styli_list(WacomDeviceDatabase *db, WacomDevice *device,
			  char **ids)
{
	GArray *array;
	guint i;

	array = g_array_new (FALSE, FALSE, sizeof(WacomStylus*));
	for (i = 0; ids && ids[i]; i++) {
		const char *str = ids[i];

		if (g_str_has_prefix(str, "0x")) {
			WacomStylusId id;
			if (parse_stylus_id(str, &id)) {
				WacomStylus *stylus = g_hash_table_lookup(db->stylus_ht, &id);
				if (stylus)
					g_array_append_val (array, stylus);
				else
					g_warning ("Invalid stylus id for '%s', ignoring stylus", str);
			} else {
				g_warning ("Invalid stylus id format for '%s', ignoring stylus", str);
			}
		} else if (g_str_has_prefix(str, "@")) {
			const char *group = &str[1];
			GHashTableIter iter;
			gpointer key, value;

			g_hash_table_iter_init(&iter, db->stylus_ht);
			while (g_hash_table_iter_next (&iter, &key, &value)) {
				WacomStylus *stylus = value;
				if (stylus->group && g_str_equal(group, stylus->group)) {
					g_array_append_val (array, stylus);
				}
			}
		} else {
			g_warning ("Invalid prefix for '%s', ignoring stylus", str);
		}
	}
	/* Using groups means we don't get the styli in ascending order.
	   Sort it so the output is predictable */
	g_array_sort(array, styli_id_sort);
	device->styli = array;

	/* The legacy PID-only stylus id list */
	device->deprecated_styli_ids = g_array_new(FALSE, FALSE, sizeof(int));
	for (guint i = 0; i < device->styli->len; i++) {
		WacomStylus *stylus = g_array_index(device->styli, WacomStylus*, i);
		/* This only ever worked for Wacom styli, so let's keep that behavior */
		if (stylus->id.vid == 0 || stylus->id.vid == WACOM_VENDOR_ID) {
			g_array_append_val(device->deprecated_styli_ids, stylus->id.tool_id);
		}
	}
}

static void
libwacom_parse_features(WacomDevice *device, GKeyFile *keyfile)
{
	char **string_list;

	/* Features */
	if (g_key_file_get_boolean(keyfile, FEATURES_GROUP, "Stylus", NULL))
		device->features |= FEATURE_STYLUS;

	if (g_key_file_get_boolean(keyfile, FEATURES_GROUP, "Touch", NULL))
		device->features |= FEATURE_TOUCH;

	if (g_key_file_get_boolean(keyfile, FEATURES_GROUP, "Reversible", NULL))
		device->features |= FEATURE_REVERSIBLE;

	if (g_key_file_get_boolean(keyfile, FEATURES_GROUP, "TouchSwitch", NULL))
		device->features |= FEATURE_TOUCHSWITCH;

	if (device->integration_flags != WACOM_DEVICE_INTEGRATED_UNSET &&
	    device->integration_flags & WACOM_DEVICE_INTEGRATED_DISPLAY &&
	    device->features & FEATURE_REVERSIBLE)
		g_warning ("Tablet '%s' is both reversible and integrated in screen. This is impossible", libwacom_get_match(device));

	if (!(device->features & FEATURE_TOUCH) &&
	    (device->features & FEATURE_TOUCHSWITCH))
		g_warning ("Tablet '%s' has touch switch but no touch tool. This is impossible", libwacom_get_match(device));

	device->num_rings = g_key_file_get_integer(keyfile, FEATURES_GROUP, "NumRings", NULL);
	device->num_strips = g_key_file_get_integer(keyfile, FEATURES_GROUP, "NumStrips", NULL);
	device->num_dials = g_key_file_get_integer(keyfile, FEATURES_GROUP, "NumDials", NULL);

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
	device->matches = g_array_new(TRUE, TRUE, sizeof(WacomMatch*));

	string_list = g_key_file_get_string_list(keyfile, DEVICE_GROUP, "DeviceMatch", NULL, NULL);
	if (!string_list) {
		DBG("Missing DeviceMatch= line in '%s'\n", path);
		goto out;
	} else {
		guint i;
		guint nmatches = 0;
		for (i = 0; string_list[i]; i++) {
			WacomMatch *m = libwacom_match_from_string(string_list[i]);
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
				g_warning ("Unrecognized integration flag '%s', ignoring flag", string_list[i]);
		}
		g_strfreev (string_list);
	}

	layout = g_key_file_get_string(keyfile, DEVICE_GROUP, "Layout", NULL);
	if (layout && layout[0] != '\0') {
		/* For the layout, we store the full path to the SVG layout */
		device->layout = g_build_filename (datadir, "layouts", layout, NULL);
	}
	g_free (layout);

	class = g_key_file_get_string(keyfile, DEVICE_GROUP, "Class", NULL);
	device->cls = libwacom_class_string_to_enum(class);
	g_free(class);

	string_list = g_key_file_get_string_list(keyfile, DEVICE_GROUP, "Styli", NULL, NULL);
	if (!string_list) {
		GError *error = NULL;
		if (g_key_file_get_boolean(keyfile, FEATURES_GROUP, "Stylus", &error) ||
		    g_error_matches (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_KEY_NOT_FOUND)) {
			string_list = g_new0(char*, 3);
			string_list[0] = g_strdup_printf("0x0:0x%x", WACOM_ERASER_FALLBACK_ID);
			string_list[1] = g_strdup_printf("0x0:0x%x", WACOM_STYLUS_FALLBACK_ID);
		}
		if (error)
			g_error_free(error);
	}
	libwacom_parse_styli_list(db, device, string_list);
	g_strfreev (string_list);

	device->num_strips = g_key_file_get_integer(keyfile, FEATURES_GROUP, "NumStrips", NULL);
	device->num_dials = g_key_file_get_integer(keyfile, FEATURES_GROUP, "NumDials", NULL);
	device->buttons = g_hash_table_new_full(g_direct_hash, g_direct_equal,
						NULL, g_free);
	device->status_leds = g_array_new (FALSE, FALSE, sizeof(WacomStatusLEDs));

	libwacom_parse_features(device, keyfile);
	libwacom_parse_buttons(device, keyfile);
	libwacom_parse_keys(device, keyfile);

	success = TRUE;

out:
	if (path)
		g_free(path);
	if (keyfile)
		g_key_file_free(keyfile);
	if (error)
		g_error_free(error);
	if (!success) {
		g_warning("Ignoring invalid .tablet file %s", filename);
		device = libwacom_unref(device);
	}

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
load_tablet_files(WacomDeviceDatabase *db, GHashTable *parsed_filenames, const char *datadir)
{
	DIR *dir;
	struct dirent *file;
	bool success = false;

	dir = opendir(datadir);
	if (!dir)
		return errno == ENOENT; /* non-existing directory is ok */

	while ((file = readdir(dir))) {
		WacomDevice *d;
		guint idx = 0;

		if (!is_tablet_file(file))
			continue;

		if (g_hash_table_lookup(parsed_filenames, file->d_name))
			continue;

		g_hash_table_add(parsed_filenames, g_strdup(file->d_name));

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
			/* no duplicate matches allowed */
			if (g_hash_table_contains(db->device_ht, matchstr)) {
				g_critical("Duplicate match of '%s' on device '%s'.",
					   matchstr, libwacom_get_name(d));
				goto out;
			}
			g_hash_table_insert(db->device_ht, g_strdup (matchstr), d);
			libwacom_ref(d);
			idx++;
		}
		libwacom_unref(d);
	}

	success = true;

out:
	closedir(dir);
	return success;
}

static void
stylus_destroy(void *data)
{
	libwacom_stylus_unref((WacomStylus*)data);
}

static bool
load_stylus_files(WacomDeviceDatabase *db, const char *datadir, AliasStatus alias)
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
		libwacom_parse_stylus_keyfile(db, path, alias);
		g_free(path);
	}

	closedir(dir);

	return true;
}

static guint
stylus_hash(WacomStylusId *id)
{
	guint64 full_id = (guint64)id->vid << 32 | id->tool_id;
	return g_int64_hash(&full_id);
}

static gboolean
stylus_compare(WacomStylusId *a, WacomStylusId *b)
{
	return wacom_stylus_id_sort(a, b) == 0;
}

static WacomDeviceDatabase *
database_new_for_paths (char * const *datadirs)
{
	WacomDeviceDatabase *db;
	char * const *datadir;
	GHashTable *parsed_filenames;

	parsed_filenames = g_hash_table_new_full (g_str_hash, g_str_equal, free, NULL);
	if (!parsed_filenames)
		return NULL;

	db = g_new0 (WacomDeviceDatabase, 1);
	db->device_ht = g_hash_table_new_full (g_str_hash,
					       g_str_equal,
					       g_free,
					       (GDestroyNotify) libwacom_destroy);
	db->stylus_ht = g_hash_table_new_full ((GHashFunc)stylus_hash,
					       (GEqualFunc)stylus_compare,
					       (GDestroyNotify) g_free,
					       (GDestroyNotify) stylus_destroy);

	for (datadir = datadirs; *datadir; datadir++) {
		if (!load_stylus_files(db, *datadir, IGNORE_ALIASES))
			goto error;
	}

	for (datadir = datadirs; *datadir; datadir++) {
		if (!load_stylus_files(db, *datadir, ONLY_ALIASES))
			goto error;
	}

	for (datadir = datadirs; *datadir;  datadir++) {
		if (!load_tablet_files(db, parsed_filenames, *datadir))
			goto error;
	}

	g_hash_table_unref(parsed_filenames);

	/* If we couldn't load _anything_ then something's wrong */
	if (g_hash_table_size (db->stylus_ht) == 0 ||
	    g_hash_table_size (db->device_ht) == 0) {
		g_warning("Zero tablet or stylus files found in datadirs");
		goto error;
	}

	libwacom_setup_paired_attributes(db);

	return db;

error:
	libwacom_database_destroy(db);
	return NULL;
}

LIBWACOM_EXPORT WacomDeviceDatabase *
libwacom_database_new_for_path (const char *datadir)
{
	WacomDeviceDatabase *db;
	char **paths;

	paths = g_strsplit(datadir, ":", 0);
	db = database_new_for_paths(paths);

	g_strfreev(paths);

	return db;
}

LIBWACOM_EXPORT WacomDeviceDatabase *
libwacom_database_new (void)
{
	WacomDeviceDatabase *db;
	char *xdgdir = NULL;
	char *xdg_config_home = g_strdup(g_getenv("XDG_CONFIG_HOME"));

	if (!xdg_config_home)
		xdg_config_home = g_strdup_printf("%s/.config/", g_get_home_dir());

	xdgdir = g_strdup_printf("%s/libwacom", xdg_config_home);

	char *datadir[] = {
		xdgdir,
		ETCDIR,
		DATADIR,
		NULL,
	};

	db = database_new_for_paths(datadir);

	free(xdgdir);
	free(xdg_config_home);

	return db;
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
