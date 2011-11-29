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

#define DBG(...) \
	printf(__VA_ARGS__)

enum WacomFeature {
	FEATURE_STYLUS		= (1 << 0),
	FEATURE_TOUCH		= (1 << 1),
	FEATURE_RING		= (1 << 2),
	FEATURE_RING2		= (1 << 3),
	FEATURE_VSTRIP		= (1 << 4),
	FEATURE_HSTRIP		= (1 << 5),
	FEATURE_BUILTIN		= (1 << 6),
};

typedef struct _WacomDeviceData {
	char *vendor;
	char *product;
	char *model;
	int width;
	int height;

	uint32_t vendor_id;
	uint32_t product_id;

	enum WacomClass cls;
	enum WacomBusType bus;
	int num_buttons;
	uint32_t features;
} WacomDeviceData;

struct _WacomDevice {
	WacomDeviceData *ref; /* points to the matching element in the database */
	WacomDeviceData **database;
	int nentries;
};

struct _WacomError {
	enum WacomErrorCode code;
	char *msg;
};

/* INTERNAL */
WacomDeviceData* libwacom_new_devicedata(void);
int libwacom_load_database(WacomDevice* device);
void libwacom_error_set(WacomError *error, enum WacomErrorCode code, const char *msg, ...);

#endif /* _LIBWACOMINT_H_ */

/* vim: set noexpandtab shiftwidth=8: */
