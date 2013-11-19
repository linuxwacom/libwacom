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

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <librsvg/rsvg.h>
#include "libwacom.h"

#ifndef LIBRSVG_CHECK_VERSION
#include <librsvg/librsvg-features.h>
#endif
#if !LIBRSVG_CHECK_VERSION(2,36,2)
#include <librsvg/rsvg-cairo.h>
#endif

#define INACTIVE_COLOR		"#ededed"
#define ACTIVE_COLOR		"#729fcf"
#define STROKE_COLOR		"#000000"
#define DARK_COLOR		"#535353"
#define BACK_COLOR		"#000000"

/* Convenient struct to store our stuff around */
typedef struct
{
	RsvgHandle  *handle;
	GtkWidget   *widget;
	guint        timeout;
	WacomDevice *device;
	GdkRectangle area;
	char         active_button;
	int          num_buttons;
} Tablet;

static gboolean
get_sub_location (cairo_t *cairo_context, RsvgHandle *handle, const char *sub, double *x, double *y, double *width, double *height)
{
	if (x || y) {
		RsvgPositionData  position;
		double tx, ty;

		if (!rsvg_handle_get_position_sub (handle, &position, sub)) {
			g_warning ("Failed to retrieve '%s' position", sub);
			return FALSE;
		}

		tx = (double) position.x;
		ty = (double) position.y;
		cairo_user_to_device (cairo_context, &tx, &ty);

		if (x)
			*x = tx;
		if (y)
			*y = ty;
	}

	if (width || height) {
		RsvgDimensionData dimensions;
		double twidth, theight;

		if (!rsvg_handle_get_dimensions_sub (handle, &dimensions, sub)) {
			g_warning ("Failed to retrieve '%s' dimension", sub);
			return FALSE;
		}

		twidth = (double) dimensions.width;
		theight = (double) dimensions.height;
		cairo_user_to_device_distance (cairo_context, &twidth, &theight);

		if (width)
			*width = twidth;
		if (height)
			*height = theight;
	}

	return TRUE;
}

static void
print_label (cairo_t *cairo_context, Tablet *tablet, const char *sub, const char *markup, WacomButtonFlags flags)
{
	GtkWidget        *widget;
	GtkAllocation     allocation;
	GtkStyle         *style;
	PangoContext     *pango_context;
	PangoLayout      *pango_layout;
	PangoRectangle    pango_rect;
	double            label_x, label_y;
	int               x, y;

	if (!get_sub_location (cairo_context, tablet->handle, sub, &label_x, &label_y, NULL, NULL))
		return;

	widget = GTK_WIDGET(tablet->widget);
	gtk_widget_get_allocation(widget, &allocation);
	style = gtk_widget_get_style (widget);
	pango_context = gtk_widget_get_pango_context (widget);
	pango_layout  = pango_layout_new (pango_context);

	pango_layout_set_markup (pango_layout, markup, -1);
	pango_layout_get_pixel_extents (pango_layout, NULL, &pango_rect);

	if (flags & WACOM_BUTTON_POSITION_LEFT) {
		pango_layout_set_alignment (pango_layout, PANGO_ALIGN_LEFT);
		x = (int) label_x + pango_rect.x;
		y = (int) label_y + pango_rect.y - pango_rect.height / 2;
	} else if (flags & WACOM_BUTTON_POSITION_RIGHT) {
		pango_layout_set_alignment (pango_layout, PANGO_ALIGN_RIGHT);
		x = (int) label_x + pango_rect.x - pango_rect.width;
		y = (int) label_y + pango_rect.y - pango_rect.height / 2;
	} else {
		pango_layout_set_alignment (pango_layout, PANGO_ALIGN_CENTER);
		x = (int) label_x + pango_rect.x - pango_rect.width / 2;
		y = (int) label_y + pango_rect.y;
	}

	gtk_paint_layout (style,
		          gtk_widget_get_window (widget),
		          0,
		          TRUE,
			  &allocation,
		          widget,
		          NULL,
		          x,
		          y,
		          pango_layout);

	g_object_unref (pango_layout);
}

static void
print_button_labels (cairo_t *cairo_context, Tablet *tablet)
{
	char button;

	for (button = 'A'; button < 'A' + tablet->num_buttons; button++) {
		WacomButtonFlags  flags;
		gchar            *sub;
		gchar            *label;

		flags = libwacom_get_button_flag(tablet->device, button);
		sub = g_strdup_printf ("#Label%c", button);
		label = g_strdup_printf ("<span foreground=\"%s\" >Button %c</span>",
		                         (button == tablet->active_button) ? ACTIVE_COLOR : INACTIVE_COLOR,
		                         button);
		print_label (cairo_context, tablet, sub, label, flags);
		g_free (label);
		g_free (sub);
	}

	/* Touch rings */
	if (libwacom_has_ring(tablet->device)) {
		print_label (cairo_context, tablet, "#LabelRingCCW", "<span foreground=\"" INACTIVE_COLOR "\" >Ring Counter Clockwise</span>", WACOM_BUTTON_POSITION_LEFT);
		print_label (cairo_context, tablet, "#LabelRingCW", "<span foreground=\"" INACTIVE_COLOR "\" >Ring Clockwise</span>", WACOM_BUTTON_POSITION_LEFT);
	}
	if (libwacom_has_ring2(tablet->device)) {
		print_label (cairo_context, tablet, "#LabelRing2CCW", "<span foreground=\"" INACTIVE_COLOR "\" >2nd Ring Counter Clockwise</span>", WACOM_BUTTON_POSITION_RIGHT);
		print_label (cairo_context, tablet, "#LabelRing2CW", "<span foreground=\"" INACTIVE_COLOR "\" >2nd Ring Clockwise</span>", WACOM_BUTTON_POSITION_RIGHT);
	}
	/* Touch strips */
	if (libwacom_get_num_strips(tablet->device) > 0) {
		print_label (cairo_context, tablet, "#LabelStripUp", "<span foreground=\"" INACTIVE_COLOR "\" >Strip Up</span>", WACOM_BUTTON_POSITION_LEFT);
		print_label (cairo_context, tablet, "#LabelStripDown", "<span foreground=\"" INACTIVE_COLOR "\" >Strip Down</span>", WACOM_BUTTON_POSITION_LEFT);
	}
	if (libwacom_get_num_strips(tablet->device) > 1) {
		print_label (cairo_context, tablet, "#LabelStrip2Up", "<span foreground=\"" INACTIVE_COLOR "\" >2nd Strip Up</span>", WACOM_BUTTON_POSITION_RIGHT);
		print_label (cairo_context, tablet, "#LabelStrip2Down", "<span foreground=\"" INACTIVE_COLOR "\" >2nd Strip Down</span>", WACOM_BUTTON_POSITION_RIGHT);
	}
}

static void
update_tablet (Tablet *tablet)
{
	char        *width, *height;
	char         button;
	GError      *error;
	gchar       *data;

	if (tablet->handle)
		g_object_unref (tablet->handle);

	width = g_strdup_printf ("%d", tablet->area.width);
	height = g_strdup_printf ("%d", tablet->area.height);

	data = g_strconcat ("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
	                    "<svg version=\"1.1\"\n"
	                    "     xmlns=\"http://www.w3.org/2000/svg\"\n"
	                    "     xmlns:xi=\"http://www.w3.org/2001/XInclude\"\n"
	                    "     width=\"", width, "\"\n"
	                    "     height=\"", height, "\">\n"
	                    "  <style type=\"text/css\">\n",
	                    "    .Leader {\n"
	                    "      stroke-width: .5 !important;\n"
	                    "      stroke: ", INACTIVE_COLOR, ";\n"
	                    "      fill:    none !important;\n"
	                    "    }\n",
	                    "    .Button {\n"
	                    "      stroke-width: 0.25;\n"
	                    "      stroke: ", INACTIVE_COLOR, ";\n"
	                    "      fill:   ", INACTIVE_COLOR, ";\n"
	                    "    }\n",
	                    NULL);
	g_free (width);
	g_free (height);

	for (button = 'A'; button < 'A' + tablet->num_buttons; button++) {
		gchar class[] = {button, '\0'};
		if (button == tablet->active_button) {
			data = g_strconcat (data,
			                    "    .", class, " {\n"
			                    "      stroke: ", ACTIVE_COLOR, " !important;\n"
			                    "      fill:   ", ACTIVE_COLOR, " !important;\n"
			                    "    }\n",
			                    NULL);
	    }
	}

	data = g_strconcat (data,
	                    "    .Leader {\n"
	                    "      fill:    none !important;\n"
	                    "    }\n",
	                    "    .Label {\n"
	                    "      stroke: none      !important;\n"
	                    "      stroke-width: 0.1 !important;\n"
	                    "      opacity:   .0     !important;\n"
	                    "      font-size: .1     !important;\n"
	                    "      fill:   ", BACK_COLOR,    " !important;\n"
	                    "    }\n",
	                    "    .TouchStrip,.TouchRing {\n"
	                    "      stroke-width: 0.1   !important;\n"
	                    "      stroke: ", INACTIVE_COLOR," !important;\n"
	                    "      fill:   ", DARK_COLOR,    " !important;\n"
	                    "    }\n",
	                    "  </style>\n"
	                    "  <xi:include href=\"", libwacom_get_layout_filename (tablet->device), "\"/>\n"
	                    "</svg>",
	                    NULL);

	tablet->handle = rsvg_handle_new_from_data ((guint8 *) data, strlen(data), &error);
	g_free (data);
}

static void
on_expose_cb (GtkWidget *widget, GdkEvent *event, Tablet *tablet)
{
	GtkAllocation  allocation;
	cairo_t       *cairo_context;
	float          scale;
	double         twidth, theight;

	if (tablet->handle == NULL)
		update_tablet (tablet);

	/* Create a Cairo for the widget */
	cairo_context = gdk_cairo_create (gtk_widget_get_window (widget));
	cairo_set_operator (cairo_context, CAIRO_OPERATOR_CLEAR);
	cairo_paint (cairo_context);
	cairo_set_operator (cairo_context, CAIRO_OPERATOR_OVER);

	/* Scale to fit in window */
	gtk_widget_get_allocation(widget, &allocation);
	scale = MIN ((float) allocation.width / tablet->area.width,
	             (float) allocation.height / tablet->area.height);
	cairo_scale (cairo_context, scale, scale);

	/* Center the result in window */
	twidth = (double) tablet->area.width;
	theight = (double) tablet->area.height;
	cairo_user_to_device_distance (cairo_context, &twidth, &theight);
	twidth = ((double) allocation.width - twidth) / 2.0;
	theight = ((double) allocation.height - theight) / 2.0;
	cairo_device_to_user_distance (cairo_context, &twidth, &theight);
	cairo_translate (cairo_context, twidth, theight);

	/* And render the tablet layout */
	rsvg_handle_render_cairo (tablet->handle, cairo_context);
	print_button_labels (cairo_context, tablet);
	cairo_destroy (cairo_context);
}

static gboolean
on_timer_cb (Tablet *tablet)
{
	GtkAllocation allocation;
	int           num_buttons;

	tablet->active_button++;
	num_buttons = libwacom_get_num_buttons (tablet->device);
	if (tablet->active_button >= 'A' + num_buttons)
		tablet->active_button = 'A';
	update_tablet (tablet);
	gtk_widget_get_allocation (GTK_WIDGET(tablet->widget), &allocation);
	gdk_window_invalidate_rect(gtk_widget_get_window (tablet->widget), &allocation, FALSE);

	return TRUE;
}

static gboolean
on_delete_cb (GtkWidget *widget, GdkEvent  *event, Tablet *tablet)
{
	gtk_main_quit ();

	return TRUE;
}

int
main (int argc, char **argv)
{
	GOptionContext      *context;
	RsvgHandle          *handle;
	RsvgDimensionData    dimensions;
	GError              *error;
	WacomDeviceDatabase *db;
	WacomDevice         *device;
	Tablet              *tablet;
	GdkWindow           *gdk_win;
	GdkColor             black;
	char                *tabletname;
	const char          *filename;
	GOptionEntry
		options[] = {
			{"tablet", 't', 0, G_OPTION_ARG_STRING, &tabletname, "Name of the tablet to show", "<string>"},
			{NULL}
		};

	handle = NULL;
	error = NULL;
	tabletname = NULL;

	context = g_option_context_new ("- libwacom tablet viewer");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_set_help_enabled (context, TRUE);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	gtk_init (&argc, &argv);
	if (tabletname == NULL) {
		g_warning ("No tablet name provided, exiting");
		return 1;
	}
	db = libwacom_database_new_for_path(TOPSRCDIR"/data");
	if (!db) {
		g_warning ("Failed to load libwacom database, exiting");
		return 1;
	}
	device = libwacom_new_from_name(db, tabletname, NULL);
	if (!device) {
		g_warning ("Device '%s' not found in libwacom database, exiting", tabletname);
		return 1;
	}

	filename = libwacom_get_layout_filename(device);
	if (filename == NULL) {
		g_warning ("Device '%s' has no layout available, exiting", tabletname);
		return 1;
	}
	handle = rsvg_handle_new_from_file (filename, &error);
	if (error || handle == NULL)
		return 1;
	rsvg_handle_get_dimensions (handle, &dimensions);
	g_object_unref (handle);

	tablet = g_new0 (Tablet, 1);
	tablet->device = device;
	tablet->area.width = dimensions.width;
	tablet->area.height = dimensions.height;
	tablet->handle = NULL;
	tablet->active_button = 'A';
	tablet->num_buttons = libwacom_get_num_buttons (device);
	tablet->widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_widget_set_app_paintable (tablet->widget, TRUE);
	gtk_widget_realize (tablet->widget);
	gdk_win = gtk_widget_get_window (tablet->widget);
	gdk_color_parse (BACK_COLOR, &black);
	gdk_window_set_background (gdk_win, &black);
	gtk_window_set_default_size (GTK_WINDOW (tablet->widget), 800, 600);

	g_signal_connect (tablet->widget, "expose-event", G_CALLBACK(on_expose_cb), tablet);
	g_signal_connect (tablet->widget, "delete-event", G_CALLBACK(on_delete_cb), tablet);
	tablet->timeout = g_timeout_add(750 /* ms */, (GSourceFunc) on_timer_cb, tablet);

	gtk_widget_show (tablet->widget);

	gtk_main();

	libwacom_destroy(device);
	libwacom_database_destroy(db);

	return 0;
}
