AC_PREREQ(2.63)
AC_INIT([gnome-shell],[2.91.3],[https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-shell],[gnome-shell])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/shell-global.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([config])

AM_INIT_AUTOMAKE([1.10 dist-bzip2 no-dist-gzip foreign])
AM_MAINTAINER_MODE

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CC
# Needed for per-target cflags, like in gnomeshell-taskpanel
AM_PROG_CC_C_O

# Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

GETTEXT_PACKAGE=gnome-shell
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])
IT_PROG_INTLTOOL(0.26)
AM_GLIB_GNU_GETTEXT

PKG_PROG_PKG_CONFIG([0.22])

# GConf stuff
AC_PATH_PROG(GCONFTOOL, gconftool-2, no)
AM_GCONF_SOURCE_2

GLIB_GSETTINGS

# Get a value to substitute into gnome-shell.in
AM_PATH_PYTHON([2.5])
AC_SUBST(PYTHON)

# We need at least this, since gst_plugin_register_static() was added
# in 0.10.16, but nothing older than 0.10.21 has been tested.
GSTREAMER_MIN_VERSION=0.10.16

recorder_modules=
build_recorder=false
AC_MSG_CHECKING([for GStreamer (needed for recording functionality)])
if $PKG_CONFIG --exists gstreamer-0.10 '>=' $GSTREAMER_MIN_VERSION ; then
   AC_MSG_RESULT(yes)
   build_recorder=true
   recorder_modules="gstreamer-0.10 gstreamer-base-0.10 xfixes"
   PKG_CHECK_MODULES(TEST_SHELL_RECORDER, $recorder_modules clutter-1.0)
else
   AC_MSG_RESULT(no)
fi

AM_CONDITIONAL(BUILD_RECORDER, $build_recorder)

CLUTTER_MIN_VERSION=1.5.8
GOBJECT_INTROSPECTION_MIN_VERSION=0.6.11
GJS_MIN_VERSION=0.7
MUTTER_MIN_VERSION=2.91.0
GTK_MIN_VERSION=2.91.7
GIO_MIN_VERSION=2.25.9

# Collect more than 20 libraries for a prize!
PKG_CHECK_MODULES(MUTTER_PLUGIN, gio-2.0 >= $GIO_MIN_VERSION
                                 gio-unix-2.0 dbus-glib-1
                                 gtk+-3.0 >= $GTK_MIN_VERSION
                                 mutter-plugins >= $MUTTER_MIN_VERSION
                                 gjs-internals-1.0 >= $GJS_MIN_VERSION
				 libgnome-menu $recorder_modules gconf-2.0
                                 gdk-x11-3.0
				 clutter-x11-1.0 >= $CLUTTER_MIN_VERSION
				 clutter-glx-1.0 >= $CLUTTER_MIN_VERSION
                                 libstartup-notification-1.0
                                 gobject-introspection-1.0 >= $GOBJECT_INTROSPECTION_MIN_VERSION
				 libcanberra)

saved_CFLAGS=$CFLAGS
saved_LIBS=$LIBS
CFLAGS=$MUTTER_PLUGIN_CFLAGS
LIBS=$MUTTER_PLUGIN_LIBS
# sn_startup_sequence_get_application_id, we can replace with a version check later
AC_CHECK_FUNCS(JS_NewGlobalObject sn_startup_sequence_get_application_id)
CFLAGS=$saved_CFLAGS
LIBS=$saved_LIBS

PKG_CHECK_MODULES(TIDY, clutter-1.0)
PKG_CHECK_MODULES(ST, clutter-1.0 gtk+-3.0 libcroco-0.6 gnome-desktop-3.0 >= 2.90.0)
PKG_CHECK_MODULES(GDMUSER, dbus-glib-1 gtk+-3.0)
PKG_CHECK_MODULES(TRAY, gtk+-3.0)
PKG_CHECK_MODULES(GVC, libpulse libpulse-mainloop-glib gobject-2.0)
PKG_CHECK_MODULES(JS_TEST, clutter-x11-1.0 gjs-1.0 gobject-introspection-1.0 gtk+-3.0)

AC_MSG_CHECKING([for bluetooth support])
PKG_CHECK_EXISTS([gnome-bluetooth-1.0 >= 2.90.0],
        [BLUETOOTH_DIR=`$PKG_CONFIG --variable=libdir gnome-bluetooth-1.0`/gnome-bluetooth
	 BLUETOOTH_LIBS="-L'$BLUETOOTH_DIR' -lgnome-bluetooth-applet"
	 AC_SUBST([BLUETOOTH_LIBS],["$BLUETOOTH_LIBS"])
	 AC_DEFINE_UNQUOTED([BLUETOOTH_DIR],["$BLUETOOTH_DIR"],[Path to installed GnomeBluetooth typelib and library])
	 AC_DEFINE([HAVE_BLUETOOTH],[1],[Define if you have libgnome-bluetooth-applet])
	 AC_SUBST([HAVE_BLUETOOTH],[1])
	 AC_MSG_RESULT([yes])],
	[AC_DEFINE([HAVE_BLUETOOTH],[0])
	 AC_SUBST([HAVE_BLUETOOTH],[0])
	 AC_MSG_RESULT([no])])

MUTTER_BIN_DIR=`$PKG_CONFIG --variable=exec_prefix mutter-plugins`/bin
# FIXME: metacity-plugins.pc should point directly to its .gir file
MUTTER_LIB_DIR=`$PKG_CONFIG --variable=libdir mutter-plugins`
MUTTER_PLUGIN_DIR=`$PKG_CONFIG --variable=plugindir mutter-plugins`
AC_SUBST(MUTTER_BIN_DIR)
AC_SUBST(MUTTER_LIB_DIR)
AC_SUBST(MUTTER_PLUGIN_DIR)

GJS_CONSOLE=`$PKG_CONFIG --variable=gjs_console gjs-1.0`
AC_SUBST(GJS_CONSOLE)

AC_CHECK_FUNCS(fdwalk)
AC_CHECK_FUNCS(mallinfo)
AC_CHECK_HEADERS([sys/resource.h])

# Sets GLIB_GENMARSHAL and GLIB_MKENUMS
AM_PATH_GLIB_2_0()
G_IR_SCANNER=`$PKG_CONFIG --variable=g_ir_scanner gobject-introspection-1.0`
AC_SUBST(G_IR_SCANNER)
G_IR_COMPILER=`$PKG_CONFIG --variable=g_ir_compiler gobject-introspection-1.0`
AC_SUBST(G_IR_COMPILER)
G_IR_GENERATE=`$PKG_CONFIG --variable=g_ir_generate gobject-introspection-1.0`
AC_SUBST(G_IR_GENERATE)
GIRDIR=`$PKG_CONFIG --variable=girdir gobject-introspection-1.0`
AC_SUBST(GIRDIR)
TYPELIBDIR="$($PKG_CONFIG --variable=typelibdir gobject-introspection-1.0)"
AC_SUBST(TYPELIBDIR)

# Stay command-line compatible with the gnome-common configure option. Here
# minimum/yes/maximum are the same, however.
AC_ARG_ENABLE(compile_warnings,
  AS_HELP_STRING([--enable-compile-warnings=@<:@no/minimum/yes/maximum/error@:>@],[Turn on compiler warnings]),,
  enable_compile_warnings=error)

changequote(,)dnl
if test "$enable_compile_warnings" != no ; then
  if test "x$GCC" = "xyes"; then
    case " $CFLAGS " in
    *[\ \	]-Wall[\ \	]*) ;;
    *) CFLAGS="$CFLAGS -Wall" ;;
    esac
    case " $CFLAGS " in
    *[\ \	]-Wmissing-prototypes[\ \	]*) ;;
    *) CFLAGS="$CFLAGS -Wmissing-prototypes" ;;
    esac
    if test "$enable_compile_warnings" = error ; then
      case " $CFLAGS " in
      *[\ \	]-Werror[\ \	]*) ;;
      *) CFLAGS="$CFLAGS -Werror" ;;
      esac
    fi
  fi
fi
changequote([,])dnl

AC_PATH_PROG(mutter, [mutter])
AC_SUBST(mutter)

AC_CONFIG_FILES([
  Makefile
  data/Makefile
  js/Makefile
  js/misc/config.js
  src/Makefile
  tests/Makefile
  po/Makefile.in
  man/Makefile
])
AC_OUTPUT
