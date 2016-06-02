libwacomtabletdir = $(datadir)/libwacom
tablet_files = $(shell find $(top_srcdir)/data -name "*.tablet" -printf "%P\n")
dist_libwacomtablet_DATA =  $(tablet_files)

libwacomstylusdir = $(datadir)/libwacom
stylus_files = $(shell find $(top_srcdir)/data -name "*.stylus" -printf "%P\n")
dist_libwacomstylus_DATA = $(stylus_files)

EXTRA_DIST = wacom.example

SUBDIRS = layouts

check:
	@for file in $(tablet_files); do \
		svg=`$(GREP) 'Layout=' "$(top_srcdir)/data/$$file" | $(SED) -e "s/Layout=//"`; \
		test -z "$$svg" || \
		test -e "$(top_srcdir)/data/layouts/$$svg" || ( \
			echo "ERROR: File $$file references nonexistent $$svg" && test); \
		rc="$$(($$rc + $$?))"; \
	done && test 0 -eq $$rc;
	@(export GIT_DIR="$(top_srcdir)/.git"; \
	 if $(GIT) ls-files >& /dev/null; then \
		for file in $(tablet_files) $(stylus_files); do \
			$(GIT) ls-files --error-unmatch "data/$$file" &> /dev/null || ( \
			echo "ERROR: File $$file is not in git" && test); \
		rc="$$(($$rc + $$?))"; \
		done && test 0 -eq $$rc; \
	fi)
