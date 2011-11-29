EXTRA_DIST = doxygen.conf

if HAVE_DOXYGEN
EXTRA_DIST += html

all-local: doxygen

doxygen: $(top_srcdir)/libwacom/libwacom.h
	doxygen doxygen.conf

clean-local:
	rm -rf html

.PHONY: doxygen
endif

