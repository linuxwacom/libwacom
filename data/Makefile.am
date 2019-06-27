libwacomtabletdir = $(datadir)/libwacom
tablet_files = $(shell find $(top_srcdir)/data -name "*.tablet" -printf "%P\n")
dist_libwacomtablet_DATA =  $(tablet_files)

libwacomstylusdir = $(datadir)/libwacom
stylus_files = $(shell find $(top_srcdir)/data -name "*.stylus" -printf "%P\n")
dist_libwacomstylus_DATA = $(stylus_files)

EXTRA_DIST = wacom.example

SUBDIRS = layouts

noinst_SCRIPTS = check-files-in-git.sh check-svg-exists.sh

AM_TESTS_ENVIRONMENT=top_srcdir="$(abs_top_srcdir)"; export top_srcdir;
TESTS = $(noinst_SCRIPTS)
