AC_PREREQ(2.63)
AC_INIT([gnome-shell],[3.5.2],[https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-shell],[gnome-shell])

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
   PKG_CHECK_MODULES(TEST_SHELL_RECORDER, $recorder_modules clutter-1.0 xfixes gl)
else
   AC_MSG_RESULT(no)
fi

AM_CONDITIONAL(BUILD_RECORDER, $build_recorder)

CLUTTER_MIN_VERSION=1.9.16
GOBJECT_INTROSPECTION_MIN_VERSION=0.10.1
GJS_MIN_VERSION=1.33.2
MUTTER_MIN_VERSION=3.5.2
FOLKS_MIN_VERSION=0.5.2
GTK_MIN_VERSION=3.3.9
GIO_MIN_VERSION=2.31.6
LIBECAL_MIN_VERSION=3.5.3
LIBEDATASERVER_MIN_VERSION=3.5.3
LIBEDATASERVERUI_MIN_VERSION=3.5.3
TELEPATHY_GLIB_MIN_VERSION=0.17.5
TELEPATHY_LOGGER_MIN_VERSION=0.2.4
POLKIT_MIN_VERSION=0.100
STARTUP_NOTIFICATION_MIN_VERSION=0.11
GCR_MIN_VERSION=3.3.90
GNOME_DESKTOP_REQUIRED_VERSION=3.5.1

AC_ARG_WITH(folks,
            AS_HELP_STRING([--with-folks],
                           [Enable folks support]),
            [with_folks=$withval], [with_folks=yes])

if test x${with_folks} = xyes; then
  FOLKS_REQUIREMENT="folks >= $FOLKS_MIN_VERSION"
  AC_DEFINE([HAVE_FOLKS], [1], [folks support])
  AC_SUBST([HAVE_FOLKS],[1])
else
  FOLKS_REQUIREMENT=
fi
AM_CONDITIONAL(BUILD_WITH_FOLKS, test x${with_folks} = xyes)

# Collect more than 20 libraries for a prize!
PKG_CHECK_MODULES(GNOME_SHELL, gio-unix-2.0 >= $GIO_MIN_VERSION
			       libxml-2.0
                               gtk+-3.0 >= $GTK_MIN_VERSION
                               atk-bridge-2.0
			       $FOLKS_REQUIREMENT
                               libmutter >= $MUTTER_MIN_VERSION
                               gjs-internals-1.0 >= $GJS_MIN_VERSION
			       libgnome-menu-3.0 $recorder_modules
                               gdk-x11-3.0 libsoup-2.4
                               gl
			       clutter-x11-1.0 >= $CLUTTER_MIN_VERSION
			       clutter-glx-1.0 >= $CLUTTER_MIN_VERSION
                               libstartup-notification-1.0 >= $STARTUP_NOTIFICATION_MIN_VERSION
                               gobject-introspection-1.0 >= $GOBJECT_INTROSPECTION_MIN_VERSION
			       libcanberra
                               telepathy-glib >= $TELEPATHY_GLIB_MIN_VERSION
                               telepathy-logger-0.2 >= $TELEPATHY_LOGGER_MIN_VERSION
                               polkit-agent-1 >= $POLKIT_MIN_VERSION xfixes
                               libnm-glib libnm-util gnome-keyring-1
                               gcr-3 >= $GCR_MIN_VERSION
                               gnome-desktop-3.0 >= $GNOME_DESKTOP_REQUIRED_VERSION)

PKG_CHECK_MODULES(SHELL_PERF_HELPER, gtk+-3.0 gio-2.0)

PKG_CHECK_MODULES(SHELL_HOTPLUG_SNIFFER, gio-2.0 gdk-pixbuf-2.0)

PKG_CHECK_MODULES(BROWSER_PLUGIN, gio-2.0 >= $GIO_MIN_VERSION json-glib-1.0 >= 0.13.2)

GOBJECT_INTROSPECTION_CHECK([$GOBJECT_INTROSPECTION_MIN_VERSION])

saved_CFLAGS=$CFLAGS
saved_LIBS=$LIBS
CFLAGS=$GNOME_SHELL_CFLAGS
LIBS=$GNOME_SHELL_LIBS
AC_CHECK_FUNCS(JS_NewGlobalObject XFixesCreatePointerBarrier)
CFLAGS=$saved_CFLAGS
LIBS=$saved_LIBS

PKG_CHECK_MODULES(GNOME_SHELL_JS, gio-2.0 gjs-internals-1.0 >= $GJS_MIN_VERSION)
PKG_CHECK_MODULES(ST, clutter-1.0 gtk+-3.0 libcroco-0.6 >= 0.6.2 x11)
PKG_CHECK_MODULES(TRAY, gtk+-3.0)
PKG_CHECK_MODULES(GVC, libpulse libpulse-mainloop-glib gobject-2.0)
PKG_CHECK_MODULES(DESKTOP_SCHEMAS, gsettings-desktop-schemas >= 3.5.1)

AC_MSG_CHECKING([for bluetooth support])
PKG_CHECK_EXISTS([gnome-bluetooth-1.0 >= 3.1.0],
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

PKG_CHECK_MODULES(CALENDAR_SERVER, libecal-1.2 >= $LIBECAL_MIN_VERSION libedataserver-1.2 >= $LIBEDATASERVER_MIN_VERSION libedataserverui-3.0 >= $LIBEDATASERVERUI_MIN_VERSION gio-2.0)
AC_SUBST(CALENDAR_SERVER_CFLAGS)
AC_SUBST(CALENDAR_SERVER_LIBS)

AC_ARG_WITH(systemd,
            AS_HELP_STRING([--with-systemd],
                           [Add systemd support]),
            [with_systemd=$withval], [with_systemd=auto])

PKG_CHECK_MODULES(SYSTEMD,
                  [libsystemd-login libsystemd-daemon],
                  [have_systemd=yes], [have_systemd=no])

if test "x$with_systemd" = "xauto" ; then
        if test x$have_systemd = xno ; then
                use_systemd=no
        else
                use_systemd=yes
        fi
else
        use_systemd=$with_systemd
fi

if test "x$use_systemd" = "xyes"; then
        if test "x$have_systemd" = "xno"; then
                AC_MSG_ERROR([Systemd support explicitly required, but systemd not found])
        fi

        AC_DEFINE(WITH_SYSTEMD, 1, [systemd support])
fi

MUTTER_GIR_DIR=`$PKG_CONFIG --variable=girdir libmutter`
MUTTER_TYPELIB_DIR=`$PKG_CONFIG --variable=typelibdir libmutter`
AC_SUBST(MUTTER_GIR_DIR)
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

GTK_DOC_CHECK([1.15], [--flavour no-tmpl])

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
      *) CFLAGS="$CFLAGS -Werror -Wno-error=deprecated-declarations" ;;
      esac
    fi
  fi
fi
changequote([,])dnl

AC_ARG_ENABLE(jhbuild-wrapper-script,
  AS_HELP_STRING([--enable-jhbuild-wrapper-script],[Make "gnome-shell" script work for jhbuild]),,enable_jhbuild_wrapper_script=no)
AM_CONDITIONAL(USE_JHBUILD_WRAPPER_SCRIPT, test "x$enable_jhbuild_wrapper_script" = xyes)

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
  src/Makefile
  browser-plugin/Makefile
  tests/Makefile
  po/Makefile.in
  man/Makefile
])
AC_OUTPUT
