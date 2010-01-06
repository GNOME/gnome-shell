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

if which lsb_release > /dev/null 2>&1; then
  system=`lsb_release -is`
elif [ -f /etc/fedora-release ] ; then
  system=Fedora
elif [ -f /etc/SuSE-release ] ; then
  system=SUSE
elif [ -f /etc/mandriva-release ]; then
  system=MandrivaLinux
fi

# Required software:
#
# For this script:
# binutils, curl, gcc, make, git
#
# General build stuff:
# automake, bison, flex, git, gnome-common, gtk-doc, intltool,
# libtool, pkgconfig
#
# Devel packages needed by gnome-shell and its deps:
# dbus-glib, gconf, GL, gnome-menus, gstreamer, gtk, libffi,
# libgnomeui, librsvg, libwnck, python, readline, spidermonkey
# ({mozilla,firefox,xulrunner}-js), xdamage
#
# Non-devel packages needed by gnome-shell and its deps:
# glxinfo, gstreamer-plugins-base, gstreamer-plugins-good,
# python, pygobject, gnome-python (gconf), gnome-terminal*
# Xephyr*, zenity
#
# (*) only needed for --xephyr

# Can this be simplified? Obvious ways don't handle handle packages
# that have been installed then removed. ('purged' status, e.g.)
dpkg_is_installed() {
    status=`dpkg-query --show --showformat='${Status}' $1 2>/dev/null`
    if [ $? = 0 ] ; then
	set $status
        if [ "$3" = installed ] ; then
             return 0
        fi
    fi

    return 1
}

if test x$system = xUbuntu -o x$system = xDebian -o x$system = xLinuxMint ; then
  reqd=""
  for pkg in \
    build-essential curl \
    automake bison flex git-core gnome-common gtk-doc-tools \
    libdbus-glib-1-dev libgconf2-dev libgtk2.0-dev libffi-dev \
    libgnome-menu-dev libgnome-desktop-dev librsvg2-dev libwnck-dev libgl1-mesa-dev \
    libreadline5-dev mesa-common-dev mesa-utils python-dev python-gconf python-gobject \
    xulrunner-dev xserver-xephyr gnome-terminal libcroco3-dev \
    libgstreamer0.10-dev gstreamer0.10-plugins-base gstreamer0.10-plugins-good \
    ; do
      if ! dpkg_is_installed $pkg; then
        reqd="$pkg $reqd"
      fi
  done
  if test ! "x$reqd" = x; then
    echo "Please run 'sudo apt-get install $reqd' and try again."
    echo
    exit 1
  fi
fi

if test x$system = xFedora ; then
  reqd=""
  for pkg in \
    binutils curl gcc gcc-c++ make \
    automake bison flex git gnome-common gnome-doc-utils intltool \
    libtool pkgconfig \
    dbus-glib-devel GConf2-devel gnome-menus-devel gnome-python2-gconf gtk2-devel libffi-devel \
    gnome-desktop-devel librsvg2-devel libwnck-devel mesa-libGL-devel python-devel pygobject2 \
    readline-devel xulrunner-devel libXdamage-devel libcroco-devel \
    gstreamer-devel gstreamer-plugins-base gstreamer-plugins-good \
    glx-utils xorg-x11-server-Xephyr gnome-terminal zenity \
    ; do
      if ! rpm -q $pkg > /dev/null 2>&1; then
        reqd="$pkg $reqd"
      fi
  done
  if test ! "x$reqd" = x; then
    gpk-install-package-name $reqd
  fi
fi

if test x$system = xSUSE ; then
  reqd=""
  for pkg in \
    curl \
    bison flex gnome-doc-utils-devel \
    gconf2-devel libffi-devel gnome-desktop-devel librsvg-devel libwnck-devel \
    xorg-x11-proto-devel readline-devel mozilla-xulrunner190-devel \
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

if test x$system = xMandrivaLinux ; then
  reqd=""
  for pkg in \
    curl \
    bison flex gnome-common gnome-doc-utils gtk-doc intltool \
    libGConf2-devel ffi5-devel libgnomeui2-devel librsvg2-devel \
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

