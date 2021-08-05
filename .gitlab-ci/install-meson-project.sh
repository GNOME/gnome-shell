#!/bin/bash

set -e

if [[ $# -lt 4 ]]; then
  echo Usage: $0 [options] [repo-url] [commit] [subdir]
  echo  Options:
  echo    -Dkey=val
  exit 1
fi

MESON_OPTIONS=()

while [[ $1 =~ ^-D ]]; do
  MESON_OPTIONS+=( "$1" )
  shift
done

REPO_URL="$1"
COMMIT="$2"
SUBDIR="$3"
PREPARE="$4"

REPO_DIR="$(basename ${REPO_URL%.git})"

git clone --depth 1 "$REPO_URL" -b "$COMMIT"
pushd "$REPO_DIR"
pushd "$SUBDIR"
sh -c "$PREPARE"
meson --prefix=/usr _build "${MESON_OPTIONS[@]}"
meson install -C _build
popd
popd
rm -rf "$REPO_DIR"
