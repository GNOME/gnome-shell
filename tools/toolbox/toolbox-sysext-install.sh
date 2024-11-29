#!/usr/bin/bash

set -eu

DEFAULT_TOOLBOX=gnome-shell-devel
CONFIG_FILE=${XDG_CONFIG_HOME:-$HOME/.config}/gnome-shell-toolbox-tools.conf

usage() {
  cat <<-EOF
	Usage: $(basename $0) [OPTION…]

	Extract and install a systemd-sysext extension from a toolbox

	Options:
	  -t, --toolbox=TOOLBOX   Use TOOLBOX instead of the default "$DEFAULT_TOOLBOX"

	  --persistent            Install extension under /var/lib/extensions/ to persist after reboots

	  -h, --help              Display this help

	EOF
}

die() {
  echo "$@" >&2
  exit 1
}

init_sysext() {
  rm -rf $HOME_SYSEXT_DIR
  mkdir -p $HOME_SYSEXT_DIR
  podman cp $TOOLBOX:/var/lib/extensions/$TOOLBOX/. $HOME_SYSEXT_DIR

  local ext_dir=$HOME_SYSEXT_DIR/usr/lib/extension-release.d
  mkdir -p $ext_dir
  podman cp $TOOLBOX:/etc/os-release $ext_dir/extension-release.$TOOLBOX
}

runtime_install_command() {
  local dest=$(dirname $RUN_SYSEXT_DIR)

  echo -n "mkdir -p $dest && "
  echo -n "ln -sf $HOME_SYSEXT_DIR $dest && "
  echo -n "rm -fr $VAR_SYSEXT_DIR"
}

system_install_command() {
  local dest=$(dirname $VAR_SYSEXT_DIR)
  local attr=mode

  selinuxenabled 2>/dev/null && attr="$attr,context"

  echo -n "mkdir -p $dest && "
  echo -n "cp -r --preserve=$attr $HOME_SYSEXT_DIR $dest && "
  echo -n "rm -fr $RUN_SYSEXT_DIR"
}

install_command() {
  if [[ ${PERSISTENT:-0} ]]; then
    system_install_command
  else
    runtime_install_command
  fi
}

sysext_merge_command() {
  #echo -n systemd-sysext refresh
  # workaround for https://github.com/systemd/systemd/issues/34387
  echo -n systemctl restart systemd-sysext.service
}

compile_schemas_command() {
  local schemadir=usr/share/glib-2.0/schemas
  if [ -d $HOME_SYSEXT_DIR/$schemadir ]
  then
    echo -n "$(runtime_install_command) && "
    echo -n "$(sysext_merge_command) && "
    echo -n "glib-compile-schemas --targetdir $HOME_SYSEXT_DIR/$schemadir /$schemadir 2>/dev/null && "
    echo -n "chown $(id -un):$(id -gn) $HOME_SYSEXT_DIR/$schemadir/gschemas.compiled"
  else
    echo -n :
  fi
}

relabel_command() {
  local setfiles=$(which setfiles 2>/dev/null)
  if [ -x "$setfiles" ]
  then
    local spec=/etc/selinux/targeted/contexts/files/file_contexts
    echo -n "$setfiles -F -r $HOME_SYSEXT_DIR $spec $HOME_SYSEXT_DIR/usr"
  else
    echo -n :
  fi
}

# load defaults
. $CONFIG_FILE
TOOLBOX=$DEFAULT_TOOLBOX

TEMP=$(getopt \
  --name $(basename $0) \
  --options 't:h' \
  --longoptions 'toolbox:' \
  --longoptions 'persistent' \
  --longoptions 'help' \
  -- "$@") || die "Run $(basename $0) --help to see available options"

eval set -- "$TEMP"
unset TEMP

while true; do
  case $1 in
    -t|--toolbox)
      TOOLBOX=$2
      shift 2
    ;;

    --persistent)
      PERSISTENT=1
      shift
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

HOME_SYSEXT_DIR=$HOME/.local/lib/toolbox-extensions/$TOOLBOX
RUN_SYSEXT_DIR=/run/extensions/$TOOLBOX
VAR_SYSEXT_DIR=/var/lib/extensions/$TOOLBOX

init_sysext

sudo sh -c "
  $(compile_schemas_command) &&
  $(relabel_command) &&
  $(install_command) &&
  $(sysext_merge_command)"
