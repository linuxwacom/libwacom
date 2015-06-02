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

#define _GNU_SOURCE

#include "libwacomint.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

WacomError*
libwacom_error_new(void)
{
	WacomError *error = malloc(sizeof(*error));
	error->code = WERROR_NONE;
	error->msg = NULL;
	return error;
}

void
libwacom_error_free(WacomError **error)
{
	free((*error)->msg);
	free(*error);
	*error = NULL;
}

enum WacomErrorCode
libwacom_error_get_code(WacomError *error)
{
	return error->code;
}

const char*
libwacom_error_get_message(WacomError *error)
{
	return error->msg;
}

/* INTERNAL */
void
libwacom_error_set(WacomError *error, enum WacomErrorCode code, const char *msg, ...)
{
	if (!error)
		return;

	error->code = code;
	if (msg) {
		va_list ap;
		va_start(ap, msg);
		if (vasprintf(&error->msg, msg, ap) == -1)
			error->msg = NULL;
		va_end(ap);
	}
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
