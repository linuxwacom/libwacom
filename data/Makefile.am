libwacomtabletdir = $(datadir)/libwacom
dist_libwacomtablet_DATA =  $(shell find $(top_srcdir)/data -name "*.tablet" -printf "%P\n")

libwacomstylusdir = $(datadir)/libwacom
dist_libwacomstylus_DATA =  $(shell find $(top_srcdir)/data -name "*.stylus" -printf "%P\n")

INCLUDES=-I$(top_srcdir)/libwacom -DTOPSRCDIR="\"$(top_srcdir)\""

noinst_PROGRAMS = list
list_SOURCES = list.c
list_LDADD=$(top_builddir)/libwacom/libwacom.la

EXTRA_DIST = wacom.example
