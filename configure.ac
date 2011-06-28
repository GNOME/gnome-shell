AC_PREREQ(2.63)
AC_INIT([gnome-shell],[3.1.3],[https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-shell],[gnome-shell])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/shell-global.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([config])

AC_SUBST([PACKAGE_NAME], ["$PACKAGE_NAME"])
AC_SUBST([PACKAGE_VERSION], ["$PACKAGE_VERSION"])

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

# i18n
IT_PROG_INTLTOOL([0.40])

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.17])

GETTEXT_PACKAGE=gnome-shell
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])

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
   recorder_modules="gstreamer-0.10 gstreamer-base-0.10 x11"
   PKG_CHECK_MODULES(TEST_SHELL_RECORDER, $recorder_modules clutter-1.0 xfixes)
else
   AC_MSG_RESULT(no)
fi

AM_CONDITIONAL(BUILD_RECORDER, $build_recorder)

CLUTTER_MIN_VERSION=1.5.15
GOBJECT_INTROSPECTION_MIN_VERSION=0.10.1
GJS_MIN_VERSION=0.7.11
MUTTER_MIN_VERSION=3.0.0
GTK_MIN_VERSION=3.0.0
GIO_MIN_VERSION=2.25.9
LIBECAL_MIN_VERSION=2.32.0
LIBEDATASERVER_MIN_VERSION=1.2.0
LIBEDATASERVERUI_MIN_VERSION=2.91.6
TELEPATHY_GLIB_MIN_VERSION=0.15.0
TELEPATHY_LOGGER_MIN_VERSION=0.2.4
POLKIT_MIN_VERSION=0.100
STARTUP_NOTIFICATION_MIN_VERSION=0.11

# Collect more than 20 libraries for a prize!
PKG_CHECK_MODULES(GNOME_SHELL, gio-2.0 >= $GIO_MIN_VERSION
                               gio-unix-2.0 dbus-glib-1 libxml-2.0
                               gtk+-3.0 >= $GTK_MIN_VERSION
                               libmutter >= $MUTTER_MIN_VERSION
                               gjs-internals-1.0 >= $GJS_MIN_VERSION
			       libgnome-menu $recorder_modules gconf-2.0
                               gdk-x11-3.0 libsoup-2.4
			       clutter-x11-1.0 >= $CLUTTER_MIN_VERSION
			       clutter-glx-1.0 >= $CLUTTER_MIN_VERSION
                               libstartup-notification-1.0 >= $STARTUP_NOTIFICATION_MIN_VERSION
                               gobject-introspection-1.0 >= $GOBJECT_INTROSPECTION_MIN_VERSION
			       libcanberra
                               telepathy-glib >= $TELEPATHY_GLIB_MIN_VERSION
                               telepathy-logger-0.2 >= $TELEPATHY_LOGGER_MIN_VERSION
                               polkit-agent-1 >= $POLKIT_MIN_VERSION xfixes)

PKG_CHECK_MODULES(SHELL_PERF_HELPER, gtk+-3.0 gio-2.0)

GJS_VERSION=`$PKG_CONFIG --modversion gjs-internals-1.0`
AC_DEFINE_UNQUOTED([GJS_VERSION], ["$GJS_VERSION"], [The version of GJS we're linking to])
AC_SUBST([GJS_VERSION], ["$GJS_VERSION"])

GOBJECT_INTROSPECTION_CHECK([$GOBJECT_INTROSPECTION_MIN_VERSION])
JHBUILD_TYPELIBDIR="$INTROSPECTION_TYPELIBDIR"
# NM is the only typelib we use that we don't jhbuild
PKG_CHECK_EXISTS([libnm-glib >= 0.8.999],
        [NM_TYPELIBDIR=`$PKG_CONFIG --variable=libdir libnm-glib`/girepository-1.0
	 if test "$INTROSPECTION_TYPELIBDIR" != "$NM_TYPELIBDIR"; then
	     JHBUILD_TYPELIBDIR="$JHBUILD_TYPELIBDIR:$NM_TYPELIBDIR"
	 fi])
AC_SUBST(JHBUILD_TYPELIBDIR)

saved_CFLAGS=$CFLAGS
saved_LIBS=$LIBS
CFLAGS=$GNOME_SHELL_CFLAGS
LIBS=$GNOME_SHELL_LIBS
AC_CHECK_FUNCS(JS_NewGlobalObject XFixesCreatePointerBarrier)
CFLAGS=$saved_CFLAGS
LIBS=$saved_LIBS

PKG_CHECK_MODULES(ST, clutter-1.0 gtk+-3.0 libcroco-0.6 >= 0.6.2 gnome-desktop-3.0 >= 2.90.0 x11)
PKG_CHECK_MODULES(GDMUSER, dbus-glib-1 gtk+-3.0)
PKG_CHECK_MODULES(TRAY, gtk+-3.0)
PKG_CHECK_MODULES(GVC, libpulse libpulse-mainloop-glib gobject-2.0)
PKG_CHECK_MODULES(DESKTOP_SCHEMAS, gsettings-desktop-schemas >= 0.1.7)

AC_MSG_CHECKING([for bluetooth support])
PKG_CHECK_EXISTS([gnome-bluetooth-1.0 >= 3.1.0],
        [BLUETOOTH_DIR=`$PKG_CONFIG --variable=applet_libdir gnome-bluetooth-1.0`
	 BLUETOOTH_LIBS=`$PKG_CONFIG --variable=applet_libs gnome-bluetooth-1.0`
	 AC_SUBST([BLUETOOTH_LIBS],["$BLUETOOTH_LIBS"])
	 AC_DEFINE_UNQUOTED([BLUETOOTH_DIR],["$BLUETOOTH_DIR"],[Path to installed GnomeBluetooth typelib and library])
	 AC_DEFINE([HAVE_BLUETOOTH],[1],[Define if you have libgnome-bluetooth-applet])
	 AC_SUBST([HAVE_BLUETOOTH],[1])
	 AC_MSG_RESULT([yes])],
	[AC_DEFINE([HAVE_BLUETOOTH],[0])
	 AC_SUBST([HAVE_BLUETOOTH],[0])
	 AC_MSG_RESULT([no])])

PKG_CHECK_MODULES(CALENDAR_SERVER, libecal-1.2 >= $LIBECAL_MIN_VERSION libedataserver-1.2 >= $LIBEDATASERVER_MIN_VERSION libedataserverui-3.0 >= $LIBEDATASERVERUI_MIN_VERSION gio-2.0)
AC_SUBST(CALENDAR_SERVER_CFLAGS)
AC_SUBST(CALENDAR_SERVER_LIBS)

MUTTER_GIR_DIR=`$PKG_CONFIG --variable=girdir libmutter`
MUTTER_TYPELIB_DIR=`$PKG_CONFIG --variable=typelibdir libmutter`
AC_SUBST(MUTTER_GIR_DIR)
AC_SUBST(MUTTER_TYPELIB_DIR)

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

AC_ARG_ENABLE(jhbuild-wrapper-script,
  AS_HELP_STRING([--jhbuild-wrapper-script=yes],[Make "gnome-shell" script work for jhbuild]),,enable_jhbuild_wrapper_script=no)
AM_CONDITIONAL(USE_JHBUILD_WRAPPER_SCRIPT, test "x$enable_jhbuild_wrapper_script" = xyes)

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
