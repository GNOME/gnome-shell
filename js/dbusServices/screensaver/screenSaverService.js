import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

import {ServiceImplementation} from './dbusService.js';

import {loadInterfaceXML} from './misc/dbusUtils.js';

const ScreenSaverIface = loadInterfaceXML('org.gnome.ScreenSaver');
const ScreenSaverProxy = Gio.DBusProxy.makeProxyWrapper(ScreenSaverIface);

export const ScreenSaverService = class extends ServiceImplementation {
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

    async LockAsync(params, invocation) {
        try {
            await this._proxy.LockAsync(...params);
            invocation.return_value(null);
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    async GetActiveAsync(params, invocation) {
        try {
            const res = await this._proxy.GetActiveAsync(...params);
            invocation.return_value(new GLib.Variant('(b)', res));
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    async SetActiveAsync(params, invocation) {
        try {
            await this._proxy.SetActiveAsync(...params);
            invocation.return_value(null);
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    async GetActiveTimeAsync(params, invocation) {
        try {
            const res = await this._proxy.GetActiveTimeAsync(...params);
            invocation.return_value(new GLib.Variant('(u)', res));
        } catch (error) {
            this._handleError(invocation, error);
        }
    }
};
