#! /bin/bash
if test -z "$SCREENS"; then
  SCREENS=2
fi
Xnest :1 -scrns $SCREENS -geometry 270x270 &
METACITY_UISLAVE_DIR=./uislave DISPLAY=:1 unst libtool --mode=execute gdb ./metacity
killall Xnest
