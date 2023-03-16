#!/bin/bash

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

generate_update_script() {
    sed s:@MUTTER_BRANCH@:$MUTTER_BRANCH: <<-'EOF'
	#!/bin/sh

	set -e

	MUTTER_REPOSITORY_URL=https://gitlab.gnome.org/GNOME/mutter.git
	MUTTER_BRANCH=@MUTTER_BRANCH@

	CHECKOUT_DIR=$(mktemp --directory --tmpdir mutter.XXXXXX)
	trap "rm -rf $CHECKOUT_DIR" EXIT

	git clone --depth 1 $MUTTER_REPOSITORY_URL --branch $MUTTER_BRANCH $CHECKOUT_DIR

	pushd $CHECKOUT_DIR
	meson setup --prefix=/usr build && meson install -C build
	popd
	EOF
}

build_container() {
    echo Building $TOOLBOX_IMAGE from $MUTTER_CI_IMAGE

    export BUILDAH_ISOLATION=chroot
    export BUILDAH_FORMAT=docker

    local build_cntr=$(buildah from $MUTTER_CI_IMAGE)
    local build_mnt=$(buildah mount $build_cntr)

    [[ -n "$build_mnt" && -n "$build_cntr" ]] || die "Failed to mount the container"

    local extra_packages=(
        toolbox-support
        gdb
        gnome-console
        flatpak # for host apps
        gnome-backgrounds # no blank background!
    )
    buildah run $build_cntr dnf config-manager --set-disabled '*-modular,*-openh264'
    buildah run $build_cntr dnf install -y "${extra_packages[@]}"
    buildah run $build_cntr dnf clean all
    buildah run $build_cntr rm -rf /var/lib/cache/dnf

    # work around non-working pkexec
    local fake_pkexec=$(mktemp)
    cat > $fake_pkexec <<-'EOF'
	#!/bin/sh
	exec su -c "$*"
	EOF
    buildah copy --chmod 755 $build_cntr $fake_pkexec /usr/bin/pkexec

    # include convenience script for updating mutter dependency
    local update_mutter=$(mktemp)
    generate_update_script > $update_mutter
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

if check_image_base; then
    echo Image $TOOLBOX_IMAGE exists and is up to date.
    exit 0
fi

[[ -n "$CI_REGISTRY" && -n "$CI_REGISTRY_USER" && -n "$CI_REGISTRY_PASSWORD" ]] ||
    die "Insufficient information to log in."

podman login -u $CI_REGISTRY_USER -p $CI_REGISTRY_PASSWORD $CI_REGISTRY

build_container

podman push $TOOLBOX_IMAGE
