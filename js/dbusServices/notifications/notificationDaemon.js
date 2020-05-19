// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported NotificationDaemon */

const { Gio, GLib } = imports.gi;

const { loadInterfaceXML } = imports.misc.fileUtils;
const { ServiceImplementation } = imports.dbusService;

const NotificationsIface = loadInterfaceXML('org.freedesktop.Notifications');
const NotificationsProxy = Gio.DBusProxy.makeProxyWrapper(NotificationsIface);

Gio._promisify(Gio.DBusConnection.prototype, 'call', 'call_finish');

var NotificationDaemon = class extends ServiceImplementation {
    constructor() {
        super(NotificationsIface, '/org/freedesktop/Notifications');

        this._autoShutdown = false;

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

    async NotifyAsync(params, invocation) {
        const pid = await this._getSenderPid(invocation.get_sender());
        const hints = params[6];

        params[6] = {
            ...hints,
            'sender-pid': new GLib.Variant('u', pid),
        };

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

    async _getSenderPid(sender) {
        const res = await Gio.DBus.session.call(
            'org.freedesktop.DBus',
            '/',
            'org.freedesktop.DBus',
            'GetConnectionUnixProcessID',
            new GLib.Variant('(s)', [sender]),
            new GLib.VariantType('(u)'),
            Gio.DBusCallFlags.NONE,
            -1,
            null);
        const [pid] = res.deepUnpack();
        return pid;
    }
};
