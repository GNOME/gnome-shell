#! /bin/bash

Xnest :1 -scrns 2 -geometry 270x270 &
METACITY_UISLAVE_DIR=./uislave DISPLAY=:1 unst libtool --mode=execute gdb ./metacity
killall Xnest
