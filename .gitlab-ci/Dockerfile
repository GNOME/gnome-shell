# Rebuild and push with
#
#     cd .gitlab-ci/
#     podman build --format docker --no-cache -t registry.gitlab.gnome.org/gnome/gnome-shell/extension-ci:v2 .
#     podman push registry.gitlab.gnome.org/gnome/gnome-shell/extension-ci:v2
#

FROM registry.fedoraproject.org/fedora:32

RUN dnf -y update && dnf -y upgrade && \
    dnf install -y 'dnf-command(copr)' git && \

    # For syntax checks with `find . -name '*.js' -exec js68 -c -s '{}' ';'`
    dnf install -y findutils mozjs68-devel && \

    # For static analysis with eslint
    dnf install -y nodejs && \
    npm install -g eslint && \

    # Shameless plug for my own tooling; useful for generating zip
    dnf copr enable -y fmuellner/gnome-shell-ci && \
    dnf install -y gnome-extensions-tool meson && \

    dnf clean all
