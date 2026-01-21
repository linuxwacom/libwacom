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

#ifndef _LIBWACOMINT_H_
#define _LIBWACOMINT_H_

#include <glib.h>
#include <stdint.h>

#include "libwacom.h"

#define LIBWACOM_EXPORT __attribute__ ((visibility("default")))

#define DBG(...) \
	printf(__VA_ARGS__)

#define GENERIC_DEVICE_MATCH "generic"
#define WACOM_DEVICE_INTEGRATED_UNSET (WACOM_DEVICE_INTEGRATED_NONE - 1U)
#define WACOM_VENDOR_ID 0x056a

enum GenericStylus {
	GENERIC_PEN_WITH_ERASER = 0xfffff,
	GENERIC_ERASER = 0xffffe,
	GENERIC_PEN_NO_ERASER = 0xffffd,
};

enum WacomFeature {
	FEATURE_STYLUS = (1 << 0),
	FEATURE_TOUCH = (1 << 1),
	FEATURE_REVERSIBLE = (1 << 4),
	FEATURE_TOUCHSWITCH = (1 << 5)
};

struct _WacomBuilder {
	char *device_name;
	char *match_name;
	char *uniq;
	WacomBusType bus;
	uint32_t vendor_id;
	uint32_t product_id;
};

/* WARNING: When adding new members to this struct
 * make sure to update libwacom_copy_match() ! */
struct _WacomMatch {
	gint refcnt;
	char *match;
	char *name;
	char *uniq;
	WacomBusType bus;
	uint32_t vendor_id;
	uint32_t product_id;
};

/* Used in the device->buttons hashtable */
typedef struct _WacomButton {
	WacomButtonFlags flags;
	int code;
	WacomModeSwitch mode;
} WacomButton;

typedef struct _WacomKeycode {
	unsigned int type;
	unsigned int code;
} WacomKeycode;

/* WARNING: When adding new members to this struct
 * make sure to update libwacom_copy() and
 * libwacom_print_device_description() ! */
struct _WacomDevice {
	char *name;
	char *model_name;
	int width;
	int height;

	WacomMatch *match; /* used match or first match by default */
	GArray *matches;

	WacomMatch *paired;

	WacomClass cls;
	int num_strips;
	int num_rings;
	int num_dials;
	uint32_t features;
	uint32_t integration_flags;

	int strips_num_modes;
	int dial_num_modes;
	int dial2_num_modes;
	int ring_num_modes;
	int ring2_num_modes;

	/* for libwacom_get_supported_styli() */
	GArray *deprecated_styli_ids; /* int */
	/* for libwacom_get_styli() */
	GArray *styli;       /* WacomStylus* */
	GHashTable *buttons; /* 'A' : WacomButton */
	WacomKeycode keycodes[32];
	size_t num_keycodes;

	GArray *status_leds;

	char *layout;

	gint refcnt; /* for the db hashtable */
};

typedef struct _WacomStylusId {
	unsigned int vid;
	unsigned int tool_id;
} WacomStylusId;

struct _WacomStylus {
	gint refcnt;
	WacomStylusId id;
	char *name;
	char *group;
	int num_buttons;
	gboolean has_eraser;
	gboolean is_generic_stylus;
	GArray *paired_styli;          /* [WacomStylus*, ...] */
	GArray *deprecated_paired_ids; /* [int, ...] */
	GArray *paired_stylus_ids;     /* [WacomStylusId, ...], NULL once parsing is
					  complete */
	WacomEraserType eraser_type;
	gboolean has_lens;
	gboolean has_wheel;
	WacomStylusType type;
	WacomAxisTypeFlags axes;
};

struct _WacomDeviceDatabase {
	GHashTable *device_ht; /* key = DeviceMatch (str), value = WacomDeviceData * */
	GHashTable *stylus_ht; /* key = WacomStylusId, value = WacomStylus * */
};

struct _WacomError {
	enum WacomErrorCode code;
	char *msg;
};

WacomDevice *
libwacom_ref(WacomDevice *device);
WacomDevice *
libwacom_unref(WacomDevice *device);
WacomStylus *
libwacom_stylus_ref(WacomStylus *stylus);
WacomStylus *
libwacom_stylus_unref(WacomStylus *stylus);
WacomMatch *
libwacom_match_ref(WacomMatch *match);
WacomMatch *
libwacom_match_unref(WacomMatch *match);

void
libwacom_error_set(WacomError *error,
		   enum WacomErrorCode code,
		   const char *msg,
		   ...);
void
libwacom_add_match(WacomDevice *device,
		   WacomMatch *newmatch);
void
libwacom_set_default_match(WacomDevice *device,
			   WacomMatch *newmatch);
void
libwacom_remove_match(WacomDevice *device,
		      WacomMatch *newmatch);
WacomMatch *
libwacom_match_new(const char *name,
		   const char *uniq,
		   WacomBusType bus,
		   int vendor_id,
		   int product_id);

WacomBusType
bus_from_str(const char *str);
const char *
bus_to_str(WacomBusType bus);
char *
make_match_string(const char *name,
		  const char *uniq,
		  WacomBusType bus,
		  int vendor_id,
		  int product_id);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(WacomMatch,
			      libwacom_match_unref);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(WacomDevice,
			      libwacom_unref);

#endif /* _LIBWACOMINT_H_ */

/* vim: set noexpandtab shiftwidth=8: */
