AC_PREREQ(2.63)
AC_INIT([gnome-shell],[3.25.4],[https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-shell],[gnome-shell])
AX_IS_RELEASE([git-directory])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/shell-global.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([config])

AC_SUBST([PACKAGE_NAME], ["$PACKAGE_NAME"])
AC_SUBST([PACKAGE_VERSION], ["$PACKAGE_VERSION"])

AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz tar-ustar foreign])
AM_MAINTAINER_MODE([enable])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CC

# Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

# i18n
GETTEXT_PACKAGE=gnome-shell
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])

AM_GNU_GETTEXT_VERSION([0.19.6])
AM_GNU_GETTEXT([external])

PKG_PROG_PKG_CONFIG([0.22])

AC_PATH_PROG([XSLTPROC], [xsltproc])

GLIB_GSETTINGS

# Get a value to substitute into gnome-shell.in
AM_PATH_PYTHON([3])
AC_SUBST(PYTHON)

# We depend on a specific version of the libmutter API. The mutter variants of
# the Cogl and Clutter libraries also use this API version.
LIBMUTTER_API_VERSION=1

LIBMUTTER=libmutter-$LIBMUTTER_API_VERSION
LIBMUTTER_COGL=mutter-cogl-$LIBMUTTER_API_VERSION
LIBMUTTER_COGL_PANGO=mutter-cogl-pango-$LIBMUTTER_API_VERSION
LIBMUTTER_CLUTTER=mutter-clutter-$LIBMUTTER_API_VERSION

# We need at least this, since gst_plugin_register_static() was added
# in 0.10.16, but nothing older than 0.10.21 has been tested.
GSTREAMER_MIN_VERSION=0.11.92

recorder_modules=
build_recorder=false
AC_MSG_CHECKING([for GStreamer (needed for recording functionality)])
if $PKG_CONFIG --exists gstreamer-1.0 '>=' $GSTREAMER_MIN_VERSION ; then
   AC_MSG_RESULT(yes)
   build_recorder=true
   recorder_modules="gstreamer-1.0 gstreamer-base-1.0 x11 gtk+-3.0"
else
   AC_MSG_RESULT(no)
fi

AM_CONDITIONAL(BUILD_RECORDER, $build_recorder)

AC_ARG_ENABLE([systemd],
              AS_HELP_STRING([--enable-systemd], [Use systemd]),
              [enable_systemd=$enableval],
              [enable_systemd=auto])
AS_IF([test x$enable_systemd != xno], [
  AC_MSG_CHECKING([for libsystemd])
  PKG_CHECK_EXISTS([libsystemd],
                   [have_systemd=yes
                    AC_DEFINE([HAVE_SYSTEMD], [1], [Define if we have systemd])],
                   [have_systemd=no])
  AC_MSG_RESULT($have_systemd)
])

AC_MSG_RESULT($enable_systemd)

GOBJECT_INTROSPECTION_MIN_VERSION=1.49.1
GJS_MIN_VERSION=1.47.0
MUTTER_MIN_VERSION=3.25.3
GTK_MIN_VERSION=3.15.0
GIO_MIN_VERSION=2.53.0
LIBECAL_MIN_VERSION=3.5.3
LIBEDATASERVER_MIN_VERSION=3.17.2
POLKIT_MIN_VERSION=0.100
STARTUP_NOTIFICATION_MIN_VERSION=0.11
GCR_MIN_VERSION=3.7.5
GNOME_DESKTOP_REQUIRED_VERSION=3.7.90
NETWORKMANAGER_MIN_VERSION=0.9.8
PULSE_MIN_VERS=2.0

# Collect more than 20 libraries for a prize!
SHARED_PCS="gio-unix-2.0 >= $GIO_MIN_VERSION
            libxml-2.0
            gtk+-3.0 >= $GTK_MIN_VERSION
            atk-bridge-2.0
            gjs-1.0 >= $GJS_MIN_VERSION
            $recorder_modules
            gdk-x11-3.0 libsoup-2.4
            $LIBMUTTER_CLUTTER >= $MUTTER_MIN_VERSION
            $LIBMUTTER_COGL_PANGO
            libstartup-notification-1.0 >= $STARTUP_NOTIFICATION_MIN_VERSION
            gobject-introspection-1.0 >= $GOBJECT_INTROSPECTION_MIN_VERSION
            libcanberra libcanberra-gtk3
            polkit-agent-1 >= $POLKIT_MIN_VERSION
            gcr-base-3 >= $GCR_MIN_VERSION"
if test x$have_systemd = xyes; then
  SHARED_PCS="${SHARED_PCS} libsystemd"
fi

PKG_CHECK_MODULES(GNOME_SHELL, $SHARED_PCS)
PKG_CHECK_MODULES(MUTTER, $LIBMUTTER >= $MUTTER_MIN_VERSION)

PKG_CHECK_MODULES(GNOME_SHELL_JS, gio-2.0 gjs-1.0 >= $GJS_MIN_VERSION)
PKG_CHECK_MODULES(ST, $LIBMUTTER_CLUTTER gtk+-3.0 libcroco-0.6 >= 0.6.8 x11)
PKG_CHECK_MODULES(SHELL_PERF_HELPER, gtk+-3.0 gio-2.0)
PKG_CHECK_MODULES(SHELL_HOTPLUG_SNIFFER, gio-2.0 gdk-pixbuf-2.0)
PKG_CHECK_MODULES(TRAY, $LIBMUTTER_CLUTTER gtk+-3.0)
PKG_CHECK_MODULES(GVC, libpulse >= $PULSE_MIN_VERS libpulse-mainloop-glib gobject-2.0)
PKG_CHECK_MODULES(DESKTOP_SCHEMAS, gsettings-desktop-schemas >= 3.21.3)

AC_SUBST(LIBMUTTER_API_VERSION)

AC_ARG_ENABLE(browser-plugin,
              [AS_HELP_STRING([--enable-browser-plugin],
                              [Enable browser plugin [default=yes]])],,
              enable_browser_plugin=yes)
AS_IF([test x$enable_browser_plugin = xyes], [
  PKG_CHECK_MODULES(BROWSER_PLUGIN, gio-2.0 >= $GIO_MIN_VERSION json-glib-1.0 >= 0.13.2)
])
AM_CONDITIONAL(BUILD_BROWSER_PLUGIN, test x$enable_browser_plugin = xyes)

PKG_CHECK_MODULES(BLUETOOTH, gnome-bluetooth-1.0 >= 3.9.0,
        [AC_DEFINE([HAVE_BLUETOOTH],[1],[Define if you have libgnome-bluetooth-applet])
	 AC_SUBST([HAVE_BLUETOOTH],[1])],
	[AC_DEFINE([HAVE_BLUETOOTH],[0])
	 AC_SUBST([HAVE_BLUETOOTH],[0])])

PKG_CHECK_MODULES(CALENDAR_SERVER, libecal-1.2 >= $LIBECAL_MIN_VERSION libedataserver-1.2 >= $LIBEDATASERVER_MIN_VERSION gio-2.0)
AC_SUBST(CALENDAR_SERVER_CFLAGS)
AC_SUBST(CALENDAR_SERVER_LIBS)

GNOME_KEYBINDINGS_KEYSDIR=`$PKG_CONFIG --variable keysdir gnome-keybindings`
AC_SUBST([GNOME_KEYBINDINGS_KEYSDIR])

GOBJECT_INTROSPECTION_CHECK([$GOBJECT_INTROSPECTION_MIN_VERSION])

MUTTER_GIR_DIR=`$PKG_CONFIG --variable=girdir $LIBMUTTER`
AC_SUBST(MUTTER_GIR_DIR)

MUTTER_TYPELIB_DIR=`$PKG_CONFIG --variable=typelibdir $LIBMUTTER`
AC_SUBST(MUTTER_TYPELIB_DIR)

GLIB_COMPILE_RESOURCES=`$PKG_CONFIG --variable glib_compile_resources gio-2.0`
AC_SUBST(GLIB_COMPILE_RESOURCES)

AC_CHECK_FUNCS(fdwalk)
AC_CHECK_FUNCS(mallinfo)
AC_CHECK_HEADERS([sys/resource.h])

# _NL_TIME_FIRST_WEEKDAY is an enum and not a define
AC_MSG_CHECKING([for _NL_TIME_FIRST_WEEKDAY])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <langinfo.h>]],
                                   [[nl_langinfo(_NL_TIME_FIRST_WEEKDAY);]])],
               [langinfo_ok=yes], [langinfo_ok=no])
AC_MSG_RESULT($langinfo_ok)
if test "$langinfo_ok" = "yes"; then
  AC_DEFINE([HAVE__NL_TIME_FIRST_WEEKDAY], [1],
            [Define if _NL_TIME_FIRST_WEEKDAY is available])
fi

AC_ARG_ENABLE(networkmanager,
             AS_HELP_STRING([--disable-networkmanager],
                            [disable NetworkManager support  @<:@default=auto@:>@]),,
              [enable_networkmanager=auto])

if test "x$enable_networkmanager" != "xno"; then
   PKG_CHECK_MODULES(NETWORKMANAGER,
                     [libnm-glib
                     libnm-util >= $NETWORKMANAGER_MIN_VERSION
                     libnm-gtk >= $NETWORKMANAGER_MIN_VERSION
                     libsecret-1 >= 0.18],
                     [have_networkmanager=yes],
                     [have_networkmanager=no])

   GNOME_SHELL_CFLAGS="$GNOME_SHELL_CFLAGS $NETWORKMANAGER_CFLAGS"
   GNOME_SHELL_LIBS="$GNOME_SHELL_LIBS $NETWORKMANAGER_LIBS"
else
   have_networkmanager="no  (disabled)"
fi

if test "x$have_networkmanager" = "xyes"; then
   AC_DEFINE(HAVE_NETWORKMANAGER, [1], [Define if we have NetworkManager])
   AC_SUBST([HAVE_NETWORKMANAGER], [1])
else
   if test "x$enable_networkmanager" = "xyes"; then
      AC_MSG_ERROR([Couldn't find NetworkManager.])
   fi
   AC_SUBST([HAVE_NETWORKMANAGER], [0])
fi

AM_CONDITIONAL(HAVE_NETWORKMANAGER, test "$have_networkmanager" = "yes")

# Sets GLIB_GENMARSHAL and GLIB_MKENUMS
AM_PATH_GLIB_2_0()

GTK_DOC_CHECK([1.15], [--flavour no-tmpl])

AC_ARG_ENABLE(man,
              [AS_HELP_STRING([--enable-man],
                              [generate man pages [default=yes]])],,
              enable_man=yes)
if test "$enable_man" != no; then
  AC_PATH_PROG([XSLTPROC], [xsltproc])
  if test -z "$XSLTPROC"; then
    AC_MSG_ERROR([xsltproc is required for --enable-man])
  fi
fi
AM_CONDITIONAL(ENABLE_MAN, test "$enable_man" != no)

AX_COMPILER_FLAGS()
case "$WARN_CFLAGS" in
    *-Werror*)
        WARN_CFLAGS="$WARN_CFLAGS -Wno-error=deprecated-declarations"
        ;;
esac

AM_CFLAGS="$AM_CFLAGS $WARN_CFLAGS"
AC_SUBST(AM_CFLAGS)

if test -z "${BROWSER_PLUGIN_DIR}"; then
  BROWSER_PLUGIN_DIR="\${libdir}/mozilla/plugins"
fi
AC_ARG_VAR([BROWSER_PLUGIN_DIR],[Where to install the plugin to])

AC_ARG_VAR([GDBUS_CODEGEN],[the gdbus-codegen programme])
AC_PATH_PROG([GDBUS_CODEGEN],[gdbus-codegen],[])
if test -z "$GDBUS_CODEGEN"; then
  AC_MSG_ERROR([gdbus-codegen not found])
fi

AC_PATH_PROG([SASSC],[sassc],[])

AC_CONFIG_FILES([
  Makefile
  data/Makefile
  docs/Makefile
  docs/reference/Makefile
  docs/reference/shell/Makefile
  docs/reference/shell/version.xml
  docs/reference/st/Makefile
  docs/reference/st/version.xml
  js/Makefile
  src/Makefile
  subprojects/gvc/Makefile
  browser-plugin/Makefile
  tests/Makefile
  po/Makefile.in
  man/Makefile
])
AC_OUTPUT

echo "
Build configuration:

       Prefix:                                 ${prefix}
       Source code location:                   ${srcdir}
       Compiler:                               ${CC}
       Compiler Warnings:                      $ax_enable_compile_warnings

       Support for NetworkManager:             $have_networkmanager
       Support for GStreamer recording:        $build_recorder
"
