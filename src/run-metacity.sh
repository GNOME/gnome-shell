#! /bin/bash

if test -z "$SCREENS"; then
  SCREENS=1
fi

if test "$DEBUG" = none; then
  DEBUG=
elif test -z "$DEBUG"; then
  DEBUG=
fi

if test -z "$CLIENTS"; then
  CLIENTS=0
fi

if test -z "$ONLY_WM"; then
  Xnest -ac :1 -scrns $SCREENS -geometry 640x600 -bw 15 &
  usleep 100000

  if test $CLIENTS != 0; then
    for I in `seq 1 $CLIENTS`; do
      DISPLAY=:1 xterm -geometry 25x15 &
    done
  fi
 
  usleep 50000

  DISPLAY=:1 xsetroot -solid royalblue3
fi

if test -z "$ONLY_SETUP"; then
  METACITY_UISLAVE_DIR=./uislave METACITY_DISPLAY=:1 exec unst libtool --mode=execute $DEBUG ./metacity $OPTIONS
fi
