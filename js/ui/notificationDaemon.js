// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

import GdkPixbuf from 'gi://GdkPixbuf';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Shell from 'gi://Shell';

import * as Config from '../misc/config.js';
import * as Main from './main.js';
import * as MessageTray from './messageTray.js';
import * as Params from '../misc/params.js';

import {loadInterfaceXML} from '../misc/fileUtils.js';
import {NotificationErrors, NotificationError} from '../misc/dbusErrors.js';

const FdoNotificationsIface = loadInterfaceXML('org.freedesktop.Notifications');

/** @enum {number} */
const NotificationClosedReason = {
    EXPIRED: 1,
    DISMISSED: 2,
    APP_CLOSED: 3,
    UNDEFINED: 4,
};

/** @enum {number} */
const Urgency = {
    LOW: 0,
    NORMAL: 1,
    CRITICAL: 2,
};

class FdoNotificationDaemon {
    constructor() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(FdoNotificationsIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/freedesktop/Notifications');

        this._sources = [];
        this._notifications = {};

        this._nextNotificationId = 1;
    }

    _imageForNotificationData(hints) {
        if (hints['image-data']) {
            const [
                width, height, rowStride, hasAlpha,
                bitsPerSample, nChannels_, data,
            ] = hints['image-data'];
            return Shell.util_create_pixbuf_from_data(data,
                GdkPixbuf.Colorspace.RGB,
                hasAlpha,
                bitsPerSample,
                width,
                height,
                rowStride);
        } else if (hints['image-path']) {
            return this._iconForNotificationData(hints['image-path']);
        }
        return null;
    }

    _iconForNotificationData(icon) {
        if (icon) {
            if (icon.substr(0, 7) === 'file://')
                return new Gio.FileIcon({file: Gio.File.new_for_uri(icon)});
            else if (icon[0] === '/')
                return new Gio.FileIcon({file: Gio.File.new_for_path(icon)});
            else
                return new Gio.ThemedIcon({name: icon});
        }
        return null;
    }

    _lookupSource(title, pid) {
        for (let i = 0; i < this._sources.length; i++) {
            let source = this._sources[i];
            if (source.pid === pid && source.initialTitle === title)
                return source;
        }
        return null;
    }

    // Returns the source associated with ndata.notification if it is set.
    // If the existing or requested source is associated with a tray icon
    // and passed in pid matches a pid of an existing source, the title
    // match is ignored to enable representing a tray icon and notifications
    // from the same application with a single source.
    //
    // If no existing source is found, a new source is created as long as
    // pid is provided.
    _getSource(title, pid, ndata, sender) {
        if (!pid && !(ndata && ndata.notification))
            throw new Error('Either a pid or ndata.notification is needed');

        // We use notification's source for the notifications we still have
        // around that are getting replaced because we don't keep sources
        // for transient notifications in this._sources, but we still want
        // the notification associated with them to get replaced correctly.
        if (ndata && ndata.notification)
            return ndata.notification.source;

        let source = this._lookupSource(title, pid);
        if (source)
            return source;

        const appId = ndata?.hints['desktop-entry'];
        source = new FdoNotificationDaemonSource(title, pid, sender, appId);

        this._sources.push(source);
        source.connect('destroy', () => {
            let index = this._sources.indexOf(source);
            if (index >= 0)
                this._sources.splice(index, 1);
        });

        Main.messageTray.add(source);
        return source;
    }

    NotifyAsync(params, invocation) {
        let [appName, replacesId, appIcon, summary, body, actions, hints, timeout] = params;
        let id;

        for (let hint in hints) {
            // unpack the variants
            hints[hint] = hints[hint].deepUnpack();
        }

        hints = Params.parse(hints, {urgency: Urgency.NORMAL}, true);

        // Be compatible with the various hints for image data and image path
        // 'image-data' and 'image-path' are the latest name of these hints, introduced in 1.2

        if (!hints['image-path'] && hints['image_path'])
            hints['image-path'] = hints['image_path']; // version 1.1 of the spec

        if (!hints['image-data']) {
            if (hints['image_data'])
                hints['image-data'] = hints['image_data']; // version 1.1 of the spec
            else if (hints['icon_data'] && !hints['image-path'])
                // early versions of the spec; 'icon_data' should only be used if 'image-path' is not available
                hints['image-data'] = hints['icon_data'];
        }

        const ndata = {
            appName,
            appIcon,
            summary,
            body,
            actions,
            hints,
            timeout,
        };
        if (replacesId !== 0 && this._notifications[replacesId]) {
            ndata.id = id = replacesId;
            ndata.notification = this._notifications[replacesId].notification;
        } else {
            replacesId = 0;
            ndata.id = id = this._nextNotificationId++;
        }
        this._notifications[id] = ndata;

        let sender = invocation.get_sender();
        let pid = hints['sender-pid'];

        let source = this._getSource(appName, pid, ndata, sender, null);
        this._notifyForSource(source, ndata);

        return invocation.return_value(GLib.Variant.new('(u)', [id]));
    }

    _notifyForSource(source, ndata) {
        const {appIcon, summary, body, actions, hints} = ndata;
        let {notification} = ndata;

        if (notification == null) {
            notification = new MessageTray.Notification(source);
            ndata.notification = notification;
            notification.connect('destroy', (n, reason) => {
                delete this._notifications[ndata.id];
                let notificationClosedReason;
                switch (reason) {
                case MessageTray.NotificationDestroyedReason.EXPIRED:
                    notificationClosedReason = NotificationClosedReason.EXPIRED;
                    break;
                case MessageTray.NotificationDestroyedReason.DISMISSED:
                    notificationClosedReason = NotificationClosedReason.DISMISSED;
                    break;
                case MessageTray.NotificationDestroyedReason.SOURCE_CLOSED:
                    notificationClosedReason = NotificationClosedReason.APP_CLOSED;
                    break;
                }
                this._emitNotificationClosed(ndata.id, notificationClosedReason);
            });
        }

        const gicon = this._imageForNotificationData(hints);

        const soundFile = 'sound-file' in hints
            ? Gio.File.new_for_path(hints['sound-file']) : null;

        notification.update(summary, body, {
            gicon,
            bannerMarkup: true,
            clear: true,
            soundFile,
            soundName: hints['sound-name'],
        });

        let hasDefaultAction = false;

        if (actions.length) {
            for (let i = 0; i < actions.length - 1; i += 2) {
                let [actionId, label] = [actions[i], actions[i + 1]];
                if (actionId === 'default') {
                    hasDefaultAction = true;
                } else {
                    notification.addAction(label, () => {
                        this._emitActionInvoked(ndata.id, actionId);
                    });
                }
            }
        }

        if (hasDefaultAction) {
            notification.connect('activated', () => {
                this._emitActionInvoked(ndata.id, 'default');
            });
        } else {
            notification.connect('activated', () => {
                source.open();
            });
        }

        switch (hints.urgency) {
        case Urgency.LOW:
            notification.setUrgency(MessageTray.Urgency.LOW);
            break;
        case Urgency.NORMAL:
            notification.setUrgency(MessageTray.Urgency.NORMAL);
            break;
        case Urgency.CRITICAL:
            notification.setUrgency(MessageTray.Urgency.CRITICAL);
            break;
        }
        notification.setResident(!!hints.resident);
        // 'transient' is a reserved keyword in JS, so we have to retrieve the value
        // of the 'transient' hint with hints['transient'] rather than hints.transient
        notification.setTransient(!!hints['transient']);

        let privacyScope = hints['x-gnome-privacy-scope'] || 'user';
        notification.setPrivacyScope(privacyScope === 'system'
            ? MessageTray.PrivacyScope.SYSTEM
            : MessageTray.PrivacyScope.USER);

        // Only fallback to 'app-icon' when the source doesn't have a valid app
        const sourceGIcon = source.app ? null : this._iconForNotificationData(appIcon);
        source.processNotification(notification, sourceGIcon);
    }

    CloseNotification(id) {
        let ndata = this._notifications[id];
        if (ndata) {
            if (ndata.notification)
                ndata.notification.destroy(MessageTray.NotificationDestroyedReason.SOURCE_CLOSED);
            delete this._notifications[id];
        }
    }

    GetCapabilities() {
        return [
            'actions',
            // 'action-icons',
            'body',
            // 'body-hyperlinks',
            // 'body-images',
            'body-markup',
            // 'icon-multi',
            'icon-static',
            'persistence',
            'sound',
        ];
    }

    GetServerInformation() {
        return [
            Config.PACKAGE_NAME,
            'GNOME',
            Config.PACKAGE_VERSION,
            '1.2',
        ];
    }

    _emitNotificationClosed(id, reason) {
        this._dbusImpl.emit_signal('NotificationClosed',
            GLib.Variant.new('(uu)', [id, reason]));
    }

    _emitActionInvoked(id, action) {
        this._dbusImpl.emit_signal('ActionInvoked',
            GLib.Variant.new('(us)', [id, action]));
    }
}

export const FdoNotificationDaemonSource = GObject.registerClass(
class FdoNotificationDaemonSource extends MessageTray.Source {
    constructor(title, pid, sender, appId) {
        const appSys = Shell.AppSystem.get_default();
        let app;

        app = Shell.WindowTracker.get_default().get_app_from_pid(pid);
        if (!app && appId)
            app = appSys.lookup_app(`${appId}.desktop`);

        if (!app)
            app = appSys.lookup_app(`${title}.desktop`);

        // Use app name as title if available, instead of whatever is provided
        // through libnotify (usually garbage)
        super({
            title: app?.get_name() ?? title,
            policy: MessageTray.NotificationPolicy.newForApp(app),
        });

        this.pid = pid;
        this.initialTitle = title;
        this.app = app;
        this._appIcon = null;


        if (sender) {
            this._nameWatcherId = Gio.DBus.session.watch_name(sender,
                Gio.BusNameWatcherFlags.NONE,
                null,
                this._onNameVanished.bind(this));
        } else {
            this._nameWatcherId = 0;
        }
    }

    _onNameVanished() {
        // Destroy the notification source when its sender is removed from DBus.
        // Only do so if this.app is set to avoid removing "notify-send" sources, senders
        // of which аre removed from DBus immediately.
        // Sender being removed from DBus would normally result in a tray icon being removed,
        // so allow the code path that handles the tray icon being removed to handle that case.
        if (this.app)
            this.destroy();
    }

    processNotification(notification, appIcon) {
        if (!this.app && appIcon) {
            this._appIcon = appIcon;
            this.notify('icon');
        }


        let tracker = Shell.WindowTracker.get_default();
        if (notification.resident && this.app && tracker.focus_app === this.app)
            this.pushNotification(notification);
        else
            this.showNotification(notification);
    }

    open() {
        this.openApp();
        this.destroyNonResidentNotifications();
    }

    openApp() {
        if (this.app == null)
            return;

        this.app.activate();
        Main.overview.hide();
        Main.panel.closeCalendar();
    }

    destroy() {
        if (this._nameWatcherId) {
            Gio.DBus.session.unwatch_name(this._nameWatcherId);
            this._nameWatcherId = 0;
        }

        super.destroy();
    }

    get icon() {
        return this.app?.get_icon() ?? this._appIcon;
    }
});

const PRIORITY_URGENCY_MAP = {
    low: MessageTray.Urgency.LOW,
    normal: MessageTray.Urgency.NORMAL,
    high: MessageTray.Urgency.HIGH,
    urgent: MessageTray.Urgency.CRITICAL,
};

const GtkNotificationDaemonNotification = GObject.registerClass(
class GtkNotificationDaemonNotification extends MessageTray.Notification {
    _init(source, notification) {
        super._init(source);
        this._serialized = GLib.Variant.new('a{sv}', notification);

        const {
            title,
            body,
            icon: gicon,
            urgent,
            priority,
            buttons,
            'default-action': defaultAction,
            'default-action-target': defaultActionTarget,
            timestamp: time,
        } = notification;

        if (priority) {
            let urgency = PRIORITY_URGENCY_MAP[priority.unpack()];
            this.setUrgency(urgency !== undefined ? urgency : MessageTray.Urgency.NORMAL);
        } else if (urgent) {
            this.setUrgency(urgent.unpack()
                ? MessageTray.Urgency.CRITICAL
                : MessageTray.Urgency.NORMAL);
        } else {
            this.setUrgency(MessageTray.Urgency.NORMAL);
        }

        if (buttons) {
            buttons.deepUnpack().forEach(button => {
                this.addAction(button.label.unpack(), () => {
                    this._onButtonClicked(button);
                });
            });
        }

        this._defaultAction = defaultAction?.unpack();
        this._defaultActionTarget = defaultActionTarget;

        this.update(title.unpack(), body?.unpack(), {
            gicon: gicon
                ? Gio.icon_deserialize(gicon) : null,
            datetime: time
                ? GLib.DateTime.new_from_unix_local(time.unpack()) : null,
        });
    }

    _activateAction(namespacedActionId, target) {
        if (namespacedActionId) {
            if (namespacedActionId.startsWith('app.')) {
                let actionId = namespacedActionId.slice('app.'.length);
                this.source.activateAction(actionId, target);
            }
        } else {
            this.source.open();
        }
    }

    _onButtonClicked(button) {
        let {action, target} = button;
        this._activateAction(action.unpack(), target);
    }

    activate() {
        this._activateAction(this._defaultAction, this._defaultActionTarget);
        super.activate();
    }

    serialize() {
        return this._serialized;
    }
});

const FdoApplicationIface = loadInterfaceXML('org.freedesktop.Application');
const FdoApplicationProxy = Gio.DBusProxy.makeProxyWrapper(FdoApplicationIface);

function objectPathFromAppId(appId) {
    return `/${appId.replace(/\./g, '/').replace(/-/g, '_')}`;
}

/**
 * @returns {{ 'desktop-startup-id': string }}
 */
function getPlatformData() {
    let startupId = GLib.Variant.new('s', `_TIME${global.get_current_time()}`);
    return {'desktop-startup-id': startupId};
}

function InvalidAppError() {}

export const GtkNotificationDaemonAppSource = GObject.registerClass(
class GtkNotificationDaemonAppSource extends MessageTray.Source {
    constructor(appId) {
        const objectPath = objectPathFromAppId(appId);
        if (!GLib.Variant.is_object_path(objectPath))
            throw new InvalidAppError();

        const app = Shell.AppSystem.get_default().lookup_app(`${appId}.desktop`);
        if (!app)
            throw new InvalidAppError();

        super({
            title: app.get_name(),
            icon: app.get_icon(),
            policy: new MessageTray.NotificationApplicationPolicy(appId),
        });

        this._appId = appId;
        this._app = app;
        this._objectPath = objectPath;

        this._notifications = {};
        this._notificationPending = false;
    }

    _createApp() {
        return new Promise((resolve, reject) => {
            new FdoApplicationProxy(Gio.DBus.session,
                this._appId, this._objectPath, (proxy, err) => {
                    if (err)
                        reject(err);
                    else
                        resolve(proxy);
                });
        });
    }

    _createNotification(params) {
        return new GtkNotificationDaemonNotification(this, params);
    }

    async activateAction(actionId, target) {
        try {
            const app = await this._createApp();
            const params = target ? [target] : [];
            app.ActivateActionAsync(actionId, params, getPlatformData());
        } catch (error) {
            logError(error, 'Failed to activate app proxy');
        }
        Main.overview.hide();
        Main.panel.closeCalendar();
    }

    async open() {
        try {
            const app = await this._createApp();
            app.ActivateAsync(getPlatformData());
        } catch (error) {
            logError(error, 'Failed to open app proxy');
        }
        Main.overview.hide();
        Main.panel.closeCalendar();
    }

    addNotification(notificationId, notificationParams, showBanner) {
        this._notificationPending = true;

        if (this._notifications[notificationId])
            this._notifications[notificationId].destroy(MessageTray.NotificationDestroyedReason.REPLACED);

        let notification = this._createNotification(notificationParams);
        notification.connect('destroy', () => {
            delete this._notifications[notificationId];
        });
        this._notifications[notificationId] = notification;

        if (showBanner)
            this.showNotification(notification);
        else
            this.pushNotification(notification);

        this._notificationPending = false;
    }

    destroy(reason) {
        if (this._notificationPending)
            return;
        super.destroy(reason);
    }

    removeNotification(notificationId) {
        if (this._notifications[notificationId])
            this._notifications[notificationId].destroy(MessageTray.NotificationDestroyedReason.SOURCE_CLOSED);
    }

    serialize() {
        let notifications = [];
        for (let notificationId in this._notifications) {
            let notification = this._notifications[notificationId];
            notifications.push([notificationId, notification.serialize()]);
        }
        return [this._appId, notifications];
    }
});

const GtkNotificationsIface = loadInterfaceXML('org.gtk.Notifications');

class GtkNotificationDaemon {
    constructor() {
        this._sources = {};

        this._loadNotifications();

        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(GtkNotificationsIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gtk/Notifications');

        Gio.DBus.session.own_name('org.gtk.Notifications', Gio.BusNameOwnerFlags.REPLACE, null, null);
    }

    _ensureAppSource(appId) {
        if (this._sources[appId])
            return this._sources[appId];

        let source = new GtkNotificationDaemonAppSource(appId);

        source.connect('destroy', () => {
            delete this._sources[appId];
            this._saveNotifications();
        });
        source.connect('notify::count', this._saveNotifications.bind(this));
        Main.messageTray.add(source);
        this._sources[appId] = source;
        return source;
    }

    _loadNotifications() {
        this._isLoading = true;

        try {
            let value = global.get_persistent_state('a(sa(sv))', 'notifications');
            if (value) {
                let sources = value.deepUnpack();
                sources.forEach(([appId, notifications]) => {
                    if (notifications.length === 0)
                        return;

                    let source;
                    try {
                        source = this._ensureAppSource(appId);
                    } catch (e) {
                        if (e instanceof InvalidAppError)
                            return;
                        throw e;
                    }

                    notifications.forEach(([notificationId, notification]) => {
                        source.addNotification(notificationId, notification.deepUnpack(), false);
                    });
                });
            }
        } catch (e) {
            logError(e, 'Failed to load saved notifications');
        } finally {
            this._isLoading = false;
        }
    }

    _saveNotifications() {
        if (this._isLoading)
            return;

        let sources = [];
        for (let appId in this._sources) {
            let source = this._sources[appId];
            sources.push(source.serialize());
        }

        global.set_persistent_state('notifications', new GLib.Variant('a(sa(sv))', sources));
    }

    AddNotificationAsync(params, invocation) {
        let [appId, notificationId, notification] = params;

        let source;
        try {
            source = this._ensureAppSource(appId);
        } catch (e) {
            if (e instanceof InvalidAppError) {
                invocation.return_error_literal(NotificationErrors,
                    NotificationError.INVALID_APP,
                    `The app by ID "${appId}" could not be found`);
                return;
            }
            throw e;
        }

        let timestamp = GLib.DateTime.new_now_local().to_unix();
        notification['timestamp'] = new GLib.Variant('x', timestamp);

        source.addNotification(notificationId, notification, true);

        invocation.return_value(null);
    }

    RemoveNotificationAsync(params, invocation) {
        let [appId, notificationId] = params;
        let source = this._sources[appId];
        if (source)
            source.removeNotification(notificationId);

        invocation.return_value(null);
    }
}

export class NotificationDaemon {
    constructor() {
        this._fdoNotificationDaemon = new FdoNotificationDaemon();
        this._gtkNotificationDaemon = new GtkNotificationDaemon();
    }
}
