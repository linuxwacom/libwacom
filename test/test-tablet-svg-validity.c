/*
 * Copyright ?? 2012 Red Hat, Inc.
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
 *        Olivier Fourdan <ofourdan@redhat.com>
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libxml/parser.h>
#include <libxml/xmlmemory.h>
#include <glib.h>
#include "libwacom.h"

static xmlNodePtr
verify_has_sub (xmlNodePtr cur, char *sub)
{
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		xmlChar *prop;

		/* Descend the tree if dealing with a group */
		if (xmlStrcmp(cur->name, (const xmlChar *) "g") == 0) {
			xmlNodePtr sub_node;
			sub_node = verify_has_sub (cur, sub);
			if (sub_node != NULL)
				return sub_node;
		}

		prop = xmlGetProp(cur, (xmlChar *) "id");
		if (prop) {
			int status = xmlStrcmp(prop, (const xmlChar *) sub);
			xmlFree(prop);
			if (status == 0)
				return cur;
		}
		cur = cur->next;
	}

	return NULL;
}

static gboolean
class_found (gchar **classes, gchar *value)
{
	gchar **ptr = classes;
	while (*ptr) {
		if (strcmp (*ptr++, value) == 0)
			return TRUE;
	}

	return FALSE;
}

static void
verify_has_class (xmlNodePtr cur, const gchar *expected)
{

	xmlChar *prop;
	gchar  **classes_present;
	gchar  **classes_expected;
	gchar  **ptr;

	prop = xmlGetProp (cur, (xmlChar *) "class");
	g_assert (prop != NULL);
	g_assert (strlen((const char *) prop) > 0);

	classes_present = g_strsplit ((const gchar *) prop, " ", -1);
	classes_expected = g_strsplit (expected, " ", -1);
	ptr = classes_expected;

	while (*ptr)
		g_assert (class_found (classes_present, *ptr++));

	g_strfreev (classes_present);
	g_strfreev (classes_expected);
	xmlFree (prop);
}

static void
check_button (xmlNodePtr cur, const WacomDevice *device, char button, gchar *type)
{
	char             *sub;
	char             *class;
	xmlNodePtr        node;
	WacomButtonFlags  flags;

	/* Check ID */
	sub = g_strdup_printf ("%s%c", type, button);
	node = verify_has_sub (cur, sub);
	g_assert (node != NULL);
	g_free (sub);

	/* Check class */
	flags = libwacom_get_button_flag(device, button);
	if (flags & WACOM_BUTTON_MODESWITCH)
		class = g_strdup_printf ("%c ModeSwitch %s", button, type);
	else
		class = g_strdup_printf ("%c %s", button, type);
	verify_has_class (node, class);
	g_free (class);
}

static void
check_touchstrip (xmlNodePtr cur, gchar *id)
{
	char             *sub;
	char             *class;
	xmlNodePtr        node;

	node = verify_has_sub (cur, id);
	g_assert (node != NULL);

	class = g_strdup_printf ("%s %s", id, "TouchStrip");
	verify_has_class (node, class);
	g_free (class);

	sub = g_strdup_printf ("Label%sUp", id);
	node = verify_has_sub (cur, sub);
	g_assert (node != NULL);
	g_free (sub);

	class = g_strdup_printf ("%sUp %s Label", id, id);
	verify_has_class (node, class);
	g_free (class);

	sub = g_strdup_printf ("Label%sDown", id);
	node = verify_has_sub (cur, sub);
	g_assert (node != NULL);
	g_free (sub);

	class = g_strdup_printf ("%sDown %s Label", id, id);
	verify_has_class (node, class);
	g_free (class);

	sub = g_strdup_printf ("Leader%sUp", id);
	node = verify_has_sub (cur, sub);
	g_assert (node != NULL);
	g_free (sub);

	class = g_strdup_printf ("%sUp %s Leader", id, id);
	verify_has_class (node, class);
	g_free (class);

	sub = g_strdup_printf ("Leader%sDown", id);
	node = verify_has_sub (cur, sub);
	g_assert (node != NULL);
	g_free (sub);

	class = g_strdup_printf ("%sDown %s Leader", id, id);
	verify_has_class (node, class);
	g_free (class);
}


static void
check_touchring (xmlNodePtr cur, gchar *id)
{
	char             *sub;
	char             *class;
	xmlNodePtr        node;

	node = verify_has_sub (cur, id);
	g_assert (node != NULL);

	class = g_strdup_printf ("%s %s", id, "TouchRing");
	verify_has_class (node, class);
	g_free (class);

	sub = g_strdup_printf ("Label%sCCW", id);
	node = verify_has_sub (cur, sub);
	g_assert (node != NULL);
	g_free (sub);

	class = g_strdup_printf ("%sCCW %s Label", id, id);
	verify_has_class (node, class);
	g_free (class);

	sub = g_strdup_printf ("Label%sCW", id);
	node = verify_has_sub (cur, sub);
	g_assert (node != NULL);
	g_free (sub);

	class = g_strdup_printf ("%sCW %s Label", id, id);
	verify_has_class (node, class);
	g_free (class);

	sub = g_strdup_printf ("Leader%sCCW", id);
	node = verify_has_sub (cur, sub);
	g_assert (node != NULL);
	g_free (sub);

	class = g_strdup_printf ("%sCCW %s Leader", id, id);
	verify_has_class (node, class);
	g_free (class);

	sub = g_strdup_printf ("Leader%sCW", id);
	node = verify_has_sub (cur, sub);
	g_assert (node != NULL);
	g_free (sub);

	class = g_strdup_printf ("%sCW %s Leader", id, id);
	verify_has_class (node, class);
	g_free (class);
}

struct fixture {
	xmlDocPtr doc;
	xmlNodePtr root;
};

static void
test_filename(struct fixture *f, gconstpointer data)
{
    const WacomDevice *device = data;
    const char *filename;

    filename = libwacom_get_layout_filename(device);
    if (libwacom_get_num_buttons(device) > 0) {
	    g_assert_nonnull(filename);
	    g_assert_cmpstr(filename, !=, "");
    }
}

static void
test_svg(struct fixture *f, gconstpointer data)
{
    g_assert_nonnull(f->doc);
    g_assert_nonnull(f->root);
    g_assert_cmpint(xmlStrcmp(f->root->name, (const xmlChar*) "svg"), ==, 0);
}

static void
test_dimensions(struct fixture *f, gconstpointer data)
{
	xmlChar *prop;

	/* width is provided */
	prop = xmlGetProp(f->root, (xmlChar *) "width") ;
	g_assert_nonnull(prop);
	xmlFree(prop);

	/* height is provided */
	prop = xmlGetProp(f->root, (xmlChar *) "height") ;
	g_assert_nonnull(prop);
	xmlFree(prop);
}

static void
test_rings(struct fixture *f, gconstpointer data)
{
	const WacomDevice *device = data;

	if (libwacom_has_ring(device))
		check_touchring(f->root, "Ring");
	if (libwacom_has_ring2(device))
		check_touchring(f->root, "Ring2");
}

static void
test_strips(struct fixture *f, gconstpointer data)
{
	const WacomDevice *device = data;

	if (libwacom_get_num_strips(device) > 0)
		check_touchstrip(f->root, "Strip");
	if (libwacom_get_num_strips(device) > 1)
		check_touchstrip(f->root, "Strip2");
}

static void
test_buttons(struct fixture *f, gconstpointer data)
{
	const WacomDevice *device = data;
	int num_buttons = libwacom_get_num_buttons (device);

	for (char button = 'A'; button < 'A' + num_buttons; button++) {
		check_button(f->root, device, button, "Button");
		check_button(f->root, device, button, "Label");
		check_button(f->root, device, button, "Leader");
	}
}

static void
setup_svg(struct fixture *f, gconstpointer data)
{
	const WacomDevice *device = data;
	const char *filename = libwacom_get_layout_filename(device);
	xmlDocPtr doc;

	if (!filename)
		return;

	doc = xmlParseFile(filename);
	f->doc = doc;
	f->root = doc ? xmlDocGetRootElement(doc) : NULL;
}

static void
teardown_svg(struct fixture *f, gconstpointer data)
{
	if (f->doc)
		xmlFreeDoc(f->doc);
}

typedef void (*testfunc)(struct fixture *f, gconstpointer d);

/* Wrapper function to make adding tests simpler. g_test requires
 * a unique test case name so we assemble that from the test function and
 * the tablet data.
 */
static inline void
_add_test(WacomDevice *device, testfunc func, const char *funcname)
{
	char buf[128];
	static int count; /* guarantee unique test case names */
	const char *prefix;

	/* tests must be test_foobar */
	g_assert(strncmp(funcname, "test_", 5) == 0);
	prefix = &funcname[5];

	snprintf(buf, 128, "/svg/%s/%03d/%04x:%04x-%s",
		 prefix,
		 ++count,
		 libwacom_get_vendor_id(device),
		 libwacom_get_product_id(device),
		 libwacom_get_name(device));

	g_test_add(buf, struct fixture, device,
		   setup_svg, func, teardown_svg);
}
#define add_test(device_, func_) \
	_add_test(device_, func_, #func_)

#define add_test(device_, func_) \
	_add_test(device_, func_, #func_)

static void setup_tests(WacomDevice *device)
{
	const char *name;

	name = libwacom_get_name(device);
	if (strcmp(name, "Generic") == 0)
		return;

	add_test(device, test_filename);
	if (!libwacom_get_layout_filename(device))
		return;

	add_test(device, test_svg);
	add_test(device, test_dimensions);
	if (libwacom_get_num_buttons(device) > 0)
		add_test(device, test_buttons);
	if (libwacom_has_ring(device) || libwacom_has_ring2(device))
		add_test(device, test_rings);
	if (libwacom_get_num_strips(device) > 0)
		add_test(device, test_strips);
}

static WacomDeviceDatabase *
load_database(void)
{
	WacomDeviceDatabase *db;
	const char *datadir;

	datadir = getenv("LIBWACOM_DATA_DIR");
	if (!datadir)
		datadir = TOPSRCDIR"/data";

	db = libwacom_database_new_for_path(datadir);
	if (!db)
		printf("Failed to load data from %s", datadir);

	g_assert(db);
	return db;
}

int main(int argc, char **argv)
{
	WacomDeviceDatabase *db;
	WacomDevice **devices;
	int rc;

        g_test_init(&argc, &argv, NULL);

        db = load_database();

	devices = libwacom_list_devices_from_database(db, NULL);
	g_assert(devices);
	g_assert(*devices);

	for (WacomDevice **device = devices; *device; device++)
		setup_tests(*device);

	rc = g_test_run();

	free(devices);
	libwacom_database_destroy (db);

	return rc;
}
/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
