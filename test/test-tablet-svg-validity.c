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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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
check_button (xmlNodePtr cur, WacomDevice *device, char button, gchar *type)
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

static void
verify_tablet_layout (WacomDeviceDatabase *db, WacomDevice *device)
{
	const char *name;
	const char *filename;
	xmlChar    *prop;
	xmlDocPtr   doc;
	xmlNodePtr  cur;
	char        button;
	int         num_buttons;

	name = libwacom_get_name(device);
	if (strcmp(name, "Generic") == 0)
		return;

	filename = libwacom_get_layout_filename(device);
	num_buttons = libwacom_get_num_buttons (device);

	if (filename == NULL) {
		if (num_buttons > 0)
			g_warning ("device '%s' has buttons but no layout", name);
		return;
	}

	g_message ("Verifying device '%s', SVG file '%s'", name, filename);

	doc = xmlParseFile(filename);
	g_assert (doc != NULL );

	cur = xmlDocGetRootElement(doc);
	g_assert (cur != NULL);

	/* Check we got an SVG layout */
	g_assert (xmlStrcmp(cur->name, (const xmlChar *) "svg") == 0);

	/* width is provided */
	prop = xmlGetProp(cur, (xmlChar *) "width") ;
	g_assert (prop != NULL);
	xmlFree(prop);

	/* height is provided */
	prop = xmlGetProp(cur, (xmlChar *) "height") ;
	g_assert (prop != NULL);
	xmlFree(prop);

	for (button = 'A'; button < 'A' + num_buttons; button++) {
		check_button (cur, device, button, "Button");
		check_button (cur, device, button, "Label");
		check_button (cur, device, button, "Leader");
	}

	/* Touch rings */
	if (libwacom_has_ring(device))
		check_touchring (cur, "Ring");
	if (libwacom_has_ring2(device))
		check_touchring (cur, "Ring2");
	/* Touch strips */
	if (libwacom_get_num_strips(device) > 0)
		check_touchstrip (cur, "Strip");
	if (libwacom_get_num_strips(device) > 1)
		check_touchstrip (cur, "Strip2");

	xmlFreeDoc(doc);

	return;
}


int main(int argc, char **argv)
{
	WacomDeviceDatabase *db;
	WacomDevice **device, **devices;

	db = libwacom_database_new_for_path(TOPSRCDIR"/data");
	if (!db)
		printf("Failed to load data from %s", TOPSRCDIR"/data");
	g_assert(db);

	devices = libwacom_list_devices_from_database(db, NULL);
	g_assert(devices);
	g_assert(*devices);

	for (device = devices; *device; device++)
		verify_tablet_layout(db, *device);

	free(devices);
	libwacom_database_destroy (db);

	return 0;
}
