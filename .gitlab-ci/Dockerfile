FROM registry.gitlab.gnome.org/gnome/mutter/master:v1

RUN dnf -y update && dnf -y upgrade && \
    dnf install -y 'dnf-command(copr)' && \
    dnf builddep -y gnome-shell && \

    # cc isn't *really* needed, bt only exports HAVE_BLUETOOTH to js,
    # rest are outdated build-requires
    dnf remove -y control-center gnome-bluetooth-libs-devel \
                  dbus-glib-devel upower-devel python3-devel && \

    # We'll build mutter ourselves
    dnf remove -y --noautoremove mutter mutter-devel && \

    # Deps that are only brought in by unused requires
    dnf install -y python3 'pkgconfig(libsystemd)' && \

    # Needed for tests
    dnf copr enable -y fmuellner/gnome-shell-ci && \
    dnf install -y '*/xvfb-run' gdm-lib accountsservice-libs && \

    dnf clean all && \
    rm -rf /var/cache/dnf
