#! /bin/bash

if test -z "$XNEST_DISPLAY"; then
  XNEST_DISPLAY=:8
fi

if test -z "$CLIENT_DISPLAY"; then
  CLIENT_DISPLAY=:8
fi

if test -z "$MUTTER_DISPLAY"; then
  export MUTTER_DISPLAY=$CLIENT_DISPLAY
fi

if test -z "$SCREENS"; then
  SCREENS=1
fi

MAX_SCREEN=`echo $SCREENS-1 | bc`

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
  TEST_CLIENT='./tools/mutter-window-demo'
fi

if test -n "$XINERAMA"; then
  XINERAMA_FLAGS='+xinerama'
fi

export EF_ALLOW_MALLOC_0=1

if test -z "$ONLY_WM"; then
  echo "Launching Xnest"
  Xnest -ac $XNEST_DISPLAY -scrns $SCREENS -geometry 640x480 -bw 15 $XINERAMA_FLAGS &
  ## usleep 800000
  sleep 1

  if test -n "$XMON_DIR"; then
    echo "Launching xmond"
    $XMON_DIR/xmonui | $XMON_DIR/xmond -server localhost:$XNEST_DISPLAY &
    sleep 1
  fi

  if test -n "$XSCOPE_DIR"; then
    ## xscope doesn't like to die when it should, it backgrounds itself
    killall -9 xscope
    killall -9 xscope
    echo "Launching xscope"
    DISPLAY= $XSCOPE_DIR/xscope -o1 -i28  > xscoped-replies.txt &
    export MUTTER_DISPLAY=localhost:28
    sleep 1
  fi

  echo "Launching clients"
  if test -n "$TEST_CLIENT"; then
      for I in `seq 0 $MAX_SCREEN`; do
          DISPLAY=$CLIENT_DISPLAY.$I $TEST_CLIENT &
      done
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

  if test -e ~/.Xmodmap; then
      DISPLAY=$CLIENT_DISPLAY xmodmap ~/.Xmodmap
  fi
 
  usleep 50000

  for I in `seq 0 $MAX_SCREEN`; do
      DISPLAY=$CLIENT_DISPLAY.$I xsetroot -solid royalblue3
  done
fi

if test -z "$ONLY_SETUP"; then
  MUTTER_VERBOSE=1 MUTTER_USE_LOGFILE=1 MUTTER_DEBUG_BUTTON_GRABS=1 exec $DEBUG ./mutter $OPTIONS
fi
