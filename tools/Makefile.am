AM_CPPFLAGS=-I$(top_srcdir)/libwacom

DBPATH_CFLAGS=-DDATABASEPATH="\"$(abs_top_srcdir)/data\""

noinst_PROGRAMS = list-devices list-compatible-styli
list_devices_SOURCES = list-devices.c
list_devices_LDADD=$(top_builddir)/libwacom/libwacom.la $(GLIB_LIBS)
list_devices_CFLAGS=$(GLIB_CFLAGS) $(DBPATH_CFLAGS)

list_compatible_styli_SOURCES = list-compatible-styli.c
list_compatible_styli_LDADD=$(top_builddir)/libwacom/libwacom.la
list_compatible_styli_CFLAGS=$(DBPATH_CFLAGS)

bin_PROGRAMS = libwacom-list-local-devices libwacom-list-devices

libwacom_list_devices_SOURCES = list-devices.c
libwacom_list_devices_LDADD=$(top_builddir)/libwacom/libwacom.la $(GLIB_LIBS)
libwacom_list_devices_CFLAGS=$(GLIB_CFLAGS)

libwacom_list_local_devices_SOURCES = list-local-devices.c
libwacom_list_local_devices_LDADD=$(top_builddir)/libwacom/libwacom.la $(GLIB_LIBS)
libwacom_list_local_devices_CFLAGS=$(GLIB_CFLAGS)

dist_man1_MANS = libwacom-list-local-devices.man libwacom-list-devices.man

rules = 65-libwacom.rules
udev_rulesdir=$(UDEV_DIR)/rules.d
dist_udev_rules_DATA = $(rules)

hwdb = 65-libwacom.hwdb
udev_hwdbdir=$(UDEV_DIR)/hwdb.d
dist_udev_hwdb_DATA = $(hwdb)

$(hwdb): libwacom-update-db
	$(PYTHON) $(builddir)/libwacom-update-db --buildsystem-mode $(abs_top_srcdir)/data > $@

bin_SCRIPTS = libwacom-show-stylus libwacom-update-db
libwacom-show-stylus: show-stylus.py
	sed -e 's|@DATADIR@|$(datadir)/libwacom|' -e 's|@ETCDIR@|$(sysconfdir)/libwacom|' $< > $@

libwacom-update-db: libwacom-update-db.py
	cp $< $@

CLEANFILES = $(hwdb) $(bin_SCRIPTS)
EXTRA_DIST = $(rules) $(hwdb) show-stylus.py libwacom-update-db.py
