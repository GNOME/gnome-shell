FROM registry.fedoraproject.org/fedora:latest

RUN dnf -y update && dnf -y upgrade && \
    dnf install -y 'dnf-command(builddep)' redhat-rpm-config && \
    dnf builddep -y mutter gnome-shell && \

    # Mutter deps
    dnf install -y gnome-settings-daemon-devel && \

    # Needed for tests
    dnf install -y findutils '*/xvfb-run' gdm && \

    # Needed for building mutter
    git clone https://gitlab.gnome.org/GNOME/gsettings-desktop-schemas.git && \
    cd gsettings-desktop-schemas && \
    meson --prefix=/usr build && \
    ninja -C build install && \
    cd .. && \

    dnf clean all && \
    rm -rf /var/cache/dnf
