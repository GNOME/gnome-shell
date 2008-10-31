AC_INIT(gnome-shell, 0.0.1)

AC_CONFIG_AUX_DIR(config)

AM_INIT_AUTOMAKE([dist-bzip2 no-dist-gzip])
AM_MAINTAINER_MODE

AC_CONFIG_HEADERS(config.h)

AC_PROG_CC
AM_PROG_LIBTOOL
AC_DISABLE_STATIC

GETTEXT_PACKAGE=gnome-shell
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
                   [The prefix for our gettext translation domains.])

PKG_CHECK_MODULES(MUTTER_PLUGIN, metacity-plugins gjs-gi-1.0)

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

AC_OUTPUT([
  Makefile
  src/Makefile
])
