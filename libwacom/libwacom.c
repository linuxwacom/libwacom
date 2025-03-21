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

#include "libwacom.h"
#include "libwacomint.h"
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <gudev/gudev.h>
#include <libevdev/libevdev.h>

#if !HAVE_G_MEMDUP2
#define g_memdup2 g_memdup
#endif

static const WacomDevice *
libwacom_get_device(const WacomDeviceDatabase *db, const char *match)
{
	return (WacomDevice *) g_hash_table_lookup (db->device_ht, match);
}

static gboolean
is_tablet (GUdevDevice *device)
{
	return g_udev_device_get_property_as_boolean (device, "ID_INPUT_TABLET");
}

static gboolean
is_touchpad (GUdevDevice *device)
{
	return g_udev_device_get_property_as_boolean (device, "ID_INPUT_TOUCHPAD");
}


static gboolean
is_tablet_or_touchpad (GUdevDevice *device)
{
	return is_touchpad (device) || is_tablet (device);
}

/* Overriding SUBSYSTEM isn't allowed in udev (works sometimes, but not
 * always). For evemu devices we need to set custom properties to make them
 * detected by libwacom.
 */
static char *
get_uinput_subsystem (GUdevDevice *device)
{
	const char *bus_str;
	GUdevDevice *parent;


	bus_str = NULL;
	parent = g_object_ref (device);

	while (parent && !g_udev_device_get_property_as_boolean (parent, "UINPUT_DEVICE")) {
		GUdevDevice *old_parent = parent;
		parent = g_udev_device_get_parent (old_parent);
		g_object_unref (old_parent);
	}

	if (parent) {
		bus_str = g_udev_device_get_property (parent, "UINPUT_SUBSYSTEM");
		g_object_unref (parent);
	}

	return bus_str ? g_strdup (bus_str) : NULL;
}

static gboolean
get_bus_vid_pid (GUdevDevice  *device,
		 WacomBusType *bus,
		 int          *vendor_id,
		 int          *product_id,
		 WacomError   *error)
{
	g_autoptr(GUdevDevice) parent = NULL;
	const char *product_str;
	g_auto(GStrv) splitted_product = NULL;
	unsigned int bus_id;
	gboolean retval = FALSE;

	/* Parse that:
	 * E: PRODUCT=5/56a/81/100
	 * into:
	 * vendor 0x56a
	 * product 0x81 */
	parent = g_object_ref (device);
	product_str = g_udev_device_get_property (device, "PRODUCT");

	while (!product_str && parent) {
		GUdevDevice *old_parent = parent;
		parent = g_udev_device_get_parent (old_parent);
		if (parent)
			product_str = g_udev_device_get_property (parent, "PRODUCT");
		g_object_unref (old_parent);
	}

	if (!product_str)
		/* PRODUCT not found, hoping the old method will work */
		goto out;

	splitted_product = g_strsplit (product_str, "/", 4);
	if (g_strv_length (splitted_product) != 4) {
		libwacom_error_set(error, WERROR_UNKNOWN_MODEL, "Unable to parse model identification");
		goto out;
	}

	bus_id = (int)strtoul (splitted_product[0], NULL, 16);
	*vendor_id = (int)strtol (splitted_product[1], NULL, 16);
	*product_id = (int)strtol (splitted_product[2], NULL, 16);

	switch (bus_id) {
	case 0:
		*bus = WBUSTYPE_UNKNOWN;
		retval = TRUE;
		break;
	case 3:
		*bus = WBUSTYPE_USB;
		retval = TRUE;
		break;
	case 5:
		*bus = WBUSTYPE_BLUETOOTH;
		retval = TRUE;
		break;
	case 24:
		*bus = WBUSTYPE_I2C;
		retval = TRUE;
		break;
	}

out:
	return retval;
}

static char *
get_bus (GUdevDevice *device)
{
	const char *subsystem;
	char *bus_str;
	GUdevDevice *parent;

	bus_str = get_uinput_subsystem (device);
	if (bus_str)
		return bus_str;

	subsystem = g_udev_device_get_subsystem (device);
	parent = g_object_ref (device);

	while (parent && subsystem &&
	       (g_str_equal(subsystem, "input") || g_str_equal (subsystem, "hid"))) {
		GUdevDevice *old_parent = parent;
		parent = g_udev_device_get_parent (old_parent);
		if (parent)
			subsystem = g_udev_device_get_subsystem (parent);
		g_object_unref (old_parent);
	}

	if (parent) {
		if (subsystem && (g_str_equal(subsystem, "tty") || g_str_equal(subsystem, "serio")))
			bus_str = g_strdup ("serial");
		else
			bus_str = g_strdup (subsystem);

		g_object_unref (parent);
	} else
		bus_str = strdup("unknown");

	return bus_str;
}

static GUdevDevice *
client_query_by_subsystem_and_device_file (GUdevClient *client,
					   const char  *subsystem,
					   const char  *path)
{
	GList *l;
	g_autoptr(GList) devices;
	GUdevDevice *ret = NULL;

	devices = g_udev_client_query_by_subsystem (client, subsystem);
	for (l = devices; l != NULL; l = l->next) {
		if (!ret && g_strcmp0 (g_udev_device_get_device_file (l->data), path) == 0)
			ret = g_object_ref (l->data);
		g_object_unref (l->data);
	}
	return ret;
}

static char *
get_device_prop(GUdevDevice *device, const char *propname)
{
	char *value = NULL;
	g_autoptr(GUdevDevice) parent = g_object_ref(device);

	do {
		GUdevDevice *next;
		const char *v = g_udev_device_get_property(parent, propname);
		if (v) {
			/* NAME and UNIQ properties are enclosed with quotes */
			size_t offset = v[0] == '"' ? 1 : 0;
			value = g_strdup(v + offset);
			if (value[strlen(value) - 1] == '"')
				value[strlen(value) - 1] = '\0';
			break;
		}
		next = g_udev_device_get_parent (parent);
		g_object_unref(parent);
		parent = next;
	} while (parent);

	return value;
}

static char *
parse_uniq(char *uniq)
{
	g_autoptr(GRegex) regex = NULL;
	g_autoptr(GMatchInfo) match_info = NULL;

	if (!uniq)
		return NULL;

	if (strlen(uniq) == 0) {
		g_free (uniq);
		return NULL;
	}

	/* The UCLogic kernel driver returns firmware names with form
	 * <vendor>_<model>_<version>. Remove the version from `uniq` to avoid
	 * mismatches on firmware updates. */
	regex = g_regex_new ("(.*_.*)_.*$", 0, 0, NULL);
	g_regex_match (regex, uniq, 0, &match_info);

	if (g_match_info_matches (match_info)) {
		gchar *tmp = uniq;
		uniq = g_match_info_fetch (match_info, 1);
		g_free (tmp);
	}


	return uniq;
}

static gboolean
get_device_info (const char            *path,
		 int                   *vendor_id,
		 int                   *product_id,
		 char                 **name,
		 char                 **uniq,
		 WacomBusType          *bus,
		 WacomIntegrationFlags *integration_flags,
		 WacomError            *error)
{
	g_autoptr(GUdevClient) client = NULL;
	g_autoptr(GUdevDevice) device = NULL;
	const char * const subsystems[] = { "input", NULL };
	gboolean retval;
	g_autofree char *bus_str;
	const char *devname;

	retval = FALSE;
	/* The integration flags from device info are unset by default */
	*integration_flags = WACOM_DEVICE_INTEGRATED_UNSET; // NOLINT: core.EnumCastOutOfRange
	*name = NULL;
	*uniq = NULL;
	bus_str = NULL;
	client = g_udev_client_new (subsystems);
	device = client_query_by_subsystem_and_device_file (client, subsystems[0], path);
	if (device == NULL)
		device = g_udev_client_query_by_device_file (client, path);
	if (device == NULL) {
		libwacom_error_set(error, WERROR_INVALID_PATH, "Could not find device '%s' in udev", path);
		goto out;
	}

	/* Touchpads are only for the "Finger" part of Bamboo devices */
	if (!is_tablet_or_touchpad(device)) {
		GUdevDevice *parent;

		parent = g_udev_device_get_parent(device);
		if (!parent || !is_tablet_or_touchpad(parent)) {
			libwacom_error_set(error, WERROR_INVALID_PATH, "Device '%s' is not a tablet", path);
			if (parent)
				g_object_unref (parent);
			goto out;
		}
		g_object_unref (parent);
	}

	/* Is the device integrated in display? */
	devname = g_udev_device_get_name (device);
	if (devname != NULL) {
		g_autofree char *sysfs_path = NULL;
		g_autofree char *contents = NULL;

		sysfs_path = g_build_filename ("/sys/class/input", devname, "device/properties", NULL);
		if (g_file_get_contents (sysfs_path, &contents, NULL, NULL)) {
			int flag;

			flag = atoi(contents);
			flag &= (1 << INPUT_PROP_DIRECT) | (1 << INPUT_PROP_POINTER);
			/*
			 * To ensure we are dealing with a screen tablet, need
			 * to check that it has DIRECT and non-POINTER (DIRECT
			 * alone is not sufficient since it's set for drawing
			 * tablets as well)
			 */
			if (flag == (1 << INPUT_PROP_DIRECT))
				*integration_flags = WACOM_DEVICE_INTEGRATED_DISPLAY;
			else
				*integration_flags = WACOM_DEVICE_INTEGRATED_NONE;
		}
	}

	*name = get_device_prop (device, "NAME");
	*uniq = parse_uniq(get_device_prop(device, "UNIQ"));
	if (*name == NULL)
		goto out;

	/* Parse the PRODUCT attribute (for Bluetooth, USB, I2C) */
	retval = get_bus_vid_pid (device, bus, vendor_id, product_id, error);
	if (retval)
		goto out;

	bus_str = get_bus (device);
	*bus = bus_from_str (bus_str);

	if (*bus == WBUSTYPE_SERIAL) {
		if (is_touchpad (device))
			goto out;

		/* The serial bus uses 0:0 as the vid/pid */
		*vendor_id = 0;
		*product_id = 0;
		retval = TRUE;
	} else {
		libwacom_error_set(error, WERROR_UNKNOWN_MODEL, "Unsupported bus '%s'", bus_str);
	}

out:
	if (retval == FALSE) {
		g_free (*name);
		g_free (*uniq);
	}
	return retval;
}

static WacomDevice *
libwacom_copy(const WacomDevice *device)
{
	WacomDevice *d;
	GHashTableIter iter;
	gpointer k, v;

	d = g_new0 (WacomDevice, 1);
	g_atomic_int_inc(&d->refcnt);
	d->name = g_strdup (device->name);
	d->model_name = g_strdup (device->model_name);
	d->width = device->width;
	d->height = device->height;
	d->integration_flags = device->integration_flags;
	d->layout = g_strdup(device->layout);
	d->matches = g_array_copy(device->matches);
	for (guint i = 0; i < device->matches->len; i++) {
		WacomMatch *m = g_array_index(d->matches, WacomMatch*, i);
		libwacom_match_ref(m);
	}
	d->match = libwacom_match_ref(device->match);
	if (device->paired)
		d->paired = libwacom_match_ref(device->paired);
	d->cls = device->cls;
	d->num_strips = device->num_strips;
	d->num_rings = device->num_rings;
	d->num_dials = device->num_dials;
	d->features = device->features;
	d->strips_num_modes = device->strips_num_modes;
	d->dial_num_modes = device->dial_num_modes;
	d->dial2_num_modes = device->dial2_num_modes;
	d->ring_num_modes = device->ring_num_modes;
	d->ring2_num_modes = device->ring2_num_modes;
	d->styli = g_array_copy(device->styli);
	d->deprecated_styli_ids = g_array_copy(device->deprecated_styli_ids);
	d->status_leds = g_array_copy(device->status_leds);

	d->buttons = g_hash_table_new_full(g_direct_hash, g_direct_equal,
					   NULL, g_free);
	g_hash_table_iter_init(&iter, device->buttons);
	while (g_hash_table_iter_next(&iter, &k, &v)) {
		WacomButton *a = v;
		WacomButton *b = g_memdup2(a, sizeof(WacomButton));
		g_hash_table_insert(d->buttons, k, b);
	}

	d->num_keycodes = device->num_keycodes;
	memcpy(d->keycodes, device->keycodes, sizeof(device->keycodes));

	return d;
}

static bool
match_is_equal(const WacomMatch *a, const WacomMatch *b)
{
	return g_str_equal(a->match, b->match);
}

static bool
matches_are_equal(const WacomDevice *a, const WacomDevice *b)
{
	const WacomMatch **ma, **mb, **match_a, **match_b;

	ma = libwacom_get_matches(a);
	mb = libwacom_get_matches(b);

	for (match_a = ma; *match_a; match_a++) {
		int found = 0;
		for (match_b = mb; !found && *match_b; match_b++) {
			if (match_is_equal(*match_a, *match_b))
				found = 1;
		}
		if (!found)
			return false;
	}

	return true;
}

/* Compare layouts based on file name, stripping the full path */
static gboolean
libwacom_same_layouts (const WacomDevice *a, const WacomDevice *b)
{
	g_autofree gchar *file1 = NULL;
	g_autofree gchar *file2 = NULL;
	gboolean rc;

	/* Conveniently handle the null case */
	if (a->layout == b->layout)
		return TRUE;

	file1 = NULL;
	file2 = NULL;
	if (a->layout != NULL)
		file1 = g_path_get_basename (a->layout);
	if (b->layout != NULL)
		file2 = g_path_get_basename (b->layout);

	rc = (g_strcmp0 (file1, file2) == 0);

	return rc;
}

LIBWACOM_EXPORT int
libwacom_compare(const WacomDevice *a, const WacomDevice *b, WacomCompareFlags flags)
{
	GHashTableIter iter;
	gpointer k, v;

	g_return_val_if_fail(a || b, 0);

	if (!a || !b)
		return 1;

	if (a == b)
		return 0;

	if (!g_str_equal(a->name, b->name))
		return 1;

	if (a->width != b->width || a->height != b->height)
		return 1;

	if (!libwacom_same_layouts (a, b))
		return 1;

	if (a->integration_flags != b->integration_flags)
		return 1;

	if (a->cls != b->cls)
		return 1;

	if (a->num_strips != b->num_strips)
		return 1;

	if (a->num_dials != b->num_dials)
		return 1;

	if (a->features != b->features)
		return 1;

	if (a->strips_num_modes != b->strips_num_modes)
		return 1;

	if (a->dial_num_modes != b->dial_num_modes)
		return 1;

	if (a->dial2_num_modes != b->dial2_num_modes)
		return 1;

	if (a->ring_num_modes != b->ring_num_modes)
		return 1;

	if (a->ring2_num_modes != b->ring2_num_modes)
		return 1;

	if (g_hash_table_size(a->buttons) != g_hash_table_size(b->buttons))
		return 1;

	/* We don't need to check deprecated_stylus_ids because if they differ
	 * when the real id doesn't that's a bug */

	if (a->styli->len != b->styli->len)
		return 1;

	/* This needs to be a deep comparison - our styli array contains
	 * WacomStylus* pointers but we want libwacom_compare() to return
	 * true if the stylus data matches (test-dbverify compares styli
	 * from two different WacomDeviceDatabase).
	 */
	for (guint i = 0; i < a->styli->len; i++) {
		WacomStylus *as = g_array_index(a->styli, WacomStylus*, i);
		WacomStylus *bs = g_array_index(b->styli, WacomStylus*, i);
		if (as->id.tool_id != bs->id.tool_id)
			return 1;
	}

	if (a->status_leds->len != b->status_leds->len)
		return 1;

	if (a->status_leds->len > 0 &&
	    memcmp(a->status_leds->data, b->status_leds->data,
		   g_array_get_element_size(a->status_leds) * a->status_leds->len) != 0)
		return 1;

	g_hash_table_iter_init(&iter, a->buttons);
	while (g_hash_table_iter_next(&iter, &k, &v)) {
		WacomButton *ba = v;
		WacomButton *bb = g_hash_table_lookup(b->buttons, k);

		if (!bb || ba->flags != bb->flags || ba->code != bb->code)
			return 1;
	}

	if ((a->paired == NULL && b->paired != NULL) ||
	    (a->paired != NULL && b->paired == NULL) ||
	    (a->paired && b->paired && !match_is_equal(a->paired, b->paired)))
		return 1;

	if ((flags & WCOMPARE_MATCHES) && !matches_are_equal(a, b))
		return 1;
	else if (!match_is_equal(a->match, b->match))
			return 1;

	return 0;
}

static const WacomDevice *
libwacom_new (const WacomDeviceDatabase *db, const char *name, const char *uniq, int vendor_id, int product_id, WacomBusType bus, WacomError *error)
{
	const WacomDevice *device;
	g_autofree char *match = NULL;

	if (!db) {
		libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
		return NULL;
	}

	match = make_match_string(name, uniq, bus, vendor_id, product_id);
	device = libwacom_get_device(db, match);

	return device;
}

static bool
builder_is_name_only(const WacomBuilder *builder)
{
	return builder->device_name != NULL && builder->match_name == NULL &&
		builder->uniq == NULL &&
		builder->vendor_id == 0 && builder->product_id == 0 &&
		builder->bus == WBUSTYPE_UNKNOWN;
}

static bool
builder_is_uniq_only(const WacomBuilder *builder)
{
	return builder->device_name == NULL && builder->match_name == NULL &&
		builder->uniq != NULL &&
		builder->vendor_id == 0 && builder->product_id == 0 &&
		builder->bus == WBUSTYPE_UNKNOWN;
}

/**
 * Return a copy of the given device or, if NULL, the fallback device with
 * the name changed to the override name.
 */
static WacomDevice *
fallback_or_device(const WacomDeviceDatabase *db, const WacomDevice *device,
		   const char *name_override, WacomFallbackFlags fallback_flags)
{
	WacomDevice *copy = NULL;
	const WacomDevice *fallback;
	const char *fallback_name = NULL;

	if (device != NULL) {
		return libwacom_copy(device);
	}

	switch (fallback_flags) {
	case WFALLBACK_NONE:
		return NULL;
	case WFALLBACK_GENERIC:
		fallback_name = "generic";
		break;
	default:
		g_assert_not_reached();
		break;
	}

	fallback = libwacom_get_device(db, fallback_name);
	if (fallback == NULL)
		return NULL;

	copy = libwacom_copy(fallback);
	if (name_override != NULL) {
		g_free(copy->name);
		copy->name = g_strdup(name_override);
	}
	return copy;
}

static gint
find_named_device(const WacomDevice *device, const char *name)
{
	return g_strcmp0(device->name, name);
}

static gint
find_uniq_device(const WacomDevice *device, const char *uniq)
{
	const WacomMatch **matches = libwacom_get_matches(device);

	for (const WacomMatch **match = matches; *match; match++) {
		if (uniq && (*match)->uniq && g_str_equal((*match)->uniq, uniq))
			return 0;
	}
	return -1;
}

LIBWACOM_EXPORT WacomDevice*
libwacom_new_from_builder(const WacomDeviceDatabase *db, const WacomBuilder *builder,
			  WacomFallbackFlags fallback, WacomError *error)
{
	const WacomDevice *device = NULL;
	WacomDevice *ret = NULL;
	switch (fallback) {
		case WFALLBACK_NONE:
		case WFALLBACK_GENERIC:
			break;
		default:
			libwacom_error_set(error, WERROR_BUG_CALLER, "invalid fallback flags");
			return NULL;
	}

	if (!db) {
		libwacom_error_set(error, WERROR_INVALID_DB, "db is NULL");
		return NULL;
	}

	/* Name-only matches behave like new_from_name */
	if (builder_is_name_only(builder)) {
		g_autoptr(GList) keys = g_hash_table_get_values(db->device_ht);
		GList *entry = g_list_find_custom(keys, builder->device_name, (GCompareFunc)find_named_device);
		if (entry)
			device = entry->data;
		ret = fallback_or_device(db, device, builder->device_name, fallback);
	/* Uniq-only behaves like new_from_name but matches on uniq in the match strings */
	} else if (builder_is_uniq_only(builder)) {
		g_autoptr(GList) keys = g_hash_table_get_values(db->device_ht);
		GList *entry = g_list_find_custom(keys, builder->uniq, (GCompareFunc)find_uniq_device);
		if (entry)
			device = entry->data;
		ret = fallback_or_device(db, device, builder->device_name, fallback);
	} else {
		WacomBusType all_busses[] = {
			WBUSTYPE_USB,
			WBUSTYPE_I2C,
			WBUSTYPE_BLUETOOTH,
			WBUSTYPE_UNKNOWN,
		};
		WacomBusType fixed_bus[] = {
			builder->bus,
			WBUSTYPE_UNKNOWN,
		};
		WacomBusType *bus;

		int vendor_id, product_id;
		char *name, *uniq;
		const char *used_match_name = NULL;
		const char *used_match_uniq = NULL;

		vendor_id = builder->vendor_id;
		product_id = builder->product_id;
		name = builder->match_name;
		uniq = builder->uniq;
		if (builder->bus)
			bus = fixed_bus;
		else
			bus = all_busses;

		while (*bus != WBUSTYPE_UNKNOWN) {
			/* Uniq (where it exists) is more reliable than the name which may be re-used
			 * across tablets. So try to find a uniq+name match first, then uniq-only, then
			 * name-only.
			 */
			struct match_approach {
				const char *name;
				const char *uniq;
			} approaches[] = {
				{ name, uniq },
				{ NULL, uniq },
				{ name, NULL },
				{ NULL, NULL },
			};
			struct match_approach *approach = approaches;
			while (true) {
				const char *match_name = approach->name;
				const char *match_uniq = approach->uniq;
				device = libwacom_new (db, match_name, match_uniq, vendor_id, product_id, *bus, error);
				if (device) {
					used_match_name = match_name;
					used_match_uniq = match_uniq;
					break;
				}

				if (approach->name == NULL && approach->uniq == NULL)
					break;

				approach++;
			}
			if (device)
				break;
			bus++;
		}
		ret = fallback_or_device(db, device, builder->device_name, fallback);
		if (ret && device != NULL) {
			/* If this isn't the fallback device: for multiple-match devices, set to the one we requested */
			WacomMatch *used_match = libwacom_match_new(used_match_name, used_match_uniq, *bus, vendor_id, product_id);
			libwacom_set_default_match(ret, used_match);
			libwacom_match_unref(used_match);
		}
	}

	if (ret == NULL)
		libwacom_error_set(error, WERROR_UNKNOWN_MODEL, "unknown model");
	return ret;
}

LIBWACOM_EXPORT WacomDevice*
libwacom_new_from_path(const WacomDeviceDatabase *db, const char *path, WacomFallbackFlags fallback, WacomError *error)
{
	int vendor_id, product_id;
	WacomBusType bus;
	WacomDevice *device;
	WacomIntegrationFlags integration_flags;
	g_autofree char *name = NULL;
	g_autofree char *uniq = NULL;
	WacomBuilder *builder;

	if (!path) {
		libwacom_error_set(error, WERROR_INVALID_PATH, "path is NULL");
		return NULL;
	}

	if (!get_device_info (path, &vendor_id, &product_id, &name, &uniq, &bus, &integration_flags, error))
		return NULL;

	builder = libwacom_builder_new();
	libwacom_builder_set_match_name(builder, name);
	libwacom_builder_set_device_name(builder, name);
	libwacom_builder_set_bustype(builder, bus);
	libwacom_builder_set_uniq(builder, uniq);
	libwacom_builder_set_usbid(builder, vendor_id, product_id);
	device = libwacom_new_from_builder(db, builder, fallback, error);
	/* if unset, use the kernel flags. Could be unset as well. */
	if (device && device->integration_flags == WACOM_DEVICE_INTEGRATED_UNSET)
		device->integration_flags = integration_flags;

	libwacom_builder_destroy(builder);

	return device;
}

LIBWACOM_EXPORT WacomDevice*
libwacom_new_from_usbid(const WacomDeviceDatabase *db, int vendor_id, int product_id, WacomError *error)
{
	WacomDevice *device;
	WacomBuilder *builder = libwacom_builder_new();

	libwacom_builder_set_usbid(builder, vendor_id, product_id);
	device = libwacom_new_from_builder(db, builder, WFALLBACK_NONE, error);
	libwacom_builder_destroy(builder);

	return device;
}

LIBWACOM_EXPORT WacomDevice*
libwacom_new_from_name(const WacomDeviceDatabase *db, const char *name, WacomError *error)
{
	WacomBuilder *builder = libwacom_builder_new();
	WacomDevice *device;
	libwacom_builder_set_device_name(builder, name);

	device = libwacom_new_from_builder(db, builder, WFALLBACK_NONE, error);
	libwacom_builder_destroy(builder);

	return device;
}

static void print_styli_for_device (int fd, const WacomDevice *device)
{
	int nstyli;
	g_autofree const WacomStylus **styli = NULL;
	int i;
	unsigned idx = 0;
	char buf[1024] = {0};

	if (!libwacom_has_stylus(device))
		return;

	styli = libwacom_get_styli(device, &nstyli);
	for (i = 0; i < nstyli; i++) {
		const WacomStylus *stylus = styli[i];
		/* 20 digits for a stylus are enough, right */
		assert(idx < sizeof(buf) - 20);

		if (stylus->id.vid != WACOM_VENDOR_ID)
			idx += snprintf(buf + idx, 20, "0x%04x:%#x;", stylus->id.vid, stylus->id.tool_id);
		else
			idx += snprintf(buf + idx, 20, "%#x;", stylus->id.tool_id);
	}

	dprintf(fd, "Styli=%s\n", buf);
}

static void print_layout_for_device (int fd, const WacomDevice *device)
{
	const char *layout_filename;
	g_autofree gchar *base_name = NULL;

	layout_filename = libwacom_get_layout_filename(device);
	if (layout_filename) {
		base_name = g_path_get_basename (layout_filename);
		dprintf(fd, "Layout=%s\n", base_name);
	}
}

static void print_supported_leds (int fd, const WacomDevice *device)
{
	char *leds_name[] = {
		"Ring;",
		"Ring2;",
		"Strip;",
		"Strip2;"
	};
	int num_leds;
	const WacomStatusLEDs *status_leds;
	char buf[256] = {0};
	bool have_led = false;

	status_leds = libwacom_get_status_leds(device, &num_leds);

	snprintf(buf, sizeof(buf), "%s%s%s%s",
		 num_leds > 0 ? leds_name[status_leds[0]] : "",
		 num_leds > 1 ? leds_name[status_leds[1]] : "",
		 num_leds > 2 ? leds_name[status_leds[2]] : "",
		 num_leds > 3 ? leds_name[status_leds[3]] : "");
	have_led = num_leds > 0;

	dprintf(fd, "%sStatusLEDs=%s\n", have_led ? "" : "# ", buf);
}

static void print_button_flag_if(int fd, const WacomDevice *device, const char *label, int flag)
{
	int nbuttons = libwacom_get_num_buttons(device);
	char buf[nbuttons * 2 + 1];
	int idx = 0;
	char b;
	bool have_flag = false;

	for (b = 'A'; b < 'A' + nbuttons; b++) {
		if (libwacom_get_button_flag(device, b) & flag) {
			buf[idx++] = b;
			buf[idx++] = ';';
			have_flag = true;
		}
	}
	buf[idx] = '\0';
	dprintf(fd, "%s%s=%s\n", have_flag ? "" : "# ", label, buf);
}

static void print_button_evdev_codes(int fd, const WacomDevice *device)
{
	int nbuttons = libwacom_get_num_buttons(device);
	char b;
	char buf[1024] = {0};
	unsigned idx = 0;

	for (b = 'A'; b < 'A' + nbuttons; b++) {
		unsigned int code = libwacom_get_button_evdev_code(device, b);
		const char *str = libevdev_event_code_get_name(EV_KEY, code);

		assert(idx < sizeof(buf) - 30);
		if (str)
			idx += snprintf(buf + idx, 30, "%s;", str);
		else
			idx += snprintf(buf + idx, 30, "0x%x;", code);
	}
	dprintf(fd, "EvdevCodes=%s\n", buf);
}

static void print_buttons_for_device (int fd, const WacomDevice *device)
{
	int nbuttons = libwacom_get_num_buttons(device);

	if (nbuttons == 0)
		return;

	dprintf(fd, "[Buttons]\n");

	print_button_flag_if(fd, device, "Left", WACOM_BUTTON_POSITION_LEFT);
	print_button_flag_if(fd, device, "Right", WACOM_BUTTON_POSITION_RIGHT);
	print_button_flag_if(fd, device, "Top", WACOM_BUTTON_POSITION_TOP);
	print_button_flag_if(fd, device, "Bottom", WACOM_BUTTON_POSITION_BOTTOM);
	print_button_flag_if(fd, device, "Strip", WACOM_BUTTON_TOUCHSTRIP_MODESWITCH);
	print_button_flag_if(fd, device, "Strip2", WACOM_BUTTON_TOUCHSTRIP2_MODESWITCH);
	print_button_flag_if(fd, device, "Dial", WACOM_BUTTON_DIAL_MODESWITCH);
	print_button_flag_if(fd, device, "OLEDs", WACOM_BUTTON_OLED);
	print_button_flag_if(fd, device, "Ring", WACOM_BUTTON_RING_MODESWITCH);
	print_button_flag_if(fd, device, "Ring2", WACOM_BUTTON_RING2_MODESWITCH);
	print_button_flag_if(fd, device, "Dial", WACOM_BUTTON_DIAL_MODESWITCH);
	print_button_flag_if(fd, device, "Dial2", WACOM_BUTTON_DIAL2_MODESWITCH);
	print_button_evdev_codes(fd, device);
	dprintf(fd, "RingNumModes=%d\n", libwacom_get_ring_num_modes(device));
	dprintf(fd, "Ring2NumModes=%d\n", libwacom_get_ring2_num_modes(device));
	dprintf(fd, "StripsNumModes=%d\n", libwacom_get_strips_num_modes(device));
	dprintf(fd, "DialNumModes=%d\n", libwacom_get_dial_num_modes(device));
	dprintf(fd, "Dial2NumModes=%d\n", libwacom_get_dial2_num_modes(device));

	dprintf(fd, "\n");
}

static void print_integrated_flags_for_device (int fd, const WacomDevice *device)
{
	/*
	 * If flag is WACOM_DEVICE_INTEGRATED_UNSET, the info is not provided
	 * by the tablet database but deduced otherwise (e.g. from sysfs device
	 * properties on Linux)
	 */
	if (device->integration_flags == WACOM_DEVICE_INTEGRATED_UNSET)
		return;
	dprintf(fd, "IntegratedIn=");
	if (device->integration_flags & WACOM_DEVICE_INTEGRATED_DISPLAY)
		dprintf(fd, "Display;");
	if (device->integration_flags & WACOM_DEVICE_INTEGRATED_SYSTEM)
		dprintf(fd, "System;");
	if (device->integration_flags & WACOM_DEVICE_INTEGRATED_REMOTE)
		dprintf(fd, "Remote;");
	dprintf(fd, "\n");
}

static void print_match(int fd, const WacomMatch *match)
{
	const char  *name       = libwacom_match_get_name(match);
	const char  *uniq       = libwacom_match_get_uniq(match);
	WacomBusType type	= libwacom_match_get_bustype(match);
	int          vendor     = libwacom_match_get_vendor_id(match);
	int          product    = libwacom_match_get_product_id(match);
	const char  *bus_name;

	switch(type) {
		case WBUSTYPE_BLUETOOTH:	bus_name = "bluetooth";	break;
		case WBUSTYPE_USB:		bus_name = "usb";	break;
		case WBUSTYPE_SERIAL:		bus_name = "serial";	break;
		case WBUSTYPE_I2C:		bus_name = "i2c";	break;
		case WBUSTYPE_UNKNOWN:		bus_name = "unknown";	break;
		default:			g_assert_not_reached(); break;
	}
	dprintf(fd, "%s|%04x|%04x", bus_name, vendor, product);
	if (name)
		dprintf(fd, "|%s", name);
	if (uniq)
		dprintf(fd, "|%s", uniq);
	dprintf(fd, ";");
}

LIBWACOM_EXPORT void
libwacom_print_device_description(int fd, const WacomDevice *device)
{
	const WacomMatch **match;
	WacomClass class;
	const char *class_name;

	class  = device->cls;
	switch(class) {
		case WCLASS_UNKNOWN:	class_name = "Unknown";	break;
		case WCLASS_INTUOS3:	class_name = "Intuos3";	break;
		case WCLASS_INTUOS4:	class_name = "Intuos4";	break;
		case WCLASS_INTUOS5:	class_name = "Intuos5";	break;
		case WCLASS_CINTIQ:	class_name = "Cintiq";	break;
		case WCLASS_BAMBOO:	class_name = "Bamboo";	break;
		case WCLASS_GRAPHIRE:	class_name = "Graphire";break;
		case WCLASS_ISDV4:	class_name = "ISDV4";	break;
		case WCLASS_INTUOS:	class_name = "Intuos";	break;
		case WCLASS_INTUOS2:	class_name = "Intuos2";	break;
		case WCLASS_PEN_DISPLAYS:	class_name = "PenDisplay";	break;
		case WCLASS_REMOTE:	class_name = "Remote";	break;
		default:		g_assert_not_reached(); break;
	}

	dprintf(fd, "[Device]\n");
	dprintf(fd, "Name=%s\n", libwacom_get_name(device));
	dprintf(fd, "ModelName=%s\n", libwacom_get_model_name(device) ? libwacom_get_model_name(device) : "");
	dprintf(fd, "DeviceMatch=");
	for (match = libwacom_get_matches(device); *match; match++)
		print_match(fd, *match);
	dprintf(fd, "\n");

	if (libwacom_get_paired_device(device)) {
		dprintf(fd, "PairedID=");
		print_match(fd, libwacom_get_paired_device(device));
		dprintf(fd, "\n");
	}

	dprintf(fd, "Class=%s\n",		class_name);
	dprintf(fd, "Width=%d\n",		libwacom_get_width(device));
	dprintf(fd, "Height=%d\n",		libwacom_get_height(device));
	print_integrated_flags_for_device(fd, device);
	print_layout_for_device(fd, device);
	print_styli_for_device(fd, device);
	dprintf(fd, "\n");

	dprintf(fd, "[Features]\n");
	dprintf(fd, "Reversible=%s\n", libwacom_is_reversible(device)	? "true" : "false");
	dprintf(fd, "Stylus=%s\n",	 libwacom_has_stylus(device)	? "true" : "false");
	dprintf(fd, "NumRings=%d\n",	 libwacom_get_num_rings(device));
	dprintf(fd, "Touch=%s\n",	 libwacom_has_touch(device)	? "true" : "false");
	dprintf(fd, "TouchSwitch=%s\n",	libwacom_has_touchswitch(device)? "true" : "false");
	print_supported_leds(fd, device);

	dprintf(fd, "NumStrips=%d\n",	libwacom_get_num_strips(device));
	dprintf(fd, "\n");

	dprintf(fd, "NumDials=%d\n",	libwacom_get_num_dials(device));
	dprintf(fd, "\n");

	print_buttons_for_device(fd, device);
}

WacomDevice *
libwacom_ref(WacomDevice *device)
{
	assert(device->refcnt >= 1);

	g_atomic_int_inc(&device->refcnt);
	return device;
}

WacomDevice *
libwacom_unref(WacomDevice *device)
{
	if (device == NULL)
		return NULL;

	assert(device->refcnt >= 1);

	if (!g_atomic_int_dec_and_test(&device->refcnt))
		return NULL;

	g_free (device->name);
	g_free (device->model_name);
	g_free (device->layout);
	if (device->paired)
		libwacom_match_unref(device->paired);
	for (guint i = 0; i < device->matches->len; i++)
		libwacom_match_unref(g_array_index(device->matches, WacomMatch*, i));
	g_clear_pointer (&device->matches, g_array_unref);
	libwacom_match_unref(device->match);
	g_clear_pointer (&device->styli, g_array_unref);
	g_clear_pointer (&device->deprecated_styli_ids, g_array_unref);
	g_clear_pointer (&device->status_leds, g_array_unref);
	g_clear_pointer (&device->buttons, g_hash_table_destroy);
	g_free (device);

	return NULL;
}

LIBWACOM_EXPORT void
libwacom_destroy(WacomDevice *device)
{
	libwacom_unref(device);
}

WacomMatch*
libwacom_match_ref(WacomMatch *match)
{
	g_atomic_int_inc(&match->refcnt);
	return match;
}

WacomMatch*
libwacom_match_unref(WacomMatch *match)
{
	if (match == NULL ||
	    !g_atomic_int_dec_and_test(&match->refcnt))
		return NULL;

	g_free (match->match);
	g_free (match->name);
	g_free (match->uniq);
	g_free (match);

	return NULL;
}

WacomMatch*
libwacom_match_new(const char *name, const char *uniq,
		   WacomBusType bus,
		   int vendor_id, int product_id)
{
	WacomMatch *match;
	char *newmatch;

	match = g_malloc(sizeof(*match));
	match->refcnt = 1;
	if (name == NULL && bus == WBUSTYPE_UNKNOWN && vendor_id == 0 && product_id == 0)
		newmatch = g_strdup("generic");
	else
		newmatch = make_match_string(name, uniq, bus, vendor_id, product_id);

	match->match = newmatch;
	match->name = g_strdup(name);
	match->uniq = g_strdup(uniq);
	match->bus = bus;
	match->vendor_id = vendor_id;
	match->product_id = product_id;

	return match;
}

LIBWACOM_EXPORT WacomBuilder*
libwacom_builder_new(void)
{
	WacomBuilder *builder = g_malloc0(sizeof(*builder));
	return builder;
}

LIBWACOM_EXPORT void
libwacom_builder_destroy(WacomBuilder *builder)
{
	g_free (builder->device_name);
	g_free (builder->match_name);
	g_free (builder->uniq);
	g_free (builder);
}

LIBWACOM_EXPORT void
libwacom_builder_set_bustype(WacomBuilder *builder, WacomBusType bustype)
{
	builder->bus = bustype;
}

LIBWACOM_EXPORT void
libwacom_builder_set_usbid(WacomBuilder *builder, int vendor_id, int product_id)
{
	builder->vendor_id = vendor_id;
	builder->product_id = product_id;
}

LIBWACOM_EXPORT void
libwacom_builder_set_device_name(WacomBuilder *builder, const char *name)
{
	g_free(builder->device_name);
	builder->device_name = g_strdup(name);
}

LIBWACOM_EXPORT void
libwacom_builder_set_match_name(WacomBuilder *builder, const char *name)
{
	g_free(builder->match_name);
	builder->match_name = g_strdup(name);
}

LIBWACOM_EXPORT void
libwacom_builder_set_uniq(WacomBuilder *builder, const char *uniq)
{
	g_free(builder->uniq);
	builder->uniq = g_strdup(uniq);
}

void
libwacom_add_match(WacomDevice *device, WacomMatch *newmatch)
{
	for (guint i = 0; i < device->matches->len; i++) {
		WacomMatch *m = g_array_index(device->matches, WacomMatch *, i);
		const char *matchstr = libwacom_match_get_match_string(m);

		if (g_str_equal(matchstr, newmatch->match)) {
			return;
		}
	}
	libwacom_match_ref(newmatch);
	g_array_append_val(device->matches, newmatch);
}

void
libwacom_set_default_match(WacomDevice *device, WacomMatch *newmatch)
{
	for (guint i = 0; i < device->matches->len; i++) {
		WacomMatch *m = g_array_index(device->matches, WacomMatch *, i);

		if (match_is_equal(m, newmatch)) {
			libwacom_match_unref(device->match);
			device->match = libwacom_match_ref(m);
			return;
		}
	}
	g_return_if_reached();
}

void
libwacom_remove_match(WacomDevice *device, WacomMatch *to_remove)
{
	for (guint i= 0; i < device->matches->len; i++) {
		WacomMatch *m = g_array_index(device->matches, WacomMatch*, i);
		if (match_is_equal(m, to_remove)) {
			WacomMatch *dflt = device->match;

			/* remove from list */
			g_array_remove_index(device->matches, i);

			/* now reset the default match if needed */
			if (match_is_equal(dflt, to_remove)) {
				WacomMatch *first = g_array_index(device->matches,
							      WacomMatch*,
							      0);
				libwacom_set_default_match(device, first);
			}

			libwacom_match_unref(to_remove);
			break;
		}
	}
}

LIBWACOM_EXPORT int
libwacom_get_vendor_id(const WacomDevice *device)
{
	g_return_val_if_fail(device->match, -1);
	return device->match->vendor_id;
}

LIBWACOM_EXPORT const char*
libwacom_get_name(const WacomDevice *device)
{
	return device->name;
}

LIBWACOM_EXPORT const char*
libwacom_get_model_name(const WacomDevice *device)
{
	return device->model_name;
}

LIBWACOM_EXPORT const char*
libwacom_get_layout_filename(const WacomDevice *device)
{
	return device->layout;
}

LIBWACOM_EXPORT int
libwacom_get_product_id(const WacomDevice *device)
{
	g_return_val_if_fail(device->match, -1);
	return device->match->product_id;
}

LIBWACOM_EXPORT const char*
libwacom_get_match(const WacomDevice *device)
{
	g_return_val_if_fail(device->match, NULL);
	return device->match->match;
}

LIBWACOM_EXPORT const WacomMatch**
libwacom_get_matches(const WacomDevice *device)
{
	return (const WacomMatch**)device->matches->data;
}

LIBWACOM_EXPORT const WacomMatch*
libwacom_get_paired_device(const WacomDevice *device)
{
	return (const WacomMatch*)device->paired;
}

LIBWACOM_EXPORT int
libwacom_get_width(const WacomDevice *device)
{
	return device->width;
}

LIBWACOM_EXPORT int
libwacom_get_height(const WacomDevice *device)
{
	return device->height;
}

LIBWACOM_EXPORT WacomClass
libwacom_get_class(const WacomDevice *device)
{
	if (device->cls != WCLASS_UNKNOWN) {
		return device->cls;
	}

	switch (device->integration_flags) {
	case WACOM_DEVICE_INTEGRATED_DISPLAY:
		return WCLASS_CINTIQ;
	case WACOM_DEVICE_INTEGRATED_DISPLAY|WACOM_DEVICE_INTEGRATED_SYSTEM:
		return WCLASS_CINTIQ;
	case WACOM_DEVICE_INTEGRATED_REMOTE:
		return WCLASS_REMOTE;
	}

	return WCLASS_BAMBOO;
}

LIBWACOM_EXPORT int
libwacom_has_stylus(const WacomDevice *device)
{
	return !!(device->features & FEATURE_STYLUS);
}

LIBWACOM_EXPORT int
libwacom_has_touch(const WacomDevice *device)
{
	return !!(device->features & FEATURE_TOUCH);
}

LIBWACOM_EXPORT int
libwacom_get_num_buttons(const WacomDevice *device)
{
	return g_hash_table_size(device->buttons);
}

LIBWACOM_EXPORT int
libwacom_get_num_keys(const WacomDevice *device)
{
	return device->num_keycodes;
}

LIBWACOM_EXPORT const int *
libwacom_get_supported_styli(const WacomDevice *device, int *num_styli)
{
	*num_styli = device->deprecated_styli_ids->len;
	return (const int *)device->deprecated_styli_ids->data;
}

LIBWACOM_EXPORT const WacomStylus **
libwacom_get_styli(const WacomDevice *device, int *num_styli)
{
	int count = device->styli->len;
	const WacomStylus **styli = g_new0(const WacomStylus*, count + 1);

	if (count > 0)
		memcpy(styli, device->styli->data, count * sizeof(WacomStylus*));

	if (num_styli)
		*num_styli = count;

	return styli;
}

LIBWACOM_EXPORT int
libwacom_has_ring(const WacomDevice *device)
{
	return device->num_rings >= 1;
}

LIBWACOM_EXPORT int
libwacom_has_ring2(const WacomDevice *device)
{
	return device->num_rings >= 2;
}

LIBWACOM_EXPORT int
libwacom_get_num_rings(const WacomDevice *device)
{
	return device->num_rings;
}

LIBWACOM_EXPORT int
libwacom_get_ring_num_modes(const WacomDevice *device)
{
	return device->ring_num_modes;
}

LIBWACOM_EXPORT int
libwacom_get_ring2_num_modes(const WacomDevice *device)
{
	return device->ring2_num_modes;
}

LIBWACOM_EXPORT int
libwacom_get_num_strips(const WacomDevice *device)
{
	return device->num_strips;
}

LIBWACOM_EXPORT int
libwacom_get_strips_num_modes(const WacomDevice *device)
{
	return device->strips_num_modes;
}

LIBWACOM_EXPORT int
libwacom_get_num_dials(const WacomDevice *device)
{
	return device->num_dials;
}

LIBWACOM_EXPORT int
libwacom_get_dial_num_modes(const WacomDevice *device)
{
	return device->dial_num_modes;
}

LIBWACOM_EXPORT int
libwacom_get_dial2_num_modes(const WacomDevice *device)
{
	return device->dial2_num_modes;
}

LIBWACOM_EXPORT const WacomStatusLEDs *
libwacom_get_status_leds(const WacomDevice *device, int *num_leds)
{
	*num_leds = device->status_leds->len;
	return (const WacomStatusLEDs*)device->status_leds->data;
}

static const struct {
	WacomButtonFlags button_flags;
	WacomStatusLEDs  status_leds;
} button_status_leds[] = {
	{ WACOM_BUTTON_RING_MODESWITCH,		WACOM_STATUS_LED_RING },
	{ WACOM_BUTTON_RING2_MODESWITCH,	WACOM_STATUS_LED_RING2 },
	{ WACOM_BUTTON_TOUCHSTRIP_MODESWITCH,	WACOM_STATUS_LED_TOUCHSTRIP },
	{ WACOM_BUTTON_TOUCHSTRIP2_MODESWITCH,	WACOM_STATUS_LED_TOUCHSTRIP2 },
	{ WACOM_BUTTON_DIAL_MODESWITCH,		WACOM_STATUS_LED_DIAL },
	{ WACOM_BUTTON_DIAL2_MODESWITCH,	WACOM_STATUS_LED_DIAL2 },
};

LIBWACOM_EXPORT int
libwacom_get_button_led_group (const WacomDevice *device, char button)
{
	WacomButton *b = g_hash_table_lookup(device->buttons,
					     GINT_TO_POINTER(button));

	if (!(b->flags & WACOM_BUTTON_MODESWITCH))
		return -1;

	for (guint led_index = 0; led_index < device->status_leds->len; led_index++) {
		guint n;

		for (n = 0; n < G_N_ELEMENTS (button_status_leds); n++) {
			WacomStatusLEDs led = g_array_index(device->status_leds,
							    WacomStatusLEDs,
							    led_index);
			if ((b->flags & button_status_leds[n].button_flags) &&
			    (led == button_status_leds[n].status_leds)) {
				return led_index;
			}
		}
	}

	return -1;
}

LIBWACOM_EXPORT int
libwacom_is_builtin(const WacomDevice *device)
{
	return !!(libwacom_get_integration_flags (device) & WACOM_DEVICE_INTEGRATED_DISPLAY);
}

LIBWACOM_EXPORT int
libwacom_is_reversible(const WacomDevice *device)
{
	return !!(device->features & FEATURE_REVERSIBLE);
}

LIBWACOM_EXPORT int
libwacom_has_touchswitch(const WacomDevice *device)
{
	return !!(device->features & FEATURE_TOUCHSWITCH);
}

LIBWACOM_EXPORT WacomIntegrationFlags
libwacom_get_integration_flags (const WacomDevice *device)
{
	/* "unset" is for internal use only */
	if (device->integration_flags == WACOM_DEVICE_INTEGRATED_UNSET)
		return WACOM_DEVICE_INTEGRATED_NONE;

	return device->integration_flags;
}

LIBWACOM_EXPORT WacomBusType
libwacom_get_bustype(const WacomDevice *device)
{
	g_return_val_if_fail(device->match, -1);
	return device->match->bus;
}

LIBWACOM_EXPORT WacomButtonFlags
libwacom_get_button_flag(const WacomDevice *device, char button)
{
	WacomButton *b = g_hash_table_lookup(device->buttons,
					     GINT_TO_POINTER(button));


	return b ? b->flags : WACOM_BUTTON_NONE;
}

LIBWACOM_EXPORT int
libwacom_get_button_evdev_code(const WacomDevice *device, char button)
{
	WacomButton *b = g_hash_table_lookup(device->buttons,
					     GINT_TO_POINTER(button));

	return b ? b->code : 0;
}

LIBWACOM_EXPORT WacomModeSwitch
libwacom_get_button_modeswitch_mode(const WacomDevice *device, char button)
{
	WacomButton *b = g_hash_table_lookup(device->buttons,
					     GINT_TO_POINTER(button));

	if (!b || (b->flags & WACOM_BUTTON_MODESWITCH) == 0)
		return WACOM_MODE_SWITCH_NEXT;

	return b->mode;
}

static const WacomStylus *
libwacom_stylus_get_for_stylus_id (const WacomDeviceDatabase *db,
				   const WacomStylusId *id)
{
	return g_hash_table_lookup (db->stylus_ht, id);
}

LIBWACOM_EXPORT const WacomStylus *
libwacom_stylus_get_for_id (const WacomDeviceDatabase *db, int tool_id)
{
	WacomStylusId id = {
		.vid = WACOM_VENDOR_ID,
		.tool_id = tool_id,
	};

	switch (tool_id) {
	case GENERIC_PEN_WITH_ERASER:
	case GENERIC_ERASER:
	case GENERIC_PEN_NO_ERASER:
		id.vid = 0;
		break;

	}
	return libwacom_stylus_get_for_stylus_id (db, &id);
}

LIBWACOM_EXPORT int
libwacom_stylus_get_id (const WacomStylus *stylus)
{
	return stylus->id.tool_id;
}

LIBWACOM_EXPORT int
libwacom_stylus_get_vendor_id (const WacomStylus *stylus)
{
	return stylus->id.vid;
}

LIBWACOM_EXPORT const char *
libwacom_stylus_get_name (const WacomStylus *stylus)
{
	return stylus->name;
}

LIBWACOM_EXPORT const int *
libwacom_stylus_get_paired_ids(const WacomStylus *stylus, int *num_paired_ids)
{
	if (num_paired_ids)
		*num_paired_ids = stylus->deprecated_paired_ids->len;
	return (const int*)stylus->deprecated_paired_ids->data;
}

LIBWACOM_EXPORT const WacomStylus **
libwacom_stylus_get_paired_styli(const WacomStylus *stylus, int *num_paired)
{
	int count = stylus->paired_styli->len;
	const WacomStylus **styli = g_new0(const WacomStylus*, count + 1);

	if (num_paired)
		*num_paired = count;

	if (count > 0)
		memcpy(styli, stylus->paired_styli->data, count * sizeof(WacomStylus*));
	return styli;
}

LIBWACOM_EXPORT int
libwacom_stylus_get_num_buttons (const WacomStylus *stylus)
{
	if (stylus->num_buttons == -1) {
		g_warning ("Stylus '0x%x' has no number of buttons defined, falling back to 2", stylus->id.tool_id);
		return 2;
	}
	return stylus->num_buttons;
}

LIBWACOM_EXPORT int
libwacom_stylus_has_eraser (const WacomStylus *stylus)
{
	return stylus->has_eraser;
}

LIBWACOM_EXPORT int
libwacom_stylus_is_eraser (const WacomStylus *stylus)
{
	return libwacom_stylus_get_eraser_type(stylus) != WACOM_ERASER_NONE;
}

LIBWACOM_EXPORT int
libwacom_stylus_has_lens (const WacomStylus *stylus)
{
	return stylus->has_lens;
}

LIBWACOM_EXPORT int
libwacom_stylus_has_wheel (const WacomStylus *stylus)
{
	return stylus->has_wheel;
}

LIBWACOM_EXPORT WacomAxisTypeFlags
libwacom_stylus_get_axes (const WacomStylus *stylus)
{
	return stylus->axes;
}

LIBWACOM_EXPORT WacomStylusType
libwacom_stylus_get_type (const WacomStylus *stylus)
{
	if (stylus->type == WSTYLUS_UNKNOWN) {
		g_warning ("Stylus '0x%x' has no type defined, falling back to 'General'", stylus->id.tool_id);
		return WSTYLUS_GENERAL;
	}
	return stylus->type;
}

LIBWACOM_EXPORT WacomEraserType
libwacom_stylus_get_eraser_type (const WacomStylus *stylus)
{
	return stylus->eraser_type;
}

LIBWACOM_EXPORT void
libwacom_print_stylus_description (int fd, const WacomStylus *stylus)
{
	const char *type;
	WacomAxisTypeFlags axes;
	g_autofree const WacomStylus **paired;
	int count;
	int i;

	if (libwacom_stylus_get_vendor_id(stylus) != WACOM_VENDOR_ID)
		dprintf(fd, "[0x%x:%#x]\n", libwacom_stylus_get_vendor_id(stylus), libwacom_stylus_get_id(stylus));
	else
		dprintf(fd, "[%#x]\n", libwacom_stylus_get_id(stylus));

	dprintf(fd, "Name=%s\n",	libwacom_stylus_get_name(stylus));
	dprintf(fd, "PairedIds=");
	paired = libwacom_stylus_get_paired_styli(stylus, &count);
	for (i = 0; i < count; i++) {
		if (paired[i]->id.vid != 0x56a)
			dprintf(fd, "%#04x:%#x;", paired[i]->id.vid, paired[i]->id.tool_id);
		else
			dprintf(fd, "%#x;", paired[i]->id.tool_id);
	}
	dprintf(fd, "\n");
	switch (libwacom_stylus_get_eraser_type(stylus)) {
		case WACOM_ERASER_UNKNOWN: type = "Unknown";       break;
		case WACOM_ERASER_NONE:    type = "None";          break;
		case WACOM_ERASER_INVERT:  type = "Invert";        break;
		case WACOM_ERASER_BUTTON:  type = "Button";        break;
		default:                   g_assert_not_reached(); break;
	}
	dprintf(fd, "EraserType=%s\n", type);
	dprintf(fd, "HasLens=%s\n",	libwacom_stylus_has_lens(stylus) ? "true" : "false");
	dprintf(fd, "HasWheel=%s\n",	libwacom_stylus_has_wheel(stylus) ? "true" : "false");
	axes = libwacom_stylus_get_axes(stylus);
	dprintf(fd, "Axes=");
	if (axes & WACOM_AXIS_TYPE_TILT)
		dprintf(fd, "Tilt;");
	if (axes & WACOM_AXIS_TYPE_ROTATION_Z)
		dprintf(fd, "RotationZ;");
	if (axes & WACOM_AXIS_TYPE_DISTANCE)
		dprintf(fd, "Distance;");
	if (axes & WACOM_AXIS_TYPE_PRESSURE)
		dprintf(fd, "Pressure;");
	if (axes & WACOM_AXIS_TYPE_SLIDER)
		dprintf(fd, "Slider;");
	dprintf(fd, "\n");

	switch(libwacom_stylus_get_type(stylus)) {
		case WSTYLUS_UNKNOWN:	type = "Unknown";	 break;
		case WSTYLUS_GENERAL:	type = "General";	 break;
		case WSTYLUS_INKING:	type = "Inking";	 break;
		case WSTYLUS_AIRBRUSH:	type = "Airbrush";	 break;
		case WSTYLUS_CLASSIC:	type = "Classic";	 break;
		case WSTYLUS_MARKER:	type = "Marker";	 break;
		case WSTYLUS_STROKE:	type = "Stroke";	 break;
		case WSTYLUS_PUCK:	type = "Puck";		break;
		case WSTYLUS_3D:	type = "3D";		break;
		case WSTYLUS_MOBILE:	type = "Mobile";	break;
		default:		g_assert_not_reached();	break;
	}

	dprintf(fd, "Type=%s\n", type);
}

WacomStylus*
libwacom_stylus_ref(WacomStylus *stylus)
{
	g_atomic_int_inc(&stylus->refcnt);

	return stylus;
}

WacomStylus*
libwacom_stylus_unref(WacomStylus *stylus)
{
	if (!g_atomic_int_dec_and_test(&stylus->refcnt))
		return NULL;

	g_free (stylus->name);
	g_free (stylus->group);
	g_clear_pointer (&stylus->deprecated_paired_ids, g_array_unref);
	g_clear_pointer (&stylus->paired_stylus_ids, g_array_unref);
	g_clear_pointer (&stylus->paired_styli, g_array_unref);
	g_free (stylus);

	return NULL;
}

LIBWACOM_EXPORT const char *
libwacom_match_get_name(const WacomMatch *match)
{
	return match->name;
}

LIBWACOM_EXPORT const char *
libwacom_match_get_uniq(const WacomMatch *match)
{
	return match->uniq;
}

LIBWACOM_EXPORT WacomBusType
libwacom_match_get_bustype(const WacomMatch *match)
{
	return match->bus;
}

LIBWACOM_EXPORT uint32_t
libwacom_match_get_product_id(const WacomMatch *match)
{
	return match->product_id;
}

LIBWACOM_EXPORT uint32_t
libwacom_match_get_vendor_id(const WacomMatch *match)
{
	return match->vendor_id;
}

LIBWACOM_EXPORT const char*
libwacom_match_get_match_string(const WacomMatch *match)
{
	return match->match;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
