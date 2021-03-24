/*
 * Copyright © 2008 Kristian Høgsberg
 * Copyright © 2013-2015 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_XLOCALE_H
#include <xlocale.h>
#endif

static inline void *
zalloc(size_t size)
{
	void *p;

	/* We never need to alloc anything more than 1,5 MB so we can assume
	 * if we ever get above that something's going wrong */
	if (size > 1536 * 1024)
		assert(!"bug: internal malloc size limit exceeded");

	p = calloc(1, size);
	if (!p)
		abort();

	return p;
}

/**
 * strdup guaranteed to succeed. If the input string is NULL, the output
 * string is NULL. If the input string is a string pointer, we strdup or
 * abort on failure.
 */
static inline char*
safe_strdup(const char *str)
{
	char *s;

	if (!str)
		return NULL;

	s = strdup(str);
	if (!s)
		abort();
	return s;
}

/**
 * Simple wrapper for asprintf that ensures the passed in-pointer is set
 * to NULL upon error.
 * The standard asprintf() call does not guarantee the passed in pointer
 * will be NULL'ed upon failure, whereas this wrapper does.
 *
 * @param strp pointer to set to newly allocated string.
 * This pointer should be passed to free() to release when done.
 * @param fmt the format string to use for printing.
 * @return The number of bytes printed (excluding the null byte terminator)
 * upon success or -1 upon failure. In the case of failure the pointer is set
 * to NULL.
 */
__attribute__ ((format (printf, 2, 3)))
static inline int
xasprintf(char **strp, const char *fmt, ...)
{
	int rc = 0;
	va_list args;

	va_start(args, fmt);
	rc = vasprintf(strp, fmt, args);
	va_end(args);
	if ((rc == -1) && strp)
		*strp = NULL;

	return rc;
}

static inline bool
safe_atoi_base(const char *str, int *val, int base)
{
	char *endptr;
	long v;

	assert(base == 10 || base == 16 || base == 8);

	errno = 0;
	v = strtol(str, &endptr, base);
	if (errno > 0)
		return false;
	if (str == endptr)
		return false;
	if (*str != '\0' && *endptr != '\0')
		return false;

	if (v > INT_MAX || v < INT_MIN)
		return false;

	*val = v;
	return true;
}

static inline bool
safe_atoi(const char *str, int *val)
{
	return safe_atoi_base(str, val, 10);
}

static inline bool
safe_atou_base(const char *str, unsigned int *val, int base)
{
	char *endptr;
	unsigned long v;

	assert(base == 10 || base == 16 || base == 8);

	errno = 0;
	v = strtoul(str, &endptr, base);
	if (errno > 0)
		return false;
	if (str == endptr)
		return false;
	if (*str != '\0' && *endptr != '\0')
		return false;

	if ((long)v < 0)
		return false;

	*val = v;
	return true;
}

static inline bool
safe_atou(const char *str, unsigned int *val)
{
	return safe_atou_base(str, val, 10);
}

static inline bool
safe_atod(const char *str, double *val)
{
	char *endptr;
	double v;
#ifdef HAVE_LOCALE_H
	locale_t c_locale;
#endif
	size_t slen = strlen(str);

	/* We don't have a use-case where we want to accept hex for a double
	 * or any of the other values strtod can parse */
	for (size_t i = 0; i < slen; i++) {
		char c = str[i];

		if (isdigit(c))
		       continue;
		switch(c) {
		case '+':
		case '-':
		case '.':
			break;
		default:
			return false;
		}
	}

#ifdef HAVE_LOCALE_H
	/* Create a "C" locale to force strtod to use '.' as separator */
	c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
	if (c_locale == (locale_t)0)
		return false;

	errno = 0;
	v = strtod_l(str, &endptr, c_locale);
	freelocale(c_locale);
#else
	/* No locale support in provided libc, assume it already uses '.' */
	errno = 0;
	v = strtod(str, &endptr);
#endif
	if (errno > 0)
		return false;
	if (str == endptr)
		return false;
	if (*str != '\0' && *endptr != '\0')
		return false;
	if (v != 0.0 && !isnormal(v))
		return false;

	*val = v;
	return true;
}

char **strv_from_string(const char *string, const char *separator);
char *strv_join(char **strv, const char *separator);

static inline void
strv_free(char **strv) {
	char **s = strv;

	if (!strv)
		return;

	while (*s != NULL) {
		free(*s);
		*s = (char*)0x1; /* detect use-after-free */
		s++;
	}

	free (strv);
}

struct key_value_str{
	char *key;
	char *value;
};

struct key_value_double {
	double key;
	double value;
};

static inline ssize_t
kv_double_from_string(const char *string,
		      const char *pair_separator,
		      const char *kv_separator,
		      struct key_value_double **result_out)

{
	char **pairs;
	char **pair;
	struct key_value_double *result = NULL;
	ssize_t npairs = 0;
	unsigned int idx = 0;

	if (!pair_separator || pair_separator[0] == '\0' ||
	    !kv_separator || kv_separator[0] == '\0')
		return -1;

	pairs = strv_from_string(string, pair_separator);
	if (!pairs)
		return -1;

	for (pair = pairs; *pair; pair++)
		npairs++;

	if (npairs == 0)
		goto error;

	result = zalloc(npairs * sizeof *result);

	for (pair = pairs; *pair; pair++) {
		char **kv = strv_from_string(*pair, kv_separator);
		double k, v;

		if (!kv || !kv[0] || !kv[1] || kv[2] ||
		    !safe_atod(kv[0], &k) ||
		    !safe_atod(kv[1], &v)) {
			strv_free(kv);
			goto error;
		}

		result[idx].key = k;
		result[idx].value = v;
		idx++;

		strv_free(kv);
	}

	strv_free(pairs);

	*result_out = result;

	return npairs;

error:
	strv_free(pairs);
	free(result);
	return -1;
}
