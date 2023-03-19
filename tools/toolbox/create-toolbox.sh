#!/bin/sh
# vi: sw=2 ts=4

set -e

TOOLBOX_IMAGE=registry.gitlab.gnome.org/gnome/gnome-shell/toolbox
DEFAULT_NAME=gnome-shell-devel

usage() {
  cat <<-EOF
	Usage: $(basename $0) [OPTION…]

	Create a toolbox for gnome-shell development

	Options:
	  -n, --name=NAME         Create container as NAME instead of the
	                          default "$DEFAULT_NAME"
	  -v, --version=VERSION   Create container for stable version VERSION
	                          (like 44) instead of the main branch
	  -r, --replace           Replace an existing container
	  -c, --classic           Set up support for GNOME Classic
	  -b, --builder           Set up GNOME Builder configuration
	  --locales=LOCALES       Enable support for additional locales LOCALES
	  --skip-mutter           Do not build mutter
	  -h, --help              Display this help

	EOF
}

die() {
  echo "$@" >&2
  exit 1
}

toolbox_run() {
  toolbox run --container $NAME "$@"
}

install_extra_packages() {
  local -a pkgs

  pkgs+=( ${LOCALES[@]/#/glibc-langpack-} )
  [[ $SETUP_CLASSIC ]] && pkgs+=( gnome-menus )

  [[ ${#pkgs[@]} > 0 ]] &&
    toolbox_run su -c "dnf install -y ${pkgs[*]}"
  true
}

create_builder_config() {
  local container_id=$(podman container inspect --format='{{.Id}}' $NAME)
  local top_srcdir=$(realpath $(dirname $0)/../..)
  local wayland_display=builder-wayland-0

  cat >> $top_srcdir/.buildconfig <<-EOF

	[toolbox-$NAME]
	name=$NAME Toolbox
	runtime=podman:$container_id
	prefix=/usr
	run-command=dbus-run-session gnome-shell --nested --wayland-display=$wayland_display

	[toolbox-$NAME.runtime_environment]
	WAYLAND_DISPLAY=$wayland_display
	G_MESSAGES_DEBUG=GNOME Shell
	XDG_DATA_DIRS=${XDG_DATA_DIRS:-/usr/local/share:/usr/share}
	EOF
}

setup_classic() {
  local branch=${VERSION:+gnome-}${VERSION:-main}

  toolbox_run /usr/libexec/install-meson-project.sh \
    --prepare "git submodule update --init" \
    -Dclassic_mode=true \
    https://gitlab.gnome.org/GNOME/gnome-shell-extensions.git $branch
}

TEMP=$(getopt \
  --name $(basename $0) \
  --options 'n:v:rcbh' \
  --longoptions 'name:' \
  --longoptions 'version:' \
  --longoptions 'replace' \
  --longoptions 'classic' \
  --longoptions 'builder' \
  --longoptions 'locales:' \
  --longoptions 'skip-mutter' \
  --longoptions 'help' \
  -- "$@")

eval set -- "$TEMP"
unset TEMP

NAME=$DEFAULT_NAME
LOCALES=()

while true; do
  case "$1" in
    -n|--name)
      NAME="$2"
      shift 2
    ;;

    -v|--version)
      VERSION="$2"
      shift 2
    ;;

    -r|--replace)
      REPLACE=1
      shift
    ;;

    -c|--classic)
      SETUP_CLASSIC=1
      shift
    ;;

    -b|--builder)
      SETUP_BUILDER=1
      shift
    ;;

    --skip-mutter)
      SKIP_MUTTER=1
      shift
    ;;

    --locales)
      IFS=" ," LOCALES+=($2)
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

TAG=${VERSION:-main}

if podman container exists $NAME; then
  [[ $REPLACE ]] ||
    die "Container $NAME already exists and --replace was not specified"
  toolbox rm --force $NAME
fi

podman pull $TOOLBOX_IMAGE:$TAG

toolbox create --image $TOOLBOX_IMAGE:$TAG $NAME
install_extra_packages

[[ $SKIP_MUTTER ]] || toolbox_run update-mutter

[[ $SETUP_CLASSIC ]] && setup_classic
[[ $SETUP_BUILDER ]] && create_builder_config
