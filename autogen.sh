#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir
PROJECT=metacity
TEST_TYPE=-f
FILE=src/display.c

DIE=0

(autoconf --version) < /dev/null > /dev/null 2>&1 || {
	echo
	echo "You must have autoconf installed to compile $PROJECT."
	echo "Download the appropriate package for your distribution,"
	echo "or get the source tarball at http://ftp.gnu.org/gnu/autoconf/"
	DIE=1
}

if automake-1.9 --version < /dev/null > /dev/null 2>&1; then
  AUTOMAKE=automake-1.9
  ACLOCAL=aclocal-1.9
elif automake-1.8 --version < /dev/null > /dev/null 2>&1; then
  AUTOMAKE=automake-1.8
  ACLOCAL=aclocal-1.8
elif automake-1.7 --version < /dev/null > /dev/null 2>&1; then
  AUTOMAKE=automake-1.7
  ACLOCAL=aclocal-1.7
else
        echo
        echo "You must have automake >= 1.7 installed to compile $PROJECT."
        echo "Get http://ftp.gnu.org/gnu/automake/automake-1.9.3.tar.bz2"
        echo "(or a newer version if it is available)"
        DIE=1
fi

(grep "^AM_PROG_LIBTOOL" configure.in >/dev/null) && {
  (libtool --version) < /dev/null > /dev/null 2>&1 || {
    echo
    echo "**Error**: You must have \`libtool' installed to compile $PROJECT."
    echo "Get http://ftp.gnu.org/gnu/libtool/libtool-1.5.10.tar.gz"
    echo "(or a newer version if it is available)"
    DIE=1
  }
}

CONFIGURE=configure.in
if grep "^AM_[A-Z0-9_]\{1,\}_GETTEXT" "$CONFIGURE" >/dev/null; then
  if grep "sed.*POTFILES" "$CONFIGURE" >/dev/null; then
    GETTEXTIZE=""
  else
    if grep "^AM_GLIB_GNU_GETTEXT" "$CONFIGURE" >/dev/null; then
      GETTEXTIZE="glib-gettextize"
      GETTEXTIZE_URL="ftp://ftp.gtk.org/pub/gtk/v2.0/glib-2.0.0.tar.gz"
    else
      GETTEXTIZE="gettextize"
      GETTEXTIZE_URL="ftp://alpha.gnu.org/gnu/gettext-0.10.35.tar.gz"
    fi

    $GETTEXTIZE --version < /dev/null > /dev/null 2>&1
    if test $? -ne 0; then
      echo
      echo "**Error**: You must have \`$GETTEXTIZE' installed to compile $PKG_NAME."
      echo "Get $GETTEXTIZE_URL"
      echo "(or a newer version if it is available)"
      DIE=1
    fi
  fi
fi

if test "$DIE" -eq 1; then
	exit 1
fi

test $TEST_TYPE $FILE || {
	echo "You must run this script in the top-level $PROJECT directory"
	exit 1
}

if test -z "$*"; then
	echo "I am going to run ./configure with no arguments - if you wish "
        echo "to pass any to it, please specify them on the $0 command line."
fi

topdir=`pwd`
for coin in .
do 
  dr=`dirname $coin`
  if test -f $dr/NO-AUTO-GEN; then
    echo skipping $dr -- flagged as no auto-gen
  else
    echo processing $dr
    cd $dr
    if grep "^AM_GLIB_GNU_GETTEXT" configure.in >/dev/null; then
      if grep "sed.*POTFILES" configure.in >/dev/null; then
	: do nothing -- we still have an old unmodified configure.in
      else
	echo "Creating $dr/aclocal.m4 ..."
	test -r $dr/aclocal.m4 || touch $dr/aclocal.m4
	echo "Running glib-gettextize...  Ignore non-fatal messages."
	echo "no" | glib-gettextize --force --copy || exit $?
	echo "Making $dr/aclocal.m4 writable ..."
	test -r $dr/aclocal.m4 && chmod u+w $dr/aclocal.m4
      fi
    fi
    if grep "^AC_PROG_INTLTOOL" configure.in >/dev/null; then
      echo "Running intltoolize..."
      intltoolize --force --copy --automake || exit $?
    fi
    if grep "^AM_PROG_LIBTOOL" configure.in >/dev/null; then
      echo "Running libtoolize..."
      libtoolize --force --copy || exit $?
    fi

    echo "Running $ACLOCAL $ACLOCAL_FLAGS ..."
    $ACLOCAL $ACLOCAL_FLAGS || exit $?
    echo "Running autoconf ..."
    autoconf || exit $?
    if grep "^AM_CONFIG_HEADER" configure.in >/dev/null; then
      echo "Running autoheader..."
      autoheader || exit $?
    fi
    echo "Running $AUTOMAKE..."
    $AUTOMAKE --add-missing --force --gnu || exit $?

    cd $topdir
  fi
done

conf_flags="--enable-maintainer-mode --enable-compile-warnings" #--enable-iso-c

cd "$ORIGDIR"

if test x$NOCONFIGURE = x; then
  echo Running $srcdir/configure $conf_flags "$@" ...
  $srcdir/configure $conf_flags "$@" \
  && echo Now type \`make\' to compile $PROJECT  || exit $?
else
  echo Skipping configure process.
fi
