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
	  --destdir=DIR      Install the project to DIR, can be used
	                     several times to install to multiple destdirs

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
DESTDIRS=()

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
      DESTDIRS+=( $2 )
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

[[ ${#DESTDIRS[@]} == 0 ]] && DESTDIRS+=( / )

CHECKOUT_DIR=$(mktemp --directory)
trap "rm -rf $CHECKOUT_DIR" EXIT

git clone --depth 1 "$REPO_URL" -b "$COMMIT" "$CHECKOUT_DIR"

pushd "$CHECKOUT_DIR/$SUBDIR"
sh -c "$PREPARE"
meson setup --prefix=/usr _build "${MESON_OPTIONS[@]}"

# Install it to all specified dest dirs
for destdir in "${DESTDIRS[@]}"; do
    # don't use --destdir when installing to root,
    # so post-install hooks are run
    [[ $destdir == / ]] && destdir=

    # also install dependencies when installing to a separate distdir
    install_deps=.gitlab-ci/install-common-dependencies.sh
    [[ "$destdir" && -x $install_deps ]] && $install_deps --destdir $destdir

    sudo meson install -C _build ${destdir:+--destdir=$destdir}
done
popd
