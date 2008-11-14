AC_INIT(gnome-shell, 0.0.1)

AC_CONFIG_AUX_DIR(config)

AM_INIT_AUTOMAKE([dist-bzip2 no-dist-gzip])
AM_MAINTAINER_MODE

AC_CONFIG_HEADERS(config.h)

AC_DISABLE_STATIC
AC_PROG_CC
AM_PROG_LIBTOOL

GETTEXT_PACKAGE=gnome-shell
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])

PKG_CHECK_MODULES(MUTTER_PLUGIN, gtk+-2.0 metacity-plugins gjs-gi-1.0)
PKG_CHECK_MODULES(TIDY, clutter-0.8)
PKG_CHECK_MODULES(TRAY, gtk+-2.0)

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
  src/Makefile
])
