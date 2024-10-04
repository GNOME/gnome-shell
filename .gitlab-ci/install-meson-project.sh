#!/bin/bash

set -e

usage() {
  cat <<-EOF
	Usage: $(basename $0) [OPTION…] REPO_URL COMMIT

	Check out and install a meson project

	Options:
	  -Dkey=val          Option to pass on to meson
	  --subdir=DIR       Build subdirectory instead of whole project
	  --prepare=SCRIPT   Script to run before build
	  --libdir=DIR       Setup the project with a different libdir
	  --destdir=DIR      Install the project to an additional destdir

	  -h, --help         Display this help

	EOF
}

TEMP=$(getopt \
  --name=$(basename $0) \
  --options='D:h' \
  --longoptions='subdir:' \
  --longoptions='prepare:' \
  --longoptions='libdir:' \
  --longoptions='destdir:' \
  --longoptions='help' \
  -- "$@")

eval set -- "$TEMP"
unset TEMP

MESON_OPTIONS=()
SUBDIR=.
PREPARE=:
DESTDIR=""

while true; do
  case "$1" in
    -D)
      MESON_OPTIONS+=( -D$2 )
      shift 2
    ;;

    --subdir)
      SUBDIR=$2
      shift 2
    ;;

    --prepare)
      PREPARE=$2
      shift 2
    ;;

    --libdir)
      MESON_OPTIONS+=( --libdir=$2 )
      shift 2
    ;;

    --destdir)
      DESTDIR=$2
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

if [[ $# -lt 2 ]]; then
  usage
  exit 1
fi

REPO_URL="$1"
COMMIT="$2"

CHECKOUT_DIR=$(mktemp --directory)
trap "rm -rf $CHECKOUT_DIR" EXIT

git clone --depth 1 "$REPO_URL" -b "$COMMIT" "$CHECKOUT_DIR"

pushd "$CHECKOUT_DIR/$SUBDIR"
sh -c "$PREPARE"
meson setup --prefix=/usr _build "${MESON_OPTIONS[@]}"

# Install it to an additional directory e.g., system extension directory
if [ -n "${DESTDIR}" ]; then
    sudo meson install -C _build --destdir=$DESTDIR
fi

sudo meson install -C _build
popd
