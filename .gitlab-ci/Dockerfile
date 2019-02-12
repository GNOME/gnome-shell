FROM registry.gitlab.gnome.org/gnome/mutter/master:v1

RUN dnf -y update && dnf -y upgrade && \
    dnf install -y 'dnf-command(copr)' && \
    dnf copr enable -y fmuellner/gnome-shell-ci && \
    dnf builddep -y gnome-shell --setopt=install_weak_deps=False && \

    # bt only exports HAVE_BLUETOOTH to js, rest are outdated build-requires
    dnf remove -y gnome-bluetooth-libs-devel dbus-glib-devel \
                  upower-devel python3-devel && \

    # We'll build mutter ourselves
    dnf remove -y --noautoremove mutter mutter-devel && \

    # Needed for tests
    dnf install -y '*/xvfb-run' gdm-lib accountsservice-libs && \

    dnf clean all && \
    rm -rf /var/cache/dnf
