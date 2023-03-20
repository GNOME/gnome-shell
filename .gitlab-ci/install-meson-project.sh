#!/bin/bash

set -e

usage() {
  cat <<-EOF
	Usage: $(basename $0) [OPTION…] REPO_URL COMMIT SUBDIR PREPARE

	Check out and install a meson project

	Options:
	  -Dkey=val      Option to pass on to meson

	EOF
}

MESON_OPTIONS=()

while [[ $1 =~ ^-D ]]; do
  MESON_OPTIONS+=( "$1" )
  shift
done

if [[ $# -lt 4 ]]; then
  usage
  exit 1
fi

REPO_URL="$1"
COMMIT="$2"
SUBDIR="$3"
PREPARE="$4"

REPO_DIR="$(basename ${REPO_URL%.git})"

git clone --depth 1 "$REPO_URL" -b "$COMMIT"
pushd "$REPO_DIR"
pushd "$SUBDIR"
sh -c "$PREPARE"
meson setup --prefix=/usr _build "${MESON_OPTIONS[@]}"
meson install -C _build
popd
popd
rm -rf "$REPO_DIR"
