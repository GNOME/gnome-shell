// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported NotificationDaemon */

const { Gio, GLib } = imports.gi;

const { loadInterfaceXML } = imports.misc.dbusUtils;
const { ServiceImplementation } = imports.dbusService;

const NotificationsIface = loadInterfaceXML('org.freedesktop.Notifications');
const NotificationsProxy = Gio.DBusProxy.makeProxyWrapper(NotificationsIface);

Gio._promisify(Gio.DBusConnection.prototype, 'call');

var NotificationDaemon = class extends ServiceImplementation {
    constructor() {
        super(NotificationsIface, '/org/freedesktop/Notifications');

        this._autoShutdown = false;

        this._activeNotifications = new Map();

        this._proxy = new NotificationsProxy(Gio.DBus.session,
            'org.gnome.Shell',
            '/org/freedesktop/Notifications',
            (proxy, error) => {
                if (error)
                    log(error.message);
            });

        this._proxy.connectSignal('ActionInvoked',
            (proxy, sender, params) => {
                const [id] = params;
                this._emitSignal(
                    this._activeNotifications.get(id),
                    'ActionInvoked',
                    new GLib.Variant('(us)', params));
            });
        this._proxy.connectSignal('NotificationClosed',
            (proxy, sender, params) => {
                const [id] = params;
                this._emitSignal(
                    this._activeNotifications.get(id),
                    'NotificationClosed',
                    new GLib.Variant('(uu)', params));
                this._activeNotifications.delete(id);
            });
    }

    _emitSignal(sender, signalName, params) {
        if (!sender)
            return;
        this._dbusImpl.get_connection()?.emit_signal(
            sender,
            this._dbusImpl.get_object_path(),
            'org.freedesktop.Notifications',
            signalName,
            params);
    }

    _untrackSender(sender) {
        super._untrackSender(sender);

        this._activeNotifications.forEach((value, key) => {
            if (value === sender)
                this._activeNotifications.delete(key);
        });
    }

    _checkNotificationId(invocation, id) {
        if (id === 0)
            return true;

        if (!this._activeNotifications.has(id))
            return true;

        if (this._activeNotifications.get(id) === invocation.get_sender())
            return true;

        const error = new GLib.Error(Gio.DBusError,
            Gio.DBusError.INVALID_ARGS, 'Invalid notification ID');
        this._handleError(invocation, error);
        return false;
    }

    register() {
        Gio.DBus.session.own_name(
            'org.freedesktop.Notifications',
            Gio.BusNameOwnerFlags.REPLACE,
            null, null);
    }

    async NotifyAsync(params, invocation) {
        const sender = invocation.get_sender();
        const pid = await this._getSenderPid(sender);
        const replaceId = params[1];
        const hints = params[6];

        if (!this._checkNotificationId(invocation, replaceId))
            return;

        params[6] = {
            ...hints,
            'sender-pid': new GLib.Variant('u', pid),
        };

        try {
            const [id] = await this._proxy.NotifyAsync(...params);
            this._activeNotifications.set(id, sender);
            invocation.return_value(new GLib.Variant('(u)', [id]));
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    async CloseNotificationAsync(params, invocation) {
        const [id] = params;
        if (!this._checkNotificationId(invocation, id))
            return;

        try {
            await this._proxy.CloseNotificationAsync(...params);
            invocation.return_value(null);
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    async GetCapabilitiesAsync(params, invocation) {
        try {
            const res = await this._proxy.GetCapabilitiesAsync(...params);
            invocation.return_value(new GLib.Variant('(as)', res));
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    async GetServerInformationAsync(params, invocation) {
        try {
            const res = await this._proxy.GetServerInformationAsync(...params);
            invocation.return_value(new GLib.Variant('(ssss)', res));
        } catch (error) {
            this._handleError(invocation, error);
        }
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
