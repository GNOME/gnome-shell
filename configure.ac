AC_PREREQ(2.53)

# clutter package version number, (as distinct from shared library version)
# An odd micro number indicates in-progress development, (eg. from CVS)
# An even micro number indicates a released version.
m4_define(clutter_version_major, 0)
m4_define(clutter_version_minor, 1)
m4_define(clutter_version_micro, 1)

AC_INIT([clutter], 
       clutter_version_major.clutter_version_minor.clutter_version_micro, 
        [http://bugzilla.o-hand.com/enter_bug.cgi?product=Clutter])
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
CLUTTER_LT_CURRENT=1
CLUTTER_LT_REV=0
CLUTTER_LT_AGE=1
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

dnl ========================================================================

GTK_DOC_CHECK([1.0])

dnl ========================================================================

AC_SUBST(GCC_FLAGS)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

CLUTTER_CFLAGS="$GLX_CLAGS $CLUTTER_DEPS_CFLAGS"
CLUTTER_LIBS="$GLX_LIBS $CLUTTER_DEPS_LIBS"

AC_SUBST(CLUTTER_CFLAGS)
AC_SUBST(CLUTTER_LIBS)

AC_OUTPUT([Makefile
clutter/pango/Makefile
clutter/Makefile
gtk/Makefile
gtk/gtk-clutter.pc
examples/Makefile
doc/Makefile
doc/reference/Makefile
doc/reference/version.xml
clutter.pc
])

dnl ========================================================================

echo ""
echo "                    Clutter $VERSION"
echo "                 ==================="
echo ""
echo "                   prefix:   ${prefix}"
echo ""
echo "              GTK+ Widget:   ${enable_gtk}"
echo "            Documentation:   ${enable_gtk_doc}"
echo ""
