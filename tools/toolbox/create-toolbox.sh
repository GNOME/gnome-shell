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
  -h, --help              Display this help
"

TEMP=$(getopt \
  --name $(basename $0) \
  --options 'n:v:h' \
  --longoptions 'name:,version:,help' -- "$@") || exit 1

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
toolbox run --container $NAME update-mutter
