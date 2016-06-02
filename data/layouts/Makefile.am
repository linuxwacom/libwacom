libwacomlayoutsdir = $(datadir)/libwacom/layouts
layouts = $(shell find $(top_srcdir)/data/layouts -name "*.svg" -printf "%P\n")
dist_libwacomlayouts_DATA =  $(layouts)

EXTRA_DIST = README

check:
	@for file in $(layouts); do \
		$(GREP) -q "$$file" $(top_srcdir)/data/*.tablet || (\
			echo "ERROR: Layout $$file is not referenced" && test); \
		rc="$$(($$rc + $$?))"; \
	done && test 0 -eq $$rc;
	@(export GIT_DIR="$(top_srcdir)/.git"; \
	if $(GIT) ls-files >& /dev/null; then \
		for file in $(layouts); do \
			$(GIT) ls-files --error-unmatch "data/layouts/$$file" &> /dev/null || ( \
			echo "ERROR: File $$file is not in git" && test); \
		rc="$$(($$rc + $$?))"; \
		done && test 0 -eq $$rc; \
	fi)
