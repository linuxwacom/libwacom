libwacomtabletdir = $(datadir)/libwacom
tablet_files = $(shell find $(top_srcdir)/data -name "*.tablet" -printf "%P\n")
dist_libwacomtablet_DATA =  $(tablet_files)

libwacomstylusdir = $(datadir)/libwacom
dist_libwacomstylus_DATA =  $(shell find $(top_srcdir)/data -name "*.stylus" -printf "%P\n")

EXTRA_DIST = wacom.example

SUBDIRS = layouts

check:
	@for file in $(tablet_files); do \
		svg=`$(GREP) 'Layout=' "$(top_srcdir)/data/$$file" | $(SED) -e "s/Layout=//"`; \
		test -z "$$svg" || \
		test -e "$(top_srcdir)/data/layouts/$$svg" || ( \
			echo "ERROR: File $$file references nonexistent $$svg" && test); \
		rc="$$(($$rc + $$?))"; \
	done && test 0 -eq $$rc
