# clutter package version number, (as distinct from shared library version)
# An odd micro number indicates in-progress development, (eg. from CVS)
# An even micro number indicates a released version.
m4_define([clutter_major_version], [0])
m4_define([clutter_minor_version], [1])
m4_define([clutter_micro_version], [1])

m4_define([clutter_version],
          [clutter_major_version.clutter_minor_version.clutter_micro_version])

m4_define([clutter_api_version],
          [clutter_major_version.clutter_minor_version])

# increase the interface age for each release; if the API changes, set to 0
m4_define([clutter_interface_age], [0])
m4_define([clutter_binary_age],
          [m4_eval(100 * clutter_minor_version + clutter_micro_version)])

AC_PREREQ(2.53)

AC_INIT([clutter], 
        [clutter_version],
        [http://bugzilla.o-hand.com/enter_bug.cgi?product=Clutter])
AC_CONFIG_SRCDIR([clutter/clutter.h])
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([1.7])

CLUTTER_MAJOR_VERSION=clutter_major_version
CLUTTER_MINOR_VERSION=clutter_minor_version
CLUTTER_MICRO_VERSION=clutter_micro_version
CLUTTER_VERSION=clutter_version
CLUTTER_MAJORMINOR=clutter_api_version
AC_SUBST(CLUTTER_MAJOR_VERSION)
AC_SUBST(CLUTTER_MINOR_VERSION)
AC_SUBST(CLUTTER_MICRO_VERSION)
AC_SUBST(CLUTTER_VERSION)
AC_SUBST(CLUTTER_MAJORMINOR)

m4_define([lt_current],
          [m4_eval(100 * clutter_minor_version + clutter_micro_version - clutter_interface_age)])
m4_define([lt_revision], [clutter_interface_age])
m4_define([lt_age], [m4_eval(clutter_binary_age - clutter_interface_age)])
CLUTTER_LT_CURRENT=lt_current
CLUTTER_LT_REV=lt_revision
CLUTTER_LT_AGE=lt_age
CLUTTER_LT_VERSION="$CLUTTER_LT_CURRENT:$CLUTTER_LT_REV:$CLUTTER_LT_AGE"
CLUTTER_LT_LDFLAGS="-version-info $CLUTTER_LT_VERSION"

AC_SUBST(CLUTTER_LT_VERSION)
AC_SUBST(CLUTTER_LT_LDFLAGS)

dnl ========================================================================

# Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
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

PKG_CHECK_MODULES(X11, x11, [have_x11=yes], [have_x11=no])
if test x$have_x11 = xno
then
   AC_PATH_X
   if test x"$x_includes" != x"NONE" && test -n "$x_includes" ; then
     X11_CFLAGS=-I`echo $x_includes | sed -e "s/:/ -I/g"`
   fi
   if test x"$x_libraries" != x"NONE" && test -n "$x_libraries" ; then
     X11_LIBS=-L`echo $x_libraries | sed -e "s/:/ -L/g"`
   fi
fi

AC_CHECK_HEADERS([GL/gl.h GL/glx.h],, 
	         [AC_MSG_ERROR([Unable to locate required GL headers])])

AC_CHECK_LIB(GL, glXCreateContext, HAVE_LIBGL=yes, HAVE_LIBGL=no)

if test "x$HAVE_LIBGL" = "xno"; then
   AC_MSG_ERROR(libGL not found);
else
   GLX_LIBS="$X11_LIBS -lGL"
fi

GLX_CFLAGS="$X11_CFLAGS"

dnl ========================================================================

pkg_modules="pangoft2 glib-2.0 >= 2.8 gthread-2.0 gdk-pixbuf-2.0 gdk-pixbuf-xlib-2.0"
PKG_CHECK_MODULES(CLUTTER_DEPS, [$pkg_modules])

dnl ========================================================================

AC_ARG_ENABLE(gtk,
            [AC_HELP_STRING([--enable-gtk], [Buld Gtk+ Clutter widget])],enable_gtk="$enableval",enable_gtk=no) 

if test "x$enable_gtk" = "xyes"; then
   PKG_CHECK_MODULES(GTK, [gtk+-2.0])
fi

AM_CONDITIONAL(ENABLE_GTK, test x$enable_gtk = xyes)

dnl ========================================================================

if test "x$GCC" = "xyes"; then
        GCC_FLAGS="-g -Wall"
fi

dnl = Enable debug level ===================================================

m4_define([debug_default],
          m4_if(m4_eval(clutter_minor_version % 2), [1], [yes], [minimum]))

AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                             [turn on debugging @<:@default=debug_default@:>@]),
,
              enable_debug=debug_default)

if test "x$enable_debug" = "xyes"; then
  test "$cflags_set" = set || CFLAGS="$CFLAGS -g"
  CLUTTER_DEBUG_CFLAGS="-DCLUTTER_ENABLE_DEBUG"
else
  if test "x$enable_debug" = "xno"; then
    CLUTTER_DEBUG_CFLAGS="-DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DG_DISABLE_CAST
_CHECKS"
  else # minimum
    CLUTTER_DEBUG_CFLAGS="-DG_ENABLE_DEBUG -DG_DISABLE_CAST_CHECKS"
  fi
fi

AC_SUBST(CLUTTER_DEBUG_CFLAGS)

dnl ========================================================================

dnl = GTK Doc check ========================================================

GTK_DOC_CHECK([1.4])

dnl ========================================================================

AC_SUBST(GCC_FLAGS)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

CLUTTER_CFLAGS="$GLX_CLAGS $CLUTTER_DEPS_CFLAGS"
CLUTTER_LIBS="$GLX_LIBS $CLUTTER_DEPS_LIBS"

AC_SUBST(CLUTTER_CFLAGS)
AC_SUBST(CLUTTER_LIBS)

AC_CONFIG_FILES([
        Makefile
        clutter/pango/Makefile
        clutter/Makefile
        clutter/clutter-version.h
        gtk/Makefile
        gtk/gtk-clutter.pc
        examples/Makefile
        doc/Makefile
        doc/reference/Makefile
        doc/reference/version.xml
        clutter.pc
])

AC_OUTPUT

dnl ========================================================================

echo ""
echo "                    Clutter $VERSION"
echo "                 ==================="
echo ""
echo "                   prefix:   ${prefix}"
echo ""
echo "              GTK+ Widget:   ${enable_gtk}"
echo "              Debug level:   ${enable_debug}"
echo "            Documentation:   ${enable_gtk_doc}"
echo ""
