AM_CPPFLAGS=-I$(top_srcdir)/libwacom -DDATABASEPATH="\"$(abs_top_srcdir)/data\""

noinst_PROGRAMS = generate-hwdb list-devices list-compatible-styli

generate_hwdb_SOURCES = generate-hwdb.c
generate_hwdb_LDADD=$(top_builddir)/libwacom/libwacom.la $(GLIB_LIBS)
generate_hwdb_CFLAGS=$(GLIB_CFLAGS)

list_devices_SOURCES = list-devices.c
list_devices_LDADD=$(top_builddir)/libwacom/libwacom.la $(GLIB_LIBS)
list_devices_CFLAGS=$(GLIB_CFLAGS)

list_compatible_styli_SOURCES = list-compatible-styli.c
list_compatible_styli_LDADD=$(top_builddir)/libwacom/libwacom.la

bin_PROGRAMS = libwacom-list-local-devices
libwacom_list_local_devices_SOURCES = list-local-devices.c
libwacom_list_local_devices_LDADD=$(top_builddir)/libwacom/libwacom.la $(GLIB_LIBS)
libwacom_list_local_devices_CFLAGS=$(GLIB_CFLAGS)

dist_man1_MANS = libwacom-list-local-devices.man

rules = 65-libwacom.rules
udev_rulesdir=$(UDEV_DIR)/rules.d
dist_udev_rules_DATA = $(rules)

hwdb = 65-libwacom.hwdb
udev_hwdbdir=$(UDEV_DIR)/hwdb.d
dist_udev_hwdb_DATA = $(hwdb)

$(hwdb): generate-hwdb
	$(builddir)/$< > $@

bin_SCRIPTS = libwacom-show-stylus
libwacom-show-stylus: show-stylus.py
	sed -e 's|@DATADIR@|$(datadir)/libwacom|' -e 's|@ETCDIR@|$(sysconfdir)/libwacom|' $< > $@


CLEANFILES = $(hwdb) $(bin_SCRIPTS)
EXTRA_DIST = $(rules) $(hwdb) show-stylus.py
