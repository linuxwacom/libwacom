lib_LTLIBRARIES=libwacom.la

AM_CPPFLAGS = \
	      -DDATADIR="\"$(datadir)/libwacom\"" \
	      -DETCDIR="\"$(sysconfdir)/libwacom\"" \
	      -DG_LOG_DOMAIN="\"$(PACKAGE)\"" \
	      -I$(top_srcdir)/include \
	      -fvisibility=hidden

libwacom_la_SOURCES = \
                      $(top_srcdir)/include/linux/input-event-codes.h \
                      libwacom.h \
                      libwacomint.h \
                      util-strings.h \
                      libwacom.c \
                      libwacom-error.c \
                      libwacom-database.c \
                      libwacom-deprecated.c

libwacom_la_LIBADD = $(GLIB_LIBS)
libwacom_la_CFLAGS = $(AM_CPPFLAGS) $(GLIB_CFLAGS)

libwacom_la_LDFLAGS = \
		      -version-info $(LIBWACOM_LT_VERSION) \
		      -Wl,--version-script="$(srcdir)/libwacom.sym" \
		      $(NULL)

libwacomincludedir = $(includedir)/libwacom-1.0/libwacom/
libwacominclude_HEADERS = libwacom.h

EXTRA_DIST = libwacom.sym
