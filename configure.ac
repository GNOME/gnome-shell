AC_PREREQ(2.53)

# clutter package version number, (as distinct from shared library version)
# An odd micro number indicates in-progress development, (eg. from CVS)
# An even micro number indicates a released version.
m4_define(clutter_version_major, 0)
m4_define(clutter_version_minor, 0)
m4_define(clutter_version_micro, 1)

AC_INIT([clutter], 
       clutter_version_major.clutter_version_minor.clutter_version_micro, 
        [mallum@o-hand.com])
AC_CONFIG_SRCDIR([clutter/clutter.h])
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([1.7])

CLUTTER_MAJORMINOR=clutter_version_major.clutter_version_minor
AC_SUBST(CLUTTER_MAJORMINOR)

# CURRENT, REVISION, AGE
# - library source changed -> increment REVISION
# - interfaces added/removed/changed -> increment CURRENT, REVISION = 0
# - interfaces added -> increment AGE
# - interfaces removed -> AGE = 0
CLUTTER_LT_CURRENT=0
CLUTTER_LT_REV=0
CLUTTER_LT_AGE=0
CLUTTER_LT_VERSION="$CLUTTER_LT_CURRENT:$CLUTTER_LT_REV:$CLUTTER_LT_AGE"
CLUTTER_LT_LDFLAGS="-version-info $CLUTTER_LT_VERSION"

AC_SUBST(CLUTTER_LT_VERSION)
AC_SUBST(CLUTTER_LT_LDFLAGS)

dnl ========================================================================

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([memset munmap strcasecmp strdup])


dnl ========================================================================

# FIXME: redo below

AC_PATH_XTRA

if test "x$have_x" = "xyes"; then
    GLX_LIBS="$X_LIBS -lX11 -lGL"
    GLX_CFLAGS="$X_CFLAGS"

    save_LIBS="$LIBS"
    save_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $GLX_CFLAGS"
    LIBS="$LIBS $GLX_LIBS"

    AC_MSG_CHECKING([for XTHREADS in Xlib])
    AC_RUN_IFELSE(
      [AC_LANG_PROGRAM([[#include <X11/Xlib.h>]],
        [[return XInitThreads() == 0 ? 0 : 1;]])],
      [xthreads=no],
      [xthreads=yes],
      [xthreads=yes])

    AC_MSG_RESULT($xthreads)

    LIBS="$save_LIBS"
    CFLAGS="$save_CFLAGS"

    if test "x$xthreads" = "xyes"
    then
      GLX_LIBS="$GLX_LIBS -lpthread"
      AC_DEFINE([XTHREADS], [], [1])
    fi
else
    AC_MSG_ERROR([*** Cannot find X + GL ****])
fi

dnl ========================================================================

pkg_modules="pangoft2 glib-2.0 >= 2.8 gthread-2.0 gdk-pixbuf-2.0"
PKG_CHECK_MODULES(CLUTTER, [$pkg_modules])

dnl ========================================================================

GST_MAJORMINOR=0.10

pkg_modules="gstreamer-$GST_MAJORMINOR gstreamer-plugins-base-$GST_MAJORMINOR"
PKG_CHECK_MODULES(GST, [$pkg_modules])

GST_LIBS="$GST_LIBS -lgstinterfaces-$GST_MAJORMINOR -lgstvideo-$GST_MAJORMINOR -lgstaudio-$GST_MAJORMINOR"

dnl ========================================================================

pkg_modules="gtk+-2.0"
PKG_CHECK_MODULES(GTK, [$pkg_modules])

dnl ========================================================================

if test "x$GCC" = "xyes"; then
        GCC_FLAGS="-g -Wall"
fi

dnl ========================================================================

GTK_DOC_CHECK([1.0])

dnl ========================================================================

AC_ARG_ENABLE(python,
            [AC_HELP_STRING([--enable-python], [Compile with python bindings])],enable_python="$enableval",enable_python=no)

if test "x$enable_python" = "xyes"; then
   AC_PATH_PROG(PYTHON, python, no)
   if test x$PYTHON = xno; then
      AC_MSG_ERROR(Please install python)
      fi
      AC_MSG_CHECKING(Python compile flags)
      changequote(<<, >>)dnl
      PY_VER=`$PYTHON -c 'import distutils.sysconfig; print distutils.sysconfig.get_config_vars("VERSION")[0];'`
      PY_LIB=`$PYTHON -c 'import distutils.sysconfig; print distutils.sysconfig.get_python_lib(standard_lib=1);'`
      PY_INC=`$PYTHON -c 'import distutils.sysconfig; print distutils.sysconfig.get_config_vars("INCLUDEPY")[0];'`
      PY_PREFIX=`$PYTHON -c 'import sys; print sys.prefix'`
      PY_EXEC_PREFIX=`$PYTHON -c 'import sys; print sys.exec_prefix'`
      changequote([, ])dnl
      if test -f $PY_INC/Python.h; then
	 PYTHON_LIBS="-L$PY_LIB/config -lpython$PY_VER -lpthread -lutil"
				       PYTHON_CFLAGS="-I$PY_INC"
					AC_MSG_RESULT(ok)
					else
						AC_MSG_ERROR([Can't find Python.h])
						fi
        PKG_CHECK_MODULES(PYGTK, pygtk-2.0)
	PYGTK_CODEGENDIR="`$PKG_CONFIG --variable=codegendir pygtk-2.0`"
	PYGTK_DEFSDIR="`$PKG_CONFIG --variable=defsdir pygtk-2.0`"
	AC_PATH_PROG(PYGTK_CODEGEN, pygtk-codegen-2.0, no)
	if test x$PYGTK_CODEGEN = xno; then
	   AC_MSG_ERROR(Please install the application pygtk-codegen-2.0)
	   fi
	   AC_MSG_CHECKING(for pygtk codegendir)
	   AC_MSG_RESULT($PYGTK_CODEGENDIR)
        AC_MSG_CHECKING(for pygtk defsdir)
        AC_MSG_RESULT($PYGTK_DEFSDIR)

	AC_DEFINE([ENABLE_PYTHON], [1], [Enable python bindings])
else
	PY_VER=""
	PYTHON_CFLAGS=""
	PYTHON_LIBS=""
        PYGTK_CFLAGS=""
        PYGTK_LIBS=""
	PYGTK_CODEGENDIR=""
	PYGTK_CODEGEN=""
	PYGTK_DEFSDIR=""
fi
AC_SUBST(PY_VER)
AC_SUBST(PYTHON_CFLAGS)
AC_SUBST(PYTHON_LIBS)
AC_SUBST(PYGTK_CFLAGS)
AC_SUBST(PYGTK_LIBS)
AC_SUBST(PYGTK_CODEGENDIR)
AC_SUBST(PYGTK_DEFSDIR)

AM_CONDITIONAL(ENABLE_PYTHON, test x$enable_python = xyes)

dnl ========================================================================

AC_SUBST(GCC_FLAGS)
AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

CLUTTER_CFLAGS="$GLX_CLAGS $CLUTTER_CFLAGS"
CLUTTER_LIBS="$GLX_LIBS $CLUTTER_LIBS"

AC_SUBST(CLUTTER_CFLAGS)
AC_SUBST(CLUTTER_LIBS)

AC_OUTPUT([Makefile
clutter/Makefile
gtk/Makefile
bindings/Makefile
bindings/python/Makefile
examples/Makefile
doc/Makefile
doc/reference/Makefile
clutter.pc
])
