#! /bin/bash

Xnest :1 -scrns 2 -geometry 270x270 &
sleep 1
DISPLAY=:1 unst $1 ./metacity
killall Xnest
