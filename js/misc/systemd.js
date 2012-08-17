// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;

const SystemdLoginManagerIface = <interface name='org.freedesktop.login1.Manager'>
<method name='PowerOff'>
    <arg type='b' direction='in'/>
</method>
<method name='Reboot'>
    <arg type='b' direction='in'/>
</method>
<method name='CanPowerOff'>
    <arg type='s' direction='out'/>
</method>
<method name='CanReboot'>
    <arg type='s' direction='out'/>
</method>
</interface>;

const SystemdLoginSessionIface = <interface name='org.freedesktop.login1.Session'>
<signal name='Lock' />
<signal name='Unlock' />
</interface>;

const SystemdLoginManagerProxy = Gio.DBusProxy.makeProxyWrapper(SystemdLoginManagerIface);

const SystemdLoginSessionProxy = Gio.DBusProxy.makeProxyWrapper(SystemdLoginSessionIface);

function SystemdLoginManager() {
    return new SystemdLoginManagerProxy(Gio.DBus.system,
                                        'org.freedesktop.login1',
                                        '/org/freedesktop/login1');
};

function SystemdLoginSession(id) {
    return new SystemdLoginSessionProxy(Gio.DBus.system,
                                        'org.freedesktop.login1',
                                        '/org/freedesktop/login1/session/' + id);
}

function haveSystemd() {
    return GLib.access("/sys/fs/cgroup/systemd", 0) >= 0;
}
