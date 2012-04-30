AM_CPPFLAGS=-I$(top_srcdir)/libwacom -DTOPSRCDIR="\"$(top_srcdir)\""

noinst_PROGRAMS = generate-udev-rules list-devices
generate_udev_rules_SOURCES = generate-udev-rules.c
generate_udev_rules_LDADD=$(top_builddir)/libwacom/libwacom.la

list_devices_SOURCES = list-devices.c
list_devices_LDADD=$(top_builddir)/libwacom/libwacom.la

bin_PROGRAMS = libwacom-list-local-devices
libwacom_list_local_devices_SOURCES = list-local-devices.c
libwacom_list_local_devices_LDADD=$(top_builddir)/libwacom/libwacom.la
libwacom_list_local_devices_CFLAGS=$(GLIB_CFLAGS)

