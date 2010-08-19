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

# Pre-check on GNOME version

gnome_version=`gnome-session --version 2>/dev/null | (read name version && echo $version)`
have_gnome_26=false
case $gnome_version in
    2.2[6789]*|2.[3456789]*|3.*)
	have_gnome_26=true
    ;;
esac

if $have_gnome_26 ; then : ; else
   echo "GNOME 2.26 or newer is required to build GNOME Shell" 1>&2
   exit 1
fi

############################################################

release_file=

if which lsb_release > /dev/null 2>&1; then
  system=`lsb_release -is`
  version=`lsb_release -rs`
elif [ -f /etc/fedora-release ] ; then
  system=Fedora
  release_file=/etc/fedora-release
elif [ -f /etc/SuSE-release ] ; then
  system=SUSE
  release_file=/etc/SuSE-release
elif [ -f /etc/mandriva-release ]; then
  system=MandrivaLinux
  release_file=/etc/mandriva-release
fi

if [ x$release_file != x ] ; then
    version=`sed 's/[^0-9\.]*\([0-9\.]\+\).*/\1/' < $release_file`
fi

# Required software:
#
# For this script:
# binutils, curl, gcc, make, git
#
# General build stuff:
# automake, bison, flex, gettext, git, gnome-common, gtk-doc, intltool,
# libtool, pkgconfig
#
# Devel packages needed by gnome-shell and its deps:
# dbus-glib, GL, gnome-menus, gstreamer, libffi,
# libjasper, libjpeg, libpng, libtiff, libwnck,
# libxml2, python,readline, spidermonkey ({mozilla,firefox,xulrunner}-js),
# startup-notification, xdamage
#
# Non-devel packages needed by gnome-shell and its deps:
# glxinfo, gstreamer-plugins-base, gstreamer-plugins-good,
# gvfs, python, pygobject, gnome-python (gconf), gnome-terminal*
# Xephyr*, zenity
#
# (*) only needed for --xephyr

if test "x$system" = xUbuntu -o "x$system" = xDebian -o "x$system" = xLinuxMint ; then
  reqd=""
  if [ ! -x /usr/bin/dpkg-checkbuilddeps ]; then
    echo "Please run 'sudo apt-get install dpkg-dev' and try again."
    echo
    exit 1
  fi
  for pkg in \
    build-essential curl \
    automake bison flex gettext git-core gnome-common gtk-doc-tools gvfs gvfs-backends \
    libdbus-glib-1-dev libffi-dev libgnome-menu-dev libgnome-desktop-dev \
    libjasper-dev libjpeg-dev libpng-dev libstartup-notification0-dev libtiff-dev \
    libwnck-dev libgl1-mesa-dev libreadline5-dev libxml2-dev mesa-common-dev mesa-utils \
    python-dev python-gconf python-gobject xulrunner-dev xserver-xephyr gnome-terminal \
    libcroco3-dev libgstreamer0.10-dev gstreamer0.10-plugins-base gstreamer0.10-plugins-good \
    ; do
      if ! dpkg-checkbuilddeps -d $pkg /dev/null 2> /dev/null; then
        reqd="$pkg $reqd"
      fi
  done
  if test ! "x$reqd" = x; then
    echo "Please run 'sudo apt-get install $reqd' and try again."
    echo
    exit 1
  fi
fi

if test "x$system" = xFedora ; then
  reqd="
    binutils curl gcc gcc-c++ make
    automake bison flex gettext git gnome-common gnome-doc-utils gvfs intltool
    libtool pkgconfig dbus-glib-devel gnome-desktop-devel gnome-menus-devel
    gnome-python2-gconf jasper-devel libffi-devel libjpeg-devel
    libpng-devel libtiff-devel libwnck-devel mesa-libGL-devel
    python-devel pygobject2 readline-devel xulrunner-devel libXdamage-devel libcroco-devel
    libxml2-devel gstreamer-devel gstreamer-plugins-base gstreamer-plugins-good
    glx-utils startup-notification-devel xorg-x11-server-Xephyr gnome-terminal zenity
    "

  if expr $version \>= 14 > /dev/null ; then
      reqd="$reqd gettext-autopoint"
  fi

  for pkg in $reqd ; do
      if ! rpm -q $pkg > /dev/null 2>&1; then
        missing="$pkg $missing"
      fi
  done
  if test ! "x$missing" = x; then
    gpk-install-package-name $missing
  fi
fi

if test "x$system" = xSUSE -o "x$system" = "xSUSE LINUX" ; then
  reqd=""
  for pkg in \
    curl \
    bison flex gtk-doc gnome-common gnome-doc-utils-devel gnome-menus-devel \
    libtiff-devel cups-devel libffi-devel gnome-desktop-devel libwnck-devel \
    xorg-x11-proto-devel readline-devel mozilla-xulrunner191-devel \
    libcroco-devel xorg-x11-devel xorg-x11 xorg-x11-server-extra \
    ; do
      if ! rpm -q $pkg > /dev/null 2>&1; then
        reqd="$pkg $reqd"
      fi
  done
  if test ! "x$reqd" = x; then
    echo "Please run 'su --command=\"zypper install $reqd\"' and try again."
    echo
    exit 1
  fi
fi

if test "x$system" = xMandrivaLinux ; then
  reqd=""
  for pkg in \
    curl \
    bison flex gnome-common gnome-doc-utils gtk-doc intltool \
    ffi5-devel \
    libwnck-1-devel GL-devel readline-devel libxulrunner-devel \
    libxdamage-devel mesa-demos x11-server-xephyr zenity \
    libcroco0.6-devel \
    ; do
      if ! rpm -q --whatprovides $pkg > /dev/null 2>&1; then
        reqd="$pkg $reqd"
      fi
  done
  if test ! "x$reqd" = x; then
	gurpmi --auto $reqd
  fi
fi

SOURCE=$HOME/Source
BASEURL=http://git.gnome.org/browse/gnome-shell/plain/tools/build

if [ -d $SOURCE ] ; then : ; else
    mkdir $SOURCE
    echo "Created $SOURCE"
fi

if [ -d $SOURCE/jhbuild ] ; then
    if [ -d $SOURCE/jhbuild/.git ] ; then
        echo -n "Updating jhbuild ... "
        ( cd $SOURCE/jhbuild && git pull --rebase > /dev/null ) || exit 1
        echo "done"
    else
        echo "$SOURCE/jhbuild is not a git repository"
        echo "You should remove it and rerun this script"
	exit 1
    fi
else
    echo -n "Checking out jhbuild into $SOURCE/jhbuild ... "
    cd $SOURCE
    git clone git://git.gnome.org/jhbuild > /dev/null || exit 1
    echo "done"
fi

echo "Installing jhbuild..."
(cd $SOURCE/jhbuild && make -f Makefile.plain DISABLE_GETTEXT=1 bindir=$HOME/bin install >/dev/null)

if [ -e $HOME/.jhbuildrc ] ; then
    if grep JHBUILDRC_GNOME_SHELL $HOME/.jhbuildrc > /dev/null ; then : ; else
	mv $HOME/.jhbuildrc $HOME/.jhbuildrc.bak
	echo "Saved ~/.jhbuildrc as ~/.jhbuildrc.bak"
    fi
fi

echo -n "Writing ~/.jhbuildrc ... "
curl -L -s -o $HOME/.jhbuildrc $BASEURL/jhbuildrc-gnome-shell
echo "done"

if [ ! -f $HOME/.jhbuildrc-custom ]; then
    echo -n "Writing example ~/.jhbuildrc-custom ... "
    curl -L -s -o $HOME/.jhbuildrc-custom $BASEURL/jhbuildrc-custom-example
    echo "done"
fi

if test "x`echo $PATH | grep $HOME/bin`" = x; then
    echo "PATH does not contain $HOME/bin, it is recommended that you add that."
    echo
fi

echo "Done."

