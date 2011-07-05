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
m4_define([cogl_1_minor_version], [7])
m4_define([cogl_1_micro_version], [3])
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
m4_define([cogl_lt_current], 1)
m4_define([cogl_lt_revision], 0)
m4_define([cogl_lt_age], 0)
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
m4_define([cogl_release_status], [git])

AC_INIT(cogl, [cogl_1_version])
AC_CONFIG_SRCDIR(cogl/cogl.h)
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build/autotools])
AC_CONFIG_HEADERS(config.h)

dnl ================================================================
dnl Required versions for dependencies
dnl ================================================================
m4_define([glib_req_version],           [2.26.0])
m4_define([pangocairo_req_version],     [1.20])
m4_define([gi_req_version],             [0.9.5])
m4_define([gdk_pixbuf_req_version],     [2.0])
m4_define([uprof_req_version],          [0.3])
m4_define([gtk_doc_req_version],        [1.13])
m4_define([xfixes_req_version],         [3])
m4_define([xcomposite_req_version],     [0.4])
m4_define([cairo_req_version],          [1.10])

dnl These variables get copied into the generated README
AC_SUBST([GLIB_REQ_VERSION], [glib_req_version])
AC_SUBST([GDK_PIXBUF_REQ_VERSION], [gdk_pixbuf_req_version])
AC_SUBST([CAIRO_REQ_VERSION], [cairo_req_version])
AC_SUBST([PANGOCAIRO_REQ_VERSION], [pangocairo_req_version])
AC_SUBST([XCOMPOSITE_REQ_VERSION], [xcomposite_req_version])
AC_SUBST([XFIXES_REQ_VERSION], [xfixes_req_version])
AC_SUBST([GTK_DOC_REQ_VERSION], [gtk_doc_req_version])
AC_SUBST([GI_REQ_VERSION], [gi_req_version])
AC_SUBST([UPROF_REQ_VERSION], [uprof_req_version])

# Save this value here, since automake will set cflags later and we
# want to know if the user specified custom cflags or not.
cflags_set=${CFLAGS+set}

AM_INIT_AUTOMAKE([1.11 foreign -Wno-portability no-define no-dist-gzip
                  dist-bzip2 tar-ustar])
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
dnl See what platform we are building for
dnl ================================================================
AC_CANONICAL_HOST
AC_MSG_CHECKING([if building for some Win32 platform])
AS_CASE([$host],
        [*-*-mingw*|*-*-cygwin*],
        [
          COGL_EXTRA_LDFLAGS="$COGL_EXTRA_LDFLAGS -no-undefined"
          platform_win32=yes
        ],

        [platform_win32=no]
)
AC_MSG_RESULT([$platform_win32])
AM_CONDITIONAL(OS_WIN32, [test "$platform_win32" = "yes"])

AC_CHECK_HEADER([OpenGL/gl.h], [platform_quartz=yes], [platform_quartz=no])
AM_CONDITIONAL(OS_QUARTZ, [test "$platform_quartz" = "yes"])

dnl ================================================================
dnl Handle extra configure options
dnl ================================================================

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
    COGL_EXTRA_CFLAGS="$COGL_EXTRA_CFLAGS -DCOGL_GL_DEBUG -DCOGL_OBJECT_DEBUG -DCOGL_HANDLE_DEBUG -DCOGL_ENABLE_DEBUG"
  ],
  [no],
  [
    COGL_EXTRA_CFLAGS="$COGL_EXTRA_CFLAGS -DCOGL_ENABLE_DEBUG -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"
  ],
  [AC_MSG_ERROR([Unknown argument for --enable-debug])]
)

AC_SUBST(COGL_DEBUG_CFLAGS)


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
              AC_HELP_STRING([--enable-uprof=@<:@no/yes@:>@],
                             [Turn on uprof profiling support. yes; All UProf profiling probe points are compiled in and may be runtime enabled. no; No profiling support will built into cogl.  @<:@default=no@:>@]),
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

MAINTAINER_COMPILER_FLAGS="-Wall -Wcast-align -Wuninitialized
                           -Wno-strict-aliasing -Wempty-body -Wformat
                           -Wformat-security -Winit-self
                           -Wdeclaration-after-statement -Wvla
                           -Wpointer-arith"

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


dnl     ============================================================
dnl     Choose image loading backend
dnl     ============================================================
AC_ARG_ENABLE(
  [gdk-pixbuf],
  [AC_HELP_STRING([--enable-gdk-pixbuf=@<:@no/yes@:>@], [Enable image loading via gdk-pixbuf @<:@default=yes@:>@])],
  [],
  enable_gdk_pixbuf=yes
)
if test "x$enable_gdk_pixbuf" = "xyes"; then
  PKG_CHECK_EXISTS([gdk-pixbuf-2.0], [have_gdk_pixbuf=yes], [have_gdk_pixbuf=no])
else
  have_gdk_pixbuf=no
fi

AC_ARG_ENABLE(
  [quartz-image],
  [AC_HELP_STRING([--enable-quartz-image=@<:@no/yes@:>@], [Enable image loading via quartz @<:@default=no@:>@])],
  [],
  enable_quartz_image=no
)

AS_IF(
  [test "x$have_gdk_pixbuf" = "xyes"],
  [
    AC_DEFINE([USE_GDKPIXBUF], 1, [Use GdkPixbuf for loading image data])
    COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES gdk-pixbuf-2.0 >= gdk_pixbuf_req_version"
    COGL_IMAGE_BACKEND="gdk-pixbuf"
  ],
  [test "x$enable_quartz_image" = "xyes"],
  [
    EXPERIMENTAL_CONFIG=yes
    EXPERIMENTAL_OPTIONS="$EXPERIMENTAL_OPTIONS Quartz Core Graphics,"
    AC_DEFINE([USE_QUARTZ], 1,
	      [Use Core Graphics (Quartz) for loading image data])
    COGL_IMAGE_BACKEND="quartz"
  ],
  [
    EXPERIMENTAL_CONFIG=yes
    EXPERIMENTAL_OPTIONS="$EXPERIMENTAL_OPTIONS fallback image decoding (stb_image),"
    AC_DEFINE([USE_INTERNAL], 1,
	      [Use internal image decoding for loading image data])
    COGL_IMAGE_BACKEND="stb_image"
  ]
)


dnl     ============================================================
dnl     Should cogl-pango be built?
dnl     ============================================================

AC_ARG_ENABLE(
  [cogl-pango],
  [AC_HELP_STRING([--enable-cogl-pango=@<:@no/yes@:>@], [Enable pango support @<:@default=yes@:>@])],
  [],
  enable_cogl_pango=yes
)
AM_CONDITIONAL([BUILD_COGL_PANGO], [test "x$enable_cogl_pango" = "xyes"])

AS_IF([test "x$enable_cogl_pango" = "xyes"],
      [
	COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES pangocairo >= pangocairo_req_version"
      ]
)

dnl     ============================================================
dnl     Determine which drivers and window systems we can support
dnl     ============================================================

dnl         ========================================================
dnl         Drivers first...
dnl         ========================================================
EGL_CHECKED=no

dnl This gets set to yes if Cogl directly links to the GL library API
dnl so it doesn't need to be dlopened. This currently happens on OSX
dnl and WGL where it's not clear if window system API can be separated
dnl from the GL API.
GL_LIBRARY_DIRECTLY_LINKED=no

enabled_drivers=""

AC_ARG_ENABLE(
  [gles1],
  [AC_HELP_STRING([--enable-gles1=@<:@no/yes@:>@], [Enable support for OpenGL-ES 1.1 @<:@default=no@:>@])],
  [],
  enable_gles1=no
)
AS_IF([test "x$enable_gles1" = "xyes"],
      [
        AS_IF([test "x$platform_win32" = "xyes"],
	      [AC_MSG_ERROR([GLES 1 not available for win32])])

        enabled_drivers="$enabled_drivers gles1"

        cogl_gl_headers="GLES/gl.h GLES/glext.h"

        AC_DEFINE([HAVE_COGL_GLES], 1, [Have GLES 1.1 for rendering])
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GLES CLUTTER_COGL_HAS_GLES"
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GLES1"

        PKG_CHECK_EXISTS([glesv1_cm],
          [COGL_PKG_REQUIRES_GL="$COGL_PKG_REQUIRES_GL glesv1_cm"
           COGL_GLES1_LIBNAME="GLESv1_CM"
           NEED_EGL=yes
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
                    AC_CHECK_HEADERS([EGL/eglext.h],
                                     [COGL_EGL_INCLUDES="$COGL_EGL_INCLUDE
#include <EGL/eglext.h>"],
                                     [])
                  ],
                  [AC_MSG_ERROR([Unable to locate EGL header])])
            AC_SUBST([COGL_EGL_INCLUDES])

            # Check for a GLES 1.x Common Profile library with/without EGL.
            #
            # Note: historically GLES 1 libraries shipped with the
            # EGL and GLES symbols all bundled in one library. Now
            # the Khronos Implementers Guide defines two naming
            # schemes: -lGLES_CM should be used for a library that
            # bundles the GLES and EGL API together and -lGLESv1_CM
            # would be used for a standalone GLES API.
            AC_CHECK_LIB(GLES_CM, [eglInitialize],
              [COGL_GLES1_LIBNAME="GLES_CM"],
              [
                AC_CHECK_LIB(GLESv1_CM, [glFlush],
                  [COGL_GLES1_LIBNAME="GLESv1_CM"
                   NEED_SEPARATE_EGL=yes
                  ],
                  [AC_MSG_ERROR([Unable to locate required GLES 1.x Common Profile library])])
              ])

            EGL_CHECKED=yes
          ])
      ])

AC_ARG_ENABLE(
  [gles2],
  [AC_HELP_STRING([--enable-gles2=@<:@no/yes@:>@], [Enable support for OpenGL-ES 2.0 @<:@default=no@:>@])],
  [],
  enable_gles2=no
)
AS_IF([test "x$enable_gles2" = "xyes"],
      [
        AS_IF([test "x$platform_win32" = "xyes"],
	      [AC_MSG_ERROR([GLES 2 not available for win32])])

        enabled_drivers="$enabled_drivers gles2"

        cogl_gl_headers="GLES2/gl2.h GLES2/gl2ext.h"
        AC_DEFINE([HAVE_COGL_GLES2], 1, [Have GLES 2.0 for rendering])
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GLES CLUTTER_COGL_HAS_GLES"
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GLES2"

        PKG_CHECK_EXISTS([glesv2],
          [COGL_PKG_REQUIRES_GL="$COGL_PKG_REQUIRES_GL glesv2"
           COGL_GLES2_LIBNAME="GLESv2"
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

            COGL_GLES2_LIBNAME="GLESv2"
          ])

        NEED_EGL=yes
      ])

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

        AS_IF([test "x$platform_quartz" = "xyes"],
              [
                cogl_gl_headers="OpenGL/gl.h"
                COGL_EXTRA_LDFLAGS="$COGL_EXTRA_LDFLAGS -framework OpenGL"
                dnl The GL API is being directly linked in so there is
                dnl no need to dlopen it separately
                GL_LIBRARY_DIRECTLY_LINKED=yes
                COGL_GL_LIBNAME=""
              ],

              [test "x$platform_win32" = "xyes"],
	      [
		COGL_EXTRA_LDFLAGS="$COGL_EXTRA_LDFLAGS -lopengl32 -lgdi32 -lwinmm"
		COGL_EXTRA_CFLAGS="$COGL_EXTRA_CFLAGS -D_WIN32_WINNT=0x0500"
                ALLOW_WGL=yes
                dnl The GL API is being directly linked in so there is
                dnl no need to dlopen it separately
                GL_LIBRARY_DIRECTLY_LINKED=yes
                COGL_GL_LIBNAME=""
	      ],

	      [
		PKG_CHECK_EXISTS([gl],
                  dnl We don't want to use COGL_PKG_REQUIRES here because we don't want to
                  dnl directly link against libGL
                  [COGL_PKG_REQUIRES_GL="$COGL_PKG_REQUIRES_GL gl"],
		  [AC_CHECK_LIB(GL, [glGetString],
				,
				[AC_MSG_ERROR([Unable to locate required GL library])])
		  ])
                COGL_GL_LIBNAME="GL"
	      ])

        AC_DEFINE([HAVE_COGL_GL], [1], [Have GL for rendering])

        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_GL"
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS CLUTTER_COGL_HAS_GL"
      ])

AM_CONDITIONAL([COGL_DRIVER_GL_SUPPORTED], [test "x$enable_gl" = "xyes"])
AM_CONDITIONAL([COGL_DRIVER_GLES_SUPPORTED],
               [test "x$enable_gles1" = "xyes" || test "x$enable_gles2" = "xyes"])

AC_SUBST([COGL_GL_LIBNAME])
AC_SUBST([COGL_GLES1_LIBNAME])
AC_SUBST([COGL_GLES2_LIBNAME])

if test "x$GL_LIBRARY_DIRECTLY_LINKED" = "xyes"; then
   AC_DEFINE([HAVE_DIRECTLY_LINKED_GL_LIBRARY], [1],
             [Defined if the GL library shouldn't be dlopened])
fi

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

        AC_DEFINE([COGL_HAS_GLX_SUPPORT], [1], [Cogl supports OpenGL using the GLX API])

        # We might fall back to DRM for sync-to-vblank on GLX
        PKG_CHECK_EXISTS([libdrm],
                         [
                           AC_DEFINE([HAVE_DRM], [1], [Have libdrm support])
                           COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES libdrm"
                         ],
                         [])
      ])
AM_CONDITIONAL(SUPPORT_GLX, [test "x$SUPPORT_GLX" = "xyes"])

AC_ARG_ENABLE(
  [wgl],
  [AC_HELP_STRING([--enable-wgl=@<:@no/yes@:>@], [Enable support for WGL @<:@default=auto@:>@])],
  [],
  [AS_IF([test "x$ALLOW_WGL" = "xyes"], [enable_wgl=yes], [enable_wgl=no])]
)
AS_IF([test "x$enable_wgl" = "xyes"],
      [
        AS_IF([test "x$ALLOW_WGL" != "xyes"],
              [AC_MSG_ERROR([WGL not supported with this configuration])])

        SUPPORT_WGL=yes
        GL_WINSYS_APIS="$GL_WINSYS_APIS wgl"

        AC_DEFINE([COGL_HAS_WGL_SUPPORT], [1], [Cogl supports OpenGL using the WGL API])
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_WIN32_SUPPORT"
      ])
AM_CONDITIONAL(SUPPORT_WGL, [test "x$SUPPORT_WGL" = "xyes"])

EGL_PLATFORM_COUNT=0

AC_ARG_ENABLE(
  [null-egl-platform],
  [AC_HELP_STRING([--enable-null-egl-platform=@<:@no/yes@:>@], [Enable support for the NULL egl platform @<:@default=no@:>@])],
  [],
  enable_null_egl_platform=no
)
AS_IF([test "x$enable_null_egl_platform" = "xyes"],
      [
        EGL_PLATFORM_COUNT=$((EGL_PLATFORM_COUNT+1))
        NEED_EGL=yes
        EGL_PLATFORMS="$EGL_PLATFORMS null"

        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_EGL_PLATFORM_POWERVR_NULL_SUPPORT"
      ])
AM_CONDITIONAL(SUPPORT_EGL_PLATFORM_POWERVR_NULL,
               [test "x$enable_null_egl_platform" = "xyes"])

AC_ARG_ENABLE(
  [gdl-egl-platform],
  [AC_HELP_STRING([--enable-gdl-egl-platform=@<:@no/yes@:>@], [Enable support for the GDL egl platform @<:@default=no@:>@])],
  [],
  enable_gdl_egl_platform=no
)
AS_IF([test "x$enable_gdl_egl_platform" == "xyes"],
      [
        EGL_PLATFORM_COUNT=$((EGL_PLATFORM_COUNT+1))
        NEED_EGL=yes
        EGL_PLATFORMS="$EGL_PLATFORMS gdl"

        AC_CHECK_HEADERS(
          [libgdl.h],
          [],
          [
            AC_CHECK_HEADERS(
              [CE4100/libgdl.h],
              [COGL_EXTRA_CFLAGS="$COGL_EXTRA_CFLAGS -I/usr/include/CE4100"],
              [AC_MSG_ERROR([libgdl.h not found])])
          ])

        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_EGL_PLATFORM_GDL_SUPPORT"
      ])
AM_CONDITIONAL(SUPPORT_EGL_PLATFORM_GDL,
               [test "x$enable_gdl_egl_platform" = "xyes"])

AC_ARG_ENABLE(
  [wayland-egl-platform],
  [AC_HELP_STRING([--enable-wayland-egl-platform=@<:@no/yes@:>@], [Enable support for the Wayland egl platform @<:@default=no@:>@])],
  [],
  enable_wayland_egl_platform=no
)
AS_IF([test "x$enable_wayland_egl_platform" == "xyes"],
      [
        EGL_PLATFORM_COUNT=$((EGL_PLATFORM_COUNT+1))
        NEED_EGL=yes
        EGL_PLATFORMS="$EGL_PLATFORMS wayland"

        PKG_CHECK_EXISTS([wayland-egl],
                         [
                           COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES wayland-egl"
                           COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES wayland-client"
                         ],
                         [AC_MSG_ERROR([Unable to locate required wayland libraries])])

        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_EGL_PLATFORM_WAYLAND_SUPPORT"
      ])
AM_CONDITIONAL(SUPPORT_EGL_PLATFORM_WAYLAND,
               [test "x$enable_wayland_egl_platform" = "xyes"])

AC_ARG_ENABLE(
  [wayland-egl-server],
  [AC_HELP_STRING([--enable-wayland-egl-server=@<:@no/yes@:>@], [Enable server side wayland support @<:@default=no@:>@])],
  [],
  enable_wayland_egl_server=no
)
AS_IF([test "x$enable_wayland_egl_server" == "xyes"],
      [
        NEED_EGL=yes
        COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES wayland-server"
        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_WAYLAND_EGL_SERVER_SUPPORT"
      ])
AM_CONDITIONAL(SUPPORT_WAYLAND_EGL_SERVER,
               [test "x$enable_wayland_egl_server" = "xyes"])

dnl Android EGL platform
AC_ARG_ENABLE(
  [android-egl-platform],
  [AC_HELP_STRING([--enable-android-egl-platform=@<:@no/yes@:>@], [Enable support for the Android egl platform @<:@default=no@:>@])],
  [],
  enable_android_egl_platform=no
)
AS_IF([test "x$enable_android_egl_platform" == "xyes"],
      [
        EGL_PLATFORM_COUNT=$((EGL_PLATFORM_COUNT+1))
        NEED_EGL=yes
        EGL_PLATFORMS="$EGL_PLATFORMS android"

        AC_CHECK_HEADER([android/native_window.h],
                        [],
                        [AC_MSG_ERROR([Unable to locate android/native_window.h])])

        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_EGL_PLATFORM_ANDROID_SUPPORT"
      ])
AM_CONDITIONAL(SUPPORT_EGL_PLATFORM_ANDROID,
               [test "x$enable_android_egl_platform" = "xyes"])

dnl This should go last, since it's the default fallback and we need
dnl to check the value of $EGL_PLATFORM_COUNT here.
AC_ARG_ENABLE(
  [xlib-egl-platform],
  [AC_HELP_STRING([--enable-xlib-egl-platform=@<:@no/yes@:>@], [Enable support for the Xlib egl platform @<:@default=auto@:>@])],
  [],
  AS_IF([test "x$enable_gles1" = "xyes" -o "x$enable_gles2" = "xyes" && test $EGL_PLATFORM_COUNT -eq 0],
        [enable_xlib_egl_platform=yes], [enable_xlib_egl_platform=no])
)
AS_IF([test "x$enable_xlib_egl_platform" = "xyes"],
      [
        EGL_PLATFORM_COUNT=$((EGL_PLATFORM_COUNT+1))
        NEED_EGL=yes
        NEED_XLIB=yes
        EGL_PLATFORMS="$EGL_PLATFORMS xlib"

        COGL_DEFINES_SYMBOLS="$COGL_DEFINES_SYMBOLS COGL_HAS_EGL_PLATFORM_POWERVR_X11_SUPPORT"
      ])
AM_CONDITIONAL(SUPPORT_EGL_PLATFORM_POWERVR_X11,
               [test "x$enable_xlib_egl_platform" = "xyes"])



AS_IF([test $EGL_PLATFORM_COUNT -gt 1],
      [AC_MSG_ERROR(['The --enable-*-egl-platform options are currently mutually exclusive'])])

AS_IF([test "x$NEED_EGL" = "xyes" && test "x$EGL_CHECKED" != "xyes"],
      [
        PKG_CHECK_EXISTS([egl],
          [COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES egl"],
          [
            AC_CHECK_HEADERS(
              [EGL/egl.h EGL/eglext.h],
              [],
              [AC_MSG_ERROR([Unable to locate required EGL headers])])

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
        X11_MODULES="x11 xext xfixes >= xfixes_req_version xdamage xcomposite >= xcomposite_req_version"
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
dnl Compiler stuff.
dnl ================================================================
AC_PROG_CC
AC_PROG_CPP
AM_PROG_CC_C_O
AC_ISC_POSIX
AC_C_CONST


dnl ================================================================
dnl Libtool stuff.
dnl ================================================================
dnl AC_PROG_LIBTOOL
dnl LIBTOOL="$LIBTOOL --preserve-dup-deps"
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

dnl ================================================================
dnl I18n stuff.
dnl ================================================================
AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE="cogl"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,
                   "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])
AS_ALL_LINGUAS


dnl ================================================================
dnl Documentation stuff.
dnl ================================================================
# gtkdocize greps for ^GTK_DOC_CHECK and parses it, so you need to have
# it on it's own line.
m4_ifdef([GTK_DOC_CHECK], [
GTK_DOC_CHECK([gtk_doc_req_version], [--flavour no-tmpl])
])
AM_CONDITIONAL([BUILD_GTK_DOC], [test "x$enable_gtk_doc" = "xyes"])

dnl ================================================================
dnl Check for dependency packages.
dnl ================================================================

dnl     ============================================================
dnl     Check glib dependencies
dnl     ============================================================
AM_PATH_GLIB_2_0([glib_req_version],
                 [have_glib=yes], [have_glib=no],
                 [gobject gthread gmodule-no-export])
AS_IF([test "x$have_glib" = "xno"], AC_MSG_ERROR([glib-2.0 is required]))

COGL_PKG_REQUIRES="$COGL_PKG_REQUIRES glib-2.0"
AC_SUBST(COGL_PKG_REQUIRES)
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


dnl ================================================================
dnl Checks for library functions.
dnl ================================================================

dnl The 'ffs' function is part of C99 so it isn't always
dnl available. Cogl has a fallback if needed.
AC_CHECK_FUNCS([ffs])

dnl ================================================================
dnl What needs to be substituted in other files
dnl ================================================================
COGL_DEFINES=""
for x in $COGL_DEFINES_SYMBOLS; do
  COGL_DEFINES="$COGL_DEFINES
#define $x 1"
done;
AC_SUBST(COGL_DEFINES)


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

AC_DEFINE([COGL_ENABLE_EXPERIMENTAL_2_0_API], [1],
          [Can use Cogl 2.0 API internally])

AC_SUBST(COGL_DEP_CFLAGS)
AC_SUBST(COGL_DEP_LIBS)
AC_SUBST(COGL_EXTRA_CFLAGS)
AC_SUBST(COGL_EXTRA_LDFLAGS)

# just for compatability with the clutter build...
MAINTAINER_CFLAGS=
AC_SUBST(MAINTAINER_CFLAGS)

AC_OUTPUT(
Makefile
README
cogl/Makefile
cogl/cogl-1.0.pc
cogl/cogl-2.0-experimental.pc
cogl/cogl-defines.h
cogl-pango/Makefile
cogl-pango/cogl-pango-1.0.pc
cogl-pango/cogl-pango-2.0-experimental.pc
doc/Makefile
doc/reference/Makefile
doc/reference/cogl/Makefile
doc/reference/cogl/cogl-docs.xml
doc/reference/cogl-2.0-experimental/Makefile
doc/reference/cogl-2.0-experimental/cogl-2.0-experimental-docs.xml
examples/Makefile
po/Makefile.in
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

echo ""
# Features
echo " • Features:"
echo "        Drivers: ${enabled_drivers}"
echo "        GL Window System APIs:${GL_WINSYS_APIS}"
if test "x$SUPPORT_EGL" = "xyes"; then
echo "        EGL Platforms:${EGL_PLATFORMS}"
echo "        Wayland compositor support: ${enable_wayland_egl_server}"
fi
echo "        Image backend: ${COGL_IMAGE_BACKEND}"
echo "        Cogl Pango: ${enable_cogl_pango}"
echo "        Profiling: ${enable_profile}"

# Compiler/Debug related flags
echo ""
echo " • Compiler options:"
echo "        Cogl debug: ${enable_debug}"
echo "        Compiler flags: ${CFLAGS} ${COGL_EXTRA_CFLAGS}"
echo "        Linker flags: ${LDFLAGS} ${COGL_EXTRA_LDFLAGS}"

# Miscellaneous
echo ""
echo " • Extra:"
echo "        Build API reference: ${enable_gtk_doc}"
echo "        Build introspection data: ${enable_introspection}"

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
