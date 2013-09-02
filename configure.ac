# clutter package version number, (as distinct from shared library version)
# An odd micro number indicates in-progress development from Git
# An even micro number indicates a released version
#
# Making a point release:
# - increase clutter_micro_version to the next even number
# - increase clutter_interface_age to the next even number
# After the release:
# - increase clutter_micro_version to the next odd number
# - increase clutter_interface_version to the next odd number
m4_define([clutter_major_version], [1])
m4_define([clutter_minor_version], [15])
m4_define([clutter_micro_version], [92])

# • for stable releases: increase the interface age by 1 for each release;
#   if the API changes, set to 0. interface_age and binary_age are used to
#   create the soname of the shared object:
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
# • for development releases: keep clutter_interface_age to 0
m4_define([clutter_interface_age], [0])

m4_define([clutter_binary_age], [m4_eval(100 * clutter_minor_version + clutter_micro_version)])

m4_define([clutter_release_status],
          [m4_if(m4_eval(clutter_micro_version % 2), [1], [git],
                 [m4_if(m4_eval(clutter_minor_version % 2), [1], [snapshot],
                                                                 [release])])])

m4_define([clutter_version], [clutter_major_version.clutter_minor_version.clutter_micro_version])

# change this only when breaking the API
m4_define([clutter_api_version], [1.0])

AC_PREREQ([2.63])

AC_INIT([clutter],
        [clutter_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=clutter],
        [clutter],
        [http://www.clutter-project.org])

AC_CONFIG_SRCDIR([clutter/clutter.h])
AC_CONFIG_HEADERS([clutter/config.h])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_MACRO_DIR([build/autotools])

AM_INIT_AUTOMAKE([1.11 foreign -Wno-portability no-define no-dist-gzip dist-xz tar-ustar])
AM_SILENT_RULES([yes])

AC_SUBST([CLUTTER_MAJOR_VERSION],  [clutter_major_version])
AC_SUBST([CLUTTER_MINOR_VERSION],  [clutter_minor_version])
AC_SUBST([CLUTTER_MICRO_VERSION],  [clutter_micro_version])
AC_SUBST([CLUTTER_VERSION],        [clutter_version])
AC_SUBST([CLUTTER_API_VERSION],    [clutter_api_version])
AC_SUBST([CLUTTER_API_VERSION_AM], [$CLUTTER_MAJOR_VERSION\_0])
AC_SUBST([CLUTTER_RELEASE_STATUS], [clutter_release_status])

m4_define([lt_current],  [m4_eval(100 * clutter_minor_version + clutter_micro_version - clutter_interface_age)])
m4_define([lt_revision], [clutter_interface_age])
m4_define([lt_age],      [m4_eval(clutter_binary_age - clutter_interface_age)])
CLUTTER_LT_CURRENT=lt_current
CLUTTER_LT_REV=lt_revision
CLUTTER_LT_AGE=lt_age
CLUTTER_LT_VERSION="$CLUTTER_LT_CURRENT:$CLUTTER_LT_REV:$CLUTTER_LT_AGE"
CLUTTER_LT_LDFLAGS="-version-info $CLUTTER_LT_VERSION"
AC_SUBST([CLUTTER_LT_CURRENT],  [lt_current])
AC_SUBST([CLUTTER_LT_REVISION], [lt_revision])
AC_SUBST([CLUTTER_LT_VERSION],  [$CLUTTER_LT_VERSION])

AC_PROG_SED

dnl = Preliminary platform checks =============================================

AC_CANONICAL_HOST

platform_win32=no
platform_quartz=no
platform_linux=no

AC_MSG_CHECKING([if building for some Win32 platform])
AS_CASE([$host],
        [*-*-mingw*|*-*-cygwin*],
        [
          CLUTTER_LT_LDFLAGS="$CLUTTER_LT_LDFLAGS -no-undefined"
          platform_win32=yes
        ],

        [*-*-linux*],
        [
          platform_linux=yes
        ],

        []
)
AC_MSG_RESULT([$platform_win32])

AM_CONDITIONAL(OS_WIN32, [test "$platform_win32" = "yes"])

AC_CHECK_HEADER([OpenGL/gl.h], [platform_quartz=yes], [platform_quartz=no])
AM_CONDITIONAL(OS_QUARTZ, [test "$platform_quartz" = "yes"])

AC_CHECK_HEADER([GL/glx.h], [platform_glx=yes], [platform_glx=no])
AC_CHECK_LIB([GL], [glXCreateContext], [platform_glx=yes], [platform_glx=no])
AM_CONDITIONAL(OS_GLX, [test "$platform_glx" = "yes"])
AM_CONDITIONAL(OS_LINUX, [test "$platform_linux" = "yes"])

AC_SUBST(CLUTTER_LT_LDFLAGS)

AC_CACHE_SAVE

dnl = Dependencies =========================================================

# Checks for programs.
AM_PROG_CC_C_O

# require libtool >= 2.2
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

# Checks for header files.
AC_HEADER_STDC

# required versions for dependencies
m4_define([glib_req_version],           [2.37.3])
m4_define([cogl_req_version],           [1.15.9])
m4_define([json_glib_req_version],      [0.12.0])
m4_define([atk_req_version],            [2.5.3])
m4_define([cairo_req_version],          [1.10])
m4_define([pango_req_version],          [1.30])
m4_define([gi_req_version],             [0.9.5])
m4_define([uprof_req_version],          [0.3])
m4_define([gtk_doc_req_version],        [1.15])
m4_define([xfixes_req_version],         [3])
m4_define([xcomposite_req_version],     [0.4])
m4_define([gdk_req_version],            [3.3.18])

AC_SUBST([GLIB_REQ_VERSION],       [glib_req_version])
AC_SUBST([COGL_REQ_VERSION],       [cogl_req_version])
AC_SUBST([JSON_GLIB_REQ_VERSION],  [json_glib_req_version])
AC_SUBST([ATK_REQ_VERSION],        [atk_req_version])
AC_SUBST([CAIRO_REQ_VERSION],      [cairo_req_version])
AC_SUBST([PANGO_REQ_VERSION],      [pango_req_version])
AC_SUBST([GI_REQ_VERSION],         [gi_req_version])
AC_SUBST([UPROF_REQ_VERSION],      [uprof_req_version])
AC_SUBST([GTK_DOC_REQ_VERSION],    [gtk_doc_req_version])
AC_SUBST([XFIXES_REQ_VERSION],     [xfixes_req_version])
AC_SUBST([XCOMPOSITE_REQ_VERSION], [xcomposite_req_version])
AC_SUBST([GDK_REQ_VERSION],        [gdk_req_version])

# Checks for typedefs, structures, and compiler characteristics.
AM_PATH_GLIB_2_0([glib_req_version],
                 [],
                 [AC_MSG_ERROR([glib-2.0 is required])],
                 [gobject gthread gmodule-no-export])

# Check for -Bsymbolic-functions to avoid intra-library PLT jumps
AC_ARG_ENABLE([Bsymbolic],
              [AS_HELP_STRING([--disable-Bsymbolic],
                              [Avoid linking with -Bsymbolic])],
              [],
              [
                saved_LDFLAGS="${LDFLAGS}"
                AC_MSG_CHECKING([for -Bsymbolic-functions linker flag])
                LDFLAGS=-Wl,-Bsymbolic-functions
                AC_TRY_LINK([], [int main (void) { return 0; }],
                            [
                              AC_MSG_RESULT([yes])
                              enable_Bsymbolic=yes
                            ],
                            [
                              AC_MSG_RESULT([no])
                              enable_Bsymbolic=no
                            ])
                LDFLAGS="${saved_LDFLAGS}"
              ])

AS_IF([test "x$enable_Bsymbolic" = "xyes"], [CLUTTER_LINK_FLAGS=-Wl[,]-Bsymbolic-functions])
AC_SUBST(CLUTTER_LINK_FLAGS)

AC_ARG_ENABLE(installed_tests,
              AS_HELP_STRING([--enable-installed-tests],
                             [Install test programs (default: no)]),,
              [enable_installed_tests=no])
AM_CONDITIONAL(ENABLE_INSTALLED_TESTS, test x$enable_installed_tests = xyes)

AC_CACHE_SAVE

dnl ========================================================================

FLAVOUR_LIBS=""
FLAVOUR_CFLAGS=""
CLUTTER_BACKENDS=""
CLUTTER_INPUT_BACKENDS=""
CLUTTER_CONFIG_DEFINES=

experimental_backend=no
experimental_input_backend=no

# base dependencies for core
CLUTTER_BASE_PC_FILES="cogl-1.0 >= $COGL_REQ_VERSION cairo-gobject >= $CAIRO_REQ_VERSION atk >= $ATK_REQ_VERSION pangocairo >= $PANGO_REQ_VERSION cogl-pango-1.0 json-glib-1.0 >= $JSON_GLIB_REQ_VERSION"

# private base dependencies
CLUTTER_BASE_PC_FILES_PRIVATE=""

# backend specific pkg-config files
BACKEND_PC_FILES=""

# private backend specific dependencies
BACKEND_PC_FILES_PRIVATE=""

dnl === Clutter windowing system backend ======================================

AC_ARG_ENABLE([x11-backend],
              [AS_HELP_STRING([--enable-x11-backend=@<:@yes/no@:>@], [Enable the X11 backend (default=check)])],
              [enable_x11=$enableval],
              [enable_x11=check])
AC_ARG_ENABLE([win32-backend],
              [AS_HELP_STRING([--enable-win32-backend=@<:@yes/no@:>@], [Enable the Windows backend (default=check)])],
              [enable_win32=$enableval],
              [enable_win32=check])
AC_ARG_ENABLE([quartz-backend],
              [AS_HELP_STRING([--enable-quartz-backend=@<:@yes/no@:>@], [Enable the OS X backend (default=check)])],
              [enable_osx=$enableval],
              [enable_osx=check])
AC_ARG_ENABLE([gdk-backend],
              [AS_HELP_STRING([--enable-gdk-backend=@<:@yes/no@:>@], [Enable the GDK backend (default=check)])],
              [enable_gdk=$enableval],
              [enable_gdk=check])
AC_ARG_ENABLE([wayland-backend],
              [AS_HELP_STRING([--enable-wayland-backend=@<:@yes/no@:>@], [Enable the Wayland client backend (default=no)])],
              [enable_wayland=$enableval],
              [enable_wayland=no])
AC_ARG_ENABLE([egl-backend],
              [AS_HELP_STRING([--enable-egl-backend=@<:@yes/no@:>@], [Enable the EGL framebuffer backend (default=no)])],
              [enable_egl=$enableval],
              [enable_egl=no])
AC_ARG_ENABLE([cex100-backend],
              [AS_HELP_STRING([--enable-cex100-backend=@<:@yes/no@:>@], [Enable the CEx100 backend (default=no)])],
              [enable_cex100=$enableval],
              [enable_cex100=no])

dnl Define default values
AS_IF([test "x$enable_x11" = "xcheck"],
      [
        AS_IF([test "x$platform_glx" = "xyes"], [enable_x11=yes], [enable_x11=no])
      ])

AS_IF([test "x$enable_win32" = "xcheck"],
      [
        AS_IF([test "x$platform_win32" = "xyes"], [enable_win32=yes], [enable_win32=no])
      ])

AS_IF([test "x$enable_osx" = "xcheck"],
      [
        AS_IF([test "x$platform_quartz" = "xyes"], [enable_osx=yes], [enable_osx=no])
      ])

AS_IF([test "x$enable_gdk" = "xcheck"],
      [
        PKG_CHECK_EXISTS([gdk-3.0 >= gdk_req_version], [enable_gdk=yes], [enable_gdk=no])
      ])

dnl Per-backend rules
AS_IF([test "x$enable_x11" = "xyes"],
      [
        CLUTTER_BACKENDS="$CLUTTER_BACKENDS x11"
        CLUTTER_INPUT_BACKENDS="$CLUTTER_INPUT_BACKENDS x11"

        SUPPORT_X11=1
        SUPPORT_GLX=1
        SUPPORT_COGL=1

        # we use fontconfig API and pango-ft2 when the fontconfig
        # configuration changes; we don't expose any API for this
        # so we add pango-ft2 to the private Requires.
        PKG_CHECK_EXISTS([pangoft2],
                         [
                           AC_DEFINE([HAVE_PANGO_FT2], [1], [Supports PangoFt2])
                           BACKEND_PC_FILES_PRIVATE="$BACKEND_PC_FILES_PRIVATE pangoft2"
                         ],
                         [])

        AC_DEFINE([HAVE_CLUTTER_GLX], [1], [Have the GLX backend])
      ])

AS_IF([test "x$enable_gdk" = "xyes"],
      [
        CLUTTER_BACKENDS="$CLUTTER_BACKENDS gdk"
        CLUTTER_INPUT_BACKENDS="$CLUTTER_INPUT_BACKENDS gdk"

	SUPPORT_GDK=1
        SUPPORT_COGL=1

	BACKEND_PC_FILES="$BACKEND_PC_FILES gdk-3.0"
      ])

AS_IF([test "x$enable_wayland" = "xyes"],
      [
        CLUTTER_BACKENDS="$CLUTTER_BACKENDS wayland"
        CLUTTER_INPUT_BACKENDS="$CLUTTER_INPUT_BACKENDS wayland"

        experimental_backend="yes"

        SUPPORT_WAYLAND=1
        SUPPORT_COGL=1

        PKG_CHECK_EXISTS([wayland-client wayland-cursor xkbcommon gdk-pixbuf-2.0],
                         [
                            BACKEND_PC_FILES="$BACKEND_PC_FILES wayland-cursor wayland-client xkbcommon"
                            BACKEND_PC_FILES_PRIVATE="$BACKEND_PC_FILES_PRIVATE gdk-pixbuf-2.0"
                         ],
                         [])

        AC_DEFINE([HAVE_CLUTTER_WAYLAND], [1], [Have the Wayland client backend])
      ])

dnl Note this is orthogonal to the client side support and you can
dnl use the Wayland compositor features with any of the clutter
dnl backends with corresponding Cogl support.
AC_ARG_ENABLE([wayland-compositor],
              [AS_HELP_STRING([--enable-wayland-compositor=@<:@yes/no@:>@],
                              [Enable Wayland compositor features (default=no)])],
              [],
              [AS_IF([test "x$SUPPORT_EGL" = "x1"],
                     [enable_wayland_compositor=yes],
                     [enable_wayland_compositor=no])
              ])

AS_IF([test "x$enable_wayland_compositor" = "xyes"],
      [
        PKG_CHECK_EXISTS([wayland-server],
			 [BACKEND_PC_FILES="$BACKEND_PC_FILES wayland-server"], [])

        SUPPORT_WAYLAND_COMPOSITOR=1
        SUPPORT_COGL=1

	CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_HAS_WAYLAND_COMPOSITOR_SUPPORT 1"
        AC_DEFINE([HAVE_CLUTTER_WAYLAND_COMPOSITOR], [1], [Have Wayland compositor support])
      ])

AM_CONDITIONAL(SUPPORT_WAYLAND_COMPOSITOR, [test "x$SUPPORT_WAYLAND_COMPOSITOR" = "x1"])

AS_IF([test "x$enable_wayland_compositor" != "xyes" -a "x$enable_wayland" != "xyes"],
      [
        # The wayland headers introduce so much symbol shadowing that build
        # logs become incomprehensible with -Wshadow so we only use it for
        # non-wayland builds.
	MAINTAINER_COMPILER_FLAGS="-Wshadow"
      ])

AS_IF([test "x$enable_cex100" = "xyes"],
      [
        CLUTTER_BACKENDS="$CLUTTER_BACKENDS cex100"

        experimental_backend="yes"

        SUPPORT_CEX100=1
        SUPPORT_COGL=1

        have_gdl=no
        AC_CHECK_HEADERS([libgdl.h], [have_gdl=yes])

        AS_IF([test "x$have_gdl" = "xno"],
              [
                AC_CHECK_HEADERS([CE4100/libgdl.h],
                                 [
                                   FLAVOUR_CFLAGS="-I/usr/include/CE4100"
                                   have_gdl=yes
                                   CLUTTER_CEX100_LIBGDL_PREFIX=CE4100/
                                 ])
              ])

        AS_IF([test "x$have_gdl" = "xno"],
              [AC_MSG_ERROR([libgdl.h not found but the CEx100 backend has been explicitly enabled])])

        AC_SUBST(CLUTTER_CEX100_LIBGDL_PREFIX)

        FLAVOUR_LIBS="$FLAVOUR_LIBS -lgdl"

        AC_DEFINE([CLUTTER_EGL_BACKEND_CEX100], [1], [Use CEX100 EGL backend])
        AC_DEFINE([CLUTTER_EGL_BACKEND_GENERIC], [1], [Use Generic EGL backend])
        AC_DEFINE([HAVE_CLUTTER_EGL], [1], [Have the EGL backend])
      ])

AS_IF([test "x$enable_egl" = "xyes"],
      [
        CLUTTER_BACKENDS="$CLUTTER_BACKENDS egl"

        experimental_backend="yes"

        SUPPORT_EGL=1
        SUPPORT_COGL=1

        FLAVOUR_LIBS="$FLAVOUR_LIBS $TSLIB_LIBS $EVDEV_LIBS"
        FLAVOUR_CFLAGS="$FLAVOUR_CFLAGS $TSLIB_CFLAGS $EVDEV_CFLAGS"

        AC_DEFINE([CLUTTER_EGL_BACKEND_GENERIC], [1], [Use Generic EGL backend])

        AC_DEFINE([HAVE_CLUTTER_EGL], [1], [Have the EGL backend])
      ])

AS_IF([test "x$enable_osx" = "xyes"],
      [
        CLUTTER_BACKENDS="$CLUTTER_BACKENDS osx"
        CLUTTER_INPUT_BACKENDS="$CLUTTER_INPUT_BACKENDS osx"

        AC_DEFINE([HAVE_CLUTTER_OSX], [1], [Have the OSX backend])

        FLAVOUR_LIBS="$FLAVOUR_LIBS -framework Cocoa -framework OpenGL"

        SUPPORT_OSX=1
      ])

AS_IF([test "x$enable_win32" = "xyes"],
      [
        CLUTTER_BACKENDS="$CLUTTER_BACKENDS win32"
        CLUTTER_INPUT_BACKENDS="$CLUTTER_INPUT_BACKENDS win32"

        AC_DEFINE([HAVE_CLUTTER_WIN32], [1], [Have the Win32 backend])

        FLAVOUR_LIBS="$FLAVOUR_LIBS -lopengl32 -lgdi32 -lwinmm"
        FLAVOUR_CFLAGS="$FLAVOUR_CFLAGS -D_WIN32_WINNT=0x0500"

        AC_CHECK_TOOL(WINDRES, [windres], [AC_MSG_ERROR([*** windres is required])])

        SUPPORT_WIN32=1
      ])

AS_IF([test "x$CLUTTER_BACKENDS" = "x"],
      [
        AC_MSG_ERROR([No backend enabled. You need to enable at least one backend.])
      ])

AS_IF([test "x$platform_linux" = "xyes"],
      [
        AC_DEFINE([OS_LINUX], [1], [Define to 1 if building for Linux])
      ])

# additional input backends

AC_ARG_ENABLE([tslib-input],
              [AS_HELP_STRING([--enable-tslib-input=@<:@yes/no@:>@], [Enable TSLib for input events (default=no)])],
              [enable_tslib=$enableval],
              [enable_tslib=no])
AC_ARG_ENABLE([evdev-input],
              [AS_HELP_STRING([--enable-evdev-input=@<:@yes/no@:>@], [Enable evdev for input events (default=no)])],
              [enable_evdev=$enableval],
              [enable_evdev=no])

AS_IF([test "x$enable_tslib" = "xyes"],
      [
        PKG_CHECK_MODULES(TSLIB, [tslib-1.0], [have_tslib=yes], [have_tslib=no])
        AS_IF([test "x$have_tslib" = "xyes"],
              [
                CLUTTER_INPUT_BACKENDS="$CLUTTER_INPUT_BACKENDS tslib"
                experimental_input_backend="yes"
                AC_DEFINE([HAVE_TSLIB], [1], [Have tslib for touchscreen handling])
                SUPPORT_TSLIB=1
              ])
      ])

AS_IF([test "x$enable_evdev" = "xyes"],
      [
        PKG_CHECK_MODULES(EVDEV, [gudev-1.0 xkbcommon], [have_evdev=yes], [have_evdev=no])
        AS_IF([test "x$have_evdev" = "xyes"],
              [
                CLUTTER_INPUT_BACKENDS="$CLUTTER_INPUT_BACKENDS evdev"
                BACKEND_PC_FILES="$BACKEND_PC_FILES gudev-1.0 xkbcommon"
                experimental_input_backend="yes"
                AC_DEFINE([HAVE_EVDEV], [1], [Have evdev support for input handling])
                SUPPORT_EVDEV=1
              ])
      ])

# conditionals for use in automake files...
AM_CONDITIONAL(SUPPORT_GLX,     [test "x$SUPPORT_GLX" = "x1"])
AM_CONDITIONAL(SUPPORT_X11,     [test "x$SUPPORT_X11" = "x1"])
AM_CONDITIONAL(SUPPORT_GDK,     [test "x$SUPPORT_GDK" = "x1"])
AM_CONDITIONAL(SUPPORT_EGL,     [test "x$SUPPORT_EGL" = "x1"])
AM_CONDITIONAL(SUPPORT_OSX,     [test "x$SUPPORT_OSX" = "x1"])
AM_CONDITIONAL(SUPPORT_WIN32,   [test "x$SUPPORT_WIN32" = "x1"])
AM_CONDITIONAL(SUPPORT_CEX100,  [test "x$SUPPORT_CEX100" = "x1"])
AM_CONDITIONAL(SUPPORT_WAYLAND, [test "x$SUPPORT_WAYLAND" = "x1"])

AM_CONDITIONAL(USE_COGL,  [test "x$SUPPORT_COGL" = "x1"])
AM_CONDITIONAL(USE_TSLIB, [test "x$have_tslib" = "xyes"])
AM_CONDITIONAL(USE_EVDEV, [test "x$have_evdev" = "xyes"])
AM_CONDITIONAL(USE_GLD,   [test "x$have_gdl" = "xyes"])

AM_CONDITIONAL(NEED_XKB_UTILS, [test "x$have_evdev" = "xyes" -o "x$SUPPORT_WAYLAND" = "x1"])

dnl Instead of using AM_CFLAGS to ensure
dnl COGL_ENABLE_EXPERIMENTAL_2_0_API is defined while compiling clutter
dnl and cogl code we use a define in config.h instead. This helps ensure
dnl other tools such as glib-mkenums and gir-scanner don't end up
dnl using the define also.
AC_DEFINE([COGL_ENABLE_EXPERIMENTAL_2_0_API], [1], [Can use Cogl 2.0 API internally])

dnl strip leading spaces
CLUTTER_BACKENDS=${CLUTTER_BACKENDS#* }
AC_SUBST(CLUTTER_BACKENDS)

CLUTTER_INPUT_BACKENDS=${CLUTTER_INPUT_BACKENDS#* }
AC_SUBST(CLUTTER_INPUT_BACKENDS)

AC_CACHE_SAVE

dnl === Clutter configuration =================================================

# windowing systems
AS_IF([test "x$SUPPORT_X11" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_WINDOWING_X11 \"x11\"
#define CLUTTER_INPUT_X11 \"x11\""])
AS_IF([test "x$SUPPORT_GDK" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_WINDOWING_GDK \"gdk\"
#define CLUTTER_INPUT_GDK \"gdk\""])
AS_IF([test "x$SUPPORT_GLX" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_WINDOWING_GLX \"glx\""])
AS_IF([test "x$SUPPORT_EGL" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_WINDOWING_EGL \"eglnative\""])
AS_IF([test "x$SUPPORT_WAYLAND" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_WINDOWING_WAYLAND \"wayland\""])
AS_IF([test "x$SUPPORT_OSX" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_WINDOWING_OSX \"osx\"
#define CLUTTER_INPUT_OSX \"osx\""])
AS_IF([test "x$SUPPORT_WIN32" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_WINDOWING_WIN32 \"win32\"
#define CLUTTER_INPUT_WIN32 \"win32\""])
AS_IF([test "x$SUPPORT_CEX100" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_WINDOWING_CEX100 \"cex100\""])
AS_IF([test "x$SUPPORT_EVDEV" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_INPUT_EVDEV \"evdev\""])
AS_IF([test "x$SUPPORT_TSLIB" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_INPUT_TSLIB \"tslib\""])
AS_IF([test "x$SUPPORT_WAYLAND" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_INPUT_WAYLAND \"wayland\""])

# the 'null' input backend is special
CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_INPUT_NULL \"null\""

AC_SUBST([CLUTTER_CONFIG_DEFINES])

dnl === Clutter substitutions kept for backwards compatibility ================
AC_SUBST([CLUTTER_WINSYS],       [deprecated])
AC_SUBST([CLUTTER_WINSYS_BASE],  [deprecated])
AC_SUBST([CLUTTER_STAGE_TYPE],   [deprecated])
AC_SUBST([CLUTTER_SONAME_INFIX], [deprecated])
AC_SUBST([CLUTTER_FLAVOUR],      [deprecated])
AC_SUBST([CLUTTER_COGL],         [deprecated])
AC_SUBST([COGL_DRIVER],          [deprecated])

dnl === X11 checks, only for X11-based backends ===============================
X11_PC_FILES=""
X11_EXTS=""
x11_tests=no

AS_IF([test "x$SUPPORT_X11" = "x1"],
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
                X11_EXTS="$X11_EXTS xext"

                AC_MSG_RESULT([found])
              ],
              [AC_MSG_ERROR([Not found])]
        )

        # XFIXES (required)
        AC_MSG_CHECKING([for XFIXES extension >= $XFIXES_REQ_VERSION])
        PKG_CHECK_EXISTS([xfixes >= $XFIXES_REQ_VERSION], [have_xfixes=yes], [have_xfixes=no])
        AS_IF([test "x$have_xfixes" = "xyes"],
              [
                AC_DEFINE(HAVE_XFIXES, [1], [Define to 1 if we have the XFIXES X extension])

                X11_LIBS="$X11_LIBS -lXfixes"
                X11_PC_FILES="$X11_PC_FILES xfixes >= $XFIXES_REQ_VERSION"
                X11_EXTS="$X11_EXTS xfixes"

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
                X11_EXTS="$X11_EXTS xdamage"

                AC_MSG_RESULT([found])
              ],
              [AC_MSG_ERROR([not found])]
        )

        # XCOMPOSITE (optional)
        AC_MSG_CHECKING([for XCOMPOSITE extension >= $XCOMPOSITE_REQ_VERSION])
        PKG_CHECK_EXISTS([xcomposite >= $XCOMPOSITE_REQ_VERSION], [have_xcomposite=yes], [have_xcomposite=no])
        AS_IF([test "x$have_xcomposite" = "xyes"],
              [
                AC_DEFINE(HAVE_XCOMPOSITE, [1], [Define to 1 if we have the XCOMPOSITE X extension])

                X11_LIBS="$X11_LIBS -lXcomposite"
                X11_PC_FILES="$X11_PC_FILES xcomposite >= $XCOMPOSITE_REQ_VERSION"
                X11_EXTS="$X11_EXTS xcomposite"

                AC_MSG_RESULT([found])
              ],
              [AC_MSG_ERROR([not found])]
        )

        # X Generic Extensions (optional)
        clutter_save_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $X11_CFLAGS"

        clutter_save_LIBS="$LIBS"
        LIBS="$LIBS $X11_LIBS"

        have_xge=no
        AC_CHECK_FUNC([XGetEventData],
                      [
                        AC_DEFINE([HAVE_XGE], [1], [Define to 1 if X Generic Extensions is available])
                        have_xge=yes
                        X11_EXTS="$X11_EXTS xge"
                      ])

        CPPFLAGS="$clutter_save_CPPFLAGS"
        LIBS="$clutter_save_LIBS"

        # XI (optional)
        AC_ARG_ENABLE([xinput],
                      [AS_HELP_STRING([--enable-xinput], [Use the XI X extension])],
                      [],
                      [enable_xinput=yes])

        AS_IF([test "x$enable_xinput" = "xyes"],
              [
                PKG_CHECK_EXISTS([xi], [have_xinput=yes], [have_xinput=no])
              ],
              [
                have_xinput=no
              ])

        AS_CASE([$have_xinput],

                [yes],
                [
                  AC_CHECK_HEADERS([X11/extensions/XInput2.h],
                                   [
                                     have_xinput2=yes
                                     AC_DEFINE([HAVE_XINPUT_2],
                                               [1],
                                               [Define to 1 if XI2 is available])
                                   ])

                  clutter_save_LIBS="$LIBS"
                  LIBS="$LIBS -lXi"

                  AC_CHECK_FUNC([XIAllowTouchEvents],
                                [
                                  AC_CHECK_MEMBER([XIScrollClassInfo.number],
                                                  [
                                                    have_xinput_2_2=yes
                                                    AC_DEFINE([HAVE_XINPUT_2_2], [1], [Define to 1 if XInput 2.2 is available])
                                                  ],
                                                  [have_xinput_2_2=no],
                                                  [[#include <X11/extensions/XInput2.h>]])])

                  LIBS="$clutter_save_LIBS"

                  X11_LIBS="$X11_LIBS $XINPUT_LIBS"
                  X11_PC_FILES="$X11_PC_FILES xi"

                  AS_IF([test "x$have_xinput_2_2" = "xyes"],
                        [X11_EXTS="$X11_EXTS xi2.2"],
                        [X11_EXTS="$X11_EXTS xi2.0"])
                ],

                [no],
                [have_xinput2=no],

                [*],
                [AC_MSG_ERROR([Invalid argument for --enable-xinput])]
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

                        X11_EXTS="$X11_EXTS xkb"
                      ])

        CPPFLAGS="$clutter_save_CPPFLAGS"
        LIBS="$clutter_save_LIBS"

        x11_tests=yes
        BACKEND_PC_FILES="$BACKEND_PC_FILES $X11_PC_FILES"
        FLAVOUR_LIBS="$FLAVOUR_LIBS $X11_LIBS"
        FLAVOUR_CFLAGS="$FLAVOUR_CFLAGS $X11_CFLAGS"
      ]
)

AM_CONDITIONAL([BUILD_XI2], [test "x$have_xinput2" = "xyes"])
AM_CONDITIONAL([X11_TESTS], [test "x$x11_tests" = "xyes"])

X11_EXTS=${X11_EXTS#* }

AC_CACHE_SAVE

dnl === Enable GDK-Pixbuf in tests ============================================

m4_define([pixbuf_default], [yes])
AC_ARG_ENABLE([gdk-pixbuf],
              [AS_HELP_STRING([--enable-gdk-pixbuf=@<:@no/yes@:>@],
                              [Enable tests using GDK-Pixbuf @<:@default=]pixbuf_default[@:>@])],
              [enable_pixbuf=$enable_val],
              [enable_pixbuf=pixbuf_default])

AS_CASE([$enable_pixbuf],

        [yes],
        [
          PKG_CHECK_MODULES([GDK_PIXBUF], [gdk-pixbuf-2.0])
          AC_SUBST(GDK_PIXBUF_CFLAGS)
          AC_SUBST(GDK_PIXBUF_LIBS)
          pixbuf_tests=yes
        ],

        [no],
        [
          pixbuf_tests=no
        ]
)

AM_CONDITIONAL([PIXBUF_TESTS], [test "x$pixbuf_tests" = "xyes"])

dnl === Enable debug level ====================================================

m4_define([debug_default], [m4_if(m4_eval(clutter_minor_version % 2), [1], [yes], [minimum])])
AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                              [Control Clutter debugging level @<:@default=]debug_default[@:>@])],
              [],
              [enable_debug=debug_default])

AS_CASE([$enable_debug],

        [yes],
        [
          test "$cflags_set" = set || CFLAGS="$CFLAGS -g"
          CLUTTER_DEBUG_CFLAGS="-DCLUTTER_ENABLE_DEBUG"
        ],

        [minimum],
        [CLUTTER_DEBUG_CFLAGS="-DG_DISABLE_CAST_CHECKS"],

        [no],
        [CLUTTER_DEBUG_CFLAGS="-DG_DISABLE_ASSERT -DG_DISABLE_CHECKS -DG_DISABLE_CAST_CHECKS"],

        [AC_MSG_ERROR([Unknown argument for --enable-debug])]
)

AC_SUBST(CLUTTER_DEBUG_CFLAGS)

dnl === Enable deprecation guards ==================================================

m4_define([deprecated_default],
          [m4_if(m4_eval(clutter_minor_version % 2),
                 [1],
                 [no],
                 [yes])])

AC_ARG_ENABLE([deprecated],
              [AS_HELP_STRING([--enable-deprecated=@<:@no/yes@:>@],
                              [Whether deprecated symbols should be disabled when compiling Clutter @<:@default=]deprecated_default[@:>@])],
              [],
              [enable_deprecated=deprecated_default])

AS_CASE([$enable_deprecated],

        [no],
        [
          CLUTTER_DEPRECATED_CFLAGS="-DG_DISABLE_SINGLE_INCLUDES -DCOGL_DISABLE_DEPRECATED"
        ],

        [yes],
        [
          CLUTTER_DEPRECATED_CFLAGS="-DGLIB_DISABLE_DEPRECATION_WARNINGS"
        ],

        [AC_MSG_ERROR([Unknown argument for --enable-deprecated])]
)

AC_SUBST([CLUTTER_DEPRECATED_CFLAGS])

dnl === Profiling =============================================================

m4_define([profile_default], [no])
AC_ARG_ENABLE(profile,
              AS_HELP_STRING([--enable-profile=@<:@no/yes@:>@],
                             [Turn on profiling support. yes; All profiling probe points are compiled in and may be runtime enabled. no; No profiling support will built into clutter.  @<:@default=no@:>@]),
              [],
              [enable_profile=profile_default])

AS_CASE([$enable_profile],

        [yes],
        [
          AS_IF([test "x$GCC" = "xyes"],
                [
                  PKG_CHECK_MODULES([PROFILE_DEP], [uprof-0.3])
                  CLUTTER_PROFILE_CFLAGS="-DCLUTTER_ENABLE_PROFILE $PROFILE_DEP_CFLAGS"
                  CLUTTER_PROFILE_LDFLAGS="$PROFILE_DEP_LIBS"

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

dnl === Enable strict compiler flags ==========================================

# use strict compiler flags only when building from git; the rules for
# distcheck will take care of turning this on when making a release
m4_define([maintainer_flags_default], [m4_if(m4_eval(clutter_micro_version % 2), [1], [yes], [no])])
AC_ARG_ENABLE([maintainer-flags],
              [AS_HELP_STRING([--enable-maintainer-flags=@<:@no/yes/error@:>@],
                              [Use strict compiler flags @<:@default=]maintainer_flags_default[@:>@])],
              [],
              [enable_maintainer_flags=maintainer_flags_default])

MAINTAINER_COMPILER_FLAGS="$MAINTAINER_COMPILER_FLAGS
                           -Wall
                           -Wcast-align
                           -Wuninitialized
                           -Wno-strict-aliasing
                           -Werror=pointer-arith
                           -Werror=missing-declarations
                           -Werror=redundant-decls
                           -Werror=empty-body
                           -Werror=format
                           -Werror=format-security
                           -Werror=format-nonliteral
                           -Werror=init-self
                           -Werror=declaration-after-statement
                           -Werror=vla"

AS_CASE([$enable_maintainer_flags],
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
MAINTAINER_CFLAGS=${MAINTAINER_CFLAGS#*  }
AC_SUBST(MAINTAINER_CFLAGS)

dnl === Dependencies, compiler flags and linker libraries =====================
# strip leading space
BACKEND_PC_FILES=${BACKEND_PC_FILES#* }

# public dependencies, will fill the Requires: field of clutter.pc
CLUTTER_REQUIRES="$CLUTTER_BASE_PC_FILES $BACKEND_PC_FILES"
PKG_CHECK_MODULES(CLUTTER_DEPS, [$CLUTTER_REQUIRES])

# private dependencies, will fill the Requires.private: field of clutter.pc
AS_IF([test "x$CLUTTER_BASE_PC_FILES_PRIVATE" = "x" && test "x$BACKEND_PC_FILES_PRIVATE" = "x"],
      [
        CLUTTER_REQUIRES_PRIVATE=""
        CLUTTER_DEPS_PRIVATE_CFLAGS=""
        CLUTTER_DEPS_PRIVATE_LIBS=""
      ],
      [
        CLUTTER_REQUIRES_PRIVATE="$CLUTTER_BASE_PC_FILES_PRIVATE $BACKEND_PC_FILES_PRIVATE"
        PKG_CHECK_MODULES(CLUTTER_DEPS_PRIVATE, [$CLUTTER_REQUIRES_PRIVATE])
      ])

AC_SUBST(CLUTTER_REQUIRES)
AC_SUBST(CLUTTER_REQUIRES_PRIVATE)

CLUTTER_CFLAGS="$FLAVOUR_CFLAGS $CLUTTER_DEPS_CFLAGS $CLUTTER_DEPS_PRIVATE_CFLAGS $CLUTTER_PROFILE_CFLAGS $GLIB_CFLAGS"
CLUTTER_LIBS="$FLAVOUR_LIBS $CLUTTER_DEPS_LIBS $CLUTTER_DEPS_PRIVATE_LIBS $CLUTTER_PROFILE_LDFLAGS $GLIB_LIBS"
AC_SUBST(CLUTTER_CFLAGS)
AC_SUBST(CLUTTER_LIBS)

dnl === Test coverage =========================================================

AC_ARG_ENABLE([gcov],
              [AS_HELP_STRING([--enable-gcov], [Enable gcov])],
              [use_gcov=$enableval],
              [use_gcov=no])

AS_IF([test "x$use_gcov" = "xyes"],
      [
        dnl we need gcc:
        AS_IF([test "$GCC" != "yes"], [AC_MSG_ERROR([GCC is required for --enable-gcov])])

        dnl Check if ccache is being used
        AC_CHECK_PROG(SHTOOL, shtool, shtool)
        AS_CASE([`$SHTOOL path $CC`],
                [*ccache*], [gcc_ccache=yes],
                [gcc_ccache=no])

        if test "$gcc_ccache" = "yes" && (test -z "$CCACHE_DISABLE" || test "$CCACHE_DISABLE" != "1"); then
          AC_MSG_ERROR([ccache must be disabled when --enable-gcov option is used. You can disable ccache by setting environment variable CCACHE_DISABLE=1.])
        fi

        ltp_version_list="1.6 1.7 1.8 1.9"
        AC_CHECK_PROG(LTP, lcov, lcov)
        AC_CHECK_PROG(LTP_GENHTML, genhtml, genhtml)

        if test "$LTP"; then
          AC_CACHE_CHECK([for ltp version], clutter_cv_ltp_version,
                         [
                           clutter_cv_ltp_version=invalid
                           ltp_version=`$LTP -v 2>/dev/null | $SED -e 's/^.* //'`
                           for ltp_check_version in $ltp_version_list; do
                             if test "$ltp_version" = "$ltp_check_version"; then
                               clutter_cv_ltp_version="$ltp_check_version (ok)"
                             fi
                           done
                         ])
        else
          ltp_msg="To enable code coverage reporting you must have one of the following LTP versions installed: $ltp_version_list"
          AC_MSG_ERROR([$ltp_msg])
        fi

        case $clutter_cv_ltp_version in
          ""|invalid[)]
          ltp_msg="You must have one of the following versions of LTP: $ltp_version_list (found: $ltp_version)."
          AC_MSG_ERROR([$ltp_msg])
          LTP="exit 0;"
          ;;
        esac

        if test -z "$LTP_GENHTML"; then
          AC_MSG_ERROR([Could not find genhtml from the LTP package])
        fi

        AC_DEFINE(HAVE_GCOV, 1, [Whether you have gcov])

        dnl Remove all optimization flags from CFLAGS
        changequote({,})
        CFLAGS=`echo "$CFLAGS" | $SED -e 's/-O[0-9]*//g'`
        CLUTTER_CFLAGS=`echo "$CLUTTER_CFLAGS" | $SED -e 's/-O[0-9]*//g'`
        changequote([,])

        dnl Define the special gcc flags
        CLUTTER_GCOV_CFLAGS="-O0 -fprofile-arcs -ftest-coverage"
        CLUTTER_GCOV_LDADD="-lgcov"

        AC_SUBST(CLUTTER_GCOV_CFLAGS)
        AC_SUBST(CLUTTER_GCOV_LDADD)

        CLUTTER_CFLAGS="$CLUTTER_CFLAGS $CLUTTER_GCOV_CFLAGS"
        CLUTTER_LIBS="$CLUTTER_LIBS $CLUTTER_GCOV_LDADD"
      ])

AM_CONDITIONAL(ENABLE_GCOV, test "x$use_gcov" = "xyes")

dnl === GObject-Introspection check ===========================================

GOBJECT_INTROSPECTION_CHECK([gi_req_version])

dnl === GTK Doc check =========================================================

GTK_DOC_CHECK([gtk_doc_req_version], [--flavour no-tmpl])

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
COGL_PREFIX="`$PKG_CONFIG --variable=prefix cogl-1.0`"
ATK_PREFIX="`$PKG_CONFIG --variable=prefix atk`"
AC_SUBST(CAIRO_PREFIX)
AC_SUBST(GLIB_PREFIX)
AC_SUBST(PANGO_PREFIX)
AC_SUBST(COGL_PREFIX)
AC_SUBST(ATK_PREFIX)

dnl === Manual ================================================================

AC_ARG_ENABLE([docs],
              [AS_HELP_STRING([--enable-docs=@<:@no/yes@:>@],
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

AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])

GETTEXT_PACKAGE="clutter-1.0"
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],
                   ["$GETTEXT_PACKAGE"],
                   [The prefix for our gettext translation domains.])
AS_ALL_LINGUAS

dnl = Build optionals =========================================================

dnl === Conformance test suite ================================================

AC_ARG_ENABLE([conformance],
              [AS_HELP_STRING([--disable-conformance], [Whether the conformance tests should be built])],
              [],
              [enable_conformance=yes])

AC_ARG_ENABLE([tests],
              [AS_HELP_STRING([--disable-tests], [Whether tests should be built])],
              [],
              [enable_tests=yes])
AM_CONDITIONAL(BUILD_TESTS, [test "x$enable_tests" = "xyes" && test "x$enable_conformance" = "xyes"])

AC_ARG_ENABLE([examples],
              [AS_HELP_STRING([--disable-examples], [Whether examples should be built])],
              [],
              [enable_examples=yes])
AM_CONDITIONAL(BUILD_EXAMPLES, [test "x$enable_examples" = "xyes"])

dnl ===========================================================================

AC_CONFIG_FILES([
	Makefile
        README

	build/Makefile
	build/autotools/Makefile
	build/mingw/Makefile
	build/win32/Makefile
	build/win32/vs9/Makefile
	build/win32/vs10/Makefile

	clutter/Makefile
	clutter/config.h.win32
        clutter/clutter-config.h
	clutter/clutter-version.h
	clutter/clutter-$CLUTTER_API_VERSION.pc:clutter/clutter.pc.in

	clutter/cally/cally-$CLUTTER_API_VERSION.pc:clutter/cally/cally.pc.in

	clutter/cex100/clutter-cex100.h

	tests/Makefile
	tests/accessibility/Makefile
	tests/conform/Makefile
	tests/conform/test-launcher.sh
	tests/data/Makefile
	tests/interactive/Makefile
	tests/interactive/wrapper.sh
	tests/micro-bench/Makefile
	tests/performance/Makefile

        examples/Makefile

	doc/Makefile
	doc/reference/Makefile
	doc/reference/clutter/Makefile
	doc/reference/clutter/clutter-docs.xml
	doc/reference/cally/Makefile
	doc/reference/cally/cally-docs.xml
	doc/common/Makefile
	doc/cookbook/Makefile
	doc/cookbook/clutter-cookbook.xml
	doc/cookbook/examples/Makefile

	po/Makefile.in
])

AC_OUTPUT

dnl === Summary ===============================================================

echo ""
echo "Clutter - $VERSION (${CLUTTER_RELEASE_STATUS})"

# Global flags
echo ""
echo " • Global:"
echo "        Prefix: ${prefix}"
echo "        Libdir: ${libdir}"
echo "        Sysconfdir: ${sysconfdir}"

# Compiler/Debug related flags
echo ""
echo " • Compiler options:"
echo "        Clutter debug level: ${enable_debug}"
echo "        Compiler flags: ${CFLAGS} ${MAINTAINER_CFLAGS}"
echo "        Profiling enabled: ${enable_profile}"
echo "        Enable coverage tests: ${use_gcov}"
echo "        Enable deprecated symbols: ${enable_deprecated}"

# Documentation
echo ""
echo " • Documentation:"
echo "        Build API Reference: ${enable_gtk_doc}"
echo "        Build Additional Documentation: ${enable_docs} (Generate PDF: ${enable_pdfs})"

# Miscellaneous
echo ""
echo " • Extra:"
echo "        Build introspection data: ${enable_introspection}"
echo "        Build test suites: ${enable_tests}"
if test "x$enable_tests" = "xyes"; then
echo "        Build X11-specific tests: ${x11_tests}"
echo "        Build tests using GDK-Pixbuf: ${pixbuf_tests}"
fi
echo "        Install test suites: ${enable_installed_tests}"
echo "        Build examples: ${enable_examples}"

# Clutter backend related flags
echo ""
echo " • Clutter Backends:"

if test "x$experimental_backend" = "xno"; then
echo "        Windowing systems: ${CLUTTER_BACKENDS}"
else
echo "        Windowing systems: ${CLUTTER_BACKENDS} (WARNING: Experimental backends enabled)"
fi

if test "x$experimental_input_backend" = "xno"; then
echo "        Input backends: ${CLUTTER_INPUT_BACKENDS}"
else
echo "        Input backends: ${CLUTTER_INPUT_BACKENDS} (WARNING: Experimental backends enabled)"
fi

if test "x$SUPPORT_X11" = "x1"; then
echo ""
echo "     - X11 backend options:"
echo "        Enabled extensions: ${X11_EXTS}"
fi

if test "x$SUPPORT_CEX100" = "x1"; then
echo ""
echo "     - CEx100 backend options (WARNING: Experimental):"
echo "        libGDL include prefix: ${CLUTTER_CEX100_LIBGDL_PREFIX}"
fi

if test "x$SUPPORT_WAYLAND_COMPOSITOR" = "x1"; then
echo ""
echo "     - Wayland compositor support enabled (WARNING: Experimental)"
fi

echo ""

# General warning about experimental features
if test "x$experimental_backend" = "xyes"; then
echo ""
echo "☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠"
echo "*WARNING* *WARNING* *WARNING* *WARNING* *WARNING* *WARNING* *WARNING*"
echo ""
echo "  The stability of your build might be affected by one or more"
echo "  experimental backends or experimental and unsupported features"
echo ""
echo "*WARNING* *WARNING* *WARNING* *WARNING* *WARNING* *WARNING* *WARNING*"
echo "☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠☠"
echo ""
fi
