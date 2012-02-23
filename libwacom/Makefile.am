lib_LTLIBRARIES=libwacom.la

AM_CPPFLAGS = $(GLIB_CFLAGS) -DDATADIR="\"$(datadir)/libwacom\"" -DG_LOG_DOMAIN="\"$(PACKAGE)\""

libwacom_la_SOURCES = \
                      libwacom.h \
                      libwacomint.h \
                      libwacom.c \
                      libwacom-error.c \
		      libwacom-database.c

libwacom_la_LIBADD = $(GLIB_LIBS)

libwacom_la_LDFLAGS = -version-info $(LIBWACOM_LT_VERSION) -export-symbols-regex '^libwacom_'

libwacomincludedir = $(includedir)/libwacom-1.0/libwacom/
libwacominclude_HEADERS = libwacom.h
