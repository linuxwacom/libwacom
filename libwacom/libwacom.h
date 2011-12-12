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


#ifndef _LIBWACOM_H_
#define _LIBWACOM_H_

/**
 @mainpage

 @section Introduction

 libwacom is a library to identify wacom tablets and their model-specific
 features. It provides easy access to information such as "is this a
 built-in on-screen tablet", "what is the size of this model", etc.

 @section Usage
 The usage of libwacom in an application could look like this:

 <pre>
      WacomDevice *device;

      device = libwacom_new_from_path("/dev/input/event0");
      if (!device)
           return; // should check for error here

      if (libwacom_device_is_builtin(device))
           printf("This is a built-in device\n");

      libwacom_destroy(&device);
 </pre>

 For a full API reference to see libwacom.h.

 @section Database

 libwacom comes with a database of models and their features in key-value
 format. If you cannot use libwacom, the files may be parsed directly. Note
 that the file format may change over time, especially in the beginning.
 */

/**
 @file libwacom.h
 */

typedef struct _WacomDevice WacomDevice;

typedef struct _WacomStylus WacomStylus;

typedef struct _WacomError WacomError;

typedef struct _WacomDeviceDatabase WacomDeviceDatabase;

#define WACOM_STYLUS_FALLBACK_ID 0xfffff
#define WACOM_ERASER_FALLBACK_ID 0xffffe

/**
 * Possible error codes.
 */
enum WacomErrorCode {
    WERROR_NONE,		/**< No error has occured */
    WERROR_BAD_ALLOC,		/**< Allocation error */
    WERROR_INVALID_PATH,	/**< A path specified is invalid */
    WERROR_INVALID_DB,		/**< The passed DB is invalid */
    WERROR_BAD_ACCESS,		/**< Invalid permissions to access the path */
    WERROR_UNKNOWN_MODEL,	/**< Unsupported/unknown device */
};

/**
 * Bus types for tablets.
 */
typedef enum {
    WBUSTYPE_UNKNOWN,		/**< Unknown/unsupported bus type */
    WBUSTYPE_USB,		/**< USB tablet */
    WBUSTYPE_SERIAL,		/**< Serial tablet */
    WBUSTYPE_BLUETOOTH		/**< Bluetooth tablet */
} WacomBusType;

/**
 * Classes of devices.
 */
typedef enum {
    WCLASS_UNKNOWN,		/**< Unknown/unsupported device class */
    WCLASS_INTUOS3,		/**< Any Intuos3 series */
    WCLASS_INTUOS4,		/**< Any Intuos4 series */
    WCLASS_CINTIQ,		/**< Any Cintiq device */
    WCLASS_BAMBOO,		/**< Any Bamboo device */
    WCLASS_GRAPHIRE,		/**< Any Graphire device */
    WCLASS_ISDV4,		/**< Any serial ISDV4 device */
} WacomClass;

/**
 * Class of stylus
 */
typedef enum {
    WSTYLUS_UNKNOWN,
    WSTYLUS_GENERAL,
    WSTYLUS_INKING,
    WSTYLUS_AIRBRUSH
} WacomStylusType;

/**
 * Allocate a new structure for error reporting.
 *
 * @return A newly allocated error structure or NULL if the allocation
 * failed.
 */
WacomError* libwacom_error_new(void);

/**
 * Free the error and associated memory.
 * Resets error to NULL.
 *
 * @param error A reference to a error struct.
 * @see libwacom_error_new
 */
void libwacom_error_free(WacomError **error);

/**
 * @return The code for this error.
 */
enum WacomErrorCode libwacom_error_get_code(WacomError *error);

/**
 * @return A human-readable message for this error
 */
const char* libwacom_error_get_message(WacomError *error);

/**
 * Loads the Tablet and Stylus databases, to be used
 * in libwacom_new_*() functions.
 *
 * @return A new database or NULL on error.
 */
WacomDeviceDatabase* libwacom_database_new(void);

/**
  * Free all memory used by the database.
  *
  * @param db A Tablet and Stylus database.
  */
void libwacom_database_destroy(WacomDeviceDatabase *db);

/**
 * Create a new device reference from the given device path.
 * In case of error, NULL is returned and the error is set to the
 * appropriate value.
 *
 * @param db A device database
 * @param path A device path in the form of e.g. /dev/input/event0
 * @param fallback Whether we should create a generic if model is unknown
 * @param error If not NULL, set to the error if any occurs
 *
 * @return A new reference to this device or NULL on errror.
 */
WacomDevice* libwacom_new_from_path(WacomDeviceDatabase *db, const char *path, int fallback, WacomError *error);

/**
 * Create a new device reference from the given vendor/product IDs.
 * In case of error, NULL is returned and the error is set to the
 * appropriate value.
 *
 * @param db A device database
 * @param vendor_id The vendor ID of the device
 * @param product_id The product ID of the device
 * @param error If not NULL, set to the error if any occurs
 *
 * @return A new reference to this device or NULL on errror.
 */
WacomDevice* libwacom_new_from_usbid(WacomDeviceDatabase *db, int vendor_id, int product_id, WacomError *error);

/**
 * Create a new device reference from the given name.
 * In case of error, NULL is returned and the error is set to the
 * appropriate value.
 *
 * @param db A device database
 * @param name The name identifying the device
 * @param error If not NULL, set to the error if any occurs
 *
 * @return A new reference to this device or NULL on error.
 */
WacomDevice* libwacom_new_from_name(WacomDeviceDatabase *db, const char *name, WacomError *error);

/**
 * Remove the device and free all memory and references to it.
 *
 * @param device The device to delete
 */
void libwacom_destroy(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return The class of the device
 */
WacomClass libwacom_get_class(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return The human-readable vendor for this device
 */
const char* libwacom_get_vendor(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return The numeric vendor ID for this device
 */
int libwacom_get_vendor_id(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return The human-readable product for this device
 */
const char* libwacom_get_product(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return The first match for the device in question
 */
const char* libwacom_get_match(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return The numeric product ID for this device
 */
int libwacom_get_product_id(WacomDevice *device);

/**
 * Retrieve the width of the device. This is the width of the usable area as
 * advertised, not the total size of the physical tablet. For e.g. an
 * Intuos4 6x9 this will return 9.
 *
 * @param device The tablet to query
 * @return The width of this device in inches
 */
int libwacom_get_width(WacomDevice *device);

/**
 * Retrieve the height of the device. This is the height of the usable area as
 * advertised, not the total size of the physical tablet. For e.g. an
 * Intuos4 6x9 this will return 6.
 *
 * @param device The tablet to query
 * @return The width of this device in inches
 */
int libwacom_get_height(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return non-zero if the device supports styli or zero otherwise
 */
int libwacom_has_stylus(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return non-zero if the device supports touch or zero otherwise
 */
int libwacom_has_touch(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return The number of buttons on the tablet
 */
int libwacom_get_num_buttons(WacomDevice *device);

/**
 * @param device The tablet to query
 * @param num_styli Return location for the number of listed styli
 * @return an array of Styli IDs supported by the device
 */
int *libwacom_get_supported_styli(WacomDevice *device, int *num_styli);

/**
 * @param device The tablet to query
 * @return non-zero if the device has a touch ring or zero otherwise
 */
int libwacom_has_ring(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return non-zero if the device has a second touch ring or zero otherwise
 */
int libwacom_has_ring2(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return non-zero if the device has a vertical touch strip or zero
 * otherwise
 */
int libwacom_has_vstrip(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return non-zero if the device has a horizontal touch strip or zero
 * otherwise
 */
int libwacom_has_hstrip(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return non-zero if the device is built-in or zero if the device is an
 * external tablet
 */
int libwacom_is_builtin(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return non-zero if the device can be used left-handed
 * (rotated 180 degrees)
 */
int libwacom_is_reversible(WacomDevice *device);

/**
 * @param device The tablet to query
 * @return The bustype of this device.
 */
WacomBusType libwacom_get_bustype(WacomDevice *device);

/**
 * Get the WacomStylus for the given tool ID.
 *
 * @param db A Tablet and Stylus database.
 * @param id The Tool ID for this stylus
 * @return A WacomStylus representing the stylus. Do not free.
 */
const WacomStylus *libwacom_stylus_get_for_id (WacomDeviceDatabase *db, int id);

/**
 * @param stylus The stylus to query
 * @return the ID of the tool
 */
int         libwacom_stylus_get_id (const WacomStylus *stylus);

/**
 * @param stylus The stylus to query
 * @return The name of the stylus
 */
const char *libwacom_stylus_get_name (const WacomStylus *stylus);

/**
 * @param stylus The stylus to query
 * @return The number of buttons on the stylus
 */
int         libwacom_stylus_get_num_buttons (const WacomStylus *stylus);

/**
 * @param stylus The stylus to query
 * @return Whether the stylus has an eraser
 */
int         libwacom_stylus_has_eraser (const WacomStylus *stylus);

/**
 * @param stylus The stylus to query
 * @return Whether the stylus is actually an eraser
 */
int         libwacom_stylus_is_eraser (const WacomStylus *stylus);

/**
 * @param stylus The stylus to query
 * @return The type of stylus
 */
WacomStylusType libwacom_stylus_get_type (const WacomStylus *stylus);

#endif /* _LIBWACOM_H_ */

/* vim: set noexpandtab shiftwidth=8: */
