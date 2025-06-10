#!/bin/bash

set -e

DEFAULT_TOOLBOX=gnome-shell-devel
CONFIG_FILE=${XDG_CONFIG_HOME:-$HOME/.config}/gnome-shell-toolbox-tools.conf

usage() {
  cat <<-EOF
	Usage: $(basename $0) [OPTION…]

	Run meson project tests in a toolbox

	Options:
	  -t, --toolbox=TOOLBOX   Use TOOLBOX instead of the default "$DEFAULT_TOOLBOX"

	  --gdb                   Run test under gdb
	  --wrapper=WRAPPER       Wrapper to run tests with (e.g. valgrind)

	  --list                  List available tests

	  --suite=SUITE           Only run tests belonging to the given suite
	  --no-suite=SUITE        Do not run tests belonging to the given suite

	  --setup=SETUP           Which test setup to use
	  --test-args=TEST_ARGS   Arguments to pass to the specified test(s) or all tests

	  -v, --verbose           Do not redirect stdout and stderr
	  -q, --quiet             Produce less output to the terminal

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

# load defaults
. $CONFIG_FILE
TOOLBOX=$DEFAULT_TOOLBOX

TEMP=$(getopt \
  --name $(basename $0) \
  --options 't:vqh' \
  --longoptions 'toolbox:' \
  --longoptions 'gdb' \
  --longoptions 'wrapper:' \
  --longoptions 'list' \
  --longoptions 'suite:' \
  --longoptions 'no-suite:' \
  --longoptions 'setup:' \
  --longoptions 'test-args:' \
  --longoptions 'verbose' \
  --longoptions 'quiet' \
  --longoptions 'help' \
  -- "$@") || die "Run $(basename $0) --help to see available options"

eval set -- "$TEMP"
unset TEMP

MESON_ARGS=()

while true; do
  case $1 in
    -t|--toolbox)
      TOOLBOX=$2
      shift 2
    ;;

    --list|--gdb|-v|--verbose|-q|--quiet)
      MESON_ARGS+=($1)
      shift
    ;;

    --wrapper|--suite|--no-suite|--setup|--test-args)
      MESON_ARGS+=($1 $2)
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

BUILD_DIR=_build-$TOOLBOX

toolbox run --container $TOOLBOX \
  meson test -C $BUILD_DIR ${MESON_ARGS[*]} "$@"
