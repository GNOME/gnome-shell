#!/usr/bin/bash

set -e
"$(dirname -- "$0")"/run-gnome-shell.sh --wayland
"$(dirname -- "$0")"/run-gnome-shell.sh --x11
