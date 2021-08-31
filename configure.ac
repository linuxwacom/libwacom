# Initialize Autoconf
AC_PREREQ([2.60])
AC_INIT([libwacom], [1.12]) # change meson.build as well
AC_CONFIG_HEADERS([config.h])

# Initialize Automake
AM_INIT_AUTOMAKE([foreign dist-bzip2 no-dist-gzip check-news])
AM_MAINTAINER_MODE([enable])

# Enable silent build when available (Automake 1.11)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Do not modify this, use symbol versioning instead.
LIBWACOM_LT_VERSION=8:1:6
AC_SUBST(LIBWACOM_LT_VERSION)

# Initialize libtool
LT_INIT([disable-static])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_INSTALL
AM_PATH_PYTHON([3])
PKG_PROG_PKG_CONFIG

AC_CHECK_PROG(HAVE_DOXYGEN, [doxygen], [yes], [no])
AM_CONDITIONAL(HAVE_DOXYGEN, test "x$HAVE_DOXYGEN" = xyes)

AC_CHECK_LIB([dl], [dlsym])
AC_SUBST(DL_LIBS)

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.36] gudev-1.0)

PKG_CHECK_MODULES(LIBXML, libxml-2.0 glib-2.0, HAVE_LIBXML="yes", HAVE_LIBXML="no")
AM_CONDITIONAL(HAVE_LIBXML, test x$HAVE_LIBXML = xyes)

udev_dir_default="$libdir/udev"
AC_ARG_WITH(udev-dir,
            AS_HELP_STRING([--with-udev-dir=DIR],
                           [udev base directory [[default=$udev_dir_default]]]),
            [],
            [with_udev_dir="yes"])
AS_CASE($with_udev_dir,
        [no|""], [AC_MSG_ERROR([You must define a udev base directory])],
        [yes], [udevdir="$udev_dir_default"],
        [udevdir="$with_udev_dir"])
UDEV_DIR=${udevdir}
AC_SUBST(UDEV_DIR)

AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 test/Makefile
                 tools/Makefile
                 tools/65-libwacom.rules
                 libwacom/Makefile
                 libwacom.pc])
AC_OUTPUT
