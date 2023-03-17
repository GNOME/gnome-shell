#!/bin/sh

set -e

TOOLBOX_IMAGE=registry.gitlab.gnome.org/gnome/gnome-shell/toolbox
NAME=gnome-shell-devel

USAGE="Usage: $(basename $0) [OPTION…]

Create a toolbox for gnome-shell development

Options:
  -n, --name=NAME         Create container as NAME instead of the
                          default \"$NAME\"
  -v, --version           Create container for stable version VERSION
                          (like 44) instead of the main branch
  -b, --builder           Set up GNOME Builder configuration
  --skip-mutter           Do not build mutter
  -h, --help              Display this help
"

TEMP=$(getopt \
  --name $(basename $0) \
  --options 'n:v:bh' \
  --longoptions 'name:,version:,skip-mutter,builder,help' -- "$@") || exit 1

eval set -- "$TEMP"
unset TEMP

while true; do
  case "$1" in
    '-n'|'--name')
      NAME="$2"
      shift 2
      continue
    ;;

    '-v'|'--version')
      VERSION="$2"
      shift 2
      continue
    ;;

    '-b'|'--builder')
      SETUP_BUILDER=1
      shift
      continue
    ;;

    '--skip-mutter')
      SKIP_MUTTER=1
      shift
      continue
    ;;

    '-h'|'--help')
      echo "$USAGE"
      exit 0
    ;;

    '--')
      shift
      break
    ;;
  esac
done

TAG=${VERSION:-main}

podman pull $TOOLBOX_IMAGE:$TAG

toolbox create --image $TOOLBOX_IMAGE:$TAG $NAME

if [ ! $SKIP_MUTTER ]; then
  toolbox run --container $NAME update-mutter
fi

if [ $SETUP_BUILDER ]; then
  CONTAINER_ID=$(podman container inspect --format='{{.Id}}' $NAME)
  TOP_SRCDIR=$(realpath $(dirname $0)/../..)
  WAYLAND_DISPLAY=builder-wayland-0

  cat >> $TOP_SRCDIR/.buildconfig <<-EOT

	[toolbox-$NAME]
	name=$NAME Toolbox
	runtime=podman:$CONTAINER_ID
	prefix=/usr
	run-command=dbus-run-session gnome-shell --nested --wayland-display=$WAYLAND_DISPLAY

	[toolbox-$NAME.runtime_environment]
	WAYLAND_DISPLAY=$WAYLAND_DISPLAY
	G_MESSAGES_DEBUG=GNOME Shell
	XDG_DATA_DIRS=${XDG_DATA_DIRS:-/usr/local/share:/usr/share}
	EOT
fi
