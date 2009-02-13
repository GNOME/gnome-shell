#!/bin/sh
#
# Script that sets up jhbuild to build gnome-shell. Run this to
# checkout jhbuild and the required configuration. 
#
# Copyright (C) 2008, Red Hat, Inc.
#
# Some ideas and code taken from gtk-osx-build
#
# Copyright (C) 2006, 2007, 2008 Imendio AB
#
SOURCE=$HOME/Source
BASEURL=http://git.gnome.org/cgit/gnome-shell/plain/tools/build

if ! which curl > /dev/null 2>&1; then
	cat <<EOF
This script requires the curl program to run
For Debian-based systems run:
	apt-get install curl

For Red Hat-based systems run:
	yum install curl

For SuSE-based systems run:
	zypper install curl

For Mandriva-based systems run:
	urpmi curl

EOF
	exit 1
fi

if [ -d $SOURCE ] ; then : ; else
    mkdir $SOURCE
    echo "Created $SOURCE"
fi

echo -n "Checking out jhbuild into $SOURCE/jhbuild ... "
cd $SOURCE
svn co http://svn.gnome.org/svn/jhbuild/trunk jhbuild > /dev/null
echo "done"

echo "Installing jhbuild..."
(cd $SOURCE/jhbuild && make -f Makefile.plain DISABLE_GETTEXT=1 install >/dev/null)

if [ -e $HOME/.jhbuildrc ] ; then
    if grep JHBUILDRC_GNOME_SHELL $HOME/.jhbuildrc > /dev/null ; then : ; else
	mv $HOME/.jhbuildrc $HOME/.jhbuildrc.bak
	echo "Saved ~/.jhbuildrc as ~/.jhbuildrc.bak"
    fi
fi

echo -n "Writing ~/.jhbuildrc ... "
curl -s -o $HOME/.jhbuildrc $BASEURL/jhbuildrc-gnome-shell
echo "done"

if [ ! -f $HOME/.jhbuildrc-custom ]; then
    echo -n "Writing example ~/.jhbuildrc-custom ... "
    curl -s -o $HOME/.jhbuildrc-custom $BASEURL/jhbuildrc-custom-example
    echo "done"
fi

if test "x`echo $PATH | grep $HOME/bin`" = x; then
    echo "PATH does not contain $HOME/bin, it is recommended that you add that."
    echo
fi

if which lsb_release > /dev/null 2>&1; then
  system=`lsb_release -is`
elif [ -f /etc/fedora-release ] ; then
  system=Fedora
elif [ -f /etc/SuSE-release ] ; then
  system=SuSE
elif [ -f /etc/mandriva-release ]; then
  system=MandrivaLinux
fi

if test x$system = xUbuntu -o x$system = xDebian ; then
  reqd=""
  for pkg in build-essential automake gnome-common flex bison curl \
    git-core subversion gtk-doc-tools mesa-common-dev xulrunner-1.9-dev \
    libdbus-glib-1-dev libffi-dev libgconf2-dev libgtk2.0-dev libgl1-mesa-dev \
    libgstreamer-plugins-base0.10-dev python2.5-dev libwnck-dev libreadline5-dev librsvg2-dev libgnomeui-dev; do
      if ! dpkg --status $pkg > /dev/null 2>&1; then
        reqd="$pkg $reqd"
      fi
  done
  if test ! "x$reqd" = x; then
    echo "Please run 'sudo apt-get install $reqd' before building gnome-shell."
    echo
  fi
fi

if test x$system = xFedora ; then
  reqd=""
  for pkg in libffi-devel libXdamage-devel gnome-doc-utils xulrunner-devel \
    librsvg2-devel libgnomeui-devel xterm xorg-x11-apps xorg-x11-server-Xephyr \
    libwnck-devel GConf2-devel readline-devel; do
      if ! rpm -q $pkg > /dev/null 2>&1; then
        reqd="$pkg $reqd"
      fi
  done
  if test ! "x$reqd" = x; then
    gpk-install-package-name $reqd
  fi
fi

if test x$system = xSuSE ; then
  reqd=""
  for pkg in libffi-devel xorg-x11-devel gnome-doc-utils-devel librsvg-devel \
    mozilla-xulrunner190-devel libgnomeui-devel xterm xorg-x11 xorg-x11-server-extra \
    libwnck-devel gconf2-devel readline-devel flex bison; do
      if ! rpm -q $pkg > /dev/null 2>&1; then
        reqd="$pkg $reqd"
      fi
  done
  if test ! "x$reqd" = x; then
    echo "Please run 'su --command=\"zypper install $reqd\"' before building gnome-shell."
    echo
  fi
fi

if test x$system = xMandrivaLinux ; then
  reqd=""
  for pkg in ffi5-devel libxdamage-devel gtk-doc gnome-common gnome-doc-utils libxulrunner-devel \
    librsvg2-devel libgnomeui2-devel xterm x11-apps x11-server-xephyr \
    libwnck-1-devel libGConf2-devel readline-devel flex bison GL-devel \
    zenity intltool mesa-demos ; do
      if ! rpm -q --whatprovides $pkg > /dev/null 2>&1; then
        reqd="$pkg $reqd"
      fi
  done
  if test ! "x$reqd" = x; then
	gurpmi --auto $reqd
  fi
fi

echo "Done."

