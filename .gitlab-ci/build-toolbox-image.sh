#!/bin/bash
# vi: sw=2 ts=4

set -e

die() {
  echo "$@" >&2
  exit 1
}

check_image_base() {
  local base=$(
    skopeo inspect docker://$TOOLBOX_IMAGE 2>/dev/null |
    jq -r '.Labels["org.opencontainers.image.base.name"]')
  [[ "$base" == "$MUTTER_CI_IMAGE" ]]
}

build_container() {
  echo Building $TOOLBOX_IMAGE from $MUTTER_CI_IMAGE

  export BUILDAH_ISOLATION=chroot
  export BUILDAH_FORMAT=docker

  local build_cntr=$(buildah from $MUTTER_CI_IMAGE)
  local build_mnt=$(buildah mount $build_cntr)

  [[ -n "$build_mnt" && -n "$build_cntr" ]] || die "Failed to mount the container"

  local extra_packages=(
    passwd # needed by toolbox
    gdb
    gnome-console # can't do without *some* terminal
    flatpak-spawn # run host commands
    flatpak # for host apps
    nautilus # FileChooser portal
    adwaita-fonts-all # system fonts
    gnome-backgrounds # no blank background!
  )
  local debug_packages=(
    glib2 # makes gdb much more useful
  )
  buildah run $build_cntr dnf config-manager setopt '*-openh264.enabled=0'
  buildah run $build_cntr dnf install -y "${extra_packages[@]}"
  buildah run $build_cntr dnf reinstall -y gdk-pixbuf2 # reinstall with glycin support
  buildah run $build_cntr dnf builddep malcontent -y # for building libmalcontent
  buildah run $build_cntr dnf debuginfo-install -y "${debug_packages[@]}"
  buildah run $build_cntr dnf clean all
  buildah run $build_cntr rm -rf /var/lib/cache/dnf

  # somehow the sysusers trigger from the flatpak package messes up the
  # permissions of /etc/passwd to be only readable by root
  buildah run $build_cntr chmod 644 /etc/passwd

  # disable gnome-keyring activation:
  # it either asks for unlocking the login keyring on startup, or it detects
  # the running host daemon and doesn't export the object on the bus, which
  # blocks the activating service until it hits the timeout
  buildah run $build_cntr rm /usr/share/dbus-1/services/org.freedesktop.secrets.service

  local srcdir=$(realpath $(dirname $0))
  buildah copy --chmod 755 $build_cntr $srcdir/install-meson-project.sh /usr/libexec

  # add latest malcontent to the toolbox image for testing future integration
  buildah run $build_cntr /usr/libexec/install-meson-project.sh https://gitlab.freedesktop.org/pwithnall/malcontent.git main -Dlibgsystemservice:gtk_doc=false

  # include convenience script for updating mutter dependency
  local update_mutter=$(mktemp)
  cat > $update_mutter <<-EOF
	#!/bin/sh
	TOOLBOX=\$(. /run/.containerenv; echo \$name)
	/usr/libexec/install-meson-project.sh \\
	  --destdir=/ --destdir=/var/lib/extensions/\$TOOLBOX \\
	  https://gitlab.gnome.org/GNOME/mutter.git $MUTTER_BRANCH
	EOF
  buildah copy --chmod 755 $build_cntr $update_mutter /usr/bin/update-mutter

  buildah config --env HOME- \
    --label com.github.containers.toolbox=true \
    --label org.opencontainers.image.base.name=$MUTTER_CI_IMAGE \
    $build_cntr

  buildah commit $build_cntr $TOOLBOX_IMAGE
}


MUTTER_CI_IMAGE=$1
MUTTER_BRANCH=${2:-$CI_COMMIT_BRANCH}

TOOLBOX_IMAGE=$CI_REGISTRY_IMAGE/toolbox:${MUTTER_BRANCH#gnome-}

[[ -n "$MUTTER_CI_IMAGE" && -n "$MUTTER_BRANCH" ]] ||
  die "Usage: $(basename $0) MUTTER_CI_IMAGE [MUTTER_BRANCH]"

if [[ -z "$FORCE_REBUILD" ]]; then
  if check_image_base; then
    echo Image $TOOLBOX_IMAGE exists and is up to date.
    exit 0
  fi
fi

[[ -n "$CI_REGISTRY" && -n "$CI_REGISTRY_USER" && -n "$CI_REGISTRY_PASSWORD" ]] ||
  die "Insufficient information to log in."

podman login -u $CI_REGISTRY_USER -p $CI_REGISTRY_PASSWORD $CI_REGISTRY

build_container

if [[ -z "$DRY_RUN" ]]; then
  podman push $TOOLBOX_IMAGE
fi
