#!/bin/bash

set -e

DEFAULT_TOOLBOX=gnome-shell-devel
CONFIG_FILE=${XDG_CONFIG_HOME:-$HOME/.config}/gnome-shell-toolbox-tools.conf

usage() {
  cat <<-EOF
	Usage: $(basename $0) [OPTION…]

	Build and install a meson project in a toolbox

	Options:
	  -t, --toolbox=TOOLBOX   Use TOOLBOX instead of the default "$DEFAULT_TOOLBOX"

	  -Dkey=val               Option to pass to meson setup
	  --dist                  Run meson dist
	  --wipe                  Wipe build directory and reconfigure

	  -h, --help              Display this help

	EOF
}

die() {
  echo "$@" >&2
  exit 1
}

find_toplevel() {
  while true; do
    grep -qs '\<project\>' meson.build && break
    [[ $(pwd) -ef / ]] && die "Error: No meson toplevel found"
    cd ..
  done
}

needs_reconfigure() {
  [[ ${#MESON_OPTIONS[*]} > 0 ]] &&
  [[ -d $BUILD_DIR ]]
}

# load defaults
. $CONFIG_FILE
TOOLBOX=$DEFAULT_TOOLBOX

TEMP=$(getopt \
  --name $(basename $0) \
  --options 't:D:h' \
  --longoptions 'toolbox:' \
  --longoptions 'dist' \
  --longoptions 'wipe' \
  --longoptions 'help' \
  -- "$@") || die "Run $(basename $0) --help to see available options"

eval set -- "$TEMP"
unset TEMP

MESON_OPTIONS=()

while true; do
  case $1 in
    -t|--toolbox)
      TOOLBOX=$2
      shift 2
    ;;

    --dist)
      RUN_DIST=1
      shift
    ;;

    --wipe)
      WIPE=--wipe
      shift
    ;;

    -D)
      MESON_OPTIONS+=(-D$2)
      shift 2
    ;;

    -h|--help)
      usage
      exit 0
    ;;

    --)
      shift
      break
    ;;
  esac
done

find_toplevel

BUILD_DIR=build-$TOOLBOX
[[ $RUN_DIST ]] && DIST="meson dist -C $BUILD_DIR" || DIST=:

needs_reconfigure && RECONFIGURE=--reconfigure

toolbox run --container $TOOLBOX sh -c "
  meson setup --prefix=/usr $RECONFIGURE $WIPE ${MESON_OPTIONS[*]} $BUILD_DIR &&
  meson compile -C $BUILD_DIR &&
  sudo meson install -C $BUILD_DIR &&
  $DIST
"
