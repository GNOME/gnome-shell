// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const UPowerGlib = imports.gi.UPowerGlib;

const SystemdLoginManagerIface = <interface name='org.freedesktop.login1.Manager'>
<method name='PowerOff'>
    <arg type='b' direction='in'/>
</method>
<method name='Reboot'>
    <arg type='b' direction='in'/>
</method>
<method name='Suspend'>
    <arg type='b' direction='in'/>
</method>
<method name='CanPowerOff'>
    <arg type='s' direction='out'/>
</method>
<method name='CanReboot'>
    <arg type='s' direction='out'/>
</method>
<method name='CanSuspend'>
    <arg type='s' direction='out'/>
</method>
</interface>;

const SystemdLoginSessionIface = <interface name='org.freedesktop.login1.Session'>
<signal name='Lock' />
<signal name='Unlock' />
</interface>;

const SystemdLoginManager = new Gio.DBusProxyClass({
    Name: 'SystemdLoginManager',
    Interface: SystemdLoginManagerIface,

    _init: function() {
        this.parent({ g_bus_type: Gio.BusType.SYSTEM,
                      g_name: 'org.freedesktop.login1',
                      g_object_path: '/org/freedesktop/login1' });
    }
});
const SystemdLoginSession = new Gio.DBusProxyClass({
    Name: 'SystemdLoginSession',
    Interface: SystemdLoginSessionIface,

    _init: function(session) {
        this.parent({ g_bus_type: Gio.BusType.SYSTEM,
                      g_name: 'org.freedesktop.login1',
                      g_object_path: session });
    }
});

const ConsoleKitManagerIface = <interface name='org.freedesktop.ConsoleKit.Manager'>
<method name='CanRestart'>
    <arg type='b' direction='out'/>
</method>
<method name='CanStop'>
    <arg type='b' direction='out'/>
</method>
<method name='Restart' />
<method name='Stop' />
<method name='GetCurrentSession'>
    <arg type='o' direction='out' />
</method>
</interface>;

const ConsoleKitSessionIface = <interface name='org.freedesktop.ConsoleKit.Session'>
<method name='IsActive'>
    <arg type='b' direction='out' />
</method>
<signal name='ActiveChanged'>
    <arg type='b' direction='out' />
</signal>
<signal name='Lock' />
<signal name='Unlock' />
</interface>;

const ConsoleKitSession = new Gio.DBusProxyClass({
    Name: 'ConsoleKitSession',
    Interface: ConsoleKitSessionIface,

    _init: function(session) {
        this.parent({ g_bus_type: Gio.BusType.SYSTEM,
                      g_name: 'org.freedesktop.ConsoleKit',
                      g_object_path: session });
    }
});
const ConsoleKitManager = new Gio.DBusProxyClass({
    Name: 'ConsoleKitManager',
    Interface: ConsoleKitManagerIface,

    _init: function() {
        this.parent({ g_bus_type: Gio.BusType.SYSTEM,
                      g_name: 'org.freedesktop.ConsoleKit',
                      g_object_path: '/org/freedesktop/ConsoleKit/Manager' });
    }
});

function haveSystemd() {
    return GLib.access("/sys/fs/cgroup/systemd", 0) >= 0;
}

let _loginManager = null;

/**
 * LoginManager:
 * An abstraction over systemd/logind and ConsoleKit.
 *
 */
function getLoginManager() {
    if (_loginManager == null) {
        if (haveSystemd())
            _loginManager = new LoginManagerSystemd();
        else
            _loginManager = new LoginManagerConsoleKit();
    }

    return _loginManager;
}

const LoginManagerSystemd = new Lang.Class({
    Name: 'LoginManagerSystemd',

    _init: function() {
        this._proxy = new SystemdLoginManager();
        this._proxy.init(null);
    },

    // Having this function is a bit of a hack since the Systemd and ConsoleKit
    // session objects have different interfaces - but in both cases there are
    // Lock/Unlock signals, and that's all we count upon at the moment.
    getCurrentSessionProxy: function() {
        if (!this._currentSession) {
            this._currentSession = new SystemdLoginSession('/org/freedesktop/login1/session/' +
                                                           GLib.getenv('XDG_SESSION_ID'));
            this._currentSession.init(null);
        }

        return this._currentSession;
    },

    get sessionActive() {
        return Shell.session_is_active_for_systemd();
    },

    canPowerOff: function(asyncCallback) {
        this._proxy.CanPowerOffRemote(null, function(proxy, result) {
            let val = false;

            try {
                val = proxy.CanPowerOffFinish(result)[0] != 'no';
            } catch(e) { }

            asyncCallback(val);
        });
    },

    canReboot: function(asyncCallback) {
        this._proxy.CanRebootRemote(null, function(proxy, result) {
            let val = false;

            try {
                val = proxy.CanRebootFinish(result)[0] != 'no';
            } catch(e) { }

            asyncCallback(val);
        });
    },

    canSuspend: function(asyncCallback) {
        this._proxy.CanSuspendRemote(null, function(proxy, result) {
            let val = false;

            try {
                val = proxy.CanRebootFinish(result)[0] != 'no';
            } catch(e) { }

            asyncCallback(val);
        });
    },

    powerOff: function() {
        this._proxy.PowerOffRemote(true, null, null);
    },

    reboot: function() {
        this._proxy.RebootRemote(true, null, null);
    },

    suspend: function() {
        this._proxy.SuspendRemote(true, null, null);
    }
});

const LoginManagerConsoleKit = new Lang.Class({
    Name: 'LoginManagerConsoleKit',

    _init: function() {
        this._proxy = new ConsoleKitManager();
        this._proxy.init(null);

        this._upClient = new UPowerGlib.Client();
    },

    // Having this function is a bit of a hack since the Systemd and ConsoleKit
    // session objects have different interfaces - but in both cases there are
    // Lock/Unlock signals, and that's all we count upon at the moment.
    getCurrentSessionProxy: function() {
        if (!this._currentSession) {
            let [currentSessionId] = this._proxy.GetCurrentSessionSync(null);
            this._currentSession = new ConsoleKitSession(currentSessionId);
            this._currentSession.init(null);
        }

        return this._currentSession;
    },

    get sessionActive() {
        if (this._sessionActive !== undefined)
            return this._sessionActive;

        let session = this.getCurrentSessionProxy();
        session.connectSignal('ActiveChanged', Lang.bind(this, function(object, senderName, [isActive]) {
            this._sessionActive = isActive;
        }));
        [this._sessionActive] = session.IsActiveSync(null);

        return this._sessionActive;
    },

    canPowerOff: function(asyncCallback) {
        this._proxy.CanStopRemote(null, function(proxy, result) {
            let val = false;

            try {
                [val] = proxy.CanStopFinish(result);
            } catch(e) { }

            asyncCallback(val);
        });
    },

    canReboot: function(asyncCallback) {
        this._proxy.CanRestartRemote(null, function(proxy, result) {
            let val = false;

            try {
                [val] = proxy.CanRestartFinish(result);
            } catch(e) { }

            asyncCallback(val);
        });
    },

    canSuspend: function(asyncCallback) {
        Mainloop.idle_add(Lang.bind(this, function() {
            asyncCallback(this._upClient.get_can_suspend());
            return false;
        }));
    },

    powerOff: function() {
        this._proxy.StopRemote(null, null);
    },

    reboot: function() {
        this._proxy.RestartRemote(null, null);
    },

    suspend: function() {
        this._upClient.suspend_sync(null);
    }
});
