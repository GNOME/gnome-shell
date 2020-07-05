#!/usr/bin/env python3

import sys
import os
import subprocess
import time
import dbus
import dbusmock
import itertools
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib

loop = GLib.MainLoop()

test_case_js = """
Main.overview.connect('shown', () => Main.overview.hide());
Main.overview.connect('hidden', () => Meta.quit(Meta.ExitCode.SUCCESS));
Main.overview.show();
"""

def eval_test_case():
    shell_iface = 'org.gnome.Shell'
    shell = bus.get_object(shell_iface, '/org/gnome/Shell')
    shell.Eval(test_case_js, dbus_interface=shell_iface)

def shell_appeared(owner):
    if not bus.name_has_owner('org.gnome.Shell'):
        return

    eval_test_case()
    loop.quit()

argv = sys.argv
if len(argv) != 2:
    print('Usage: run-gnome-shell.py [--wayland|--x11]')
    sys.exit(1)

[_, mode] = argv
if mode == '--wayland':
    xdg_session_type = 'wayland'
    args = ['--nested']
elif mode == '--x11':
    xdg_session_type = 'x11'
    args = ['--x11']

DBusGMainLoop(set_as_default=True)

bus = dbus.SessionBus()

if bus.name_has_owner('org.gnome.Shell'):
    print('org.gnome.Shell already has owner on the session bus, bailing')
    sys.exit(1)

bus.watch_name_owner('org.gnome.Shell', shell_appeared)

env = {}
env.update(os.environ)
env['XDG_SESSION_TYPE'] = xdg_session_type

p = subprocess.Popen(['xvfb-run', '--', 'gnome-shell'] + args, env=env)

loop.run()
ret = p.wait()
sys.exit(ret)
