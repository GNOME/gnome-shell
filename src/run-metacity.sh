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

if test -z "$SM_CLIENTS"; then
  SM_CLIENTS=0
fi

if test -n "$EVIL_TEST"; then
  TEST_CLIENT='./wm-tester/wm-tester --evil'
fi

if test -n "$ICON_TEST"; then
  TEST_CLIENT='./wm-tester/wm-tester --icon-windows'
fi

if test -z "$ONLY_WM"; then
  Xnest -ac :1 -scrns $SCREENS -geometry 640x480 -bw 15 &
  ## usleep 800000
  sleep 1

  if test -n "$TEST_CLIENT"; then
    DISPLAY=:1 $TEST_CLIENT &
  fi

  if test $CLIENTS != 0; then
    for I in `seq 1 $CLIENTS`; do
      echo "Launching xterm $I"
      DISPLAY=:1 xterm -geometry 25x15 &
    done
  fi

  if test $SM_CLIENTS != 0; then
    for I in `seq 1 $SM_CLIENTS`; do
      echo "Launching gnome-terminal $I"
      DISPLAY=:1 gnome-terminal --geometry 25x15 &
    done
  fi
 
  usleep 50000

  DISPLAY=:1 xsetroot -solid royalblue3
fi

if test -z "$ONLY_SETUP"; then
  METACITY_DEBUG_BUTTON_GRABS=1 METACITY_DISPLAY=:1 exec unst libtool --mode=execute $DEBUG ./metacity $OPTIONS
fi
