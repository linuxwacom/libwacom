# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([libwacom], [0.1])
AC_CONFIG_HEADERS([config.h])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2 no-dist-gzip check-news])
AM_MAINTAINER_MODE

# Enable silent build when available (Automake 1.11)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Initialize libtool
AC_PROG_LIBTOOL

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
PKG_PROG_PKG_CONFIG

AC_CHECK_PROG(HAVE_DOXYGEN, [doxygen], [yes], [no])
AM_CONDITIONAL(HAVE_DOXYGEN, test "x$HAVE_DOXYGEN" = xyes)

PKG_CHECK_MODULES(GLIB, glib-2.0 gudev-1.0)

AC_CONFIG_FILES([Makefile
                 data/Makefile
                 doc/Makefile
                 test/Makefile
                 libwacom/Makefile
                 libwacom.pc])
AC_OUTPUT
