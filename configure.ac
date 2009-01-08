# clutter package version number, (as distinct from shared library version)
# An odd micro number indicates in-progress development, (eg. from CVS)
# An even micro number indicates a released version.
m4_define([clutter_major_version], [0])
m4_define([clutter_minor_version], [9])
m4_define([clutter_micro_version], [0])

m4_define([clutter_version],
          [clutter_major_version.clutter_minor_version.clutter_micro_version])

m4_define([clutter_api_version],
          [clutter_major_version.clutter_minor_version])

# increase the interface age by 2 for each release;
# if the API changes, set to 0
m4_define([clutter_interface_age], [0])
m4_define([clutter_binary_age],
          [m4_eval(100 * clutter_minor_version + clutter_micro_version)])

AC_PREREQ(2.53)

AC_INIT([clutter],
        [clutter_version],
        [http://bugzilla.o-hand.com/enter_bug.cgi?product=Clutter])
AC_CONFIG_SRCDIR([clutter/clutter.h])
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([1.9])

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
AM_PROG_CC_C_O
#_AM_DEPENDENCIES([OBJC]) 
#AC_PROG_OBJC            
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Make libtool respect the make -s switch (kudos to jacob berman)
changequote(,)dnl
LIBTOOL="${LIBTOOL} \$(shell echo \"\$(MFLAGS)\" | awk '/^[^ ]*s/ { print \"--silent\" }')"
changequote([,])dnl

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([memset munmap strcasecmp strdup])

AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])

PKG_PROG_PKG_CONFIG

dnl ========================================================================

# Peek which flavour the user wants so that we can couple the osx flavour with
# quartz imageloader.
if test "x$with_flavour" = "xosx"; then
  imagebackend="quartz"
else
  imagebackend="gdk-pixbuf"
fi

AC_ARG_WITH([imagebackend],
            AC_HELP_STRING([--with-imagebackend=@<:@gdk-pixbuf/quartz/internal@:>@],
                           [Select COGL image loading backend]),
            imagebackend=$with_imagebackend)

case $imagebackend in

  quartz)
    AC_DEFINE([USE_QUARTZ], 1, [Use Core Graphics (Quartz) for loading image files])
    ;;

  gdk-pixbuf)
    AC_DEFINE([USE_GDKPIXBUF], 1, [Use GdkPixbuf for loading image files])
    ;;

  internal)
    ;;


esac


dnl ========================================================================

backendextra=
backendextralib=
clutterbackend=glx
AC_ARG_WITH([flavour],
            AC_HELP_STRING([--with-flavour=@<:@glx/eglx/eglnative/sdl/osx/win32/fruity@:>@],
                           [Select the Clutter backend]),
            clutterbackend=$with_flavour)

case $clutterbackend in
  glx|eglx|eglnative|sdl|osx|win32|fruity) ;;
  *) AC_MSG_ERROR([Invalid flavour $clutterbackend]) ;;
esac

glesversion=1.1
AC_ARG_WITH([gles],
            AC_HELP_STRING([--with-gles=@<:@1.1/2.0@:>@],
                           [Select Clutter GLES version (for EGL backends)]),
            glesversion=$with_gles)

case $glesversion in
  1.1|2.0) ;;
  *) AC_MSG_ERROR([Invalid GL ES version $glesversion])
esac

if test "x$clutterbackend" = "xfruity" 
then
    if test "x$glesversion" != "x1.1"; then
       AC_MSG_ERROR([Fruity backend only supports GL ES 1.1.]);
    fi
    glesversion="fruity"
fi

BACKEND_PC_FILES=""

# Check for X though could be redundant if backend does not need it.
AC_MSG_CHECKING([for X11])
PKG_CHECK_EXISTS([x11], [have_x11=yes], [have_x11=no])
if test "x$have_x11" = "xno"; then
  AC_PATH_X

  if test x"$no_x" = "yes" ; then
    if test "x$clutterbackend" = "xglx" or "x$clutterbackend" = "xeglx"
    then
      AC_MSG_ERROR([No X11 Libraries found and required by backend.])
    fi

    AC_MSG_RESULT([not found])
  else
    if test x"$x_includes" != x"NONE" && test -n "$x_includes" ; then
      X11_CFLAGS=-I`echo $x_includes | sed -e "s/:/ -I/g"`
    fi

    if test x"$x_libraries" != x"NONE" && test -n "$x_libraries" ; then
      X11_LIBS=-L`echo $x_libraries | sed -e "s/:/ -L/g"`
    fi

    AC_MSG_RESULT([found])
  fi
else
  AC_MSG_RESULT([found])
fi

AC_MSG_CHECKING([for XFIXES extension >= 3])
PKG_CHECK_EXISTS([xfixes >= 3], [have_xfixes=yes], [have_xfixes=no])
if test "x$have_xfixes" = "xyes"; then
  AC_DEFINE(HAVE_XFIXES, 1, [Define to 1 if we have the XFIXES X extension])
  X11_LIBS="$X11_LIBS -lXfixes"
  AC_MSG_RESULT([found])
else
  AC_MSG_RESULT([not found])
fi

AC_MSG_CHECKING([for XDAMAGE extension])
PKG_CHECK_EXISTS([xdamage], [have_xdamage=yes], [have_xdamage=no])
if test "x$have_xdamage" = "xyes"; then
  AC_DEFINE(HAVE_XDAMAGE, 1, [Define to 1 if we have the XDAMAGE X extension])
  X11_LIBS="$X11_LIBS -lXdamage"
  AC_MSG_RESULT([found])
else
  AC_MSG_RESULT([not found])
fi

AC_MSG_CHECKING([for XCOMPOSITE extension >= 0.4])
PKG_CHECK_EXISTS([xcomposite >= 0.4], [have_xcomposite=yes], [have_xcomposite=no])
if test "x$have_xcomposite" = "xyes"; then
  AC_DEFINE(HAVE_XCOMPOSITE, 1, [Define to 1 if we have the XCOMPOSITE X extension])
  X11_LIBS="$X11_LIBS -lXcomposite"
  AC_MSG_RESULT([found])
else
  AC_MSG_RESULT([not found])
fi

x11_tests=no

# Currently require all extentions, may not for actual release.
if test "x$clutterbackend" = "xglx" || test "x$clutterbackend" = "xeglx" 
then
  if test "x$have_xdamage" = "xno" || test "x$have_xfixes" = "xno" || test "x$have_xcomposite" = "xno"; then
    AC_MSG_ERROR([Required backend X11 Libraries not found.])
  fi

  if test "x$have_xcomposite" = "xyes"; then
    x11_tests=yes
  fi
fi

AM_CONDITIONAL(X11_TESTS, test "x$x11_tests" != "xno")

xinput=no
AC_ARG_ENABLE(xinput,
      AS_HELP_STRING([--enable-xinput], 
                     ["Use the XINPUT X extension"]),[
          if test "x$enableval" = "xyes" ; then
            PKG_CHECK_MODULES(XINPUT,[xi],
                              xinput=yes,
                              xinput=no)
          fi],
          [xinput=yes])

if test "x$xinput" = "xyes"; then
      AC_DEFINE(USE_XINPUT, 1, Use the XINPUT X extension)
      X11_LIBS="$X11_LIBS -lXi"
fi

dnl This is only used to decide whether to build
dnl tests/tools/disable-npots.la
AC_CHECK_LIB(dl, dlopen, HAVE_LIBDL=yes, HAVE_LIBDL=no)
AM_CONDITIONAL(HAVE_LIBDL, test "x$HAVE_LIBDL" != "xno")

clutter_gl_header=""
use_gles2_wrapper="no"

if test "x$clutterbackend" = "xeglnative" || test "x$clutterbackend" = "xeglx" || test "x$clutterbackend" = "xfruity" 
then
case $glesversion in

  1*)
    clutter_gl_header="GLES/gl.h"
    CLUTTER_COGL="gles"
    AC_DEFINE([HAVE_COGL_GLES], 1, [Have GL/ES for rendering])

    AC_CHECK_HEADERS([GLES/egl.h],,
	         [AC_MSG_ERROR([Unable to locate required GLES headers])])
    AC_CHECK_HEADERS([$clutter_gl_header],,
	         [AC_MSG_ERROR([Unable to locate required GLES headers])])


    # check for upper/lower case libgles_em
    # The powervr sdk uses lower case.
    AC_CHECK_LIB(GLES_CM, eglInitialize, HAVE_LIBGLES=yes, HAVE_LIBGLES=no)
    if test "x$HAVE_LIBGLES" = "xno"; then

        AC_CHECK_LIB(GLESv1_CM, eglInitialize, HAVE_LIBGLES=yes, HAVE_LIBGLES=no)
	if test "x$HAVE_LIBGLES" = "xno"; then

            AC_CHECK_LIB(gles_cm, eglInitialize, HAVE_LIBGLES=yes, HAVE_LIBGLES=no)
            if test "x$HAVE_LIBGLES" = "xno"; then
                AC_MSG_ERROR([GLES library not found and egl backend requested.]);
            else
                GLES_LIBS="-lgles_cm"
            fi
        else
            GLES_LIBS="-lGLESv1_CM"
        fi

   else
        GLES_LIBS="-lGLES_CM"
   fi
  ;;
  2*)
    clutter_gl_header="GLES2/gl2.h"
    CLUTTER_COGL="gles"
    AC_DEFINE([HAVE_COGL_GLES2], 1, [Have GL/ES for rendering])

    AC_CHECK_HEADERS([EGL/egl.h],,
	         [AC_MSG_ERROR([Unable to locate required GLES headers])])

    AC_CHECK_HEADERS([$clutter_gl_header],,
	         [AC_MSG_ERROR([Unable to locate required GLES headers])])

    GLES_LIBS="-lGLESv2 -lEGL"

    use_gles2_wrapper=yes
  ;;
  fruity)
    CLUTTER_COGL="gles"
    AC_DEFINE([HAVE_COGL_GLES], 1, [Have GL/ES for rendering])

    clutter_gl_header="GLES/gl.h"
    CLUTTER_COGL="gles"
    AC_DEFINE([HAVE_COGL_GLES], 1, [Have GL/ES for rendering])

    AC_CHECK_HEADERS([GLES/egl.h],,
                 [AC_MSG_ERROR([Unable to locate required GLES headers])])
    AC_CHECK_HEADERS([$clutter_gl_header],,
                 [AC_MSG_ERROR([Unable to locate required GLES headers])])
  ;;

  *) AC_MSG_ERROR([Invalid GL ES Version '$glesversion' specified])
    ;;

esac
fi

AM_CONDITIONAL(USE_GLES2_WRAPPER, test "x$use_gles2_wrapper" != "xno")

case $clutterbackend in

  sdl)

    clutter_gl_header="GL/gl.h"
    CLUTTER_FLAVOUR="sdl"
    CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_SDL"
    AC_DEFINE([HAVE_CLUTTER_SDL], 1, [Have the SDL backend])

    CLUTTER_COGL="gl"
    AC_DEFINE([HAVE_COGL_GL], 1, [Have GL for rendering])

    AC_PATH_PROG(SDL_CONFIG, sdl-config)
    if test "x$SDL_CONFIG" = "x"; then
       AC_MSG_ERROR([[No sdl-config binary found in path and SDL flavour requested.]])
    else
       SDL_CFLAGS=`$SDL_CONFIG --cflags`
       SDL_LIBS=`$SDL_CONFIG --libs`

       AC_CHECK_HEADERS([$clutter_gl_header],,
                        [AC_MSG_ERROR([Unable to locate required GL headers])])

       dnl Use -lopengl32 under Windows instead of -lGL
       case "$host" in
         *mingw32*)
           SDL_LIBS="$SDL_LIBS -lopengl32"
	   CLUTTER_LT_LDFLAGS="$CLUTTER_LT_LDFLAGS -no-undefined"
           ;;
         *)
	   AC_CHECK_LIB(GL, glEnable, HAVE_LIBGL=yes, HAVE_LIBGL=no)
	   if test "x$HAVE_LIBGL" = "xno"; then
	     AC_MSG_ERROR([libGL not found]);
	   fi
           SDL_LIBS="$SDL_LIBS -lGL"
           ;;
       esac
    fi
    ;;

  glx)

    clutter_gl_header="GL/gl.h"
    CLUTTER_FLAVOUR="glx"
    CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_GLX"
    AC_DEFINE([HAVE_CLUTTER_GLX], 1, [Have the GLX backend])

    CLUTTER_COGL="gl"
    AC_DEFINE([HAVE_COGL_GL], 1, [Have GL for rendering])

    AC_CHECK_HEADERS([$clutter_gl_header],,
                     [AC_MSG_ERROR([Unable to locate required GL headers])])
    AC_CHECK_HEADERS([GL/glx.h],,
                     [AC_MSG_ERROR([Unable to locate required GLX headers])])

    AC_CHECK_LIB(GL, glXCreateContext, HAVE_LIBGLX=yes, HAVE_LIBGLX=no)

    if test "x$HAVE_LIBGLX" = "xno"; then
      AC_MSG_ERROR([GLX not found and GLX backend requested]);
    fi

    GLX_LIBS="$X11_LIBS -lGL"
    GLX_CFLAGS="$X11_CFLAGS"
    backendextra=x11
    backendextralib="x11/libclutter-x11.la"
    ;;

  eglx)

    CLUTTER_FLAVOUR="eglx"
    CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_EGL"
    AC_DEFINE([HAVE_CLUTTER_EGL], 1, [Have the EGL backend])

    EGL_LIBS="$GLES_LIBS $X11_LIBS"
    EGL_CFLAGS="$GLES_CFLAGS $X11_CFLAGS"
    backendextra=x11
    backendextralib="x11/libclutter-x11.la"
    ;;

  eglnative)

    CLUTTER_FLAVOUR="eglnative"
    CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_EGLNATIVE"
    AC_DEFINE([HAVE_CLUTTER_EGL], 1, [Have the EGL backend])

    PKG_CHECK_MODULES(TSLIB, tslib-1.0, [have_tslib=yes], [have_tslib=no])
    if test x$have_tslib = xyes; then
           AC_DEFINE([HAVE_TSLIB], 1, [Have tslib for touchscreen handling])
    fi

    EGL_LIBS="$GLES_LIBS $TSLIB_LIBS"
    EGL_CFLAGS="$TSLIB_CFLAGS"
    ;;

  fruity)

    CLUTTER_FLAVOUR="fruity"
    CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_FRUITY"
    AC_DEFINE([HAVE_CLUTTER_FRUITY], 1, [We're building a fruity version of the eglnative backend])

    EGL_LIBS="-ObjC -framework Foundation  -framework CoreFoundation  -framework CoreGraphics  -framework CoreSurface  -framework GraphicsServices  -framework OpenGLES  -framework LayerKit  -framework UIKit"
    EGL_CFLAGS=""
     ;;

  osx)

    clutter_gl_header="OpenGL/gl.h"
    CLUTTER_FLAVOUR="osx"
    CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_OSX"
    AC_DEFINE([HAVE_CLUTTER_OSX], [1], [Have the OSX backend])

    CLUTTER_COGL="gl"
    AC_DEFINE([HAVE_COGL_GL], [1], [Have GL for rendering])

    OSX_LIBS="-framework Cocoa -framework OpenGL"
    AC_CHECK_HEADERS([$clutter_gl_header],,
                     [AC_MSG_ERROR([Unable to locate required GL headers])])
    ;;

  win32)

    clutter_gl_header="GL/gl.h"
    CLUTTER_FLAVOUR="win32"
    CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_WIN32"
    AC_DEFINE([HAVE_CLUTTER_WIN32], 1, [Have the Win32 backend])

    CLUTTER_COGL="gl"
    AC_DEFINE([HAVE_COGL_GL], 1, [Have GL for rendering])

    AC_CHECK_HEADERS([$clutter_gl_header],,
                     [AC_MSG_ERROR([Unable to locate required GL headers])])

    WIN32_CFLAGS="-D_WIN32_WINNT=0x0500"
    WIN32_LIBS="-lopengl32 -lgdi32 -lwinmm"
    CLUTTER_LT_LDFLAGS="$CLUTTER_LT_LDFLAGS -no-undefined"
    ;;

  *) AC_MSG_ERROR([Invalid backend for Clutter: use glx,sdl,osx,win32,eglx, eglnative or fruity])
    ;;

esac




CLUTTER_GL_HEADER=$clutter_gl_header

AC_SUBST([clutterbackend])
AC_SUBST([imagebackend])
AC_SUBST([backendextra])
AC_SUBST([backendextralib])
AC_SUBST(CLUTTER_FLAVOUR)
AC_SUBST(CLUTTER_COGL)
AC_SUBST(CLUTTER_GL_HEADER)
AC_SUBST(CLUTTER_STAGE_TYPE)

clutterbackendlib=libclutter$CLUTTER_REAL-$clutterbackend-$CLUTTER_MAJORMINOR.la
AC_SUBST([clutterbackendlib])

dnl ========================================================================

# allow building clutter with an external dependency on json-glib
# using the --with-json=check argument, but keep the default to
# the internal version
AC_ARG_WITH([json],
            AC_HELP_STRING([--with-json=@<:@internal/check@:>@],
                           [Select the JSON-GLib copy to use @<:@default=internal@:>@]),
            [],
            [with_json=internal])

case $with_json in
        internal|check) ;;
        *) AC_MSG_ERROR([Invalid value for the JSON library: use internal or check]) ;;
esac

if test "x$with_json" = "xinternal"; then
        JSON_PREFIX=json
        have_json=no
else
        # if the user asked for it and if we have json-glib installed,
        # prefer the system library instead of the internal copy
        AC_MSG_CHECKING([for installed JSON-GLib])
        PKG_CHECK_EXISTS(json-glib-1.0, [have_json=yes], [have_json=no])
        if test "x$have_json" = "xyes"; then
                JSON_PREFIX=json-glib
                JSON_GLIB_PC=json-glib-1.0
                AC_DEFINE(HAVE_JSON_GLIB, 1, [Have the JSON-GLib library installed])
                AC_MSG_RESULT([found])
        else
                JSON_PREFIX=json
                AC_MSG_RESULT([not found, using internal copy])
        fi
fi

AC_SUBST(JSON_PREFIX)
AM_CONDITIONAL(LOCAL_JSON_GLIB, test "x$have_json" = "xno")

CLUTTER_REQUIRES="cairo >= 1.4 pangocairo >= 1.18 gobject-2.0 >= 2.16 gthread-2.0 gmodule-no-export-2.0 $BACKEND_PC_FILES $JSON_GLIB_PC"

if test "x$imagebackend" = "xgdk-pixbuf"; then
        CLUTTER_REQUIRES="$CLUTTER_REQUIRES gdk-pixbuf-2.0"
fi

PKG_CHECK_MODULES(CLUTTER_DEPS, [$CLUTTER_REQUIRES])

AC_SUBST(CLUTTER_REQUIRES)

# prefixes for fixing gtk-doc references
GLIB_PREFIX="`$PKG_CONFIG --variable=prefix glib-2.0`"
PANGO_PREFIX="`$PKG_CONFIG --variable=prefix pango`"
GDKPIXBUF_PREFIX="`$PKG_CONFIG --variable=prefix gdk-pixbuf-2.0`"
AC_SUBST(GLIB_PREFIX)
AC_SUBST(PANGO_PREFIX)
AC_SUBST(GDKPIXBUF_PREFIX)

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
  CPPFLAGS="$CPPFLAGS -Werror -Wall -Wshadow -Wcast-align -Wno-uninitialized -Wempty-body -Wformat-security -Winit-self"
fi


dnl = GTK Doc check ========================================================

GTK_DOC_CHECK([1.8])

# we don't want to build the documentation from a SVN checkout unless we
# explicitly tell configure to do so; this allows avoiding to recurse into
# the documentation directory when building Clutter inside Poky for a
# target device that doesn't have gtk-doc installed. for reference
# see: http://bugzilla.openedhand.com/show_bug.cgi?id=1047
#
# we use autogen.sh as it exists only inside the SVN checkouts, and it
# is not packaged into tarballs.
AM_CONDITIONAL([BUILD_GTK_DOC],
               [test "x$enable_gtk_doc" = "xyes" || test ! -f "autogen.sh"])

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

CLUTTER_CFLAGS="$SDL_CFLAGS $EGL_CFLAGS $GLX_CFLAGS $OSX_CFLAGS $WIN32_CFLAGS $CLUTTER_DEPS_CFLAGS"
CLUTTER_LIBS="$SDL_LIBS $EGL_LIBS $GLX_LIBS $OSX_LIBS $WIN32_LIBS $CLUTTER_DEPS_LIBS"

AC_SUBST(CLUTTER_CFLAGS)
AC_SUBST(CLUTTER_LIBS)

GETTEXT_PACKAGE="clutter-$CLUTTER_API_VERSION"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])

ALL_LINGUAS=""
AM_GLIB_GNU_GETTEXT
GLIB_DEFINE_LOCALEDIR(LOCALEDIR)

AC_CONFIG_FILES([
        Makefile
        clutter/Makefile
        clutter/clutter-version.h
        clutter/clutter-json.h
        clutter/x11/Makefile
        clutter/x11/clutter-x11.pc
        clutter/glx/Makefile
        clutter/eglx/Makefile
        clutter/eglnative/Makefile
        clutter/fruity/Makefile
        clutter/osx/Makefile
        clutter/win32/Makefile
        clutter/win32/clutter-win32.pc
        clutter/sdl/Makefile
        clutter/cogl/Makefile
	clutter/cogl/common/Makefile
        clutter/cogl/cogl.h
        clutter/cogl/cogl-defines-gl.h:clutter/cogl/gl/cogl-defines.h.in
        clutter/cogl/cogl-defines-gles.h:clutter/cogl/gles/cogl-defines.h.in
        clutter/cogl/gl/Makefile
        clutter/cogl/gles/Makefile
        clutter/json/Makefile
        clutter/pango/Makefile
        tests/Makefile
        tests/conform/Makefile
        tests/data/Makefile
        tests/interactive/Makefile
        tests/micro-bench/Makefile
        tests/tools/Makefile
        doc/Makefile
        doc/reference/Makefile
        doc/reference/clutter/Makefile
        doc/reference/clutter/version.xml
        doc/reference/cogl/Makefile
        doc/reference/cogl/version.xml
        doc/manual/clutter-manual.xml
        doc/manual/Makefile
        po/Makefile.in
        clutter.pc
])

AC_OUTPUT

dnl ========================================================================

echo ""
echo "                     Clutter    $VERSION"
echo "                   ===================="
echo ""
echo "                      prefix:   ${prefix}"
echo ""
echo "                     Flavour:   ${clutterbackend}/${CLUTTER_COGL}"
if test "x$clutterbackend" = "xeglx" || test "x$clutterbackend" = "xglx"; then
  echo "                      XInput:   ${xinput}"
fi
echo "                  GL Headers:   ${CLUTTER_GL_HEADER}"
echo "               Image backend:   ${imagebackend}"
echo "              Target library:   ${clutterbackendlib}"
echo "                 Debug level:   ${enable_debug}"
echo "              Compiler flags:   ${CPPFLAGS}"
echo "     Build API Documentation:   ${enable_gtk_doc}"
echo "  Build Manual Documentation:   ${enable_manual}"
echo ""


# you can add more configuration options or flags that will bring up the
# the experimental/unstable warning summary.

if test "x$imagebackend" = "xinternal"; then
  experimental_stuff="$experimental_stuff      imagebackend:  $imagebackend\n"
fi
if test "x$clutterbackend" = "xfruity"; then
  experimental_stuff="$experimental_stuff    clutterbackend:  $clutterbackend\n"
fi


if test "x$experimental_stuff" != "x"; then
echo ""
echo "☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠"
echo " *WARNING*"
echo ""
echo "  Experimental features configured, stability of your build either uses"
echo "  experimental backends or experimental and unsupported features:"
echo ""
echo -e "$experimental_stuff"
echo "☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠"
echo ""
fi


