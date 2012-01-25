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

#ifndef _LIBWACOMINT_H_
#define _LIBWACOMINT_H_

#include "libwacom.h"
#include <stdint.h>
#include <glib.h>

#define DBG(...) \
	printf(__VA_ARGS__)

#define GENERIC_DEVICE_MATCH "generic"
#define STYLUS_DATA_FILE "libwacom.stylus"

typedef enum {
	IS_BUILTIN_UNSET	= -1,
	IS_BUILTIN_FALSE	= 0,
	IS_BUILTIN_TRUE		= 1
} IsBuiltin;

enum WacomFeature {
	FEATURE_STYLUS		= (1 << 0),
	FEATURE_TOUCH		= (1 << 1),
	FEATURE_RING		= (1 << 2),
	FEATURE_RING2		= (1 << 3),
	FEATURE_VSTRIP		= (1 << 4),
	FEATURE_HSTRIP		= (1 << 5),
	FEATURE_BUILTIN		= (1 << 6),
	FEATURE_REVERSIBLE	= (1 << 7)
};

struct _WacomDevice {
	char *name;
	int width;
	int height;

	char *match;
	uint32_t vendor_id;
	uint32_t product_id;

	WacomClass cls;
	WacomBusType bus;
	int num_buttons;
	int *supported_styli;
	gsize num_styli;
	uint32_t features;
};

struct _WacomStylus {
	int id;
	char *name;
	int num_buttons;
	gboolean has_eraser;
	gboolean is_eraser;
	WacomStylusType type;
};

struct _WacomDeviceDatabase {
	GHashTable *device_ht; /* key = DeviceMatch (str), value = WacomDeviceData * */
	GHashTable *stylus_ht; /* key = ID (int), value = WacomStylus * */
};

struct _WacomError {
	enum WacomErrorCode code;
	char *msg;
};

/* INTERNAL */
void libwacom_error_set(WacomError *error, enum WacomErrorCode code, const char *msg, ...);
void libwacom_stylus_destroy(WacomStylus *stylus);

WacomBusType  bus_from_str (const char *str);
const char   *bus_to_str   (WacomBusType bus);

#endif /* _LIBWACOMINT_H_ */

/* vim: set noexpandtab shiftwidth=8: */
