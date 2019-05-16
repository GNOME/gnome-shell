// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported NotificationDaemon */

const { Gio, GLib } = imports.gi;

const { loadInterfaceXML } = imports.misc.fileUtils;
const { ServiceImplementation } = imports.dbusService;

const NotificationsIface = loadInterfaceXML('org.freedesktop.Notifications');
const NotificationsProxy = Gio.DBusProxy.makeProxyWrapper(NotificationsIface);

var NotificationDaemon = class extends ServiceImplementation {
    constructor() {
        super(NotificationsIface, '/org/freedesktop/Notifications');

        this._proxy = new NotificationsProxy(Gio.DBus.session,
            'org.gnome.Shell',
            '/org/freedesktop/Notifications',
            (proxy, error) => {
                if (error)
                    log(error.message);
            });

        this._proxy.connectSignal('ActionInvoked',
            (proxy, sender, params) => {
                this._dbusImpl.emit_signal('ActionInvoked',
                    new GLib.Variant('(us)', params));
            });
        this._proxy.connectSignal('NotificationClosed',
            (proxy, sender, params) => {
                this._dbusImpl.emit_signal('NotificationClosed',
                    new GLib.Variant('(uu)', params));
            });
    }

    register() {
        Gio.DBus.session.own_name(
            'org.freedesktop.Notifications',
            Gio.BusNameOwnerFlags.REPLACE,
            null, null);
    }

    NotifyAsync(params, invocation) {
        this._proxy.NotifyRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(new GLib.Variant('(u)', res));
        });
    }

    CloseNotificationAsync(params, invocation) {
        this._proxy.CloseNotificationRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(null);
        });
    }

    GetCapabilitiesAsync(params, invocation) {
        this._proxy.GetCapabilitiesRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(new GLib.Variant('(as)', res));
        });
    }

    GetServerInformationAsync(params, invocation) {
        this._proxy.GetServerInformationRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(new GLib.Variant('(ssss)', res));
        });
    }

    _handleError(invocation, error) {
        if (error)
            invocation.return_gerror(error);

        return error !== null;
    }
};
