libwacomlayoutsdir = $(datadir)/libwacom/layouts
layouts = $(shell find $(top_srcdir)/data/layouts -name "*.svg" -printf "%P\n")
dist_libwacomlayouts_DATA =  $(layouts)

EXTRA_DIST = README

check:
	@for file in $(layouts); do \
		$(GREP) -q "$$file" $(top_srcdir)/data/*.tablet || (\
			echo "ERROR: Layout $$file is not referenced" && test); \
		rc="$$(($$rc + $$?))"; \
	done && test 0 -eq $$rc
