EXTRA_DIST = doxygen.conf.in

if HAVE_DOXYGEN
EXTRA_DIST += html

all-local: doxygen

doxygen.conf: doxygen.conf.in
	$(AM_V_GEN) sed -e "s|\@TOPSRCDIR\@|$(top_srcdir)|" $< > $@

doxygen: $(top_srcdir)/libwacom/libwacom.h doxygen.conf
	$(AM_V_GEN) doxygen $(builddir)/doxygen.conf

clean-local:
	rm -rf html

CLEANFILES = doxygen.conf

.PHONY: doxygen
endif

