# clutter package version number, (as distinct from shared library version)
# An odd micro number indicates in-progress development, (eg. from CVS)
# An even micro number indicates a released version.
m4_define([clutter_major_version], [0])
m4_define([clutter_minor_version], [3])
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
CLUTTER_API_VERSION=clutter_api_version
CLUTTER_MAJORMINOR=clutter_api_version
AC_SUBST(CLUTTER_MAJOR_VERSION)
AC_SUBST(CLUTTER_MINOR_VERSION)
AC_SUBST(CLUTTER_MICRO_VERSION)
AC_SUBST(CLUTTER_VERSION)
AC_SUBST(CLUTTER_API_VERSION)
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

CLUTTER_NO_FPU="0"
AC_ARG_WITH(fpu,
            AS_HELP_STRING([--without-fpu],
                           [Assume target hardware has no fpu]),
            [with_fpu=$withval],
            [with_fpu=yes])

if test "x$with_fpu" != "xyes" ; then
   CLUTTER_NO_FPU="1"
fi

AC_SUBST(CLUTTER_NO_FPU)

clutterbackend=glx
AC_ARG_WITH([flavour],
            AC_HELP_STRING([--with-flavour=@<:@glx/eglx/eglnative/sdl@:>@],
                           [Select the Clutter backend]),
            clutterbackend=$with_flavour)

BACKEND_PC_FILES=""

# Check for X though could be redundant if backend does not need it.
PKG_CHECK_MODULES(X11, x11, [have_x11=yes], [have_x11=no])
if test x$have_x11 = xno; then
     AC_PATH_X
     if test x"$no_x" = "yes" ; then
        if test "x$clutterbackend" = "xglx" or "x$clutterbackend" = "xeglx"
        then
	   AC_MSG_ERROR([[No X11 Libraries found and required by backend.]])
	fi
     fi
     if test x"$x_includes" != x"NONE" && test -n "$x_includes" ; then
	X11_CFLAGS=-I`echo $x_includes | sed -e "s/:/ -I/g"`
     fi	     
     if test x"$x_libraries" != x"NONE" && test -n "$x_libraries" ; then
	X11_LIBS=-L`echo $x_libraries | sed -e "s/:/ -L/g"`
     fi
fi
   
PKG_CHECK_MODULES(XFIXES, xfixes >= 4, [have_xfixes=yes], [have_xfixes=no])
if test x$have_xfixes = xyes; then
      AC_DEFINE(HAVE_XFIXES, 1, Have the XFIXES X extension)
      X11_LIBS="$X11_LIBS -lXfixes"
fi

case $clutterbackend in

  sdl)
    CLUTTER_FLAVOUR="sdl"
    AC_DEFINE([HAVE_CLUTTER_SDL], 1, [Have the SDL backend])

    CLUTTER_COGL="gl"
    AC_DEFINE([HAVE_COGL_GL], 1, [Have GL for rendering])

    AC_PATH_PROG(SDL_CONFIG, sdl-config)
    if test "x$SDL_CONFIG" = "x"; then
       AC_MSG_ERROR([[No sdl-config binary found in path and SDL flavour requested.]])
    else
       SDL_CFLAGS=`$SDL_CONFIG --cflags`
       SDL_LIBS=`$SDL_CONFIG --libs`
       AC_CHECK_HEADERS([GL/gl.h],,[AC_MSG_ERROR([Unable to locate required GL headers])])
       AC_CHECK_LIB(GL, glEnable, HAVE_LIBGL=yes, HAVE_LIBGL=no)
       if test "x$HAVE_LIBGL" = "xno"; then
         AC_MSG_ERROR([libGL not found]);
       fi
       SDL_LIBS="$SDL_LIBS -lGL"
     fi
    ;;

  sdles)
#
# Temp Hack for building with dgles (runs atop SDL)
#
    clutterbackend=sdl
    CLUTTER_FLAVOUR="sdl"
    AC_DEFINE([HAVE_CLUTTER_SDL], 1, [Have the SDL backend])

    CLUTTER_COGL="gles"
    AC_DEFINE([HAVE_COGL_GLES], 1, [Have GL for rendering])

    AC_PATH_PROG(SDL_CONFIG, sdl-config)
    if test "x$SDL_CONFIG" = "x"; then
       AC_MSG_ERROR([[No sdl-config binary found in path and SDL flavour requested.]])
    else
       SDL_CFLAGS=`$SDL_CONFIG --cflags`
       SDL_LIBS=`$SDL_CONFIG --libs`
     fi

# FIXME: Obviously we need some real detection here
     SDL_CFLAGS="-I/usr/local/include $SDL_CFLAGS"
     SDL_LIBS="-L/usr/local/lib -lGLES_CM $SDL_LIBS"

     AC_MSG_WARN([])
     AC_MSG_WARN([The SDL/Open GL ES Backend is purely for experimental])
     AC_MSG_WARN([and devlopment purposes. Do not use in production code!!])
     AC_MSG_WARN([])
     ;;

  glx)

    CLUTTER_FLAVOUR="glx"
    AC_DEFINE([HAVE_CLUTTER_GLX], 1, [Have the GLX backend])

    CLUTTER_COGL="gl"
    AC_DEFINE([HAVE_COGL_GL], 1, [Have GL for rendering])

    AC_CHECK_HEADERS([GL/gl.h GL/glx.h],,
	         [AC_MSG_ERROR([Unable to locate required GL headers])])

    AC_CHECK_LIB(GL, glXCreateContext, HAVE_LIBGLX=yes, HAVE_LIBGLX=no)

    if test "x$HAVE_LIBGLX" = "xno"; then
      AC_MSG_ERROR([GLX not found and GLX backend requested]);
    fi

    # xlib pixbuf needed for exporting the stage
    BACKEND_PC_FILES="gdk-pixbuf-xlib-2.0"
 
    GLX_LIBS="$X11_LIBS -lGL"
    GLX_CFLAGS="$X11_CFLAGS"
    ;;

  eglx)
    
    CLUTTER_FLAVOUR="eglx"
    AC_DEFINE([HAVE_CLUTTER_EGL], 1, [Have the EGL backend])

    # We currently assume having egl means also having gles..
    CLUTTER_COGL="gles"
    AC_DEFINE([HAVE_COGL_GLES], 1, [Have GL/ES for rendering])

    # try for libvincent first
    PKG_CHECK_MODULES(EGL, libvincent, HAVE_OGLES=yes, HAVE_OGLES=no)
    if test "x$HAVE_OGLES" = "xno"; then
      AC_CHECK_HEADERS([GLES/egl.h GLES/gl.h],,
	         [AC_MSG_ERROR([Unable to locate required GLES headers])])

      AC_CHECK_LIB(GLES_CM, eglInitialize, HAVE_LIBGLES=yes, HAVE_LIBGLES=no)

      if test "x$HAVE_LIBGLES" = "xno"; then
	  AC_MSG_ERROR([GLES library not found and egl backend requested.]);
      fi

      EGL_LIBS="-lGLES_CM"
    fi

    EGL_LIBS="$EGL_LIBS $X11_LIBS"
    EGL_CFLAGS="$EGL_CFLAGS $X11_CFLAGS"
    ;;

  eglnative)
    
    CLUTTER_FLAVOUR="eglnative"
    AC_DEFINE([HAVE_CLUTTER_EGL], 1, [Have the EGL backend])

    # We currently assume having egl means also having gles..
    CLUTTER_COGL="gles"
    AC_DEFINE([HAVE_COGL_GLES], 1, [Have GL/ES for rendering])

    AC_CHECK_HEADERS([GLES/egl.h GLES/gl.h],,
	         [AC_MSG_ERROR([Unable to locate required GLES headers])])

    AC_CHECK_LIB(GLES_CM, eglInitialize, HAVE_LIBGLES=yes, HAVE_LIBGLES=no)

    if test "x$HAVE_LIBGLES" = "xno"; then
      AC_MSG_ERROR([libGLES_CM not found and egl backend requested.]);
    fi

    PKG_CHECK_MODULES(TSLIB, tslib-1.0, [have_tslib=yes], [have_tslib=no])
    if test x$have_tslib = xyes; then
           AC_DEFINE([HAVE_TSLIB], 1, [Have tslib for touchscreen handling])
    fi

    EGL_LIBS="-lGLES_CM $TSLIB_LIBS"
    EGL_CFLAGS="$TSLIB_CFLAGS"
    ;;


  *) AC_MSG_ERROR([Invalid backend for Clutter: use glx,sdl, eglx or eglnative])
    ;;

esac

AC_SUBST([clutterbackend])
AC_SUBST(CLUTTER_FLAVOUR)
AC_SUBST(CLUTTER_COGL)

clutterbackendlib=libclutter$CLUTTER_REAL-$clutterbackend-$CLUTTER_MAJORMINOR.la
AC_SUBST([clutterbackendlib])

dnl ========================================================================

pkg_modules="pangoft2 glib-2.0 >= 2.10 gobject-2.0 gthread-2.0 gdk-pixbuf-2.0 $BACKEND_PC_FILES"
PKG_CHECK_MODULES(CLUTTER_DEPS, [$pkg_modules])

dnl ========================================================================

if test "x$GCC" = "xyes"; then
        GCC_FLAGS="-g -Wall"
fi

dnl = Enable debug level ===================================================

m4_define([debug_default],
          m4_if(m4_eval(clutter_minor_version % 2), [1], [yes], [minimum]))

AC_ARG_ENABLE(debug,
              AC_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                             [turn on debugging. yes; All glib asserts, checks and runtime clutter verbose messages. minimum; Just glib cast checks and runtime clutter verbose messagaes. no; No glib asserts or checks and no runtime clutter verbose messages.  @<:@default=debug_default@:>@]),
,
              enable_debug=debug_default)

if test "x$enable_debug" = "xyes"; then
  test "$cflags_set" = set || CFLAGS="$CFLAGS -g"
  CLUTTER_DEBUG_CFLAGS="-DCLUTTER_ENABLE_DEBUG"
else
  if test "x$enable_debug" = "xno"; then
    CLUTTER_DEBUG_CFLAGS="-DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"
  else # minimum
    CLUTTER_DEBUG_CFLAGS="-DCLUTTER_ENABLE_DEBUG -DG_DISABLE_CAST_CHECKS"
  fi
fi

AC_SUBST(CLUTTER_DEBUG_CFLAGS)

dnl = Disable fast floating point conversions ==============================

AC_ARG_ENABLE(fast-fp-conversions,
              AC_HELP_STRING([--disable-fast-fp-conversions=@<:@no/yes@:>@],
                             [Turn off fast conversion from floating point to int. no: fast floating point conversion are enabled. yes: fast floating point conversions are disabled.  @<:@default=no@:>@]),
,
              enable_fast_fp_conversions=yes)

if test "x$enable_fast_fp_conversions" = "xno"; then
   FIXED_POINT_CFLAGS="$FIXED_POINT_CFLAGS -DCFX_NO_FAST_CONVERSIONS"
else
   FIXED_POINT_CFLAGS="$FIXED_POINT_CFLAGS -UCFX_NO_FAST_CONVERSIONS"
fi

AC_SUBST(FIXED_POINT_CFLAGS)

dnl = Enable strict compiler flags =========================================

# use strict compiler flags only on development releases
#m4_define([maintainer_flags_default],
#          m4_if(m4_eval(clutter_minor_version % 2), [1], [yes], [no]))
m4_define([maintainer_flags_default], [no])
AC_ARG_ENABLE([maintainer-flags],
              AC_HELP_STRING([--enable-maintainer-flags=@<:@no/yes@:>@],
                             [Use strict compiler flags @<:@default=maintainer_flags_default@:>@]),,
              enable_maintainer_flags=maintainer_flags_default)

if test "x$enable_maintainer_flags" = "xyes"; then
  CPPFLAGS="$CPPFLAGS -Werror -Wall -Wshadow -Wcast-align -Wno-uninitialized"
fi


dnl = GTK Doc check ========================================================

GTK_DOC_CHECK([1.6])

dnl = Manual ===============================================================

AC_ARG_ENABLE(manual,
              AC_HELP_STRING([--enable-manual=@<:@no/yes@:>@],
                             [Build application developers manual. Requires jw and xmlto binaries.]),
              enable_manual=$enableval, enable_manual=no)

if test "x$enable_manual" = "xyes"; then
  AC_PATH_PROG(JW, jw, no)
  if test "x$JW" = "xno"; then
    AC_MSG_ERROR(['jw' program needed by the manual not found in path])
  fi

  AC_PATH_PROG(XMLTO, xmlto, no)
  if test "x$XMLTO" = "xno"; then
    AC_MSG_ERROR(['xmlto' program needed by the manual not found in path])
  fi
fi

AM_CONDITIONAL(ENABLE_MANUAL, test "x$enable_manual" != "xno")

dnl ========================================================================

AC_SUBST(GCC_FLAGS)

CLUTTER_CFLAGS="$SDL_CFLAGS $EGL_CFLAGS $GLX_CFLAGS $CLUTTER_DEPS_CFLAGS \
                $FIXED_POINT_CFLAGS"

CLUTTER_LIBS="$SDL_LIBS $EGL_LIBS $GLX_LIBS $CLUTTER_DEPS_LIBS"

AC_SUBST(CLUTTER_CFLAGS)
AC_SUBST(CLUTTER_LIBS)

AC_CONFIG_FILES([
        Makefile
        clutter/pango/Makefile
        clutter/Makefile
        clutter/clutter-version.h
        clutter/glx/Makefile
        clutter/eglx/Makefile
        clutter/eglnative/Makefile
        clutter/sdl/Makefile
        clutter/cogl/Makefile
        clutter/cogl/gl/Makefile
        clutter/cogl/gles/Makefile
        tests/Makefile
        doc/Makefile
        doc/reference/Makefile
        doc/reference/version.xml
        doc/manual/clutter-manual.xml
        doc/manual/Makefile
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
echo "                  Flavour:   ${clutterbackend}/${CLUTTER_COGL}"
echo "           Target library:   ${clutterbackendlib}"
echo "              Debug level:   ${enable_debug}"
<<<<<<< HEAD:configure.ac
echo "      Fast FP conversions:   ${enable_fast_fp_conversions}"
=======
echo "           Compiler flags:   ${CPPFLAGS}"
>>>>>>> Enable strict compiler flags with a configure flag:configure.ac
echo "        API Documentation:   ${enable_gtk_doc}"
echo "     Manual Documentation:   ${enable_manual}"
echo "              FPU Support:   ${with_fpu}"
echo ""
