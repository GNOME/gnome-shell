#! /bin/bash

if test -z "$XNEST_DISPLAY"; then
  XNEST_DISPLAY=:1
fi

if test -z "$CLIENT_DISPLAY"; then
  CLIENT_DISPLAY=:1
fi

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

if test -n "$DEMO_TEST"; then
  TEST_CLIENT='./tools/metacity-window-demo'
fi

if test -n "$XINERAMA"; then
  XINERAMA_FLAGS='+xinerama'
fi

if test -z "$ONLY_WM"; then
  echo "Launching Xnest"
  Xnest -ac $XNEST_DISPLAY -scrns $SCREENS -geometry 640x480 -bw 15 $XINERAMA_FLAGS &
  ## usleep 800000
  sleep 1

  if test -n "$XMON_DIR"; then
    echo "Launching xmond"
    $XMON_DIR/xmonui | $XMON_DIR/xmond -server $XNEST_DISPLAY &
    sleep 1
  fi

  echo "Launching clients"
  if test -n "$TEST_CLIENT"; then
    DISPLAY=$CLIENT_DISPLAY $TEST_CLIENT &
  fi

  if test $CLIENTS != 0; then
    for I in `seq 1 $CLIENTS`; do
      echo "Launching xterm $I"
      DISPLAY=$CLIENT_DISPLAY xterm -geometry 25x15 &
    done
  fi

  if test $SM_CLIENTS != 0; then
    for I in `seq 1 $SM_CLIENTS`; do
      echo "Launching gnome-terminal $I"
      DISPLAY=$CLIENT_DISPLAY gnome-terminal --geometry 25x15 &
    done
  fi
 
  usleep 50000

  DISPLAY=$CLIENT_DISPLAY xsetroot -solid royalblue3
fi

if test -z "$ONLY_SETUP"; then
  METACITY_VERBOSE=1 METACITY_DEBUG=1 METACITY_USE_LOGFILE=1 METACITY_DEBUG_BUTTON_GRABS=1 METACITY_DISPLAY=$CLIENT_DISPLAY exec libtool --mode=execute $DEBUG ./metacity $OPTIONS
fi
