#! /bin/bash
if test -z "$SCREENS"; then
  SCREENS=2
fi

if test "$DEBUG" = none; then
  DEBUG=
elif test -z "$DEBUG"; then
  DEBUG=gdb
fi

if test -z "$CLIENTS"; then
  CLIENTS=0
fi

Xnest :1 -scrns $SCREENS -geometry 640x480 -bw 15 &
usleep 50000

if test $CLIENTS != 0; then
  for I in `seq 1 $CLIENTS`; do
    DISPLAY=:1 xterm -geometry 25x15 &
  done
fi

DISPLAY=:1 xsetroot -solid royalblue3
METACITY_UISLAVE_DIR=./uislave DISPLAY=:1 unst libtool --mode=execute $DEBUG ./metacity
killall Xnest
