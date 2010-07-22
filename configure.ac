# clutter package version number, (as distinct from shared library version)
# An odd micro number indicates in-progress development from Git
# An even micro number indicates a released version
#
# Making a point release:
# - increase clutter_micro_version to the next even number
# - increase clutter_interface_age to the next even number
#   UNLESS there was an API addition/deprecation, in which case
#   - set clutter_interface_age to 0
# After the release:
# - increase clutter_micro_version to the next odd number
# - increase clutter_interface_version to the next odd number
m4_define([clutter_major_version], [1])
m4_define([clutter_minor_version], [3])
m4_define([clutter_micro_version], [9])

m4_define([clutter_release_status],
          [m4_if(m4_eval(clutter_micro_version % 2), [1], [git],
                 [m4_if(m4_eval(clutter_minor_version % 2), [1], [snapshot],
                                                                 [release])])])

m4_define([clutter_version], [clutter_major_version.clutter_minor_version.clutter_micro_version])

# change this only when breaking the API
m4_define([clutter_api_version], [1.0])

# increase the interface age by 1 for each release; if the API changes,
# set to 0. interface_age and binary_age are used to create the soname
# of the shared object:
#
#  (<minor> * 100 + <micro>) - <interface_age>
#
# this allows using the same soname for different micro-releases in case
# no API was added or deprecated. for instance:
#
#   clutter 1.2.0  -> 100 * 2 + 0  = 200, interface age = 0 -> 200
#   clutter 1.2.2  -> 100 * 2 + 2  = 202, interface age = 2 -> 200
#   clutter 1.2.4  -> 100 * 2 + 4  = 204, interface age = 4 -> 200
#   [ API addition, deprecation ]
#   clutter 1.2.6  -> 100 * 2 + 6  = 206, interface age = 0 -> 206
#   clutter 1.2.8  -> 100 * 2 + 8  = 208, interface age = 2 -> 206
#   clutter 1.2.10 -> 100 * 2 + 10 = 210, interface age = 4 -> 206
#   ...
#
m4_define([clutter_interface_age], [0])
m4_define([clutter_binary_age], [m4_eval(100 * clutter_minor_version + clutter_micro_version)])

AC_PREREQ([2.63])

AC_INIT([clutter],
        [clutter_version],
        [http://bugzilla.clutter-project.org/enter_bug.cgi?product=clutter],
        [clutter],
        [http://www.clutter-project.org])

AC_CONFIG_SRCDIR([clutter/clutter.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build/autotools])

AM_INIT_AUTOMAKE([1.11 foreign -Wno-portability no-define dist-bzip2 check-news])

AM_SILENT_RULES([yes])

CLUTTER_MAJOR_VERSION=clutter_major_version
CLUTTER_MINOR_VERSION=clutter_minor_version
CLUTTER_MICRO_VERSION=clutter_micro_version
CLUTTER_VERSION=clutter_version
CLUTTER_API_VERSION=clutter_api_version
CLUTTER_MAJORMINOR=clutter_major_version.clutter_minor_version
CLUTTER_RELEASE_STATUS=clutter_release_status
AC_SUBST(CLUTTER_MAJOR_VERSION)
AC_SUBST(CLUTTER_MINOR_VERSION)
AC_SUBST(CLUTTER_MICRO_VERSION)
AC_SUBST(CLUTTER_VERSION)
AC_SUBST(CLUTTER_API_VERSION)
AC_SUBST(CLUTTER_MAJORMINOR)
AC_SUBST(CLUTTER_RELEASE_STATUS)

m4_define([lt_current],  [m4_eval(100 * clutter_minor_version + clutter_micro_version - clutter_interface_age)])
m4_define([lt_revision], [clutter_interface_age])
m4_define([lt_age],      [m4_eval(clutter_binary_age - clutter_interface_age)])
CLUTTER_LT_CURRENT=lt_current
CLUTTER_LT_REV=lt_revision
CLUTTER_LT_AGE=lt_age
CLUTTER_LT_VERSION="$CLUTTER_LT_CURRENT:$CLUTTER_LT_REV:$CLUTTER_LT_AGE"
CLUTTER_LT_LDFLAGS="-version-info $CLUTTER_LT_VERSION"

AC_SUBST(CLUTTER_LT_VERSION)
AC_SUBST(CLUTTER_LT_LDFLAGS)
AC_SUBST(CLUTTER_LINK_FLAGS)

dnl ========================================================================

# Checks for programs.
AM_PROG_CC_C_O

# require libtool >= 2.2
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

# Checks for header files.
AC_HEADER_STDC

# Checks for typedefs, structures, and compiler characteristics.
AM_PATH_GLIB_2_0([2.18.0], [have_glib=yes], [have_glib=no],
                 [gobject gthread gmodule-no-export])
AS_IF([test "x$have_glib" = "xno"], AC_MSG_ERROR([glib-2.0 is required]))

# Check for -Bsymbolic-functions to avoid intra-library PLT jumps
clutter_LDFLAGS="${LDFLAGS}"
AC_MSG_CHECKING([for -Bsymbolic-functions linker flag])
LDFLAGS=-Wl,-Bsymbolic-functions
AC_TRY_LINK([], [int main (void) { return 0; }],
            [
              CLUTTER_LINK_FLAGS=-Wl[,]-Bsymbolic-functions
              AC_MSG_RESULT([yes])
            ],
            [
              CLUTTER_LINK_FLAGS=
              AC_MSG_RESULT([no])
            ])
LDFLAGS="${clutter_LDFLAGS}"

dnl ========================================================================

# defaults
CLUTTER_FLAVOUR=glx
FLAVOUR_LIBS=""
FLAVOUR_CFLAGS=""
CLUTTER_WINSYS_BASE=
CLUTTER_WINSYS_BASE_LIB=
CLUTTER_WINSYS=
cogl_gl_headers=""
glesversion=1.1
use_gles2_wrapper=no
experimental_backend=no
experimental_image=no

AC_ARG_WITH([flavour],
            [AC_HELP_STRING([--with-flavour=@<:@glx/opengl-egl-xlib/eglx/eglnative/osx/win32/fruity@:>@],
                            [Select the Clutter window system backend])],
            [CLUTTER_FLAVOUR=$with_flavour])

AC_ARG_WITH([gles],
            [AC_HELP_STRING([--with-gles=@<:@1.1/2.0@:>@],
                            [Select Clutter GLES version (for EGL backends)])],
            [glesversion=$with_gles])

# backend specific pkg-config files
BACKEND_PC_FILES=""

# This is only used to decide whether to build tests/tools/disable-npots.la
AC_CHECK_LIB(dl, [dlopen], [HAVE_LIBDL=yes], [HAVE_LIBDL=no])
AM_CONDITIONAL(HAVE_LIBDL, [test "x$HAVE_LIBDL" = "xyes"])

dnl === Clutter windowing system backend ======================================

AS_CASE([$CLUTTER_FLAVOUR],

        [glx],
        [
          cogl_gl_headers="GL/gl.h"
          CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_GLX"

          SUPPORT_X11=1
          SUPPORT_XLIB=1
          SUPPORT_GLX=1

          COGL_DRIVER="gl"

          CLUTTER_WINSYS=glx
          CLUTTER_WINSYS_BASE=x11
          CLUTTER_WINSYS_BASE_LIB="x11/libclutter-x11.la"
          CLUTTER_SONAME_INFIX=glx

          # Mesa 7.3 added a GL pkg-config file, finally
          PKG_CHECK_EXISTS([gl],
                           [BACKEND_PC_FILES="$BACKEND_PC_FILES gl"],
                           # if a pkg-config file isn't found just add -lGL
                           # and hope for the best.
                           [FLAVOUR_LIBS="$FLAVOUR_LIBS -lGL"])
        ],

        [opengl-egl-xlib],
        [
          cogl_gl_headers="GL/gl.h"
          CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_EGL"

          SUPPORT_X11=1
          SUPPORT_XLIB=1
          SUPPORT_EGL=1
          SUPPORT_EGL_PLATFORM_POWERVR_X11=1

          COGL_DRIVER="gl"

          CLUTTER_WINSYS=egl
          CLUTTER_WINSYS_BASE=x11
          CLUTTER_WINSYS_BASE_LIB="x11/libclutter-x11.la"
          # I think this winsys can be API and ABI compatible with the
          # glx flavour so we can also be cheeky and use the same soname
          CLUTTER_SONAME_INFIX=glx

          PKG_CHECK_EXISTS([gl], [BACKEND_PC_FILES="$BACKEND_PC_FILES gl"], [])
          PKG_CHECK_EXISTS([egl], [BACKEND_PC_FILES="$BACKEND_PC_FILES egl"], [])
        ],

        [eglx],
        [
          # the GL header is defined in the COGL checks above
          CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_EGL"

          SUPPORT_X11=1
          SUPPORT_XLIB=1
          SUPPORT_EGL=1
          SUPPORT_EGL_PLATFORM_POWERVR_X11=1

          COGL_DRIVER="gles"

          CLUTTER_WINSYS=egl
          CLUTTER_WINSYS_BASE=x11
          CLUTTER_WINSYS_BASE_LIB="x11/libclutter-x11.la"
          CLUTTER_SONAME_INFIX=eglx
        ],

        [eglnative],
        [
          # the GL header is defined in the COGL checks above
          CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_EGLNATIVE"

          SUPPORT_EGL=1
          SUPPORT_EGL_PLATFORM_POWERVR_NULL=1

          PKG_CHECK_MODULES(TSLIB, [tslib-1.0], [have_tslib=yes], [have_tslib=no])
          AS_IF([test "x$have_tslib" = "xyes"],
                [AC_DEFINE([HAVE_TSLIB], [1], [Have tslib for touchscreen handling])]
          )

          COGL_DRIVER="gles"

          FLAVOUR_LIBS="$FLAVOUR_LIBS $TSLIB_LIBS"
          FLAVOUR_CFLAGS="$FLAVOUR_CFLAGS $TSLIB_CFLAGS"

          CLUTTER_WINSYS=egl
          CLUTTER_SONAME_INFIX=eglnative
        ],

        [fruity],
        [
          experimental_backend="yes"
          # the GL header is defined in the COGL checks above
          CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_FRUITY"
          AC_DEFINE([HAVE_CLUTTER_FRUITY], [1], [Have the Fruity backend])
          AC_DEFINE([COGL_HAS_EGL_PLATFORM_FRUITY_SUPPORT], [1],
                     [Cogl supports OpenGLES using the EGL API with Fruity platform typedefs])

          COGL_DRIVER="gles"
          # the fruity backend requires a different handling for GLES
          glesversion=fruity

          FLAVOUR_LIBS="$FLAVOUR_LIBS -ObjC -framework Foundation  -framework CoreFoundation  -framework CoreGraphics  -framework CoreSurface  -framework GraphicsServices  -framework OpenGLES  -framework LayerKit  -framework UIKit"

          CLUTTER_WINSYS=fruity
          CLUTTER_SONAME_INFIX=fruity
        ],

        [osx],
        [
          experimental_backend="yes"
          cogl_gl_headers="OpenGL/gl.h"
          CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_OSX"

          AC_DEFINE([HAVE_CLUTTER_OSX], [1], [Have the OSX backend])
          AC_DEFINE([COGL_HAS_OSX_SUPPORT], [1], [Cogl supports the OSX window system])

          COGL_DRIVER="gl"
          AC_DEFINE([HAVE_COGL_GL], [1], [Have GL for rendering])

          FLAVOUR_LIBS="$FLAVOUR_LIBS -framework Cocoa -framework OpenGL"

          CLUTTER_WINSYS=osx
          CLUTTER_SONAME_INFIX=osx
        ],

        [win32],
        [
          cogl_gl_headers="GL/gl.h"
          CLUTTER_STAGE_TYPE="CLUTTER_TYPE_STAGE_WIN32"
          AC_DEFINE([HAVE_CLUTTER_WIN32], [1], [Have the Win32 backend])
          AC_DEFINE([COGL_HAS_WIN32_SUPPORT], [1], [Cogl supports the win32 window system])
          AC_DEFINE([COGL_HAS_WGL_SUPPORT], [1], [Cogl supports OpenGL using the WGL API])

          COGL_DRIVER="gl"
          AC_DEFINE([HAVE_COGL_GL], [1], [Have GL for rendering])

          FLAVOUR_LIBS="$FLAVOUR_LIBS -lopengl32 -lgdi32 -lwinmm"
          FLAVOUR_CFLAGS="$FLAVOUR_CFLAGS -D_WIN32_WINNT=0x0500"
          CLUTTER_LT_LDFLAGS="$CLUTTER_LT_LDFLAGS -no-undefined"

          AC_CHECK_TOOL(WINDRES, windres, no)
          if test "$WINDRES" = no; then
             AC_MSG_ERROR([*** windres is required])
          fi

          CLUTTER_WINSYS=win32
          CLUTTER_SONAME_INFIX=win32
        ],

        [AC_MSG_ERROR([Invalid backend for Clutter: use glx, osx, win32, eglx, eglnative or fruity])]
)

AS_IF([test "x$SUPPORT_X11" = "x1"],
      [ AC_DEFINE([COGL_HAS_X11_SUPPORT], [1], [Cogl supports the X11 window system])])
AS_IF([test "x$SUPPORT_XLIB" = "x1"],
      [ AC_DEFINE([COGL_HAS_XLIB_SUPPORT], [1], [Cogl supports X11 using the Xlib API])])

AS_IF([test "x$SUPPORT_GLX" = "x1"],
      [
        AC_DEFINE([COGL_HAS_GLX_SUPPORT], [1], [Cogl supports OpenGL using the GLX API])

        AC_DEFINE([HAVE_CLUTTER_GLX], [1], [Have the GLX backend])

        AC_CHECK_HEADERS([GL/glx.h],
                         [],
                         [AC_MSG_ERROR([Unable to locate required GLX headers])])

        AC_CHECK_LIB(GL, [glXCreateContext],
                     [HAVE_LIBGLX=yes],
                     [AC_MSG_ERROR([Required GLX library not found])])
      ])

AS_IF([test "x$SUPPORT_EGL" = "x1"],
      [
        AC_DEFINE([HAVE_CLUTTER_EGL], [1], [Have the EGL backend])
        AC_DEFINE([COGL_HAS_EGL_SUPPORT], [1], [Cogl supports GLES using the EGL API])
      ])

AS_IF([test "x$SUPPORT_EGL_POWERVR_X11" = "x1"],
      [
        AC_DEFINE([COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT], [1],
                  [Cogl supports OpenGLES using the EGL API with PowerVR X11 platform typedefs])
      ])

AS_IF([test "x$SUPPORT_EGL_POWERVR_NULL" = "x1"],
      [
        AC_DEFINE([COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT], [1],
                  [Cogl supports OpenGLES using the EGL API with PowerVR NULL platform typedefs])
      ])

# winsys conditionals for use in automake files...
AM_CONDITIONAL(SUPPORT_GLX, [test "x$SUPPORT_GLX" = "x1"])
AM_CONDITIONAL(SUPPORT_X11, [test "x$SUPPORT_X11" = "x1"])
AM_CONDITIONAL(SUPPORT_XLIB, [test "x$SUPPORT_XLIB" = "x1"])
AM_CONDITIONAL(SUPPORT_EGL, [test "x$SUPPORT_EGL" = "x1"])
AM_CONDITIONAL(SUPPORT_EGL_PLATFORM_POWERVR_X11, [test "x$SUPPORT_EGL_PLATFORM_POWERVR_X11" = "x1"])
AM_CONDITIONAL(SUPPORT_EGL_PLATFORM_POWERVR_NULL, [test "x$SUPPORT_EGL_PLATFORM_POWERVR_NULL" = "x1"])
AM_CONDITIONAL(SUPPORT_EGL_PLATFORM_FRUITY, [test "x$CLUTTER_WINSYS" = "xfruity"])
AM_CONDITIONAL(SUPPORT_OSX, [test "x$CLUTTER_WINSYS" = "xosx"])
AM_CONDITIONAL(SUPPORT_WIN32, [test "x$CLUTTER_WINSYS" = "xwin32"])

dnl === COGL driver backend =====================================================

AS_IF([test "x$COGL_DRIVER" = "xgl"],
      [ AC_DEFINE([HAVE_COGL_GL], [1], [Have GL for rendering]) ])

cogl_gles_version_define=""

AS_IF([test "x$COGL_DRIVER" = "xgles"],

      [
        AS_CASE([$glesversion],

          [1*],
          [
            cogl_gl_headers="GLES/gl.h GLES/glext.h"

            AC_DEFINE([HAVE_COGL_GLES], 1, [Have GL/ES for rendering])
            cogl_gles_version_define="COGL_HAS_GLES1"

            PKG_CHECK_EXISTS([glesv1_cm],
              [BACKEND_PC_FILES="$BACKEND_PC_FILES glesv1_cm"
               NEED_SEPARATE_EGL=yes
              ],
              [
                AC_CHECK_HEADERS([$cogl_gl_headers],
                                 [],
                                 [AC_MSG_ERROR([Unable to locate required GLES headers])])

                # Check for a GLES 1.x Common Profile library with/without EGL.
                #
                # Note: historically GLES 1 libraries shipped with the
                # EGL and GLES symbols all bundled in one library. Now
                # the Khronos Implementers Guide defines two naming
                # schemes: -lGLES_CM should be used for a library that
                # bundles the GLES and EGL API together and -lGLESv1_CM
                # would be used for a standalone GLES API.
                AC_CHECK_LIB(GLES_CM, [eglInitialize],
                  [FLAVOUR_LIBS="$FLAVOUR_LIBS -lGLES_CM"],
                  [
                    AC_CHECK_LIB(GLESv1_CM, [glFlush],
                      [FLAVOUR_LIBS="$FLAVOUR_LIBS -lGLESv1_CM"
                       NEED_SEPARATE_EGL=yes
                      ],
                      [AC_MSG_ERROR([Unable to locate required GLES 1.x Common Profile library])])
                  ])
              ])

            AS_IF([test "x$NEED_SEPARATE_EGL" = "xyes"],
              [
                PKG_CHECK_EXISTS([egl],
                  [BACKEND_PC_FILES="$BACKEND_PC_FILES egl"],
                  [
                    AC_CHECK_HEADERS([GLES/egl.h],
                                     [],
                                     [AC_MSG_ERROR([Unable to locate required EGL headers])])

                    AC_CHECK_LIB(EGL, [eglInitialize],
                      [FLAVOUR_LIBS="$FLAVOUR_LIBS -lEGL"],
                      [AC_MSG_ERROR([Unable to locate required EGL library])])
                  ])
              ])
          ],

          [2*],
          [
            cogl_gl_headers="GLES2/gl2.h GLES2/gl2ext.h"
            use_gles2_wrapper=yes
            AC_DEFINE([HAVE_COGL_GLES2], 1, [Have GL/ES for rendering])
            cogl_gles_version_define="COGL_HAS_GLES2"

            PKG_CHECK_EXISTS([glesv2],
              [BACKEND_PC_FILES="$BACKEND_PC_FILES glesv2"],
              [
                AC_CHECK_HEADERS([$cogl_gl_headers],
                                 [],
                                 [AC_MSG_ERROR([Unable to locate required GLES headers])])

                FLAVOUR_LIBS="$FLAVOUR_LIBS -lGLESv2"
              ])

            PKG_CHECK_EXISTS([egl],
              [BACKEND_PC_FILES="$BACKEND_PC_FILES egl"],
              [
                AC_CHECK_HEADERS([EGL/egl.h],
                                 [],
                                 [AC_MSG_ERROR([Unable to locate required EGL headers])])

                AC_CHECK_LIB(EGL, [eglInitialize],
                  [FLAVOUR_LIBS="$FLAVOUR_LIBS -lEGL"],
                  [AC_MSG_ERROR([Unable to locate required EGL library])])

                FLAVOUR_LIBS="$FLAVOUR_LIBS -lEGL"
              ]
              )
          ],

          [fruity],
          [
            cogl_gl_headers="GLES/gl.h GLES/glext.h"
            AC_DEFINE([HAVE_COGL_GLES], 1, [Have GL/ES for rendering])
            cogl_gles_version_define="COGL_HAS_GLES1"

            AC_CHECK_HEADERS([GLES/egl.h],
                             [],
                             [AC_MSG_ERROR([Unable to locate required GLES headers])])
          ],

          [AC_MSG_ERROR([Unknown argument for --with-gles])]
        )
      ]
)

AM_CONDITIONAL(USE_TSLIB, [test "x$have_tslib" = "xyes"])

AM_CONDITIONAL(USE_GLES2_WRAPPER, [test "x$use_gles2_wrapper" = "xyes"])

dnl The value of this variable will directly go in the install
dnl cogl-defines.h header
COGL_DEFINES="";
dnl Space-separated list of symbols that should be defined in
dnl cogl-defines.h
COGL_DEFINES_SYMBOLS="";

if test "x$SUPPORT_X11" = "x1"; then
  COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_X11"
fi;

if test "x$SUPPORT_XLIB" = "x1"; then
  COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_XLIB"
fi;

AS_CASE([$COGL_DRIVER],
        [gl],
        [COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GL"
         COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS CLUTTER_COGL_HAS_GL"],
        [gles],
        [COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GLES"
         COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS CLUTTER_COGL_HAS_GLES"
         COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS $cogl_gles_version_define"
         ],
        [AC_MSG_ERROR([Unknown cogl driver $COGL_DRIVER])])

dnl Copy all of the words in COGL_DEFINES_SYMBOLS as separate #define
dnl lines in COGL_DEFINES
for x in $COGL_DEFINES_SYMBOLS; do
  COGL_DEFINES="$COGL_DEFINES
#define $x 1"
done;

AC_SUBST(COGL_DEFINES)

AS_IF([test "x$cogl_gl_headers" = "x"], [AC_MSG_ERROR([Internal error: no GL header set])])

dnl cogl_gl_headers is a space separate list of headers to
dnl include. We'll now convert them to a single variable with a
dnl #include line for each header
COGL_GL_HEADER_INCLUDES=""
for x in $cogl_gl_headers; do
  COGL_GL_HEADER_INCLUDES="$COGL_GL_HEADER_INCLUDES
#include <$x>"
done;
AC_SUBST(COGL_GL_HEADER_INCLUDES)


dnl === Clutter substitutions =================================================
# Eventually the idea of a winsys should be hidden from Clutter and moved
# into Cogl, but for now we have CLUTTER_WINSYS...
AC_SUBST([CLUTTER_WINSYS])
# The same goes for the winsys-base...
AC_SUBST([CLUTTER_WINSYS_BASE])
AC_SUBST([CLUTTER_WINSYS_BASE_LIB])
AC_SUBST(CLUTTER_STAGE_TYPE)
AC_SUBST(CLUTTER_SONAME_INFIX)

CLUTTER_WINSYS_LIB=libclutter-$CLUTTER_SONAME_INFIX-$CLUTTER_API_VERSION.la
AC_SUBST([CLUTTER_WINSYS_LIB])

dnl === Clutter substitutions kept for backwards compatibility ================
AC_SUBST([CLUTTER_FLAVOUR])
CLUTTER_COGL=$COGL_DRIVER
AC_SUBST([CLUTTER_COGL])

dnl === Cogl substitutions ====================================================
AC_SUBST([COGL_IMAGE_BACKEND])
COGL_WINSYS=$CLUTTER_WINSYS
AC_SUBST([COGL_WINSYS])
AC_SUBST([COGL_DRIVER])

dnl === Image loading backend =================================================
IMAGE_PC_FILES=""

# Peek which winsys the user wants so that we can couple the osx winsys with
# quartz imageloader.
AS_IF([test "x$COGL_WINSYS" = "xosx"], [COGL_IMAGE_BACKEND=quartz], [COGL_IMAGE_BACKEND=gdk-pixbuf])

AC_ARG_WITH([imagebackend],
            [AC_HELP_STRING([--with-imagebackend=@<:@gdk-pixbuf/quartz/internal@:>@],
                            [Select COGL image loading backend])],
            [COGL_IMAGE_BACKEND=$with_imagebackend])

AS_CASE([$COGL_IMAGE_BACKEND],

        [quartz],
        [
          experimental_image=yes
          AC_DEFINE([USE_QUARTZ], 1, [Use Core Graphics (Quartz) for loading image data])
        ],

        [gdk-pixbuf],
        [
          AC_DEFINE([USE_GDKPIXBUF], 1, [Use GdkPixbuf for loading image data])
          IMAGE_PC_FILES="gdk-pixbuf-2.0"
        ],

        [internal],
        [
          experimental_image=yes
          AC_DEFINE([USE_INTERNAL], 1, [Use internal image decoding for loading image data])
        ],

        [AC_MSG_ERROR([Unknown argument for --with-imagebackend])]
)

dnl === X11 checks, only for X11-based backends ===============================
X11_PC_FILES=""
x11_tests=no

AS_IF([test "x$SUPPORT_XLIB" = "x1"],
      [
        # base X11 includes and libraries
        AC_MSG_CHECKING([for X11])

        # start with pkg-config
        PKG_CHECK_EXISTS([x11], [have_x11=yes], [have_x11=no])
        AS_IF([test "x$have_x11" = "xyes"],
              [
                X11_PC_FILES="x11"

                # we actually need to ask for CFLAGS and LIBS
                X11_CFLAGS=`$PKG_CONFIG --cflags $X11_PC_FILES`
                X11_LIBS=`$PKG_CONFIG --libs $X11_PC_FILES`

                AC_MSG_RESULT([found])
              ],
              [
                # no pkg-config, let's go old school
                AC_PATH_X

                AS_IF([test "x$no_x" = "xyes"],
                      [AC_MSG_ERROR([No X11 Libraries found])],
                      [
                        AS_IF([test "x$x_includes" != "xNONE" && test -n "$x_includes"],
                              [X11_CFLAGS=-I`echo $x_includes | sed -e "s/:/ -I/g"`])

                        AS_IF([test "x$x_libraries" != "xNONE" && test -n "$x_libraries"],
                              [X11_LIBS=-L`echo $x_libraries | sed -e "s/:/ -L/g"`])

                        AC_MSG_RESULT([found])
                      ]
                )
              ]
        )

        # XEXT (required)
        AC_MSG_CHECKING([for XEXT extension])
        PKG_CHECK_EXISTS([xext], [have_xext=yes], [have_xext=no])
        AS_IF([test "x$have_xext" = "xyes"],
              [
                AC_DEFINE(HAVE_XEXT, [1], [Define to 1 if we have the XEXT X extension])

                X11_LIBS="$X11_LIBS -lXext"
                X11_PC_FILES="$X11_PC_FILES xext"

                AC_MSG_RESULT([found])
              ],
              [AC_MSG_ERROR([Not found])]
        )

        # XFIXES (required)
        AC_MSG_CHECKING([for XFIXES extension >= 3])
        PKG_CHECK_EXISTS([xfixes >= 3], [have_xfixes=yes], [have_xfixes=no])
        AS_IF([test "x$have_xfixes" = "xyes"],
              [
                AC_DEFINE(HAVE_XFIXES, [1], [Define to 1 if we have the XFIXES X extension])

                X11_LIBS="$X11_LIBS -lXfixes"
                X11_PC_FILES="$X11_PC_FILES xfixes"

                AC_MSG_RESULT([found])
              ],
              [AC_MSG_ERROR([Not found])]
        )

        # XDAMAGE (required)
        AC_MSG_CHECKING([for XDAMAGE extension])
        PKG_CHECK_EXISTS([xdamage], [have_xdamage=yes], [have_xdamage=no])
        AS_IF([test "x$have_xdamage" = "xyes"],
              [
                AC_DEFINE(HAVE_XDAMAGE, [1], [Define to 1 if we have the XDAMAGE X extension])

                X11_LIBS="$X11_LIBS -lXdamage"
                X11_PC_FILES="$X11_PC_FILES xdamage"

                AC_MSG_RESULT([found])
              ],
              [AC_MSG_ERROR([not found])]
        )

        # XCOMPOSITE (optional)
        AC_MSG_CHECKING([for XCOMPOSITE extension >= 0.4])
        PKG_CHECK_EXISTS([xcomposite >= 0.4], [have_xcomposite=yes], [have_xcomposite=no])
        AS_IF([test "x$have_xcomposite" = "xyes"],
              [
                AC_DEFINE(HAVE_XCOMPOSITE, [1], [Define to 1 if we have the XCOMPOSITE X extension])

                X11_LIBS="$X11_LIBS -lXcomposite"
                X11_PC_FILES="$X11_PC_FILES xcomposite"

                AC_MSG_RESULT([found])
              ],
              [AC_MSG_ERROR([not found])]
        )

        # XINPUT (optional)
        xinput=no
        AC_ARG_ENABLE([xinput],
                      [AS_HELP_STRING([--enable-xinput], [Use the XINPUT X extension])],
                      [
                        AS_IF([test "x$enableval" = "xyes"],
                              [PKG_CHECK_MODULES(XINPUT, [xi], [xinput=yes], [xinput=no])]
                        )
                      ],
                      [xinput=no])

        AS_CASE([$xinput],

                [yes],
                [
                  AC_DEFINE(HAVE_XINPUT, 1, [Use the XINPUT X extension])

                  X11_LIBS="$X11_LIBS -lXi"
                  X11_PC_FILES="$X11_PC_FILES xi"
                ],

                [no],
                [],
        )

        # XKB
        clutter_save_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $X11_CFLAGS"

        clutter_save_LIBS="$LIBS"
        LIBS="$LIBS $X11_LIBS"

        have_xkb=no
        AC_CHECK_FUNC([XkbQueryExtension],
                      [
                        AC_DEFINE(HAVE_XKB, 1, [Define to use XKB extension])
                        have_xkb=yes
                      ])

        CPPFLAGS="$clutter_save_CPPFLAGS"
        LIBS="$clutter_save_LIBS"

        x11_tests=yes
        BACKEND_PC_FILES="$BACKEND_PC_FILES $X11_PC_FILES"
        FLAVOUR_LIBS="$FLAVOUR_LIBS $X11_LIBS"
        FLAVOUR_CFLAGS="$FLAVOUR_CFLAGS $X11_CFLAGS"
      ]
)

AM_CONDITIONAL(X11_TESTS, [test "x$x11_tests" = "xyes"])

dnl === JSON parser check =====================================================

# we allow building clutter with the internal copy of json-glib
# for platforms without it, but by default we depend on the
# system copy
m4_define([default_json], [check])
AC_ARG_WITH([json],
            AC_HELP_STRING([--with-json=@<:@internal/check/system@:>@],
                           [Select the JSON-GLib copy to use @<:@default=default_json@:>@]),
            [],
            [with_json=default_json])

experimental_json=no
AS_CASE([$with_json],

        [internal],
        [
          JSON_PREFIX=json
          experimental_json=yes
          have_json=no
        ],

        [system],
        [
          AC_MSG_CHECKING([for installed JSON-GLib])
          PKG_CHECK_EXISTS([json-glib-1.0 >= 0.10],
                           [have_json=yes],
                           [have_json=no])
          AS_IF([test "x$have_json" = "xyes"],
                [
                  JSON_PREFIX=json-glib
                  JSON_GLIB_PC="json-glib-1.0 >= 0.10"

                  AC_DEFINE([HAVE_JSON_GLIB],
                            [1],
                            [Have the JSON-GLib library installed])

                  AC_MSG_RESULT([found])
                ],
                [AC_MSG_ERROR([not found])]
          )
        ],

        [check],
        [
          AC_MSG_CHECKING([for installed JSON-GLib])
          PKG_CHECK_EXISTS([json-glib-1.0 >= 0.8],
                           [have_json=yes],
                           [have_json=no])
          AS_IF([test "x$have_json" = "xyes"],
                [
                  JSON_PREFIX=json-glib
                  JSON_GLIB_PC="json-glib-1.0 >= 0.8"

                  AC_DEFINE([HAVE_JSON_GLIB],
                            [1],
                            [Have the JSON-GLib library installed])

                  AC_MSG_RESULT([found])
                ],
                [
                  JSON_PREFIX=json
                  experimental_json=yes
                  AC_MSG_RESULT([not found, using internal copy])
                ]
          )
        ],

        [AC_MSG_ERROR([Unknown argument for --with-json])]
)

AC_SUBST(JSON_PREFIX)
AM_CONDITIONAL(LOCAL_JSON_GLIB, test "x$have_json" = "xno")

dnl === Enable debug level ====================================================

m4_define([debug_default], [m4_if(m4_eval(clutter_minor_version % 2), [1], [yes], [minimum])])
AC_ARG_ENABLE([debug],
              [AC_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                              [Control Clutter debugging level @<:@default=debug_default@:>@])],
              [],
              [enable_debug=debug_default])

AS_CASE([$enable_debug],

        [yes],
        [
          test "$cflags_set" = set || CFLAGS="$CFLAGS -g"
          CLUTTER_DEBUG_CFLAGS="-DCLUTTER_ENABLE_DEBUG"
        ],

        [minimum],
        [CLUTTER_DEBUG_CFLAGS="-DCLUTTER_ENABLE_DEBUG -DG_DISABLE_CAST_CHECKS"],

        [no],
        [CLUTTER_DEBUG_CFLAGS="-DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"],

        [AC_MSG_ERROR([Unknown argument for --enable-debug])]
)

AC_SUBST(CLUTTER_DEBUG_CFLAGS)

m4_define([cogl_debug_default], [minimum])
AC_ARG_ENABLE([cogl-debug],
              [AC_HELP_STRING([--enable-cogl-debug=@<:@no/yes@:>@],
                              [Control COGL debugging level @<:@default=cogl_debug_default@:>@])],
              [],
              [enable_cogl_debug=cogl_debug_default])

AS_CASE([$enable_cogl_debug],

        [yes],
        [COGL_DEBUG_CFLAGS="-DCOGL_GL_DEBUG -DCOGL_HANDLE_DEBUG -DCOGL_ENABLE_DEBUG"],

        [minimum],
        [COGL_DEBUG_CFLAGS="-DCOGL_ENABLE_DEBUG"],

        [no],
        [COGL_DEBUG_CFLAGS=""],

        [AC_MSG_ERROR([Invalid value for --enable-cogl-debug])]
)

AC_SUBST(COGL_DEBUG_CFLAGS)

dnl === Conformance test suite ================================================

AC_ARG_ENABLE([conformance],
              [AC_HELP_STRING([--enable-conformance=@<:@no/yes@:>@],
                              [Build conformance test suite @<:@default=yes@:>@])],
              [],
              [enable_conformance=yes])

AM_CONDITIONAL([BUILD_TESTS], [test "x$enable_conformance" = "xyes"])

dnl === Profiling =============================================================

m4_define([profile_default], [no])
AC_ARG_ENABLE(profile,
              AC_HELP_STRING([--enable-profile=@<:@no/yes@:>@],
                             [Turn on profiling support. yes; All profiling probe points are compiled in and may be runtime enabled. no; No profiling support will built into clutter.  @<:@default=no@:>@]),
              [],
              [enable_profile=profile_default])

AS_CASE([$enable_profile],

        [yes],
        [
          AS_IF([test "x$GCC" = "xyes"],
                [
                  PKG_CHECK_MODULES([PROFILE_DEP], [uprof-0.2])
                  CLUTTER_PROFILE_CFLAGS=" -DCLUTTER_ENABLE_PROFILE -DCOGL_ENABLE_PROFILE $PROFILE_DEP_CFLAGS"
                  CLUTTER_PROFILE_LDFLAGS=" $PROFILE_DEP_LIBS"

                  AS_IF([test "x$enable_debug" = "xyes"], [CLUTTER_PROFILE_CFLAGS+=" -DUPROF_DEBUG"])
                ],
                [
                  AC_MSG_ERROR([--enable-profile is currently only supported if using GCC])
                ])
        ],

        [no],
        [
          CLUTTER_PROFILE_CFLAGS=""
          CLUTTER_PROFILE_LDFLAGS=""
        ],

        [*], [AC_MSG_ERROR([Invalid value for --enable-profile])]
)
AM_CONDITIONAL(PROFILE, test "x$enable_profile" != "xno")
AC_SUBST(CLUTTER_PROFILE_CFLAGS)
AC_SUBST(CLUTTER_PROFILE_LDFLAGS)

dnl === Coverage report =======================================================

AC_PATH_PROG([GCOV], [gcov], [enable_gcov=no])

AC_MSG_CHECKING([whether to build with gcov testing])

AC_ARG_ENABLE([gcov],
              [AS_HELP_STRING([--enable-gcov],
                              [Whether to enable coverage testing (requires gcc
and gcov)])],
              [],
              [enable_gcov=no])

AS_IF([test "x$enable_gcov" = "xyes" && test "x$GCC" = "xyes"],
      [
        AS_IF([test "x$enable_conformance" = "xno"],
              [AC_MSG_WARN([Conformance test suite is disabled, the coverage report will be incomplete])],
              [AC_MSG_RESULT([yes])]
        )

        GCOV_CFLAGS="-g -O0 -fprofile-arcs -ftest-coverage"
        GCOV_LDFLAGS="-lgcov"
      ],
      [AC_MSG_RESULT([no])]
)

AM_CONDITIONAL([GCOV_ENABLED], [test "x$enable_gcov" = "xyes"])
AC_SUBST([GCOV_CFLAGS])
AC_SUBST([GCOV_LDFLAGS])

dnl === Enable strict compiler flags ==========================================

# use strict compiler flags only on development releases
m4_define([maintainer_flags_default], [m4_if(m4_eval(clutter_minor_version % 2), [1], [yes], [no])])
AC_ARG_ENABLE([maintainer-flags],
              [AC_HELP_STRING([--enable-maintainer-flags=@<:@no/yes@:>@],
                              [Use strict compiler flags @<:@default=maintainer_flags_default@:>@])],
              [],
              [enable_maintainer_flags=maintainer_flags_default])

AS_IF([test "x$enable_maintainer_flags" = "xyes" && test "x$GCC" = "xyes"],
      [
        AS_COMPILER_FLAGS([MAINTAINER_CFLAGS],
                          ["-Wall -Wshadow -Wcast-align -Wuninitialized
                            -Wno-strict-aliasing -Wempty-body -Wformat
                            -Wformat-security -Winit-self"])
      ]
)

AC_SUBST(MAINTAINER_CFLAGS)

dnl === Dependencies, compiler flags and linker libraries =====================
CLUTTER_REQUIRES="cairo >= 1.4 atk >= 1.7 pangocairo >= 1.20 $IMAGE_PC_FILES $BACKEND_PC_FILES $JSON_GLIB_PC"

PKG_CHECK_MODULES(CLUTTER_DEPS, [$CLUTTER_REQUIRES])

AC_SUBST(CLUTTER_REQUIRES)

CLUTTER_CFLAGS="$FLAVOUR_CFLAGS $CLUTTER_DEPS_CFLAGS $CLUTTER_PROFILE_CFLAGS $GLIB_CFLAGS"
CLUTTER_LIBS="$FLAVOUR_LIBS $CLUTTER_DEPS_LIBS $CLUTTER_PROFILE_LDFLAGS $GLIB_LIBS"

AC_SUBST(CLUTTER_CFLAGS)
AC_SUBST(CLUTTER_LIBS)

dnl === GObject-Introspection check ===========================================

GOBJECT_INTROSPECTION_CHECK([0.9.0])

dnl === GTK Doc check =========================================================

GTK_DOC_CHECK([1.13], [--flavour no-tmpl])

# we don't want to build the documentation from a Git clone unless we
# explicitly tell configure to do so; this allows avoiding to recurse into
# the documentation directory when building Clutter inside Poky for a
# target device that doesn't have gtk-doc installed. for reference
# see: http://bugzilla.openedhand.com/show_bug.cgi?id=1047
#
# we use autogen.sh as it exists only inside the Git clones, and it
# is not packaged into tarballs.
AM_CONDITIONAL([BUILD_GTK_DOC], [test "x$enable_gtk_doc" = "xyes" || test ! -f "autogen.sh"])

# prefixes for fixing gtk-doc references
CAIRO_PREFIX="`$PKG_CONFIG --variable=prefix cairo`"
GLIB_PREFIX="`$PKG_CONFIG --variable=prefix glib-2.0`"
PANGO_PREFIX="`$PKG_CONFIG --variable=prefix pango`"
AC_SUBST(CAIRO_PREFIX)
AC_SUBST(GLIB_PREFIX)
AC_SUBST(PANGO_PREFIX)

dnl === Manual ================================================================

AC_ARG_ENABLE([docs],
              [AC_HELP_STRING([--enable-docs=@<:@no/yes@:>@],
                              [Build optional documentation; requires xsltproc and jw.])],
              [enable_docs=$enableval],
              [enable_docs=no])

enable_pdfs=no
AS_IF([test "x$enable_docs" = "xyes"],
      [
        AC_PATH_PROG(JW, [jw], [no])
        AS_IF([test "x$JW" = "xno"],
              [
                AC_MSG_WARN([jw not found; pdf generation has been disabled])
              ],
              [enable_pdfs=yes])

        AC_PATH_PROG(XSLTPROC, [xsltproc], [no])
        AS_IF([test "x$XSLTPROC" = "xno"],
              [
                AC_MSG_ERROR([xsltproc not found])
              ])
      ]
)

AM_CONDITIONAL(ENABLE_DOCS, [test "x$enable_docs" = "xyes"])
AM_CONDITIONAL(ENABLE_PDFS, [test "x$enable_pdfs" = "xyes"])

dnl === I18N ==================================================================

GETTEXT_PACKAGE="clutter-$CLUTTER_API_VERSION"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,
                   "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])
AS_ALL_LINGUAS
AM_GLIB_GNU_GETTEXT
GLIB_DEFINE_LOCALEDIR(CLUTTER_LOCALEDIR)

dnl ===========================================================================

AC_CONFIG_FILES([
        Makefile
        build/Makefile
        build/autotools/Makefile
        build/mingw/Makefile
        clutter/Makefile
        clutter/clutter-version.h
        clutter/clutter-json.h
        clutter/x11/Makefile
        clutter/x11/clutter-x11.pc
        clutter/glx/Makefile
        clutter/egl/Makefile
        clutter/fruity/Makefile
        clutter/osx/Makefile
        clutter/win32/Makefile
        clutter/win32/clutter-win32.pc
        clutter/cally/Makefile
        clutter/cally/cally.pc
        clutter/cogl/Makefile
        clutter/cogl/cogl/Makefile
        clutter/cogl/cogl/cogl-defines.h
        clutter/cogl/cogl/cogl.pc
        clutter/cogl/cogl/driver/Makefile
        clutter/cogl/cogl/driver/gl/Makefile
        clutter/cogl/cogl/driver/gles/Makefile
        clutter/cogl/cogl/tesselator/Makefile
        clutter/cogl/pango/Makefile
        clutter/json/Makefile
        tests/Makefile
        tests/accessibility/Makefile
        tests/conform/Makefile
        tests/conform/test-launcher.sh
        tests/data/Makefile
        tests/interactive/Makefile
        tests/interactive/wrapper.sh
        tests/micro-bench/Makefile
        tests/tools/Makefile
        doc/Makefile
        doc/reference/Makefile
        doc/reference/clutter/Makefile
        doc/reference/clutter/clutter-docs.xml
        doc/reference/cogl/Makefile
        doc/reference/cogl/cogl-docs.xml
        doc/reference/cally/Makefile
        doc/reference/cally/cally-docs.xml
        doc/common/Makefile
        doc/manual/Makefile
        doc/manual/clutter-manual.xml
        doc/cookbook/Makefile
        doc/cookbook/clutter-cookbook.xml
        doc/cookbook/examples/Makefile
        po/Makefile.in
        clutter.pc
])

AC_OUTPUT

dnl === Summary ===============================================================

echo ""
echo "Clutter - $VERSION (${CLUTTER_RELEASE_STATUS})"

# Global flags
echo ""
echo " • Global:"
echo "        Prefix: ${prefix}"
echo "        Flavour: ${CLUTTER_WINSYS}/${COGL_DRIVER}"
echo "        Target library: ${CLUTTER_WINSYS_LIB}"

# Compiler/Debug related flags
echo ""
echo " • Compiler options:"
echo "        Clutter debug level: ${enable_debug}"
echo "        COGL debug level: ${enable_cogl_debug}"
echo "        Compiler flags: ${MAINTAINER_CFLAGS}"
echo "        Profiling enabled: ${enable_profile}"
echo "        Enable coverage tests: ${enable_gcov}"

# Documentation
echo ""
echo " • Documentation:"
echo "        Build API Reference: ${enable_gtk_doc}"
echo "        Build Additional Documentation: ${enable_docs} (Generate PDF: ${enable_pdfs})"

# Miscellaneous
echo ""
echo " • Extra:"
echo "        Build introspection data: ${enable_introspection}"
echo "        Build conformance test suite: ${enable_conformance}"

if test "x$have_json" = "xyes"; then
echo "        JSON-GLib: system"
else
echo "        JSON-GLib: internal (WARNING: Experimental)"
fi

# COGL flags
echo ""
echo " • COGL Backend:"
echo "        Driver: ${COGL_DRIVER}"
echo "        GL header: ${cogl_gl_headers}"

if test "x$COGL_DRIVER" = "xgles"; then
  if test "x$use_gles2_wrapper" = "xyes"; then
echo "        GLES version: 2.0"
  else
echo "        GLES version: 1.1"
  fi
fi

if test "x$experimental_image" = "xno"; then
echo "        Image backend: ${COGL_IMAGE_BACKEND}"
else
echo "        Image backend: ${COGL_IMAGE_BACKEND} (WARNING: Experimental)"
fi

# Clutter backend related flags
echo ""
echo " • Clutter Backend:"

if test "x$experimental_backend" = "xno"; then
echo "        Windowing system: ${CLUTTER_WINSYS}"
else
echo "        Windowing system: ${CLUTTER_WINSYS} (WARNING: Experimental)"
fi

if test "x$SUPPORT_XLIB" = "x1"; then
echo "        Enable XInput 1.0: ${xinput}"
echo "        Enable XKB: ${have_xkb}"
echo "        Enable X11 tests: ${x11_tests}"
fi

echo ""

# General warning about experimental features
if test "x$experimental_backend" = "xyes" ||
   test "x$experimental_image" = "xyes" ||
   test "x$experimental_json" = "xyes"; then
echo ""
echo "☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠"
echo " *WARNING*"
echo ""
echo "  The stability of your build might be affected by one or more"
echo "  experimental backends or experimental and unsupported features"
echo "☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠"
echo ""
fi
