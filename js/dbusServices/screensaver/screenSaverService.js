// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ScreenSaverService */

const { Gio, GLib } = imports.gi;

const { loadInterfaceXML } = imports.misc.fileUtils;
const { ServiceImplementation } = imports.dbusService;

const ScreenSaverIface = loadInterfaceXML('org.gnome.ScreenSaver');
const ScreenSaverProxy = Gio.DBusProxy.makeProxyWrapper(ScreenSaverIface);

var ScreenSaverService = class extends ServiceImplementation {
    constructor() {
        super(ScreenSaverIface, '/org/gnome/ScreenSaver');

        this._autoShutdown = false;

        this._proxy = new ScreenSaverProxy(Gio.DBus.session,
            'org.gnome.Shell.ScreenShield',
            '/org/gnome/ScreenSaver',
            (proxy, error) => {
                if (error)
                    log(error.message);
            });

        this._proxy.connectSignal('ActiveChanged',
            (proxy, sender, params) => {
                this._dbusImpl.emit_signal('ActiveChanged',
                    new GLib.Variant('(b)', params));
            });
        this._proxy.connectSignal('WakeUpScreen',
            () => this._dbusImpl.emit_signal('WakeUpScreen', null));
    }

    LockAsync(params, invocation) {
        this._proxy.LockRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(null);
        });
    }

    GetActiveAsync(params, invocation) {
        this._proxy.GetActiveRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(new GLib.Variant('(b)', res));
        });
    }

    SetActiveAsync(params, invocation) {
        this._proxy.SetActiveRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(null);
        });
    }

    GetActiveTimeAsync(params, invocation) {
        this._proxy.GetActiveTimeRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(new GLib.Variant('(u)', res));
        });
    }
};
