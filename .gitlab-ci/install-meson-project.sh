#!/bin/bash

set -e

usage() {
  cat <<-EOF
	Usage: $(basename $0) [OPTION…] REPO_URL COMMIT

	Check out and install a meson project

	Options:
	  -Dkey=val      Option to pass on to meson
	  --subdir       Build subdirectory instead of whole project
	  --prepare      Script to run before build

	  -h, --help     Display this help

	EOF
}

TEMP=$(getopt \
  --name=$(basename $0) \
  --options='D:h' \
  --longoptions='subdir:' \
  --longoptions='prepare:' \
  --longoptions='help' \
  -- "$@")

eval set -- "$TEMP"
unset TEMP

MESON_OPTIONS=()
SUBDIR=.
PREPARE=:

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
meson install -C _build
popd
