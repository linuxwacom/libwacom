/*
 * Copyright © 2024 Red Hat, Inc.
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
 *        Peter Hutterer <peter.hutterer@redhat.com>
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <glib/gi18n.h>
#include <glib.h>
#include "libwacom.h"

static char *database_path;
static gboolean with_styli;

static GOptionEntry opts[] = {
	{"database", 0, 0, G_OPTION_ARG_FILENAME, &database_path, N_("Path to device database"), NULL },
	{"with-styli", 0, 0, G_OPTION_ARG_NONE, &with_styli, N_("Select to also list styli for this device"), NULL },
	{ .long_name = NULL}
};

static int indent = 0;

#define push() indent += 2
#define pop() indent -= 2

#define ip(fmt_, ...) \
	printf("%-*s" fmt_, indent, "", __VA_ARGS__)
/* Usage: p("function_name", "return value is %d", a) */
#define p(f_, fmt_, ...) \
	printf("%-*s%-*s -> " fmt_ "\n", indent, "", (46 - indent), f_, __VA_ARGS__)

/* Usage: func(myfunc, "return value is %d", a) */
#define func(f_, fmt_, ...) \
	p(#f_"()", fmt_, __VA_ARGS__)

/* Usage: func(myfunc, "%d", argval, "return value is %d", a) */
#define func_arg(f_, arg_fmt_, arg_val_, fmt_, ...) \
	do { \
		char buf_[256]; \
		snprintf(buf_, sizeof(buf_), #f_"(" arg_fmt_ ")", arg_val_); \
		p(buf_, fmt_, __VA_ARGS__); \
	} while(0)

#define strfunc(f_, dev_) \
	func(f_, "\"%s\"", f_(dev_))

#define hexfunc(f_, dev_) \
	func(f_, "0x%04x", f_(dev_))

#define intfunc(f_, dev_) \
	func(f_, "%d", f_(dev_))

static void
handle_match(const WacomMatch *m)
{
	if (m == NULL) {
		printf(" <none>\n");
		return;
	}
	push();
	ip("%s\n", "{");
	push();
	strfunc(libwacom_match_get_match_string, m);
	strfunc(libwacom_match_get_name, m);
	strfunc(libwacom_match_get_uniq, m);
	hexfunc(libwacom_match_get_bustype, m);
	hexfunc(libwacom_match_get_vendor_id, m);
	hexfunc(libwacom_match_get_product_id, m);
	pop();
	ip("%s\n", "}");
	pop();
}

static WacomDevice *
device_from_device_match(WacomDeviceDatabase *db, char **parts)
{
	WacomBusType bustype;
	guint64 vid, pid;
	const char *name = NULL;
	const char *uniq = NULL;
	WacomBuilder *builder;
	WacomDevice *device;

	if (g_str_equal(parts[0], "usb"))
		bustype = WBUSTYPE_USB;
	else if (g_str_equal(parts[0], "serial"))
		bustype = WBUSTYPE_SERIAL;
	else if (g_str_equal(parts[0], "bluetooth"))
		bustype = WBUSTYPE_BLUETOOTH;
	else if (g_str_equal(parts[0], "i2c"))
		bustype = WBUSTYPE_I2C;
	else {
		fprintf(stderr, "Unknown bus type %s\n", parts[0]);
		return NULL;
	}

	if (!g_ascii_string_to_unsigned(parts[1], 16, 0, 0xffff, &vid, NULL) ||
	    !g_ascii_string_to_unsigned(parts[2], 16, 0, 0xffff, &pid, NULL)) {
		fprintf(stderr, "Failed to parse vid/pid\n");
		return NULL;
	}

	if (parts[3]) {
		name = parts[3];
		if (parts[4])
			uniq = parts[4];
	}

	builder = libwacom_builder_new();
	libwacom_builder_set_bustype(builder, bustype);
	libwacom_builder_set_usbid(builder, vid, pid);
	if (name)
		libwacom_builder_set_match_name(builder, name);
	if (name)
		libwacom_builder_set_uniq(builder, uniq);

	device = libwacom_new_from_builder(db, builder, WFALLBACK_NONE, NULL);
	libwacom_builder_destroy(builder);
	return device;
}

static int
handle_device(WacomDeviceDatabase *db, const char *path)
{
	WacomDevice *device;
	char **parts = g_strsplit(path, "|", 5);

	if (parts && parts[0] && parts[1]) {
		device = device_from_device_match(db, parts);
	} else {
		device = libwacom_new_from_path(db, path, WFALLBACK_NONE, NULL);
	}
	g_strfreev(parts);

	if (!device) {
		fprintf(stderr, "Device not known to libwacom\n");
		return EXIT_FAILURE;
	}

	strfunc(libwacom_get_name, device);
	strfunc(libwacom_get_model_name, device);
	strfunc(libwacom_get_layout_filename, device);

	hexfunc(libwacom_get_vendor_id, device);
	hexfunc(libwacom_get_product_id, device);
	{
		char *busstr = NULL;
		switch (libwacom_get_bustype(device)) {
			case WBUSTYPE_UNKNOWN: busstr = "UNKNOWN"; break;
			case WBUSTYPE_USB: busstr = "USB"; break;
			case WBUSTYPE_SERIAL: busstr = "SERIAL"; break;
			case WBUSTYPE_BLUETOOTH: busstr = "BLUETOOTH"; break;
			case WBUSTYPE_I2C: busstr = "I2C"; break;
		}
		func(libwacom_get_bustype, "%s", busstr);
	}

	{
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		WacomClass cls = libwacom_get_class(device);
		#pragma GCC diagnostic pop
		const char *str = NULL;

		switch (cls) {
		case WCLASS_UNKNOWN: str = "UNKNOWN"; break;
		case WCLASS_INTUOS3: str = "INTUOS3"; break;
		case WCLASS_INTUOS4: str = "INTUOS4"; break;
		case WCLASS_INTUOS5: str = "INTUOS5"; break;
		case WCLASS_CINTIQ: str = "CINTIQ"; break;
		case WCLASS_BAMBOO: str = "BAMBOO"; break;
		case WCLASS_GRAPHIRE: str = "GRAPHIRE"; break;
		case WCLASS_ISDV4: str = "ISDV4"; break;
		case WCLASS_INTUOS: str = "INTUOS"; break;
		case WCLASS_INTUOS2: str = "INTUOS2"; break;
		case WCLASS_PEN_DISPLAYS: str = "PEN_DISPLAYS"; break;
		case WCLASS_REMOTE: str = "REMOTE"; break;
			break;
		}
		func(libwacom_get_class, "%s", str);
	}

	intfunc(libwacom_get_width, device);
	intfunc(libwacom_get_height, device);

	intfunc(libwacom_is_reversible, device);

	{
		const WacomMatch **matches = libwacom_get_matches(device);
		const WacomMatch **m;

		printf("libwacom_get_matches() -> {\n");
		for (m = matches; *m; m++) {
			handle_match(*m);
		}
		printf("}\n");
	}

	strfunc(libwacom_get_match, device);

	printf("libwacom_get_paired_device() -> {");
	handle_match(libwacom_get_paired_device(device));
	printf("}\n");

	intfunc(libwacom_has_stylus, device);
	intfunc(libwacom_has_touch, device);
	intfunc(libwacom_get_num_buttons, device);
	intfunc(libwacom_get_num_keys, device);
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
	intfunc(libwacom_has_ring, device);
	intfunc(libwacom_has_ring2, device);
	#pragma GCC diagnostic pop
	intfunc(libwacom_has_touchswitch, device);
	intfunc(libwacom_get_ring_num_modes, device);
	intfunc(libwacom_get_ring2_num_modes, device);
	intfunc(libwacom_get_num_strips, device);
	intfunc(libwacom_get_strips_num_modes, device);
	intfunc(libwacom_get_num_dials, device);
	intfunc(libwacom_get_dial_num_modes, device);
	intfunc(libwacom_get_dial2_num_modes, device);

	{
		WacomIntegrationFlags flags = libwacom_get_integration_flags(device);
		func(libwacom_get_integration_flags, "%s%s %s %s",
		     flags == WACOM_DEVICE_INTEGRATED_NONE ? "NONE" : "",
		     flags == WACOM_DEVICE_INTEGRATED_DISPLAY ? "DISPLAY" : "",
		     flags == WACOM_DEVICE_INTEGRATED_SYSTEM ? "SYSTEM" : "",
		     flags == WACOM_DEVICE_INTEGRATED_REMOTE ? "REMOTE" : ""
		);
	}

	{
		for (int i = 0; i < libwacom_get_num_buttons(device); i++) {
			char b = 'A' + i;
			func_arg(libwacom_get_button_led_group, "%c", b, "%d",
				libwacom_get_button_led_group(device, b));
		}

		for (int i = 0; i < libwacom_get_num_buttons(device); i++) {
			char b = 'A' + i;
			func_arg(libwacom_get_button_evdev_code, "%c", b, "0x%x",
				 libwacom_get_button_evdev_code(device, b));
		}

		for (int i = 0; i < libwacom_get_num_buttons(device); i++) {
			char b = 'A' + i;
			WacomButtonFlags flags;

			flags = libwacom_get_button_flag(device, b);
			func_arg(libwacom_get_button_flag, "%c", b, "%s%s%s%s%s%s%s%s%s%s%s%s",
				 flags == WACOM_BUTTON_NONE ? "NONE" : "",
				 flags & WACOM_BUTTON_POSITION_LEFT ? "POSITION_LEFT|" : "",
				 flags & WACOM_BUTTON_POSITION_RIGHT ? "POSITION_RIGHT|" : "",
				 flags & WACOM_BUTTON_POSITION_TOP ? "POSITION_TOP|" : "",
				 flags & WACOM_BUTTON_POSITION_BOTTOM ? "POSITION_BOTTOM|" : "",
				 flags & WACOM_BUTTON_RING_MODESWITCH ? "RING_MODESWITCH|" : "",
				 flags & WACOM_BUTTON_RING2_MODESWITCH ? "RING2_MODESWITCH|" : "",
				 flags & WACOM_BUTTON_TOUCHSTRIP_MODESWITCH ? "TOUCHSTRIP_MODESWITCH|" : "",
				 flags & WACOM_BUTTON_TOUCHSTRIP2_MODESWITCH ? "TOUCHSTRIP2_MODESWITCH|" : "",
				 flags & WACOM_BUTTON_DIAL_MODESWITCH ? "DIAL_MODESWITCH|" : "",
				 flags & WACOM_BUTTON_DIAL2_MODESWITCH ? "DIAL2_MODESWITCH|" : "",
				 flags & WACOM_BUTTON_OLED ? "OLED " : "");
		}
	}

	{
		char buf[1024] = {0};
		int nleds;
		const WacomStatusLEDs *leds = libwacom_get_status_leds(device, &nleds);

		for (int i = 0; i < nleds; i++) {
			char *ledstr = NULL;
			switch (leds[i]) {
				case WACOM_STATUS_LED_UNAVAILABLE: ledstr = "UNAVAILABLE"; break;
				case WACOM_STATUS_LED_RING: ledstr = "RING"; break;
				case WACOM_STATUS_LED_RING2: ledstr = "RING2"; break;
				case WACOM_STATUS_LED_TOUCHSTRIP: ledstr = "TOUCHSTRIP"; break;
				case WACOM_STATUS_LED_TOUCHSTRIP2: ledstr = "TOUCHSTRIP2"; break;
				case WACOM_STATUS_LED_DIAL: ledstr = "DIAL"; break;
				case WACOM_STATUS_LED_DIAL2: ledstr = "DIAL2"; break;
			}
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s%s", i > 0 ? ", " : "", ledstr);
		}
		func(libwacom_get_status_leds, "[%s]", buf);
	}

	{
		int nstyli;
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
		const int *styli = libwacom_get_supported_styli(device, &nstyli);
		#pragma GCC diagnostic pop

		{
			char buf[1024] = {0};
			for (int i = 0; i < nstyli; i++)
				snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s0x%06x", i > 0 ? ", " : "", styli[i]);

			func(libwacom_get_supported_styli, "[%s]", buf);
		}
	}

	{
		int nstyli;
		const WacomStylus **styli = libwacom_get_styli(device, &nstyli);
		{
			char buf[1024] = {0};
			for (int i = 0; i < nstyli; i++) {
				const WacomStylus *s = styli[i];
				snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s[0x%04x, 0x%06x]", i > 0 ? ", " : "",
					 libwacom_stylus_get_vendor_id(s), libwacom_stylus_get_id(s));
			}

			func(libwacom_get_styli, "[%s]", buf);
		}

		if (with_styli) {
			printf("\n---------- Listing styli for this device ----------\n");

			for (int i = 0; i < nstyli; i++) {
				const WacomStylus *stylus = styli[i];
				int id = libwacom_stylus_get_id(stylus);

				ip("%s\n", "{");
				push();
				func_arg(libwacom_stylus_get_id, "0x%04x", id, "0x%04x", libwacom_stylus_get_id(stylus));
				func_arg(libwacom_stylus_get_name, "0x%04x", id, "%s", libwacom_stylus_get_name(stylus));
				func_arg(libwacom_stylus_get_num_buttons, "0x%04x", id, "%d", libwacom_stylus_get_num_buttons(stylus));
				func_arg(libwacom_stylus_has_eraser, "0x%04x", id, "%d", libwacom_stylus_has_eraser(stylus));
				func_arg(libwacom_stylus_is_eraser, "0x%04x", id, "%d", libwacom_stylus_is_eraser(stylus));
				func_arg(libwacom_stylus_has_lens, "0x%04x", id, "%d", libwacom_stylus_has_lens(stylus));
				func_arg(libwacom_stylus_has_wheel, "0x%04x", id, "%d", libwacom_stylus_has_wheel(stylus));

				{
					char buf[1024] = {0};
					int npaired;
					#pragma GCC diagnostic push
					#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
					const int *paired = libwacom_stylus_get_paired_ids(stylus, &npaired);
					#pragma GCC diagnostic pop

					for (int i = 0; i < npaired; i++)
						snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s0x%06x", i > 0 ? ", " : "", paired[i]);
					func_arg(libwacom_stylus_get_paired_ids, "0x%04x", id, "[%s]", buf);
				}

				{
					char buf[1024] = {0};
					int npaired;
					const WacomStylus **paired = libwacom_stylus_get_paired_styli(stylus, &npaired);

					for (int i = 0; i < npaired; i++) {
						const WacomStylus *p = paired[i];
						snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s[0x%04x, 0x%06x]", i > 0 ? ", " : "",
							 libwacom_stylus_get_vendor_id(p), libwacom_stylus_get_id(p));
					}
					func_arg(libwacom_stylus_get_paired_ids, "0x%04x", id, "[%s]", buf);

					g_clear_pointer(&paired, g_free);
				}

				{
					WacomAxisTypeFlags flags = libwacom_stylus_get_axes(stylus);
					func_arg(libwacom_stylus_has_wheel, "0x%04x", id, "%s%s%s%s%s%s",
						 flags == WACOM_AXIS_TYPE_NONE ? "NONE" : "",
						 flags & WACOM_AXIS_TYPE_TILT ? "TILT|" : "",
						 flags & WACOM_AXIS_TYPE_ROTATION_Z ? "ROTATION_Z|" : "",
						 flags & WACOM_AXIS_TYPE_DISTANCE ? "DISTANCE|" : "",
						 flags & WACOM_AXIS_TYPE_PRESSURE ? "PRESSURE|" : "",
						 flags & WACOM_AXIS_TYPE_SLIDER ? "SLIDER" : "");
				}

				{
					const char *typestr = NULL;
					switch (libwacom_stylus_get_type(stylus)) {
						case WSTYLUS_UNKNOWN: typestr = "UNKNOWN"; break;
						case WSTYLUS_GENERAL: typestr = "GENERAL"; break;
						case WSTYLUS_INKING: typestr = "INKING"; break;
						case WSTYLUS_AIRBRUSH: typestr = "AIRBRUSH"; break;
						case WSTYLUS_CLASSIC: typestr = "CLASSIC"; break;
						case WSTYLUS_MARKER: typestr = "MARKER"; break;
						case WSTYLUS_STROKE: typestr = "STROKE"; break;
						case WSTYLUS_PUCK: typestr = "PUCK"; break;
						case WSTYLUS_3D: typestr = "3D"; break;
						case WSTYLUS_MOBILE: typestr = "MOBILE"; break;
					}

					func_arg(libwacom_stylus_get_type, "0x%04x", id, "%s", typestr);
				}

				{
					const char *eraserstr = NULL;
					switch (libwacom_stylus_get_eraser_type(stylus)) {
						case WACOM_ERASER_UNKNOWN: eraserstr = "UNKNOWN"; break;
						case WACOM_ERASER_NONE: eraserstr = "NONE"; break;
						case WACOM_ERASER_INVERT: eraserstr = "INVERT"; break;
						case WACOM_ERASER_BUTTON: eraserstr = "BUTTON"; break;
					}

					func_arg(libwacom_stylus_get_type, "0x%04x", id, "%s", eraserstr);
				}
				pop();
				ip("%s\n", "}");
			}
		}

		g_clear_pointer(&styli, g_free);
	}

	libwacom_destroy(device);

	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	WacomDeviceDatabase *db;
	GOptionContext *context;
	GError *error;
	int rc;

	context = g_option_context_new ("[/dev/input/event0 | \"usb|0123|abcd|some tablet\"]");
	g_option_context_set_description(context, "The argument may be a device node or a single DeviceMatch string as listed in .tablet files.");

	g_option_context_add_main_entries (context, opts, NULL);
	error = NULL;

	if (!g_option_context_parse (context, &argc, &argv, &error)) {
		if (error != NULL) {
			fprintf (stderr, "%s\n", error->message);
			g_error_free (error);
		}
		return EXIT_FAILURE;
	}

	g_option_context_free (context);

	if (database_path) {
		db = libwacom_database_new_for_path(database_path);
		g_free (database_path);
	} else {
#ifdef DATABASEPATH
		db = libwacom_database_new_for_path(DATABASEPATH);
#else
		db = libwacom_database_new();
#endif
	}

	if (!db) {
		fprintf(stderr, "Failed to initialize device database\n");
		return EXIT_FAILURE;
	}

	if (argc <= 1) {
		fprintf(stderr, "Missing device node or match string\n");
		libwacom_database_destroy (db);
		return EXIT_FAILURE;
	}

	rc = handle_device(db, argv[1]);

	libwacom_database_destroy (db);
	return rc;
}

/* vim: set noexpandtab tabstop=8 shiftwidth=8: */
