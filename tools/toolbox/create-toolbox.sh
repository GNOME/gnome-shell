#!/bin/bash
# vi: sw=2 ts=4

set -e

TOOLBOX_IMAGE=registry.gitlab.gnome.org/gnome/gnome-shell/toolbox
DEFAULT_NAME=gnome-shell-devel
CONFIG_FILE=${XDG_CONFIG_HOME:-$HOME/.config}/gnome-shell-toolbox-tools.conf

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
	  --set-default           Set as default for other gnome-shell
	                          toolbox tools
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

  cat >> $top_srcdir/.buildconfig <<-EOF

	[toolbox-$NAME]
	name=$NAME Toolbox
	runtime=podman:$container_id
	prefix=/usr
	run-command=dbus-run-session gnome-shell --devkit

	[toolbox-$NAME.runtime_environment]
	G_MESSAGES_DEBUG=GNOME Shell
	XDG_DATA_DIRS=${XDG_DATA_DIRS:-/usr/local/share:/usr/share}
	EOF
}

setup_classic() {
  local branch=${VERSION:+gnome-}${VERSION:-main}

  toolbox_run /usr/libexec/install-meson-project.sh \
    -Dclassic_mode=true \
    https://gitlab.gnome.org/GNOME/gnome-shell-extensions.git $branch
}

set_default() {
  mkdir -p $(dirname $CONFIG_FILE)
  echo DEFAULT_TOOLBOX=$NAME > $CONFIG_FILE
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
  --longoptions 'set-default' \
  --longoptions 'help' \
  -- "$@") || die "Run $(basename $0) --help to see available options"

eval set -- "$TEMP"
unset TEMP

NAME=$DEFAULT_NAME
LOCALES=()

# set up first toolbox as default
[ ! -f $CONFIG_FILE ] && SET_DEFAULT=1

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

    --set-default)
      SET_DEFAULT=1
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
[[ $SET_DEFAULT ]] && set_default
