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
# automake, bison, flex, gettext, gnome-common, gperf, gtk-doc, intltool,
# libtool, pkgconfig
#
# Devel packages needed by gnome-shell and its deps:
# dbus-glib, expat, GL, gnome-menus, gstreamer, libffi,
# libjasper, libjpeg, libpng, libpulse, libtiff, libwnck,
# iso-codes, libical, libxml2, ORBit2, pam, python, readline, upower,
# spidermonkey ({mozilla,firefox,xulrunner}-js), startup-notification,
# xdamage, icon-naming-utils, upower, libtool-ltdl, libvorbis,
# libgcrypt, libtasn1, libgnome-keyring, libgtop, cups, xcb, libwebkitgtk,
# libusb, libproxy, libdb, libproxy, sqlite, gudev, liblcms2, sane,
# python-gobject, libxtst, liboauth, libnm
#
# Non-devel packages needed by gnome-shell and its deps:
# glxinfo, gstreamer-plugins-base, gstreamer-plugins-good,
# gvfs, python, gnome-python (gconf),
# icon-naming-utils, zenity, libtasn1-tools accountsservice

if test "x$system" = xUbuntu -o "x$system" = xDebian -o "x$system" = xLinuxMint ; then
  reqd="
    build-essential curl
    automake bison flex gettext git-core gperf gnome-common gtk-doc-tools
    gvfs gvfs-backends icon-naming-utils
    libdbus-glib-1-dev libexpat-dev libffi-dev libgnome-menu-dev libgnome-desktop-dev libgtop2-dev
    libical-dev libjasper-dev libjpeg-dev libpng-dev libstartup-notification0-dev libtiff-dev
    libwnck-dev libgl1-mesa-dev liborbit2-dev libpulse-dev libreadline-dev libxml2-dev
    mesa-common-dev mesa-utils libpam-dev python-dev python-gconf python-gobject-dev
    libcroco3-dev
    libgstreamer0.10-dev gstreamer0.10-plugins-base gstreamer0.10-plugins-good
    libltdl-dev libvorbis-dev iso-codes libgnome-keyring-dev libusb-1.0-0-dev
    libupower-glib-dev libcups2-dev libproxy-dev libdb-dev libproxy-dev
    libsqlite3-dev libgudev-1.0-dev libsane-dev libwebkitgtk-3.0-dev
    libx11-xcb-dev libupower-glib-dev accountsservice
    gnome-doc-utils liblcms2-dev libxtst-dev liboauth-dev
    libnm-glib-dev libnm-util-dev
    "

  if apt-cache show libmozjs-dev > /dev/null 2> /dev/null; then
    reqd="$reqd libmozjs-dev"
  elif apt-cache show xulrunner-dev > /dev/null 2> /dev/null; then
    reqd="$reqd xulrunner-dev"
  fi

  if apt-cache show libxcb-util0-dev > /dev/null 2> /dev/null; then
    reqd="$reqd libxcb-util0-dev"
  else
    reqd="$reqd libxcb-event1-dev libxcb-aux0-dev"
  fi

  if apt-cache show autopoint > /dev/null 2> /dev/null; then
    reqd="$reqd autopoint"
  fi

  if [ ! -x /usr/bin/dpkg-checkbuilddeps ]; then
    echo "Please run 'sudo apt-get install dpkg-dev' and try again."
    echo
    exit 1
  fi

  for pkg in $reqd ; do
      if ! dpkg-checkbuilddeps -d $pkg /dev/null 2> /dev/null; then
        missing="$pkg $missing"
      fi
  done
  if test ! "x$missing" = x; then
    echo "Please run 'sudo apt-get install $missing' and try again."
    echo
    exit 1
  fi
fi

if test "x$system" = xFedora ; then
  reqd="
    binutils curl gcc gcc-c++ make accountsservice
    automake bison flex gettext git gnome-common gnome-doc-utils gperf gvfs intltool
    libtool pkgconfig dbus-glib-devel gnome-desktop-devel gnome-menus-devel
    gnome-python2-gconf jasper-devel libffi-devel libical-devel libjpeg-devel libpng-devel
    libtiff-devel libwnck-devel mesa-libGL-devel ORBit2-devel pam-devel
    pulseaudio-libs-devel python-devel pygobject2 readline-devel xulrunner-devel
    libXdamage-devel libcroco-devel libxml2-devel gstreamer-devel
    gstreamer-plugins-base gstreamer-plugins-good glx-utils expat-devel
    startup-notification-devel zenity webkitgtk3-devel upower-devel
    icon-naming-utils upower-devel libtool-ltdl-devel libvorbis-devel
    iso-codes-devel libgcrypt-devel libtasn1-devel libtasn1-tools libusb1-devel
    libgnome-keyring-devel libgtop2-devel cups-devel db4-devel libproxy-devel
    sqlite-devel libgudev1-devel lcms2-devel sane-backends-devel xcb-util-devel
    "

  if expr $version = 14 > /dev/null ; then
      reqd="$reqd gettext-autopoint"
  elif expr $version \>= 15 > /dev/null ; then
      reqd="$reqd gettext-devel"
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
    bison flex gtk-doc gnome-common gnome-doc-utils-devel \
    gnome-desktop-devel gnome-menus-devel icon-naming-utils \
    libgtop-devel libpulse-devel libtiff-devel cups-devel libffi-devel \
    orbit2-devel libwnck-devel xorg-x11-proto-devel readline-devel \
    mozilla-xulrunner191-devel libcroco-devel \
    xorg-x11-devel xorg-x11 xorg-x11-server-extra evolution-data-server-devel \
    iso-codes-devel \
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
    bison flex gnome-common gnome-doc-utils gtk-doc icon-naming-utils \
    intltool ffi5-devel libwnck-1-devel GL-devel ORBit2-devel \
    readline-devel libxulrunner-devel \
    libxdamage-devel mesa-demos zenity \
    libcroco0.6-devel libevolution-data-server3-devel \
    iso-codes-devel \
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

