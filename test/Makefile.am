noinst_PROGRAMS=test-load test-dbverify test-tablet-validity test-ltversion test-deprecated

TESTS=$(noinst_PROGRAMS)

AM_CPPFLAGS= \
	     -I$(top_srcdir)/libwacom \
	     -I$(top_srcdir)/include \
	     -DTOPSRCDIR="\"$(abs_top_srcdir)\""

test_load_SOURCES = test-load.c
test_load_LDADD=$(top_builddir)/libwacom/libwacom.la $(GLIB_LIBS)
test_load_LDFLAGS = -no-install
test_load_CFLAGS=$(AM_CFLAGS) $(GLIB_CFLAGS)
test_dbverify_SOURCES = test-dbverify.c
test_dbverify_LDADD=$(top_builddir)/libwacom/libwacom.la $(GLIB_LIBS)
test_dbverify_LDFLAGS = -no-install
test_dbverify_CFLAGS=$(AM_CFLAGS) $(GLIB_CFLAGS)
test_tablet_validity_SOURCES = test-tablet-validity.c
test_tablet_validity_LDADD=$(top_builddir)/libwacom/libwacom.la
test_tablet_validity_LDFLAGS = -no-install
test_ltversion_SOURCES = test-ltversion.c
test_ltversion_CFLAGS = $(AM_CFLAGS) -DLIBWACOM_LT_VERSION="\"$(LIBWACOM_LT_VERSION)\""
test_deprecated_SOURCES = test-deprecated.c
test_deprecated_LDADD=$(top_builddir)/libwacom/libwacom.la $(DL_LIBS)
test_deprecated_LDFLAGS = -no-install

if HAVE_LIBXML
noinst_PROGRAMS += test-tablet-svg-validity
test_tablet_svg_validity_SOURCES = test-tablet-svg-validity.c
test_tablet_svg_validity_LDADD = $(top_builddir)/libwacom/libwacom.la $(LIBXML_LIBS)
test_tablet_svg_validity_LDFLAGS = -no-install
test_tablet_svg_validity_CFLAGS = $(LIBXML_CFLAGS)
endif

clean-local: clean-local-check
.PHONY: clean-local-check

clean-local-check:
	rm -rf tmp.dbverify.*
