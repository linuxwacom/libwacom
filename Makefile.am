SUBDIRS = libwacom doc test tools

libwacomtabletdir = $(datadir)/libwacom
tablet_files = $(shell cd $(top_srcdir)/data; printf 'data/%s\n' *.tablet)
dist_libwacomtablet_DATA =  $(tablet_files)

libwacomstylusdir = $(datadir)/libwacom
stylus_files = $(shell cd $(top_srcdir)/data; printf 'data/%s\n' *.stylus)
dist_libwacomstylus_DATA = $(stylus_files)

libwacomlayoutsdir = $(datadir)/libwacom/layouts
layouts = $(shell cd $(top_srcdir)/data/layouts; printf 'data/layouts/%s\n' *.svg)
dist_libwacomlayouts_DATA =  $(layouts)

EXTRA_DIST = \
	     data/wacom.example \
	     $(NULL)


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libwacom.pc

EXTRA_DIST = libwacom.pc.in NEWS README.md meson.build meson_options.txt autogen.sh
