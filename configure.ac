AC_PREREQ(2.53)
AC_INIT([clutter], 0.0.1, [mallum@o-hand.com])
AC_CONFIG_SRCDIR([clutter/cltr.h])

AM_INIT_AUTOMAKE()

AM_CONFIG_HEADER([config.h])

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

dnl ------ X + GL -------------------------------------------------------------

AC_PATH_XTRA

# below is broken 

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
    AC_MSG_ERROR([*** Cannot find X + GL****])
fi

dnl ----- Pango, glib etc ---------------------------------------------------

pkg_modules="pangoft2 glib-2.0 gthread-2.0"
PKG_CHECK_MODULES(CLTR, pangoft2 glib-2.0 gthread-2.0)

dnl ----- Gstreamer ---------------------------------------------------------

pkg_modules="gstreamer-0.8 gstreamer-interfaces-0.8 gthread-2.0 gstreamer-play-0.8 gstreamer-gconf-0.8"
PKG_CHECK_MODULES(GST, [$pkg_modules])

dnl ----- Gconf -------------------------------------------------------------

PKG_CHECK_MODULES(GCONF, gconf-2.0, HAVE_GCONF="yes", HAVE_GCONF="no")

dnl ------ Check for PNG ---------------------------------------------------

AC_MSG_CHECKING(for libpng12)

if test x$PKG_CONFIG != xno && $PKG_CONFIG --exists libpng12; then 
        AC_MSG_RESULT(yes)
        PNG_LIBS=`$PKG_CONFIG --libs libpng12`
	PNG_CFLAGS=`$PKG_CONFIG --cflags libpng12`
else
	AC_MSG_RESULT(no)
        AC_CHECK_LIB([png], [png_create_read_struct], 
                      [have_png="yes"], [have_png="no"])

        if test x$have_png=xyes && test x$have_png_h=xyes; then 
            PNG_LIBS="-lpng -lz"
        else
	    AC_MSG_ERROR([*** Cannot find libpng12 ****])
	fi
fi

dnl ------ Check for JPEG ---------------------------------------------------

AC_CHECK_LIB([jpeg], [jpeg_read_header], [have_jpg="yes"], [have_jpg="no"])

if test x$have_jpg=xyes && test x$have_jpg_h=xyes; then 
	JPEG_LIBS="-ljpeg"
  else
	AC_MSG_ERROR([*** Cannot find libjpeg ****])
fi

dnl ----- GCC ---------------------------------------------------------------

if test "x$GCC" = "xyes"; then
        GCC_FLAGS="-g -Wall"
fi

AC_SUBST(GCC_FLAGS)

AC_SUBST(GST_CFLAGS)
AC_SUBST(GST_LIBS)

AC_SUBST(GCONF_CFLAGS)
AC_SUBST(GCONF_LIBS)

CLTR_CFLAGS="$GLX_CLAGS $CLTR_CFLAGS"
CLTR_LIBS="$GLX_LIBS $PNG_LIBS $JPEG_LIBS $CLTR_LIBS"

AC_SUBST(CLTR_CFLAGS)
AC_SUBST(CLTR_LIBS)

AC_OUTPUT([Makefile
clutter/Makefile
examples/Makefile
gst/Makefile
])
