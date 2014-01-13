noinst_PROGRAMS=load dbverify tablet-validity

TESTS=$(noinst_PROGRAMS)

AM_CPPFLAGS=-I$(top_srcdir)/libwacom -DTOPSRCDIR="\"$(abs_top_srcdir)\""

load_LDADD=$(top_builddir)/libwacom/libwacom.la
load_LDFLAGS = -static
dbverify_LDADD=$(top_builddir)/libwacom/libwacom.la
dbverify_LDFLAGS = -static
tablet_validity_LDADD=$(top_builddir)/libwacom/libwacom.la
tablet_validity_LDFLAGS = -static

if HAVE_LIBXML
noinst_PROGRAMS += tablet-svg-validity
tablet_svg_validity_SOURCES = tablet-svg-validity.c
tablet_svg_validity_LDADD = $(top_builddir)/libwacom/libwacom.la $(LIBXML_LIBS)
tablet_svg_validity_LDFLAGS = -static
tablet_svg_validity_CFLAGS = $(LIBXML_CFLAGS)
endif

clean-local: clean-local-check
.PHONY: clean-local-check

clean-local-check:
	rm -rf tmp.dbverify.*
