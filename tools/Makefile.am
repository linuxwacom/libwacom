AM_CPPFLAGS=-I$(top_srcdir)/libwacom -DTOPSRCDIR="\"$(abs_top_srcdir)\""

noinst_PROGRAMS = generate-udev-rules list-devices list-compatible-styli

generate_udev_rules_SOURCES = generate-udev-rules.c
generate_udev_rules_LDADD=$(top_builddir)/libwacom/libwacom.la $(GLIB_LIBS)
generate_udev_rules_CFLAGS=$(GLIB_CFLAGS)

list_devices_SOURCES = list-devices.c
list_devices_LDADD=$(top_builddir)/libwacom/libwacom.la

list_compatible_styli_SOURCES = list-compatible-styli.c
list_compatible_styli_LDADD=$(top_builddir)/libwacom/libwacom.la

bin_PROGRAMS = libwacom-list-local-devices
libwacom_list_local_devices_SOURCES = list-local-devices.c
libwacom_list_local_devices_LDADD=$(top_builddir)/libwacom/libwacom.la $(GLIB_LIBS)
libwacom_list_local_devices_CFLAGS=$(GLIB_CFLAGS)

if HAVE_GTK2
noinst_PROGRAMS += show-svg-image
show_svg_image_SOURCES = show-svg-image.c
show_svg_image_LDADD = $(top_builddir)/libwacom/libwacom.la $(GTK2_LIBS)
show_svg_image_CFLAGS = $(GTK2_CFLAGS)
endif

dist_man1_MANS = libwacom-list-local-devices.man

rules = 65-libwacom.rules
udev_rulesdir=$(UDEV_DIR)/rules.d
dist_udev_rules_DATA = $(rules)

$(rules): generate-udev-rules
	$(builddir)/$< > $@

CLEANFILES = $(rules)
EXTRA_DIST = $(rules)
