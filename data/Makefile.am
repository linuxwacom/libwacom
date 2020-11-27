libwacomtabletdir = $(datadir)/libwacom
tablet_files = $(shell cd $(top_srcdir)/data; printf '%s\n' *.tablet)
dist_libwacomtablet_DATA =  $(tablet_files)

libwacomstylusdir = $(datadir)/libwacom
stylus_files = $(shell cd $(top_srcdir)/data; printf '%s\n' *.stylus)
dist_libwacomstylus_DATA = $(stylus_files)

EXTRA_DIST = \
             wacom.example \
             check-files-in-git.sh \
             check-svg-exists.sh

SUBDIRS = layouts

noinst_SCRIPTS = check-files-in-git.sh check-svg-exists.sh test_data_files.py
EXTRA_DIST += $(noinst_SCRIPTS)

AM_TESTS_ENVIRONMENT=top_srcdir="$(abs_top_srcdir)"; export top_srcdir;
TESTS = $(noinst_SCRIPTS)
