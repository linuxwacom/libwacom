/*
 * Copyright © 2019 Red Hat, Inc.
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
 */

#include "config.h"

#define _GNU_SOURCE
#include <stdio.h>
#include <libwacom.h>

/* Deprecated symbols. Up to including libwacom 0.33, these symbols ended up
 * in the public ABI.But not the API, the declarations were always in an
 * internal header only.
 *
 * All of these have no use for the caller and in some cases only serve to
 * corrupt memory if the caller actually invokes them. There aren't any
 * legitimate users out there, but let's be safe anyway. For backwards
 * compatibility, we create noop functions for those and alias
 * them to the previously exposed public symbols.
 *
 * If the soname is ever bumped for an incompatible change, we can remove
 * all these.
 */
#define LIBWACOM_EXPORT __attribute__ ((visibility("default")))

void _match_destroy(WacomMatch *match);
WacomMatch* _match_new(const char *name, WacomBusType bus,
                       int vendor_id, int product_id);
void _error_set(WacomError *error, enum WacomErrorCode code,
                const char *msg, ...);
void _stylus_destroy(WacomStylus *stylus);
void _update_match(WacomDevice *device, const WacomMatch *match);

LIBWACOM_EXPORT void
_match_destroy(WacomMatch *match)
{
}
asm(".symver _match_destroy,libwacom_match_destroy@LIBWACOM_0.33");


LIBWACOM_EXPORT WacomMatch*
_match_new(const char *name, WacomBusType bus, int vendor_id, int product_id)
{
	return NULL;
}
asm(".symver _match_new,libwacom_match_new@LIBWACOM_0.33");

LIBWACOM_EXPORT void
_error_set(WacomError *error, enum WacomErrorCode code, const char *msg, ...)
{
}
asm(".symver _error_set,libwacom_error_set@LIBWACOM_0.33");

LIBWACOM_EXPORT void
_stylus_destroy(WacomStylus *stylus)
{
}
asm(".symver _stylus_destroy,libwacom_stylus_destroy@LIBWACOM_0.33");

LIBWACOM_EXPORT void
_update_match(WacomDevice *device, const WacomMatch *newmatch)
{
}
asm(".symver _update_match,libwacom_update_match@LIBWACOM_0.33");
