// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Shell = imports.gi.Shell;

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

const SystemdLoginManager = Gio.DBusProxy.makeProxyWrapper(SystemdLoginManagerIface);
const SystemdLoginSession = Gio.DBusProxy.makeProxyWrapper(SystemdLoginSessionIface);

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

const ConsoleKitSession = Gio.DBusProxy.makeProxyWrapper(ConsoleKitSessionIface);
const ConsoleKitManager = Gio.DBusProxy.makeProxyWrapper(ConsoleKitManagerIface);

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
        this._proxy = new SystemdLoginManager(Gio.DBus.system,
                                              'org.freedesktop.login1',
                                              '/org/freedesktop/login1');
    },

    // Having this function is a bit of a hack since the Systemd and ConsoleKit
    // session objects have different interfaces - but in both cases there are
    // Lock/Unlock signals, and that's all we count upon at the moment.
    getCurrentSessionProxy: function() {
        if (!this._currentSession) {
            this._currentSession = new SystemdLoginSession(Gio.DBus.system,
                                                           'org.freedesktop.login1',
                                                           '/org/freedesktop/login1/session/' +
                                                           GLib.getenv('XDG_SESSION_ID'));
        }

        return this._currentSession;
    },

    get sessionActive() {
        return Shell.session_is_active_for_systemd();
    },

    canPowerOff: function(asyncCallback) {
        this._proxy.CanPowerOffRemote(function(result, error) {
            if (error)
                asyncCallback(false);
            else
                asyncCallback(result[0] != 'no');
        });
    },

    canReboot: function(asyncCallback) {
        this._proxy.CanRebootRemote(function(result, error) {
            if (error)
                asyncCallback(false);
            else
                asyncCallback(result[0] != 'no');
        });
    },

    powerOff: function() {
        this._proxy.PowerOffRemote(true);
    },

    reboot: function() {
        this._proxy.RebootRemote(true);
    }
});

const LoginManagerConsoleKit = new Lang.Class({
    Name: 'LoginManagerConsoleKit',

    _init: function() {
        this._proxy = new ConsoleKitManager(Gio.DBus.system,
                                            'org.freedesktop.ConsoleKit',
                                            '/org/freedesktop/ConsoleKit/Manager');
    },

    // Having this function is a bit of a hack since the Systemd and ConsoleKit
    // session objects have different interfaces - but in both cases there are
    // Lock/Unlock signals, and that's all we count upon at the moment.
    getCurrentSessionProxy: function() {
        if (!this._currentSession) {
            let [currentSessionId] = this._proxy.GetCurrentSessionSync();
            this._currentSession = new ConsoleKitSession(Gio.DBus.system,
                                                         'org.freedesktop.ConsoleKit',
                                                         currentSessionId);
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
        [this._sessionActive] = session.IsActiveSync();

        return this._sessionActive;
    },

    canPowerOff: function(asyncCallback) {
        this._proxy.CanStopRemote(function(result, error) {
            if (error)
                asyncCallback(false);
            else
                asyncCallback(result[0]);
        });
    },

    canReboot: function(asyncCallback) {
        this._proxy.CanRestartRemote(function(result, error) {
            if (error)
                asyncCallback(false);
            else
                asyncCallback(result[0]);
        });
    },

    powerOff: function() {
        this._proxy.StopRemote();
    },

    reboot: function() {
        this._proxy.RestartRemote();
    }
            
});
