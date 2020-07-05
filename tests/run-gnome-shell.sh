#!/bin/sh

dbus-run-session -- "$(dirname -- "$0")"/run-gnome-shell.py "$@"
