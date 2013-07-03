#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`

cd $srcdir
PROJECT=Cogl
TEST_TYPE=-f
FILE=cogl/cogl.h

test $TEST_TYPE $FILE || {
	echo "You must run this script in the top-level $PROJECT directory"
	exit 1
}

# GNU gettext automake support doesn't get along with git.
# https://bugzilla.gnome.org/show_bug.cgi?id=661128
touch -t 200001010000 po/cogl.pot

AUTOMAKE_VERSIONS="1.14 1.13 1.12 1.11"
for version in $AUTOMAKE_VERSIONS; do
	if automake-$version --version < /dev/null > /dev/null 2>&1 ; then
		AUTOMAKE=automake-$version
		ACLOCAL=aclocal-$version
		export AUTOMAKE ACLOCAL
		break
	fi
done

if test -z "$AUTOMAKE"; then
	echo
	echo "You must have one of automake $AUTOMAKE_VERSIONS to compile $PROJECT."
	echo "Install the appropriate package for your distribution,"
	echo "or get the source tarball at http://ftp.gnu.org/gnu/automake/"
	exit 1
fi

(gtkdocize --version) < /dev/null > /dev/null 2>&1 || {
	echo "You don't have gtk-doc installed to compile $PROJECT, and thus"
	echo "won't be able to generate the $PROJECT documentation."
	NOGTKDOC=1
}

# NOCONFIGURE is used by gnome-common
if test -z "$NOCONFIGURE"; then
        if test -z "$*"; then
                echo "I am going to run ./configure with no arguments - if you wish "
                echo "to pass any to it, please specify them on the $0 command line."
        fi
fi

if test -z "$ACLOCAL_FLAGS"; then
        acdir=`$ACLOCAL --print-ac-dir`
        m4list="glib-2.0.m4"
        for file in $m4list; do
                if [ ! -f "$acdir/$file" ]; then
                        echo "WARNING: aclocal's directory is $acdir, but..."
                        echo "         no file $acdir/$file"
                        echo "         You may see fatal macro warnings below."
                        echo "         If these files are installed in /some/dir, set the ACLOCAL_FLAGS "
                        echo "         environment variable to \"-I /some/dir\", or install"
                        echo "         $acdir/$file."
                        echo ""
                fi
        done
fi

rm -rf autom4te.cache

if test -z "$NOGTKDOC"; then
	gtkdocize || exit $?
fi

autoreconf -vfi || exit $?
cd $ORIGDIR || exit $?

if test -z "$NOCONFIGURE"; then
        $srcdir/configure $AUTOGEN_CONFIGURE_ARGS "$@" || exit $?
        echo "Now type 'make' to compile $PROJECT."
fi
