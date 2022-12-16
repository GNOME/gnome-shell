// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported NotificationDaemon */

const { GdkPixbuf, Gio, GLib, GObject, Shell, St } = imports.gi;

const Config = imports.misc.config;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const Params = imports.misc.params;

const { loadInterfaceXML } = imports.misc.fileUtils;

const FdoNotificationsIface = loadInterfaceXML('org.freedesktop.Notifications');

var NotificationClosedReason = {
    EXPIRED: 1,
    DISMISSED: 2,
    APP_CLOSED: 3,
    UNDEFINED: 4,
};

var Urgency = {
    LOW: 0,
    NORMAL: 1,
    CRITICAL: 2,
};

var FdoNotificationDaemon = class FdoNotificationDaemon {
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
            return Shell.util_create_pixbuf_from_data(data, GdkPixbuf.Colorspace.RGB, hasAlpha,
                                                      bitsPerSample, width, height, rowStride);
        } else if (hints['image-path']) {
            return this._iconForNotificationData(hints['image-path']);
        }
        return null;
    }

    _fallbackIconForNotificationData(hints) {
        let stockIcon;
        switch (hints.urgency) {
        case Urgency.LOW:
        case Urgency.NORMAL:
            stockIcon = 'dialog-information';
            break;
        case Urgency.CRITICAL:
            stockIcon = 'dialog-error';
            break;
        }
        return new Gio.ThemedIcon({ name: stockIcon });
    }

    _iconForNotificationData(icon) {
        if (icon) {
            if (icon.substr(0, 7) == 'file://')
                return new Gio.FileIcon({ file: Gio.File.new_for_uri(icon) });
            else if (icon[0] == '/')
                return new Gio.FileIcon({ file: Gio.File.new_for_path(icon) });
            else
                return new Gio.ThemedIcon({ name: icon });
        }
        return null;
    }

    _lookupSource(title, pid) {
        for (let i = 0; i < this._sources.length; i++) {
            let source = this._sources[i];
            if (source.pid == pid && source.initialTitle == title)
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
        if (source) {
            source.setTitle(title);
            return source;
        }

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
        let [appName, replacesId, icon, summary, body, actions, hints, timeout] = params;
        let id;

        for (let hint in hints) {
            // unpack the variants
            hints[hint] = hints[hint].deepUnpack();
        }

        hints = Params.parse(hints, { urgency: Urgency.NORMAL }, true);

        // Filter out chat, presence, calls and invitation notifications from
        // Empathy, since we handle that information from telepathyClient.js
        //
        // Note that empathy uses im.received for one to one chats and
        // x-empathy.im.mentioned for multi-user, so we're good here
        if (appName == 'Empathy' && hints['category'] == 'im.received') {
            // Ignore replacesId since we already sent back a
            // NotificationClosed for that id.
            id = this._nextNotificationId++;
            let idleId = GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
                this._emitNotificationClosed(id, NotificationClosedReason.DISMISSED);
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(idleId, '[gnome-shell] this._emitNotificationClosed');
            return invocation.return_value(GLib.Variant.new('(u)', [id]));
        }

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
            icon,
            summary,
            body,
            actions,
            hints,
            timeout,
        };
        if (replacesId != 0 && this._notifications[replacesId]) {
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
        const { icon, summary, body, actions, hints } = ndata;
        let { notification } = ndata;

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

        // 'image-data' (or 'image-path') takes precedence over 'app-icon'.
        let gicon = this._imageForNotificationData(hints);

        if (!gicon)
            gicon = this._iconForNotificationData(icon);

        if (!gicon)
            gicon = this._fallbackIconForNotificationData(hints);

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
                if (actionId == 'default') {
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
        notification.setPrivacyScope(privacyScope == 'system'
            ? MessageTray.PrivacyScope.SYSTEM
            : MessageTray.PrivacyScope.USER);

        let sourceGIcon = source.useNotificationIcon ? gicon : null;
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
};

var FdoNotificationDaemonSource = GObject.registerClass(
class FdoNotificationDaemonSource extends MessageTray.Source {
    _init(title, pid, sender, appId) {
        this.pid = pid;
        this.initialTitle = title;
        this.app = this._getApp(appId);

        super._init(title);

        if (this.app)
            this.title = this.app.get_name();
        else
            this.useNotificationIcon = true;

        if (sender) {
            this._nameWatcherId = Gio.DBus.session.watch_name(sender,
                                                              Gio.BusNameWatcherFlags.NONE,
                                                              null,
                                                              this._onNameVanished.bind(this));
        } else {
            this._nameWatcherId = 0;
        }
    }

    _createPolicy() {
        if (this.app && this.app.get_app_info()) {
            let id = this.app.get_id().replace(/\.desktop$/, '');
            return new MessageTray.NotificationApplicationPolicy(id);
        } else {
            return new MessageTray.NotificationGenericPolicy();
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

    processNotification(notification, gicon) {
        if (gicon)
            this._gicon = gicon;
        this.iconUpdated();

        let tracker = Shell.WindowTracker.get_default();
        if (notification.resident && this.app && tracker.focus_app == this.app)
            this.pushNotification(notification);
        else
            this.showNotification(notification);
    }

    _getApp(appId) {
        const appSys = Shell.AppSystem.get_default();
        let app;

        app = Shell.WindowTracker.get_default().get_app_from_pid(this.pid);
        if (app != null)
            return app;

        if (appId)
            app = appSys.lookup_app(`${appId}.desktop`);

        if (!app)
            app = appSys.lookup_app(`${this.initialTitle}.desktop`);

        return app;
    }

    setTitle(title) {
        // Do nothing if .app is set, we don't want to override the
        // app name with whatever is provided through libnotify (usually
        // garbage)
        if (this.app)
            return;

        super.setTitle(title);
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

    createIcon(size) {
        if (this.app) {
            return this.app.create_icon_texture(size);
        } else if (this._gicon) {
            return new St.Icon({
                gicon: this._gicon,
                icon_size: size,
            });
        } else {
            return null;
        }
    }
});

const PRIORITY_URGENCY_MAP = {
    low: MessageTray.Urgency.LOW,
    normal: MessageTray.Urgency.NORMAL,
    high: MessageTray.Urgency.HIGH,
    urgent: MessageTray.Urgency.CRITICAL,
};

var GtkNotificationDaemonNotification = GObject.registerClass(
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
            this.setUrgency(urgency != undefined ? urgency : MessageTray.Urgency.NORMAL);
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
        let { action, target } = button;
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

function getPlatformData() {
    let startupId = GLib.Variant.new('s', `_TIME${global.get_current_time()}`);
    return { "desktop-startup-id": startupId };
}

function InvalidAppError() {}

var GtkNotificationDaemonAppSource = GObject.registerClass(
class GtkNotificationDaemonAppSource extends MessageTray.Source {
    _init(appId) {
        let objectPath = objectPathFromAppId(appId);
        if (!GLib.Variant.is_object_path(objectPath))
            throw new InvalidAppError();

        let app = Shell.AppSystem.get_default().lookup_app(`${appId}.desktop`);
        if (!app)
            throw new InvalidAppError();

        this._appId = appId;
        this._app = app;
        this._objectPath = objectPath;

        super._init(app.get_name());

        this._notifications = {};
        this._notificationPending = false;
    }

    createIcon(size) {
        return this._app.create_icon_texture(size);
    }

    _createPolicy() {
        return new MessageTray.NotificationApplicationPolicy(this._appId);
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

var GtkNotificationDaemon = class GtkNotificationDaemon {
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
                    if (notifications.length == 0)
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
                invocation.return_dbus_error('org.gtk.Notifications.InvalidApp',
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
};

var NotificationDaemon = class NotificationDaemon {
    constructor() {
        this._fdoNotificationDaemon = new FdoNotificationDaemon();
        this._gtkNotificationDaemon = new GtkNotificationDaemon();
    }
};
