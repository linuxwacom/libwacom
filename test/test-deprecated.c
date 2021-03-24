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

#include <assert.h>
#include <dlfcn.h>

#include "libwacom.h"

/**
 * The normal symbols are hidden now when linking against a new library.
 * We know which version they're in though so we can remap them for this
 * test - if we get an unresolved symbol something has gone wrong.
 */
asm(".symver libwacom_match_destroy,libwacom_match_destroy@LIBWACOM_0.33");
asm(".symver libwacom_match_new,libwacom_match_new@LIBWACOM_0.33");
asm(".symver libwacom_error_set,libwacom_error_set@LIBWACOM_0.33");
asm(".symver libwacom_update_match,libwacom_update_match@LIBWACOM_0.33");
asm(".symver libwacom_stylus_destroy,libwacom_stylus_destroy@LIBWACOM_0.33");

/* Only generally matches the real functions, but since they're all noops
 * anyway it doesn't matter that we only have generic pointers. The basic
 * signatures are the same.
 */
extern void libwacom_match_destroy(void*);
void* libwacom_match_new(void *, int, int, int);
void libwacom_error_set(void *error, int, char *, ...);
void libwacom_stylus_destroy(void *);
void libwacom_update_match(void *, const void *);

int main(void) {
    const char *syms[] = {
        "libwacom_match_destroy",
        "libwacom_match_new",
        "libwacom_error_set",
        "libwacom_update_match",
        "libwacom_stylus_destroy",
        NULL,
    };
    void *lib, *sym;


    lib = dlopen("libwacom.so", RTLD_LAZY);
    assert(lib != NULL);

    for (const char **s = syms; *s; s++) {
        sym = dlsym(lib, *s);
        assert(sym == NULL);
    }

    /* These are all noops, so all we're looking for is not getting a linker
     * error */
    libwacom_match_destroy(NULL);
    libwacom_match_new(NULL, 0, 0, 0);
    libwacom_error_set(NULL, 0, NULL);
    libwacom_stylus_destroy(NULL);
    libwacom_update_match(NULL, NULL);

    return 0;
}
