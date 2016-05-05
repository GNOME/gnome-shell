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
m4_define([clutter_minor_version], [26])
m4_define([clutter_micro_version], [1])

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
m4_define([clutter_interface_age], [1])

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
AC_CONFIG_HEADERS([clutter/clutter-build-config.h])
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
CLUTTER_LT_LDFLAGS="-avoid-version"
AC_SUBST([CLUTTER_LT_CURRENT],  [lt_current])
AC_SUBST([CLUTTER_LT_REVISION], [lt_revision])
AC_SUBST([CLUTTER_LT_VERSION],  [$CLUTTER_LT_VERSION])

AC_PROG_SED

dnl = Preliminary platform checks =============================================

AC_CANONICAL_HOST

AC_DEFINE([OS_LINUX], [1], [Define to 1 if building for Linux])

AC_SUBST(CLUTTER_LT_LDFLAGS)

AC_CACHE_SAVE

dnl = Dependencies =========================================================

# Checks for programs.
AM_PROG_CC_C_O

# require libtool >= 2.2
LT_PREREQ([2.2.6])
LT_INIT([disable-static])
LT_LIB_M

# Checks for header files.
AC_HEADER_STDC

# required versions for dependencies
m4_define([glib_req_version],           [2.44.0])
m4_define([cogl_req_version],           [1.21.2])
m4_define([json_glib_req_version],      [0.12.0])
m4_define([atk_req_version],            [2.5.3])
m4_define([cairo_req_version],          [1.14.0])
m4_define([pango_req_version],          [1.30])
m4_define([gi_req_version],             [1.39.0])
m4_define([xcomposite_req_version],     [0.4])
m4_define([gdk_req_version],            [3.3.18])
m4_define([libinput_req_version],       [0.19.0])
m4_define([libudev_req_version],        [136])

AC_SUBST([GLIB_REQ_VERSION],       [glib_req_version])
AC_SUBST([COGL_REQ_VERSION],       [cogl_req_version])
AC_SUBST([JSON_GLIB_REQ_VERSION],  [json_glib_req_version])
AC_SUBST([ATK_REQ_VERSION],        [atk_req_version])
AC_SUBST([CAIRO_REQ_VERSION],      [cairo_req_version])
AC_SUBST([PANGO_REQ_VERSION],      [pango_req_version])
AC_SUBST([GI_REQ_VERSION],         [gi_req_version])
AC_SUBST([XCOMPOSITE_REQ_VERSION], [xcomposite_req_version])
AC_SUBST([GDK_REQ_VERSION],        [gdk_req_version])
AC_SUBST([LIBINPUT_REQ_VERSION],   [libinput_req_version])
AC_SUBST([LIBUDEV_REQ_VERSION],    [libudev_req_version])

# Checks for typedefs, structures, and compiler characteristics.
AM_PATH_GLIB_2_0([glib_req_version],
                 [],
                 [AC_MSG_ERROR([glib-2.0 is required])],
                 [gobject gio gthread gmodule-no-export])

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

# Check for the visibility flags
CLUTTER_HIDDEN_VISIBILITY_CFLAGS=""
dnl on other compilers, check if we can do -fvisibility=hidden
SAVED_CFLAGS="${CFLAGS}"
CFLAGS="-fvisibility=hidden"
AC_MSG_CHECKING([for -fvisibility=hidden compiler flag])
AC_TRY_COMPILE([], [int main (void) { return 0; }],
	       AC_MSG_RESULT(yes)
	       enable_fvisibility_hidden=yes,
	       AC_MSG_RESULT(no)
	       enable_fvisibility_hidden=no)
CFLAGS="${SAVED_CFLAGS}"

AS_IF([test "${enable_fvisibility_hidden}" = "yes"], [
  AC_DEFINE([_CLUTTER_EXTERN], [__attribute__((visibility("default"))) extern],
	    [defines how to decorate public symbols while building])
  CLUTTER_HIDDEN_VISIBILITY_CFLAGS="-fvisibility=hidden"
])
AC_SUBST(CLUTTER_HIDDEN_VISIBILITY_CFLAGS)

AC_CACHE_SAVE

dnl ========================================================================

FLAVOUR_LIBS=""
FLAVOUR_CFLAGS=""
CLUTTER_BACKENDS=""
CLUTTER_INPUT_BACKENDS=""
CLUTTER_CONFIG_DEFINES=

# base dependencies for core
CLUTTER_BASE_PC_FILES="cairo-gobject >= $CAIRO_REQ_VERSION gio-2.0 >= glib_req_version atk >= $ATK_REQ_VERSION pangocairo >= $PANGO_REQ_VERSION json-glib-1.0 >= $JSON_GLIB_REQ_VERSION"

# private base dependencies
CLUTTER_BASE_PC_FILES_PRIVATE=""

# backend specific pkg-config files
BACKEND_PC_FILES=""

# private backend specific dependencies
BACKEND_PC_FILES_PRIVATE=""

dnl === Clutter windowing system backend ======================================

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

AC_ARG_ENABLE(
  [wayland-egl-server],
  [AC_HELP_STRING([--enable-wayland-egl-server=@<:@no/yes@:>@], [Enable server side wayland support @<:@default=no@:>@])],
  [],
  enable_wayland_egl_server=yes
)

AS_IF([test "x$enable_wayland_egl_server" = "xyes"],
      [
        CLUTTER_BACKENDS="$CLUTTER_BACKENDS egl"
        SUPPORT_EGL=1
        AC_DEFINE([CLUTTER_EGL_BACKEND_GENERIC], [1], [Use Generic EGL backend])

        BACKEND_PC_FILES="$BACKEND_PC_FILES wayland-egl wayland-server libdrm gbm"

        SUPPORT_WAYLAND_COMPOSITOR=1
        CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
        #define CLUTTER_HAS_WAYLAND_COMPOSITOR_SUPPORT 1"

        CLUTTER_INPUT_BACKENDS="$CLUTTER_INPUT_BACKENDS evdev"

        SUPPORT_EVDEV=1
        BACKEND_PC_FILES_PRIVATE="$BACKEND_PC_FILES_PRIVATE libudev >= $LIBUDEV_REQ_VERSION libinput >= $LIBINPUT_REQ_VERSION xkbcommon"
        AC_DEFINE([HAVE_EVDEV], [1], [Have evdev support for input handling])
      ])

AM_CONDITIONAL(SUPPORT_WAYLAND, [test "x$enable_wayland_egl_server" = "xyes"])

dnl Instead of using AM_CFLAGS to ensure
dnl COGL_ENABLE_EXPERIMENTAL_2_0_API is defined while compiling clutter
dnl and cogl code we use a define in config.h instead. This helps ensure
dnl other tools such as glib-mkenums and gir-scanner don't end up
dnl using the define also.
AC_DEFINE([COGL_ENABLE_EXPERIMENTAL_2_0_API], [1], [Can use Cogl 2.0 API internally])
AC_DEFINE_UNQUOTED([CLUTTER_DRIVERS], ["*"], [List of Cogl drivers])

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
AS_IF([test "x$SUPPORT_GLX" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_WINDOWING_GLX \"glx\""])
AS_IF([test "x$SUPPORT_EGL" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_WINDOWING_EGL \"eglnative\""])
AS_IF([test "x$SUPPORT_EVDEV" = "x1"],
      [CLUTTER_CONFIG_DEFINES="$CLUTTER_CONFIG_DEFINES
#define CLUTTER_INPUT_EVDEV \"evdev\""])

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
                           -Wshadow"

AC_ARG_ENABLE([Werror],
              [AS_HELP_STRING([--disable-Werror], [Removes -Werror from compiler flags])],
              [],
              [enable_Werror=yes])

AS_IF([test "x$enable_Werror" = xyes], [
  MAINTAINER_COMPILER_FLAGS="$MAINTAINER_COMPILER_FLAGS
                             -Werror=logical-op
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
])

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

CLUTTER_CFLAGS="$FLAVOUR_CFLAGS $CLUTTER_DEPS_CFLAGS $CLUTTER_DEPS_PRIVATE_CFLAGS $GLIB_CFLAGS"
CLUTTER_LIBS="$FLAVOUR_LIBS $CLUTTER_DEPS_LIBS $CLUTTER_DEPS_PRIVATE_LIBS $GLIB_LIBS"
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

        ltp_version_list="1.6 1.7 1.8 1.9 1.10"
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

dnl === Conformance test suite ================================================

GLIB_TESTS

AC_ARG_ENABLE([examples],
              [AS_HELP_STRING([--enable-examples], [Whether examples should be built])],
              [],
              [enable_examples=no])
AM_CONDITIONAL(BUILD_EXAMPLES, [test "x$enable_examples" = "xyes"])

dnl ===========================================================================

AC_CONFIG_FILES([
	Makefile

	build/Makefile
	build/autotools/Makefile

	clutter/Makefile
        clutter/clutter-config.h
	clutter/clutter-version.h
	clutter/mutter-clutter-$CLUTTER_API_VERSION.pc:clutter/mutter-clutter.pc.in

	tests/Makefile
	tests/accessibility/Makefile
	tests/conform/Makefile
	tests/interactive/Makefile
	tests/interactive/wrapper.sh
	tests/micro-bench/Makefile
	tests/performance/Makefile

        examples/Makefile
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
echo "        Enable coverage tests: ${use_gcov}"
echo "        Enable deprecated symbols: ${enable_deprecated}"

# Miscellaneous
echo ""
echo " • Extra:"
echo "        Build introspection data: ${enable_introspection}"
if test "x$x11_tests" = "xyes"; then
echo "        Build X11-specific tests: ${x11_tests}"
fi
if test "x$pixbuf_tests" = "xyes"; then
echo "        Build tests using GDK-Pixbuf: ${pixbuf_tests}"
fi
echo "        Install test suites: ${enable_installed_tests}"
echo "        Build examples: ${enable_examples}"

# Clutter backend related flags
echo ""
echo " • Clutter Backends:"
echo "        Windowing systems: ${CLUTTER_BACKENDS}"
echo "        Input backends: ${CLUTTER_INPUT_BACKENDS}"

if test "x$SUPPORT_X11" = "x1"; then
echo ""
echo "     - X11 backend options:"
echo "        Enabled extensions: ${X11_EXTS}"
fi

if test "x$SUPPORT_WAYLAND_COMPOSITOR" = "x1"; then
echo ""
echo "     - Wayland compositor support enabled"
fi

echo ""
