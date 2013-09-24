// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const SystemdLoginManagerIface = <interface name='org.freedesktop.login1.Manager'>
<method name='Suspend'>
    <arg type='b' direction='in'/>
</method>
<method name='CanSuspend'>
    <arg type='s' direction='out'/>
</method>
<method name='Inhibit'>
    <arg type='s' direction='in'/>
    <arg type='s' direction='in'/>
    <arg type='s' direction='in'/>
    <arg type='s' direction='in'/>
    <arg type='h' direction='out'/>
</method>
<method name='GetSession'>
    <arg type='s' direction='in'/>
    <arg type='o' direction='out'/>
</method>
<method name='ListSessions'>
    <arg name='sessions' type='a(susso)' direction='out'/>
</method>
<signal name='PrepareForSleep'>
    <arg type='b' direction='out'/>
</signal>
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
<signal name='Lock' />
<signal name='Unlock' />
</interface>;

const ConsoleKitSession = Gio.DBusProxy.makeProxyWrapper(ConsoleKitSessionIface);
const ConsoleKitManager = Gio.DBusProxy.makeProxyWrapper(ConsoleKitManagerIface);

function haveSystemd() {
    return GLib.access("/run/systemd/seats", 0) >= 0;
}

function versionCompare(required, reference) {
    required = required.split('.');
    reference = reference.split('.');

    for (let i = 0; i < required.length; i++) {
        let requiredInt = parseInt(required[i]);
        let referenceInt = parseInt(reference[i]);
        if (requiredInt != referenceInt)
            return requiredInt < referenceInt;
    }

    return true;
}

function canLock() {
    try {
        let params = GLib.Variant.new('(ss)', ['org.gnome.DisplayManager.Manager', 'Version']);
        let result = Gio.DBus.system.call_sync('org.gnome.DisplayManager',
                                               '/org/gnome/DisplayManager/Manager',
                                               'org.freedesktop.DBus.Properties',
                                               'Get', params, null,
                                               Gio.DBusCallFlags.NONE,
                                               -1, null);

        let version = result.deep_unpack()[0].deep_unpack();
        return versionCompare('3.5.91', version);
    } catch(e) {
        return false;
    }
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
        this._proxy.connectSignal('PrepareForSleep',
                                  Lang.bind(this, this._prepareForSleep));
    },

    // Having this function is a bit of a hack since the Systemd and ConsoleKit
    // session objects have different interfaces - but in both cases there are
    // Lock/Unlock signals, and that's all we count upon at the moment.
    getCurrentSessionProxy: function(callback) {
        if (this._currentSession) {
            callback (this._currentSession);
            return;
        }

        this._proxy.GetSessionRemote(GLib.getenv('XDG_SESSION_ID'), Lang.bind(this,
            function(result, error) {
                if (error) {
                    logError(error, 'Could not get a proxy for the current session');
                } else {
                    this._currentSession = new SystemdLoginSession(Gio.DBus.system,
                                                                   'org.freedesktop.login1',
                                                                   result[0]);
                    callback(this._currentSession);
                }
            }));
    },

    canSuspend: function(asyncCallback) {
        this._proxy.CanSuspendRemote(function(result, error) {
            if (error)
                asyncCallback(false);
            else
                asyncCallback(result[0] != 'no');
        });
    },

    listSessions: function(asyncCallback) {
        this._proxy.ListSessionsRemote(function(result, error) {
            if (error)
                asyncCallback([]);
            else
                asyncCallback(result[0]);
        });
    },

    suspend: function() {
        this._proxy.SuspendRemote(true);
    },

    inhibit: function(reason, callback) {
        let inVariant = GLib.Variant.new('(ssss)',
                                         ['sleep',
                                          'GNOME Shell',
                                          reason,
                                          'delay']);
        this._proxy.call_with_unix_fd_list('Inhibit', inVariant, 0, -1, null, null,
            Lang.bind(this, function(proxy, result) {
                let fd = -1;
                try {
                    let [outVariant, fdList] = proxy.call_with_unix_fd_list_finish(result);
                    fd = fdList.steal_fds(outVariant.deep_unpack())[0];
                    callback(new Gio.UnixInputStream({ fd: fd }));
                } catch(e) {
                    logError(e, "Error getting systemd inhibitor");
                    callback(null);
                }
            }));
    },

    _prepareForSleep: function(proxy, sender, [aboutToSuspend]) {
        this.emit('prepare-for-sleep', aboutToSuspend);
    }
});
Signals.addSignalMethods(LoginManagerSystemd.prototype);

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
    getCurrentSessionProxy: function(callback) {
        if (this._currentSession) {
            callback (this._currentSession);
            return;
        }

        this._proxy.GetCurrentSessionRemote(Lang.bind(this,
            function(result, error) {
                if (error) {
                    logError(error, 'Could not get a proxy for the current session');
                } else {
                    this._currentSession = new ConsoleKitSession(Gio.DBus.system,
                                                                 'org.freedesktop.ConsoleKit',
                                                                 result[0]);
                    callback(this._currentSession);
                }
            }));
    },

    canSuspend: function(asyncCallback) {
        asyncCallback(false);
    },

    listSessions: function(asyncCallback) {
        asyncCallback([]);
    },

    suspend: function() {
        this.emit('prepare-for-sleep', true);
        this.emit('prepare-for-sleep', false);
    },

    inhibit: function(reason, callback) {
        callback(null);
    }
});
Signals.addSignalMethods(LoginManagerConsoleKit.prototype);
