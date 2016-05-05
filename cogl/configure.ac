AC_PREREQ(2.59)

dnl ================================================================
dnl XXX: If you are making a release then you need to check these
dnl sections:
dnl » API versions (Only the 1.x version needs to change)
dnl   (the pretty numbers that the users see)
dnl
dnl » Interface version details for libtool
dnl   (the shared library versioning information)
dnl
dnl » Source code release status
dnl   (mark the source code as being part of a "release" or from "git")
dnl ================================================================

dnl ================================================================
dnl API versions (i.e. the pretty numbers that users see)
dnl ================================================================
m4_define([cogl_major_version], [2])
m4_define([cogl_minor_version], [0])
m4_define([cogl_micro_version], [0])
m4_define([cogl_version],
          [cogl_major_version.cogl_minor_version.cogl_micro_version])

dnl Since the core Cogl library has to also maintain support for the
dnl Cogl 1.x API for Clutter then we track the 1.x version separately.
m4_define([cogl_1_minor_version], [22])
m4_define([cogl_1_micro_version], [1])
m4_define([cogl_1_version], [1.cogl_1_minor_version.cogl_1_micro_version])

dnl ================================================================
dnl Interface version details for libtool
dnl ================================================================
# Note: we don't automatically deduce the libtool version info from
# the pretty version number that users sees. This is because we want
# to update the pretty version number before making a release since it
# can affect the name of our pkg-config file and the naming or
# location of other installed files which we want to be able to verify
# as correct well before making a release.
#
# For reference on how the various numbers should be updated at
# release time these rules are adapted from the libtool info pages:
#
#  1. Update the version information only immediately before a public
#     release.
#
#  2. If the library source code has changed at all since the last
#     update, then increment REVISION (`C:R:A' becomes `C:r+1:A').
#
#  3. If any interfaces have been added, removed, or changed since the
#     last update, increment CURRENT, and set REVISION to 0.
#
#  4. If any interfaces have been added since the last public release,
#     then increment AGE.
#
#  5. If any interfaces have been removed since the last public release,
#     then set AGE to 0.
m4_define([cogl_lt_current], 24)
m4_define([cogl_lt_revision], 1)
m4_define([cogl_lt_age], 4)
# We do also tell libtool the pretty version:
m4_define([cogl_lt_release], [cogl_version])


dnl ================================================================
dnl Source code release status
dnl ================================================================
# Finally we explicitly track when we are building development source
# from Git vs building source corresponding to a release. As with the
# libtool version info we don't automatically derive this from the
# pretty version number because we want to test the results of
# updating the version number in advance of a release.
#
# Possible status values are: git, snapshot or release
#
m4_define([cogl_release_status], [git])

AC_INIT(cogl, [cogl_1_version])
AC_CONFIG_SRCDIR(cogl/cogl.h)
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build/autotools])
AC_CONFIG_HEADERS(cogl-config.h)
AC_GNU_SOURCE

dnl ================================================================
dnl Required versions for dependencies
dnl ================================================================
m4_define([glib_req_version],           [2.32.0])
m4_define([pangocairo_req_version],     [1.20])
m4_define([gi_req_version],             [0.9.5])
m4_define([gdk_pixbuf_req_version],     [2.0])
m4_define([uprof_req_version],          [0.3])
m4_define([xfixes_req_version],         [3])
m4_define([xcomposite_req_version],     [0.4])
m4_define([xrandr_req_version],         [1.2])
m4_define([cairo_req_version],          [1.10])
m4_define([wayland_server_req_version], [1.1.90])
m4_define([mirclient_req_version],      [0.9.0])

dnl These variables get copied into the generated README
AC_SUBST([GLIB_REQ_VERSION], [glib_req_version])
AC_SUBST([GDK_PIXBUF_REQ_VERSION], [gdk_pixbuf_req_version])
AC_SUBST([CAIRO_REQ_VERSION], [cairo_req_version])
AC_SUBST([PANGOCAIRO_REQ_VERSION], [pangocairo_req_version])
AC_SUBST([XCOMPOSITE_REQ_VERSION], [xcomposite_req_version])
AC_SUBST([XFIXES_REQ_VERSION], [xfixes_req_version])
AC_SUBST([GI_REQ_VERSION], [gi_req_version])
AC_SUBST([UPROF_REQ_VERSION], [uprof_req_version])
AC_SUBST([WAYLAND_SERVER_REQ_VERSION], [wayland_server_req_version])

# Save this value here, since automake will set cflags later and we
# want to know if the user specified custom cflags or not.
cflags_set=${CFLAGS+set}

AM_INIT_AUTOMAKE([1.14 foreign -Wno-portability no-define no-dist-gzip dist-xz tar-ustar subdir-objects])
AM_SILENT_RULES([yes])

AH_BOTTOM([#include "config-custom.h"])

dnl ================================================================
dnl Export the API versioning
dnl ================================================================
AC_SUBST([COGL_MAJOR_VERSION],[cogl_major_version])
AC_SUBST([COGL_MINOR_VERSION],[cogl_minor_version])
AC_SUBST([COGL_MICRO_VERSION],[cogl_micro_version])
AC_SUBST([COGL_VERSION],[cogl_version])
AC_SUBST([COGL_API_VERSION],[cogl_major_version.0])
AC_SUBST([COGL_API_VERSION_AM],[$COGL_MAJOR_VERSION\_0])

AC_SUBST([COGL_1_MINOR_VERSION],[cogl_1_minor_version])
AC_SUBST([COGL_1_MICRO_VERSION],[cogl_1_micro_version])
AC_SUBST([COGL_1_VERSION],[cogl_1_version])


dnl ================================================================
dnl Export the libtool versioning
dnl ================================================================
AC_SUBST([COGL_LT_CURRENT], [cogl_lt_current])
AC_SUBST([COGL_LT_REVISION], [cogl_lt_revision])
AC_SUBST([COGL_LT_AGE], [cogl_lt_age])
AC_SUBST([COGL_LT_RELEASE], [cogl_lt_release])


dnl ================================================================
dnl Export the source code release status
dnl ================================================================
AC_SUBST([COGL_RELEASE_STATUS], [cogl_release_status])

dnl ================================================================
dnl Compiler stuff.
dnl ================================================================
AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX
AM_PROG_CC_C_O
AC_ISC_POSIX
AC_C_CONST

dnl     ============================================================
dnl     Compiler features
dnl     ============================================================
AC_MSG_CHECKING([for _Static_assert])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([_Static_assert (1, "");],
                                   [(void) 0])],
                  [AC_DEFINE([HAVE_STATIC_ASSERT], [1],
                             [Whether _Static_assert can be used or not])
                   AC_MSG_RESULT([yes])],
                  [AC_MSG_RESULT([no])])

dnl ================================================================
dnl Libtool stuff.
dnl ================================================================
dnl AC_PROG_LIBTOOL
dnl LIBTOOL="$LIBTOOL --preserve-dup-deps"
LT_PREREQ([2.2.6])
LT_INIT([disable-static])
dnl when using libtool 2.x create libtool early, because it's used in the
dnl internal glib configure (as-glibconfig.m4)
m4_ifdef([LT_OUTPUT], [LT_OUTPUT])

dnl ================================================================
dnl Find an appropriate libm, for sin() etc.
dnl ================================================================
LT_LIB_M
AC_SUBST(LIBM)

dnl ================================================================
dnl See what platform we are building for
dnl ================================================================
AC_CANONICAL_HOST

dnl     ============================================================
dnl     Installed tests
dnl     ============================================================

AC_ARG_ENABLE(installed_tests,
              AS_HELP_STRING([--enable-installed-tests],
                             [Install test programs (default: no)]),,
              [enable_installed_tests=no])
AM_CONDITIONAL(ENABLE_INSTALLED_TESTS, test x$enable_installed_tests = xyes)

dnl     ============================================================
dnl     Enable debugging
dnl     ============================================================
m4_define([debug_default], [m4_if(cogl_release_status, [git], [yes], [no])])
AC_ARG_ENABLE(
  [debug],
  [AC_HELP_STRING([--enable-debug=@<:@no/yes@:>@], [Control Cogl debugging level @<:@default=]debug_default[@:>@])],
  [],
  enable_debug=debug_default
)
AS_CASE(
  [$enable_debug],
  [yes],
  [
    test "$cflags_set" = set || CFLAGS="$CFLAGS -g -O0"
    COGL_EXTRA_CFLAGS="$COGL_EXTRA_CFLAGS -DCOGL_GL_DEBUG -DCOGL_OBJECT_DEBUG -DCOGL_ENABLE_DEBUG"
  ],
  [no],
  [
    COGL_EXTRA_CFLAGS="$COGL_EXTRA_CFLAGS -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"
  ],
  [AC_MSG_ERROR([Unknown argument for --enable-debug])]
)

AC_SUBST(COGL_DEBUG_CFLAGS)

AC_ARG_ENABLE(
  [unit-tests],
  [AC_HELP_STRING([--enable-unit-tests=@<:@no/yes@:>@], [Build Cogl unit tests @<:@default=yes@:>@])],
  [],
  enable_unit_tests=yes
)
AS_IF([test "x$enable_unit_tests" = "xyes"],
      [
        AC_DEFINE([ENABLE_UNIT_TESTS], [1], [Whether to enable building unit tests])
      ]
)
AM_CONDITIONAL(UNIT_TESTS, test "x$enable_unit_tests" = "xyes")

dnl     ============================================================
dnl     Enable cairo usage for debugging
dnl       (debugging code can use cairo to dump the atlas)
dnl     ============================================================

PKG_CHECK_EXISTS([CAIRO], [cairo >= cairo_req_version], [have_cairo=yes])
AC_ARG_ENABLE(
  [cairo],
  [AC_HELP_STRING([--enable-cairo=@<:@no/yes@:>@], [Control Cairo usage in Cogl debugging code @<:@default=auto@:>@])],
  [],
  [
    AS_IF([test "x$enable_debug" = "xyes"],
          [enable_cairo=$have_cairo],
          [enable_cairo=no])
  ]
)
AS_IF([test "x$enable_cairo" = "xyes" && test "x$enable_debug" = "xyes"],
      [
        AS_IF([test "x$have_cairo" != "xyes"],
              [AC_MSG_ERROR([Could not find Cairo])])

        COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES cairo >= cairo_req_version"
        AC_DEFINE([HAVE_CAIRO], [1], [Whether we have cairo or not])
      ])


dnl     ============================================================
dnl     Enable profiling
dnl     ============================================================
AC_ARG_ENABLE(profile,
              [AC_HELP_STRING([--enable-profile=@<:@no/yes@:>@],
                             [Turn on uprof profiling support. yes; All UProf profiling probe points are compiled in and may be runtime enabled. no; No profiling support will built into cogl.  @<:@default=no@:>@])],
              [],
              [enable_profile=no])
AS_IF([test "x$enable_profile" = "xyes"],
      [
        AS_IF([test "x$GCC" = "xyes"],
              [
                COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES uprof-0.3"
                COGL_EXTRA_CFLAGS="$COGL_EXTRA_CFLAGS -DCOGL_ENABLE_PROFILE"
                AS_IF([test "x$enable_debug" = "xyes"], [COGL_EXTRA_CFLAGS="$COGL_EXTRA_CFLAGS -DUPROF_DEBUG"])
              ],
              [
                AC_MSG_ERROR([--enable-profile is currently only supported if using GCC])
              ])
      ])
AM_CONDITIONAL(PROFILE, test "x$enable_profile" != "xno")


dnl     ============================================================
dnl     Enable strict compiler flags
dnl     ============================================================

# use strict compiler flags only when building from git; the rules for
# distcheck will take care of turning this on when making a release
m4_define([maintainer_default], [m4_if(cogl_release_status, [git], [yes], [no])])
AC_ARG_ENABLE(
  [maintainer-flags],
  [AC_HELP_STRING([--enable-maintainer-flags=@<:@no/yes/error@:>@], [Use strict compiler flags @<:@default=]maintainer_default[@:>@])],
  [],
  enable_maintainer_flags=maintainer_default
)

MAINTAINER_COMPILER_FLAGS="-Wall -Wcast-align -Wformat -Wformat-security
                           -Werror=uninitialized -Werror=no-strict-aliasing
                           -Werror=empty-body -Werror=init-self -Werror=undef
                           -Werror=declaration-after-statement -Werror=vla
                           -Werror=pointer-arith -Werror=missing-declarations
                           -Werror=maybe-uninitialized"

AS_CASE(
  [$enable_maintainer_flags],
  [yes],
  [
    AS_COMPILER_FLAGS([MAINTAINER_CFLAGS], [$MAINTAINER_COMPILER_FLAGS])
  ],
  [no],
  [
  ],
  [error],
  [
    MAINTAINER_COMPILER_FLAGS="$MAINTAINER_COMPILER_FLAGS -Werror"
    AS_COMPILER_FLAGS([MAINTAINER_CFLAGS], [$MAINTAINER_COMPILER_FLAGS])
  ],
  [*],
  [AC_MSG_ERROR([Invalid option for --enable-maintainer-flags])]
)

# strip leading spaces
COGL_EXTRA_CFLAGS="$COGL_EXTRA_CFLAGS ${MAINTAINER_CFLAGS#*  }"
COGL_EXTRA_CFLAGS="$COGL_EXTRA_CFLAGS -Wno-error=sign-compare"


dnl     ============================================================
dnl     Enable deprecation guards
dnl     ============================================================

# disable deprecated options from Glib only when building from git;
# the rules for distcheck will take care of turning this on when
# making a release
m4_define([deprecated_default],
          [m4_if(cogl_release_status, [git], [no], [yes])])

AC_ARG_ENABLE([deprecated],
              [AS_HELP_STRING([--enable-deprecated=@<:@no/yes@:>@],
                              [Whether deprecated symbols should be disabled when compiling Cogl @<:@default=]deprecated_default[@:>@])],
              [],
              [enable_deprecated=deprecated_default])

AS_CASE([$enable_deprecated],

        [no],
        [
          DEPRECATED_CFLAGS="-DG_DISABLE_DEPRECATED -DG_DISABLE_SINGLE_INCLUDES"
        ],

        [yes],
        [
          DEPRECATED_CFLAGS=""
        ],

        [AC_MSG_ERROR([Unknown argument for --enable-deprecated])]
)

# strip leading spaces
COGL_EXTRA_CFLAGS="$COGL_EXTRA_CFLAGS ${DEPRECATED_CFLAGS#*  }"

dnl ================================================================
dnl Check for dependency packages.
dnl ================================================================

AM_PATH_GLIB_2_0([glib_req_version],
                 [],
                 [AC_MSG_ERROR([glib-2.0 is required])],
                 [gobject gthread gmodule-no-export])

COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GLIB_SUPPORT"
COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GTYPE_SUPPORT"
COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES gobject-2.0 gmodule-no-export-2.0"

dnl     ============================================================
dnl     Should cogl-pango be built?
dnl     ============================================================

AC_ARG_ENABLE(
  [cogl-pango],
  [AC_HELP_STRING([--enable-cogl-pango=@<:@no/yes@:>@], [Enable pango support @<:@default=yes@:>@])],
  [],
  enable_cogl_pango=yes
)
AS_IF([test "x$enable_cogl_pango" = "xyes"],
      [
	COGL_PANGO_PKG_REQUIRES="$COGL_PANGO_PKG_REQUIRES pangocairo >= pangocairo_req_version"
      ]
)

dnl     ============================================================
dnl     Should cogl-path be built?
dnl     ============================================================

AC_ARG_ENABLE(
  [cogl-path],
  [AC_HELP_STRING([--enable-cogl-path=@<:@no/yes@:>@], [Enable 2D path support @<:@default=yes@:>@])],
  [],
  enable_cogl_path=yes
)
AS_IF([test "x$enable_cogl_path" = "xyes"],
      [
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_COGL_PATH_SUPPORT"
      ]
)

dnl     ============================================================
dnl     Choose image loading backend
dnl     ============================================================
COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES gdk-pixbuf-2.0 >= gdk_pixbuf_req_version"
COGL_IMAGE_BACKEND="gdk-pixbuf"

dnl     ============================================================
dnl     Determine which drivers and window systems we can support
dnl     ============================================================

dnl         ========================================================
dnl         Drivers first...
dnl         ========================================================
EGL_CHECKED=no

enabled_drivers=""

HAVE_GLES1=0
AC_ARG_ENABLE(
  [gles1],
  [AC_HELP_STRING([--enable-gles1=@<:@no/yes@:>@], [Enable support for OpenGL-ES 1.1 @<:@default=no@:>@])],
  [],
  enable_gles1=no
)
AS_IF([test "x$enable_gles1" = "xyes"],
      [
        enabled_drivers="$enabled_drivers gles1"

        cogl_gl_headers="GLES/gl.h GLES/glext.h"

        AC_DEFINE([HAVE_COGL_GLES], 1, [Have GLES 1.1 for rendering])
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GLES CLUTTER_COGL_HAS_GLES"
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GLES1"
        HAVE_GLES1=1

        PKG_CHECK_EXISTS([glesv1_cm],
          [COGL_PKG_REQUIRES_GL="$COGL_PKG_REQUIRES_GL glesv1_cm"
           COGL_GLES1_LIBNAME="libGLESv1_CM.so"
          ],
          [
            # We have to check the two headers independently as GLES/glext.h
            # needs to include GLES/gl.h to have the GL types defined (eg.
            # GLenum).
            AC_CHECK_HEADER([GLES/gl.h],
                            [],
                            [AC_MSG_ERROR([Unable to locate GLES/gl.h])])
            AC_CHECK_HEADER([GLES/glext.h],
                            [],
                            [AC_MSG_ERROR([Unable to locate GLES/glext.h])],
                            [#include <GLES/gl.h>])

            # Early implementations provided only a GLES/egl.h while Khronos's
            # implementer guide now states EGL/egl.h is the One. Some
            # implementations keep a GLES/egl.h wrapper around EGL/egl.h for
            # backward compatibility while others provide EGL/egl.h only.
            AC_CHECK_HEADERS([GLES/egl.h EGL/egl.h])

            AS_IF([test "x$ac_cv_header_GLES_egl_h" = "xyes"],
                  [COGL_EGL_INCLUDES="#include <GLES/egl.h>"],
                  [test "x$ac_cv_header_EGL_egl_h" = "xyes"],
                  [
                    COGL_EGL_INCLUDES="#include <EGL/egl.h>"
                  ],
                  [AC_MSG_ERROR([Unable to locate EGL header])])
            AC_SUBST([COGL_EGL_INCLUDES])

            AC_CHECK_HEADERS([EGL/eglext.h],
                             [COGL_EGL_INCLUDES="$COGL_EGL_INCLUDE
#include <EGL/eglext.h>"],
                             [],
                             [$COGL_EGL_INCLUDES])

            # Check for a GLES 1.x Common Profile library with/without EGL.
            #
            # Note: historically GLES 1 libraries shipped with the
            # EGL and GLES symbols all bundled in one library. Now
            # the Khronos Implementers Guide defines two naming
            # schemes: -lGLES_CM should be used for a library that
            # bundles the GLES and EGL API together and -lGLESv1_CM
            # would be used for a standalone GLES API.
            AC_CHECK_LIB(GLES_CM, [eglInitialize],
              [COGL_GLES1_LIBNAME="libGLES_CM.so"],
              [
                AC_CHECK_LIB(GLESv1_CM, [glFlush],
                  [COGL_GLES1_LIBNAME="libGLESv1_CM.so"
                   NEED_SEPARATE_EGL=yes
                  ],
                  [AC_MSG_ERROR([Unable to locate required GLES 1.x Common Profile library])])
              ])

            EGL_CHECKED=yes
          ])
      ])

HAVE_GLES2=0
AC_ARG_ENABLE(
  [gles2],
  [AC_HELP_STRING([--enable-gles2=@<:@no/yes@:>@], [Enable support for OpenGL-ES 2.0 @<:@default=no@:>@])],
  [],
  enable_gles2=no
)
AS_IF([test "x$enable_gles2" = "xyes"],
      [
        enabled_drivers="$enabled_drivers gles2"

        cogl_gl_headers="GLES2/gl2.h GLES2/gl2ext.h"
        AC_DEFINE([HAVE_COGL_GLES2], 1, [Have GLES 2.0 for rendering])
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GLES CLUTTER_COGL_HAS_GLES"
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GLES2"
        HAVE_GLES2=1

        PKG_CHECK_EXISTS([glesv2],
          [COGL_PKG_REQUIRES_GL="$COGL_PKG_REQUIRES_GL glesv2"
           COGL_GLES2_LIBNAME="libGLESv2.so"
          ],
          [
            # We have to check the two headers independently as GLES2/gl2ext.h
            # needs to include GLES2/gl2.h to have the GL types defined (eg.
            # GLenum).
            AC_CHECK_HEADER([GLES2/gl2.h],
                            [],
                            [AC_MSG_ERROR([Unable to locate GLES2/gl2.h])])
            AC_CHECK_HEADER([GLES2/gl2ext.h],
                            [],
                            [AC_MSG_ERROR([Unable to locate GLES2/gl2ext.h])],
                            [#include <GLES2/gl2.h>])

            COGL_GLES2_LIBNAME="libGLESv2.so"
          ])
      ])

HAVE_GL=0
AC_ARG_ENABLE(
  [gl],
  [AC_HELP_STRING([--enable-gl=@<:@no/yes@:>@], [Enable support for OpenGL @<:@default=yes@:>@])],
  [],
  [enable_gl=yes]
)
AS_IF([test "x$enable_gl" = "xyes"],
      [
        enabled_drivers="$enabled_drivers gl"

        PKG_CHECK_EXISTS([x11], [ALLOW_GLX=yes])

        cogl_gl_headers="GL/gl.h"

        PKG_CHECK_EXISTS([gl],
        dnl We don't want to use COGL_PKG_REQUIRES here because we don't want to
        dnl directly link against libGL
        [COGL_PKG_REQUIRES_GL="$COGL_PKG_REQUIRES_GL gl"],
        [AC_CHECK_LIB(GL, [glGetString],
        ,
        [AC_MSG_ERROR([Unable to locate required GL library])])
        ])
        COGL_GL_LIBNAME="libGL.so.1"

        AC_DEFINE([HAVE_COGL_GL], [1], [Have GL for rendering])

        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GL"
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS CLUTTER_COGL_HAS_GL"
        HAVE_GL=1
      ])

AM_CONDITIONAL([COGL_DRIVER_GL_SUPPORTED], [test "x$enable_gl" = "xyes"])
AM_CONDITIONAL([COGL_DRIVER_GLES_SUPPORTED],
               [test "x$enable_gles1" = "xyes" || test "x$enable_gles2" = "xyes"])

dnl Allow the GL library names and default driver to be overridden with configure options
AC_ARG_WITH([gl-libname],
            [AS_HELP_STRING([--with-gl-libname],
                            override the name of the GL library to dlopen)],
            [COGL_GL_LIBNAME="$withval"])
AC_ARG_WITH([gles1-libname],
            [AS_HELP_STRING([--with-gles1-libname],
                            override the name of the GLESv1 library to dlopen)],
            [COGL_GLES1_LIBNAME="$withval"])
AC_ARG_WITH([gles2-libname],
            [AS_HELP_STRING([--with-gles2-libname],
                            override the name of the GLESv2 library to dlopen)],
            [COGL_GLES2_LIBNAME="$withval"])
AC_ARG_WITH([default-driver],
            [AS_HELP_STRING([--with-default-driver],
                            specify a default cogl driver)],
            [COGL_DEFAULT_DRIVER="${withval}"],
            [COGL_DEFAULT_DRIVER="" ])

AM_CONDITIONAL(HAVE_COGL_DEFAULT_DRIVER,
  [ test "x$COGL_DEFAULT_DRIVER" != "x" ])


AC_SUBST([COGL_GL_LIBNAME])
AC_SUBST([HAVE_GL])
AC_SUBST([COGL_GLES1_LIBNAME])
AC_SUBST([HAVE_GLES1])
AC_SUBST([COGL_GLES2_LIBNAME])
AC_SUBST([HAVE_GLES2])
AC_SUBST([COGL_DEFAULT_DRIVER])

AC_ARG_ENABLE(
  [cogl-gles2],
  [AC_HELP_STRING([--enable-cogl-gles2=@<:@no/yes@:>@],
                  [Enable libcogl-gles2 frontend api for OpenGL-ES 2.0 @<:@default=auto@:>@])],
  [],
  [
    AS_IF([test "x$HAVE_GLES2" = "x1"],
          [enable_cogl_gles2=yes],
          [enable_cogl_gles2=no])
  ]
)
AS_IF([test "x$enable_cogl_gles2" = "xyes"],
      [
        AS_IF([test "x$HAVE_GLES2" != "x1"],
              [
                AC_MSG_ERROR([libcogl-gles2 is currently only supported on systems with a native GLES 2.0 library])
              ])
      ])
AM_CONDITIONAL([BUILD_COGL_GLES2], [test "x$enable_cogl_gles2" = "xyes"])


dnl         ========================================================
dnl         Check window system integration libraries...
dnl         ========================================================

AC_ARG_ENABLE(
  [glx],
  [AC_HELP_STRING([--enable-glx=@<:@no/yes@:>@], [Enable support GLX @<:@default=auto@:>@])],
  [],
  [AS_IF([test "x$ALLOW_GLX" = "xyes"], [enable_glx=yes], [enable_glx=no])]
)
AS_IF([test "x$enable_glx" = "xyes"],
      [
        AS_IF([test "x$ALLOW_GLX" != "xyes"],
              [AC_MSG_ERROR([GLX not supported with this configuration])])

        NEED_XLIB=yes
        SUPPORT_GLX=yes
        GL_WINSYS_APIS="$GL_WINSYS_APIS glx"

        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GLX_SUPPORT"
      ])
AM_CONDITIONAL(SUPPORT_GLX, [test "x$SUPPORT_GLX" = "xyes"])

EGL_PLATFORM_COUNT=0

AC_ARG_ENABLE(
  [kms-egl-platform],
  [AC_HELP_STRING([--enable-kms-egl-platform=@<:@no/yes@:>@], [Enable support for the KMS egl platform @<:@default=no@:>@])],
  [],
  enable_kms_egl_platform=yes
)
AS_IF([test "x$enable_kms_egl_platform" = "xyes"],
      [
        EGL_PLATFORM_COUNT=$((EGL_PLATFORM_COUNT+1))
        NEED_EGL=yes
        EGL_PLATFORMS="$EGL_PLATFORMS kms"

        PKG_CHECK_EXISTS([gbm],
                         [
                           COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES gbm"
                           COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES libdrm"
                         ],
                         [AC_MSG_ERROR([Unable to locate required libgbm library for the KMS egl platform])])

        GBM_VERSION=`$PKG_CONFIG --modversion gbm`
        GBM_MAJOR=`echo $GBM_VERSION | cut -d'.' -f1`
        GBM_MINOR=`echo $GBM_VERSION | cut -d'.' -f2`
        GBM_MICRO=`echo $GBM_VERSION | cut -d'.' -f3 | sed 's/-.*//'`

        AC_DEFINE_UNQUOTED([COGL_GBM_MAJOR], [$GBM_MAJOR], [The major version for libgbm])
        AC_DEFINE_UNQUOTED([COGL_GBM_MINOR], [$GBM_MINOR], [The minor version for libgbm])
        AC_DEFINE_UNQUOTED([COGL_GBM_MICRO], [$GBM_MICRO], [The micro version for libgbm])

        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_EGL_PLATFORM_KMS_SUPPORT"
      ])
AM_CONDITIONAL(SUPPORT_EGL_PLATFORM_KMS,
               [test "x$enable_kms_egl_platform" = "xyes"])

AC_ARG_ENABLE(
  [wayland-egl-server],
  [AC_HELP_STRING([--enable-wayland-egl-server=@<:@no/yes@:>@], [Enable server side wayland support @<:@default=no@:>@])],
  [],
  enable_wayland_egl_server=yes
)
AS_IF([test "x$enable_wayland_egl_server" = "xyes"],
      [
        NEED_EGL=yes

        PKG_CHECK_MODULES(WAYLAND_SERVER,
                          [wayland-server >= wayland_server_req_version])
        COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES wayland-server >= wayland_server_req_version"

        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT"
      ])
AM_CONDITIONAL(SUPPORT_WAYLAND_EGL_SERVER,
               [test "x$enable_wayland_egl_server" = "xyes"])

dnl This should go last, since it's the default fallback and we need
dnl to check the value of $EGL_PLATFORM_COUNT here.
AC_ARG_ENABLE(
  [xlib-egl-platform],
  [AC_HELP_STRING([--enable-xlib-egl-platform=@<:@no/yes@:>@], [Enable support for the Xlib egl platform @<:@default=auto@:>@])],
  [],
  enable_xlib_egl_platform=yes
)
AS_IF([test "x$enable_xlib_egl_platform" = "xyes"],
      [
        EGL_PLATFORM_COUNT=$((EGL_PLATFORM_COUNT+1))
        NEED_EGL=yes
        NEED_XLIB=yes
        EGL_PLATFORMS="$EGL_PLATFORMS xlib"

        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_EGL_PLATFORM_XLIB_SUPPORT"
      ])
AM_CONDITIONAL(SUPPORT_EGL_PLATFORM_XLIB,
               [test "x$enable_xlib_egl_platform" = "xyes"])

AS_IF([test "x$NEED_EGL" = "xyes" && test "x$EGL_CHECKED" != "xyes"],
      [
        PKG_CHECK_EXISTS([egl],
          [COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES egl"],
          [
            AC_CHECK_HEADERS(
              [EGL/egl.h],
              [],
              [AC_MSG_ERROR([Unable to locate required EGL headers])])
            AC_CHECK_HEADERS(
              [EGL/eglext.h],
              [],
              [AC_MSG_ERROR([Unable to locate required EGL headers])],
              [#include <EGL/egl.h>])

            AC_CHECK_LIB(EGL, [eglInitialize],
              [COGL_EXTRA_LDFLAGS="$COGL_EXTRA_LDFLAGS -lEGL"],
              [AC_MSG_ERROR([Unable to locate required EGL library])])

            COGL_EXTRA_LDFLAGS="$COGL_EXTRA_LDFLAGS -lEGL"
          ]
          )

        COGL_EGL_INCLUDES="#include <EGL/egl.h>
#include <EGL/eglext.h>"
        AC_SUBST([COGL_EGL_INCLUDES])
      ])

AS_IF([test "x$NEED_EGL" = "xyes"],
      [
        SUPPORT_EGL=yes
        GL_WINSYS_APIS="$GL_WINSYS_APIS egl"
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_EGL_SUPPORT"
      ])

AM_CONDITIONAL(SUPPORT_EGL, [test "x$SUPPORT_EGL" = "xyes"])

dnl         ========================================================
dnl         Check X11 dependencies if required
dnl         ========================================================
AS_IF([test "x$NEED_XLIB" = "xyes"],
      [
        X11_MODULES="x11 xext xfixes >= xfixes_req_version xdamage xcomposite >= xcomposite_req_version xrandr >= xrandr_req_version"
        PKG_CHECK_MODULES(DUMMY, [$X11_MODULES],
                          [COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES $X11_MODULES"])
        SUPPORT_X11=yes
        SUPPORT_XLIB=yes

        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_X11"
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_X11_SUPPORT"
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_XLIB"
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_XLIB_SUPPORT"
      ])

AM_CONDITIONAL(X11_TESTS, [test "x$SUPPORT_X11" = "xyes"])
AM_CONDITIONAL(SUPPORT_X11, [test "x$SUPPORT_X11" = "xyes"])
AM_CONDITIONAL(SUPPORT_XLIB, [test "x$SUPPORT_XLIB" = "xyes"])

dnl ================================================================
dnl Documentation stuff.
dnl ================================================================
GLIB_PREFIX="`$PKG_CONFIG --variable=prefix glib-2.0`"
GDKPIXBUF_PREFIX="`$PKG_CONFIG --variable=prefix gdk-pixbuf-2.0`"
AC_SUBST(GLIB_PREFIX)
AC_SUBST(GDKPIXBUF_PREFIX)


AC_SUBST(COGL_PKG_REQUIRES)
if test -n "$COGL_PKG_REQUIRES"; then
  PKG_CHECK_MODULES(COGL_DEP, [$COGL_PKG_REQUIRES])

  if test -n "$COGL_PKG_REQUIRES_GL"; then
    PKG_CHECK_MODULES(COGL_DEP_GL, [$COGL_PKG_REQUIRES_GL])

    dnl Strip out the GL libraries from the GL pkg-config files so we can
    dnl dynamically load them instead
    gl_libs=""
    for x in $COGL_DEP_GL_LIBS; do
      AS_CASE([$x],
              [-lGL], [],
              [-lGLESv2], [],
              [-lGLESv1_CM], [],
              [*], [gl_libs="$gl_libs $x"])
    done
    COGL_DEP_CFLAGS="$COGL_DEP_CFLAGS $COGL_DEP_CFLAGS_GL"
    COGL_DEP_LIBS="$COGL_DEP_LIBS $gl_libs"
  fi
fi
AC_SUBST(COGL_PANGO_PKG_REQUIRES)

AS_IF([test "x$enable_cogl_pango" = "xyes"],
  [PKG_CHECK_MODULES(COGL_PANGO_DEP, [$COGL_PANGO_PKG_REQUIRES])]
)
AM_CONDITIONAL([BUILD_COGL_PANGO], [test "x$enable_cogl_pango" = "xyes"])

AM_CONDITIONAL([BUILD_COGL_PATH], [test "x$enable_cogl_path" = "xyes"])

dnl ================================================================
dnl Misc program dependencies.
dnl ================================================================
AC_PROG_INSTALL

dnl ================================================================
dnl GObject-Introspection check
dnl ================================================================
GOBJECT_INTROSPECTION_CHECK([gi_req_version])

dnl ================================================================
dnl Checks for header files.
dnl ================================================================
AC_PATH_X
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h limits.h unistd.h)
AC_CHECK_HEADER([endian.h],
                [AC_CHECK_DECL([__FLOAT_WORD_ORDER],
                               AC_DEFINE([HAVE_FLOAT_WORD_ORDER], [1],
                                         [Has the __FLOAT_WORD_ORDER macro]))])

dnl ================================================================
dnl Checks for library functions.
dnl ================================================================

dnl The 'ffs' function is part of C99 so it isn't always
dnl available. Cogl has a fallback if needed.
AC_CHECK_FUNCS([ffs])

dnl 'memmem' is a GNU extension but we have a simple fallback
AC_CHECK_FUNCS([memmem])

dnl This is used in the cogl-gles2-gears example but it is a GNU extension
save_libs="$LIBS"
LIBS="$LIBS $LIBM"
AC_CHECK_FUNCS([sincos])
LIBS="$save_libs"

dnl ================================================================
dnl Platform values
dnl ================================================================

dnl These are values from system headers that we want to copy into the
dnl public Cogl headers without having to include the system header
have_poll_h=no
AC_CHECK_HEADER(poll.h,
	       [
		AC_COMPUTE_INT(COGL_SYSDEF_POLLIN, POLLIN, [#include <poll.h>],
			       AC_MSG_ERROR([Unable to get value of POLLIN]))
		AC_COMPUTE_INT(COGL_SYSDEF_POLLPRI, POLLPRI, [#include <poll.h>],
			       AC_MSG_ERROR([Unable to get value of POLLPRI]))
		AC_COMPUTE_INT(COGL_SYSDEF_POLLOUT, POLLOUT, [#include <poll.h>],
			       AC_MSG_ERROR([Unable to get value of POLLOUT]))
		AC_COMPUTE_INT(COGL_SYSDEF_POLLERR, POLLERR, [#include <poll.h>],
			       AC_MSG_ERROR([Unable to get value of POLLERR]))
		AC_COMPUTE_INT(COGL_SYSDEF_POLLHUP, POLLHUP, [#include <poll.h>],
			       AC_MSG_ERROR([Unable to get value of POLLHUP]))
		AC_COMPUTE_INT(COGL_SYSDEF_POLLNVAL, POLLNVAL, [#include <poll.h>],
			       AC_MSG_ERROR([Unable to get value of POLLNVAL]))
		COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_POLL_SUPPORT"
		have_poll_h=yes
	       ])

AS_IF([test "x$have_poll_h" = "xno"],
      [
       COGL_SYSDEF_POLLIN=1
       COGL_SYSDEF_POLLPRI=2
       COGL_SYSDEF_POLLOUT=4
       COGL_SYSDEF_POLLERR=8
       COGL_SYSDEF_POLLHUP=16
       COGL_SYSDEF_POLLNVAL=32
      ])

COGL_DEFINES_EXTRA="$COGL_DEFINES_EXTRA
#define COGL_SYSDEF_POLLIN $COGL_SYSDEF_POLLIN
#define COGL_SYSDEF_POLLPRI $COGL_SYSDEF_POLLPRI
#define COGL_SYSDEF_POLLOUT $COGL_SYSDEF_POLLOUT
#define COGL_SYSDEF_POLLERR $COGL_SYSDEF_POLLERR
#define COGL_SYSDEF_POLLHUP $COGL_SYSDEF_POLLHUP
#define COGL_SYSDEF_POLLNVAL $COGL_SYSDEF_POLLNVAL
"

dnl ================================================================
dnl What needs to be substituted in other files
dnl ================================================================
COGL_DEFINES="$COGL_DEFINES_EXTRA"
for x in $COGL_DEFINES_SYMBOLS; do
  COGL_DEFINES="$COGL_DEFINES
#define $x 1"
done;
AC_SUBST(COGL_DEFINES)
AM_SUBST_NOTMAKE(COGL_DEFINES)

AS_IF([test "x$cogl_gl_headers" = "x"],
      [AC_MSG_ERROR([Internal error: no GL header set])])
dnl cogl_gl_headers is a space separate list of headers to
dnl include. We'll now convert them to a single variable with a
dnl #include line for each header
COGL_GL_HEADER_INCLUDES=""
for x in $cogl_gl_headers; do
  COGL_GL_HEADER_INCLUDES="$COGL_GL_HEADER_INCLUDES
#include <$x>"
done;
AC_SUBST(COGL_GL_HEADER_INCLUDES)
AM_SUBST_NOTMAKE(COGL_GL_HEADER_INCLUDES)

AC_DEFINE([COGL_ENABLE_EXPERIMENTAL_2_0_API], [1],
          [Can use Cogl 2.0 API internally])
AC_DEFINE([COGL_ENABLE_EXPERIMENTAL_API], [1],
          [Can use experimental API internally])

AC_SUBST(COGL_DEP_CFLAGS)
AC_SUBST(COGL_DEP_LIBS)
AC_SUBST(COGL_PANGO_DEP_CFLAGS)
AC_SUBST(COGL_PANGO_DEP_LIBS)
AC_SUBST(COGL_GST_DEP_CFLAGS)
AC_SUBST(COGL_GST_DEP_LIBS)
AC_SUBST(COGL_EXTRA_CFLAGS)
AC_SUBST(COGL_EXTRA_LDFLAGS)

# just for compatability with the clutter build...
MAINTAINER_CFLAGS=
AC_SUBST(MAINTAINER_CFLAGS)

AC_OUTPUT(
Makefile
test-fixtures/Makefile
cogl/Makefile
cogl/mutter-cogl-1.0.pc
cogl/cogl-defines.h
cogl/cogl-gl-header.h
cogl/cogl-egl-defines.h
cogl-pango/Makefile
cogl-pango/mutter-cogl-pango-1.0.pc
cogl-path/Makefile
cogl-path/mutter-cogl-path-1.0.pc
cogl-gles2/Makefile
cogl-gles2/mutter-cogl-gles2-1.0.pc
tests/Makefile
tests/config.env
tests/conform/Makefile
tests/unit/Makefile
tests/micro-perf/Makefile
tests/data/Makefile
)

dnl ================================================================
dnl Dah Da!
dnl ================================================================
echo ""
echo "Cogl - $COGL_1_VERSION/$COGL_VERSION (${COGL_RELEASE_STATUS})"

# Global flags
echo ""
echo " • Global:"
echo "        Prefix: ${prefix}"
if test "x$COGL_DEFAULT_DRIVER" != "x"; then
echo "        Default driver: ${COGL_DEFAULT_DRIVER}"
fi

echo ""
# Features
echo " • Features:"
echo "        Drivers: ${enabled_drivers}"
for driver in $enabled_drivers; do
        driver=`echo $driver | tr "[gles]" "[GLES]"`
        libname=`eval echo \\$COGL_${driver}_LIBNAME`
        echo "        Library name for $driver: $libname"
done
echo "        GL Window System APIs:${GL_WINSYS_APIS}"
if test "x$SUPPORT_EGL" = "xyes"; then
echo "        EGL Platforms:${EGL_PLATFORMS}"
echo "        Wayland compositor support: ${enable_wayland_egl_server}"
fi
echo "        Build libcogl-gles2 GLES 2.0 frontend api: ${enable_cogl_gles2}"
echo "        Image backend: ${COGL_IMAGE_BACKEND}"
echo "        Cogl Pango: ${enable_cogl_pango}"
echo "        Cogl Path: ${enable_cogl_path}"

# Compiler/Debug related flags
echo ""
echo " • Build options:"
echo "        Debugging: ${enable_debug}"
echo "        Profiling: ${enable_profile}"
echo "        Enable deprecated symbols: ${enable_deprecated}"
echo "        Compiler flags: ${CFLAGS} ${COGL_EXTRA_CFLAGS}"
echo "        Linker flags: ${LDFLAGS} ${COGL_EXTRA_LDFLAGS}"

# Miscellaneous
echo ""
echo " • Extra:"
echo "        Build introspection data: ${enable_introspection}"
echo "        Build unit tests: ${enable_unit_tests}"

echo ""

# General warning about experimental features
if test "x$EXPERIMENTAL_CONFIG" = "xyes"; then
echo ""
echo "☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠"
echo " *WARNING*"
echo ""
echo "  The stability of your build might be affected by one or more"
echo "  experimental configuration options."
echo
echo "  experimental options: $EXPERIMENTAL_OPTIONS"
echo "☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠"
echo ""
fi
