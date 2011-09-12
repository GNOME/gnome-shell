#!/bin/sh
#
# Copyright (C) 2010, Intel Corp.
#
# Modified version of gnome-shell-build-setup.sh for building Clutter
# and its dependencies using jhbuild
#
# Copyright (C) 2008, Red Hat, Inc.
#
# Some ideas and code taken from gtk-osx-build
#
# Copyright (C) 2006, 2007, 2008 Imendio AB
#

# Pre-check on GNOME version

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

if test x$system = xUbuntu -o x$system = xDebian -o x$system = xLinuxMint ; then
  reqd=""
  if [ ! -x /usr/bin/dpkg-checkbuilddeps ]; then
    echo "Please run 'sudo apt-get install dpkg-dev' and try again."
    echo
    exit 1
  fi
  for pkg in \
    build-essential curl \
    automake bison flex gettext git-core gnome-common gtk-doc-tools \
    libjasper-dev libjpeg-dev libpng-dev libstartup-notification0-dev libtiff-dev \
    libgl1-mesa-dev libxml2-dev mesa-common-dev mesa-utils \
    python-dev python-gconf python-gobject \
    libgstreamer0.10-dev gstreamer0.10-plugins-base gstreamer0.10-plugins-good \
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

if test x$system = xFedora ; then
  reqd="
    binutils curl gcc gcc-c++ make
    automake bison flex gettext git gnome-common gnome-doc-utils intltool
    libtool pkgconfig jasper-devel libffi-devel libjpeg-devel
    libpng-devel libtiff-devel libwnck-devel mesa-libGL-devel
    python-devel pygobject2 libXdamage-devel libxml2-devel
    gstreamer-devel gstreamer-plugins-base gstreamer-plugins-good
    glx-utils
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

if test x$system = xSUSE ; then
  reqd=""
  for pkg in \
    curl \
    bison flex gnome-doc-utils-devel \
    xorg-x11-proto-devel xorg-x11-devel xorg-x11 xorg-x11-server-extra \
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
    libwnck-1-devel GL-devel \
    libxdamage-devel mesa-demos \
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
BASEURL=http://git.gnome.org/browse/clutter/plain/build

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
curl -L -s -o $HOME/.jhbuildrc $BASEURL/jhbuildrc-clutter
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
