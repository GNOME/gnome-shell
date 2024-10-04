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
	  --reconfigure           Reconfigure the project
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

setup_command() {
  if [[ ${#MESON_OPTIONS[*]} > 0 && -d $BUILD_DIR ]]; then
    RECONFIGURE=--reconfigure
  fi

  echo -n "meson setup --prefix=/usr $RECONFIGURE $WIPE ${MESON_OPTIONS[*]} $BUILD_DIR"
}

compile_command() {
  echo -n "meson compile -C $BUILD_DIR"
}

install_command() {
  if [[ $RUN_DIST ]]; then
    echo -n :
  else
    echo -n "sudo meson install -C $BUILD_DIR"
  fi
}

dist_command() {
  if [[ $RUN_DIST ]]; then
    echo -n "meson dist -C $BUILD_DIR --include-subprojects"
  else
    echo -n :
  fi
}

# load defaults
. $CONFIG_FILE
TOOLBOX=$DEFAULT_TOOLBOX

TEMP=$(getopt \
  --name $(basename $0) \
  --options 't:D:h' \
  --longoptions 'toolbox:' \
  --longoptions 'dist' \
  --longoptions 'reconfigure' \
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

    --reconfigure)
       RECONFIGURE=--reconfigure
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

BUILD_DIR=build-$TOOLBOX

find_toplevel

toolbox run --container $TOOLBOX sh -c "
  $(setup_command) &&
  $(compile_command) &&
  $(install_command) &&
  $(dist_command)"
