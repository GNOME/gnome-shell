#! /bin/bash

Xnest :1 -scrns 2 -geometry 200x200 &
sleep 1
DISPLAY=:1 unst ./metacity
