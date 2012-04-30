noinst_PROGRAMS=load dbverify tablet-validity

TESTS=$(noinst_PROGRAMS)

AM_CPPFLAGS=-I$(top_srcdir)/libwacom -DTOPSRCDIR="\"$(top_srcdir)\""

load_LDADD=$(top_builddir)/libwacom/libwacom.la
dbverify_LDADD=$(top_builddir)/libwacom/libwacom.la
tablet_validity_LDADD=$(top_builddir)/libwacom/libwacom.la
