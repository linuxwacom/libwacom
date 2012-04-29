# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([libwacom], [0.5])
AC_CONFIG_HEADERS([config.h])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2 no-dist-gzip check-news])
AM_MAINTAINER_MODE

# Enable silent build when available (Automake 1.11)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Before making a release, the LIBWACOM_LT_VERSION string should be modified.
# The string is of the form C:R:A.
# - If interfaces have been changed or added, but binary compatibility has
#   been preserved, change to C+1:0:A+1
# - If binary compatibility has been broken (eg removed or changed interfaces)
#   change to C+1:0:0
# - If the interface is the same as the previous version, change to C:R+1:A
LIBWACOM_LT_VERSION=2:0:0
AC_SUBST(LIBWACOM_LT_VERSION)

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
                 tools/Makefile
                 libwacom/Makefile
                 libwacom.pc])
AC_OUTPUT
