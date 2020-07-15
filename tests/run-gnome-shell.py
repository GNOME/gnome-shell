#!/usr/bin/env python3

import sys
import os
import fcntl
import subprocess
import dbus
import unittest
from dbusmock import DBusTestCase
from gi.repository import GLib
from dbus.mainloop.glib import DBusGMainLoop

DBusGMainLoop(set_as_default=True)

def set_nonblock(fd):
    '''Set a file object to non-blocking'''

    flags = fcntl.fcntl(fd, fcntl.F_GETFL)
    fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)

class GnomeShellTestCase(DBusTestCase):
    @classmethod
    def setUpClass(klass):
        DBusTestCase.setUpClass()
        klass.start_session_bus()
        klass.start_system_bus()
        klass.start_logind()
        klass.start_network_manager()

        klass.system_bus_con = klass.get_dbus(True)
        klass.session_bus_con = klass.get_dbus(False)

        if klass.session_bus_con.name_has_owner('org.gnome.Shell'):
            print('org.gnome.Shell already has owner on the session bus, bailing')
            sys.exit(1)

    @classmethod
    def tearDownClass(klass):
        klass.stop_network_manager()
        klass.stop_logind()
        DBusTestCase.tearDownClass()

    @classmethod
    def start_logind(klass):
        klass.logind, klass.logind_obj = \
            klass.spawn_server_template('logind',
                                        None,
                                        stdout=subprocess.PIPE)
        set_nonblock(klass.logind.stdout)

    @classmethod
    def stop_logind(klass):
        klass.logind.terminate()
        klass.logind.wait()

    @classmethod
    def start_network_manager(klass):
        klass.network_manager, klass.nm_obj = \
            klass.spawn_server_template('networkmanager',
                                        None,
                                        stdout=subprocess.PIPE)

    @classmethod
    def stop_network_manager(klass):
        klass.network_manager.terminate()
        klass.network_manager.wait()

    def start_gnome_shell(self, session_type):
        env = {}
        env.update(os.environ)
        env['XDG_SESSION_TYPE'] = session_type
        if session_type == 'wayland':
            args = ['--nested']
        elif session_type == 'x11':
            args = ['--x11']

        self.name_watch = self.session_bus_con.watch_name_owner('org.gnome.Shell',
                                                                self.shell_appeared)
        self.loop = GLib.MainLoop()

        p = subprocess.Popen(['xvfb-run', '--', 'gnome-shell'] + args, env=env)
        self.loop.run()
        self.loop = None
        ret = p.wait()
        assert ret == 0

    def eval_test_case(self):
        test_case_js = """
        Main.overview.connect('shown', () => Main.overview.hide());
        Main.overview.connect('hidden', () => Meta.quit(Meta.ExitCode.SUCCESS));
        Main.overview.show();
        """

        shell_iface = 'org.gnome.Shell'
        shell = self.session_bus_con.get_object(shell_iface, '/org/gnome/Shell')
        shell.Eval(test_case_js, dbus_interface=shell_iface)

    def shell_appeared(self, owner):
        if not self.session_bus_con.name_has_owner('org.gnome.Shell'):
            return

        self.name_watch.cancel()
        self.eval_test_case()
        self.loop.quit()

    def test_wayland(self):
        self.start_gnome_shell('wayland')

    def test_x11(self):
        self.start_gnome_shell('x11')

unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
