libwacomtabletdir = $(datadir)/libwacom
tablet_files = $(shell cd $(top_srcdir)/data; printf '%s\n' *.tablet)
dist_libwacomtablet_DATA =  $(tablet_files)

libwacomstylusdir = $(datadir)/libwacom
stylus_files = $(shell cd $(top_srcdir)/data; printf '%s\n' *.stylus)
dist_libwacomstylus_DATA = $(stylus_files)

EXTRA_DIST = \
             wacom.example

SUBDIRS = layouts
