AC_PREREQ(2.63)
AC_INIT([gnome-shell],[3.10.0.1],[https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-shell],[gnome-shell])

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
IT_PROG_INTLTOOL([0.40])

GETTEXT_PACKAGE=gnome-shell
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])

PKG_PROG_PKG_CONFIG([0.22])

AC_PATH_PROG([XSLTPROC], [xsltproc])

GLIB_GSETTINGS

# Get a value to substitute into gnome-shell.in
AM_PATH_PYTHON([2.5])
AC_SUBST(PYTHON)

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
   PKG_CHECK_MODULES(TEST_SHELL_RECORDER, $recorder_modules clutter-1.0)
else
   AC_MSG_RESULT(no)
fi

AM_CONDITIONAL(BUILD_RECORDER, $build_recorder)

CLUTTER_MIN_VERSION=1.13.4
GOBJECT_INTROSPECTION_MIN_VERSION=0.10.1
GJS_MIN_VERSION=1.38.1
MUTTER_MIN_VERSION=3.10.0
GTK_MIN_VERSION=3.7.9
GIO_MIN_VERSION=2.37.0
LIBECAL_MIN_VERSION=3.5.3
LIBEDATASERVER_MIN_VERSION=3.5.3
TELEPATHY_GLIB_MIN_VERSION=0.17.5
POLKIT_MIN_VERSION=0.100
STARTUP_NOTIFICATION_MIN_VERSION=0.11
GCR_MIN_VERSION=3.7.5
GNOME_DESKTOP_REQUIRED_VERSION=3.7.90
GNOME_MENUS_REQUIRED_VERSION=3.5.3
NETWORKMANAGER_MIN_VERSION=0.9.8
PULSE_MIN_VERS=2.0

# Collect more than 20 libraries for a prize!
SHARED_PCS="gio-unix-2.0 >= $GIO_MIN_VERSION
            libxml-2.0
            gtk+-3.0 >= $GTK_MIN_VERSION
            atk-bridge-2.0
            gjs-internals-1.0 >= $GJS_MIN_VERSION
            libgnome-menu-3.0 >= $GNOME_MENUS_REQUIRED_VERSION
            $recorder_modules
            gdk-x11-3.0 libsoup-2.4
            xtst
            clutter-x11-1.0 >= $CLUTTER_MIN_VERSION
            clutter-glx-1.0 >= $CLUTTER_MIN_VERSION
            libstartup-notification-1.0 >= $STARTUP_NOTIFICATION_MIN_VERSION
            gobject-introspection-1.0 >= $GOBJECT_INTROSPECTION_MIN_VERSION
            libcanberra libcanberra-gtk3
            telepathy-glib >= $TELEPATHY_GLIB_MIN_VERSION
            polkit-agent-1 >= $POLKIT_MIN_VERSION
            libnm-glib libnm-util >= $NETWORKMANAGER_MIN_VERSION
            libnm-gtk >= $NETWORKMANAGER_MIN_VERSION
            libsecret-unstable gcr-base-3 >= $GCR_MIN_VERSION"

PKG_CHECK_MODULES(GNOME_SHELL, $SHARED_PCS)
PKG_CHECK_MODULES(MUTTER, libmutter >= $MUTTER_MIN_VERSION)
PKG_CHECK_MODULES(MUTTER_WAYLAND, [libmutter-wayland >= $MUTTER_MIN_VERSION],
                 [MUTTER_WAYLAND_TYPELIB_DIR=`$PKG_CONFIG --variable=typelibdir libmutter-wayland`
                  AC_SUBST(MUTTER_WAYLAND_TYPELIB_DIR)
                  have_mutter_wayland=yes],
                 [have_mutter_wayland=no])

AM_CONDITIONAL(HAVE_MUTTER_WAYLAND, test $have_mutter_wayland != no)

PKG_CHECK_MODULES(GNOME_SHELL_JS, gio-2.0 gjs-internals-1.0 >= $GJS_MIN_VERSION)
PKG_CHECK_MODULES(ST, clutter-1.0 gtk+-3.0 libcroco-0.6 >= 0.6.8 x11)
PKG_CHECK_MODULES(SHELL_PERF_HELPER, gtk+-3.0 gio-2.0)
PKG_CHECK_MODULES(SHELL_HOTPLUG_SNIFFER, gio-2.0 gdk-pixbuf-2.0)
PKG_CHECK_MODULES(BROWSER_PLUGIN, gio-2.0 >= $GIO_MIN_VERSION json-glib-1.0 >= 0.13.2)
PKG_CHECK_MODULES(TRAY, gtk+-3.0)
PKG_CHECK_MODULES(GVC, libpulse >= $PULSE_MIN_VERS libpulse-mainloop-glib gobject-2.0)
PKG_CHECK_MODULES(DESKTOP_SCHEMAS, gsettings-desktop-schemas >= 3.7.4)
PKG_CHECK_MODULES(CARIBOU, caribou-1.0 >= 0.4.8)

AC_MSG_CHECKING([for bluetooth support])
PKG_CHECK_EXISTS([gnome-bluetooth-1.0 >= 3.9.0],
        [BLUETOOTH_DIR=`$PKG_CONFIG --variable=applet_libdir gnome-bluetooth-1.0`
	 BLUETOOTH_LIBS=`$PKG_CONFIG --variable=applet_libs gnome-bluetooth-1.0`
	 AC_SUBST([BLUETOOTH_LIBS],["$BLUETOOTH_LIBS"])
	 AC_SUBST([BLUETOOTH_DIR],["$BLUETOOTH_DIR"])
	 AC_DEFINE_UNQUOTED([BLUETOOTH_DIR],["$BLUETOOTH_DIR"],[Path to installed GnomeBluetooth typelib and library])
	 AC_DEFINE([HAVE_BLUETOOTH],[1],[Define if you have libgnome-bluetooth-applet])
	 AC_SUBST([HAVE_BLUETOOTH],[1])
	 AC_MSG_RESULT([yes])],
	[AC_DEFINE([HAVE_BLUETOOTH],[0])
	 AC_SUBST([HAVE_BLUETOOTH],[0])
	 AC_MSG_RESULT([no])])

PKG_CHECK_MODULES(CALENDAR_SERVER, libecal-1.2 >= $LIBECAL_MIN_VERSION libedataserver-1.2 >= $LIBEDATASERVER_MIN_VERSION gio-2.0)
AC_SUBST(CALENDAR_SERVER_CFLAGS)
AC_SUBST(CALENDAR_SERVER_LIBS)

GNOME_KEYBINDINGS_KEYSDIR=`$PKG_CONFIG --variable keysdir gnome-keybindings`
AC_SUBST([GNOME_KEYBINDINGS_KEYSDIR])

GOBJECT_INTROSPECTION_CHECK([$GOBJECT_INTROSPECTION_MIN_VERSION])

MUTTER_GIR_DIR=`$PKG_CONFIG --variable=girdir libmutter`
AC_SUBST(MUTTER_GIR_DIR)

MUTTER_TYPELIB_DIR=`$PKG_CONFIG --variable=typelibdir libmutter`
AC_SUBST(MUTTER_TYPELIB_DIR)

GJS_CONSOLE=`$PKG_CONFIG --variable=gjs_console gjs-1.0`
AC_SUBST(GJS_CONSOLE)

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

GNOME_COMPILE_WARNINGS([error])

BROWSER_PLUGIN_DIR="${BROWSER_PLUGIN_DIR:-"\${libdir}/mozilla/plugins"}"
AC_ARG_VAR([BROWSER_PLUGIN_DIR],[Where to install the plugin to])

AC_CONFIG_FILES([
  Makefile
  data/Makefile
  docs/Makefile
  docs/reference/Makefile
  docs/reference/shell/Makefile
  docs/reference/shell/shell-docs.sgml
  docs/reference/st/Makefile
  docs/reference/st/st-docs.sgml
  js/Makefile
  src/calendar-server/evolution-calendar.desktop.in
  src/Makefile
  src/gvc/Makefile
  browser-plugin/Makefile
  tests/Makefile
  po/Makefile.in
  man/Makefile
])
AC_OUTPUT
