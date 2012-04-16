INCLUDES=-I$(top_srcdir)/libwacom -DTOPSRCDIR="\"$(top_srcdir)\""

noinst_PROGRAMS = generate-udev-rules
generate_udev_rules_SOURCES = generate-udev-rules.c
generate_udev_rules_LDADD=$(top_builddir)/libwacom/libwacom.la
