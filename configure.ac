AC_INIT(gnome-shell, 0.0.1)

AC_CONFIG_AUX_DIR(config)

AM_INIT_AUTOMAKE([dist-bzip2 no-dist-gzip])
AM_MAINTAINER_MODE

AC_CONFIG_HEADERS(config.h)

AC_DISABLE_STATIC
AC_PROG_CC
# Needed for per-target cflags, like in gnomeshell-taskpanel
AM_PROG_CC_C_O
AM_PROG_LIBTOOL

GETTEXT_PACKAGE=gnome-shell
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])

PKG_CHECK_MODULES(MUTTER_PLUGIN, gtk+-2.0 dbus-glib-1 metacity-plugins gjs-gi-1.0)
PKG_CHECK_MODULES(TIDY, clutter-0.9)
PKG_CHECK_MODULES(BIG, clutter-0.9 gtk+-2.0 librsvg-2.0)
PKG_CHECK_MODULES(GDMUSER, dbus-glib-1 gtk+-2.0)
PKG_CHECK_MODULES(TRAY, gtk+-2.0)
PKG_CHECK_MODULES(TASKPANEL, libwnck-1.0 dbus-glib-1)
# We require libgnomeui for generating thumbnails for recent files with GnomeThumbnailFactory.
# We'll switch to using GnomeDesktopThumbnailFactory once the branch of gnome-desktop that contains
# it becomes stable.
PKG_CHECK_MODULES(LIBGNOMEUI, libgnomeui-2.0)

META_BIN_DIR=`$PKG_CONFIG --variable=exec_prefix metacity-plugins`/bin
# FIXME: metacity-plugins.pc should point directly to its .gir file
META_LIB_DIR=`$PKG_CONFIG --variable=libdir metacity-plugins`
AC_SUBST(META_BIN_DIR)
AC_SUBST(META_LIB_DIR)

GJS_JS_DIR=`$PKG_CONFIG --variable=jsdir gjs-1.0`
GJS_JS_NATIVE_DIR=`$PKG_CONFIG --variable=jsnativedir gjs-1.0`
AC_SUBST(GJS_JS_DIR)
AC_SUBST(GJS_JS_NATIVE_DIR)

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

changequote(,)dnl
if test "x$GCC" = "xyes"; then
  case " $CFLAGS " in
  *[\ \	]-Wall[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wall" ;;
  esac

  case " $OBJCFLAGS " in
  *[\ \	]-Wall[\ \	]*) ;;
  *) OBJCFLAGS="$OBJCFLAGS -Wall" ;;
  esac
fi
changequote([,])dnl

AC_PATH_PROG(metacity, [metacity])
AC_SUBST(metacity)

AC_OUTPUT([
  Makefile
  data/Makefile
  js/Makefile
  js/ui/Makefile
  src/Makefile
])
