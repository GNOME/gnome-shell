#! /bin/bash
if test -z "$SCREENS"; then
  SCREENS=2
fi

if test "$DEBUG" = none; then
  DEBUG=
elif test -z "$DEBUG"; then
  DEBUG=gdb
fi

Xnest :1 -scrns $SCREENS -geometry 640x480 &
DISPLAY=:1 xsetroot -solid royalblue3
METACITY_UISLAVE_DIR=./uislave DISPLAY=:1 unst libtool --mode=execute $DEBUG ./metacity
killall Xnest
