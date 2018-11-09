// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GdkPixbuf = imports.gi.GdkPixbuf;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Mainloop = imports.mainloop;
const St = imports.gi.St;

const Config = imports.misc.config;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const Params = imports.misc.params;
const Util = imports.misc.util;

const { loadInterfaceXML } = imports.misc.fileUtils;

// Should really be defined in Gio.js
const BusIface = loadInterfaceXML('org.freedesktop.DBus');
var BusProxy = Gio.DBusProxy.makeProxyWrapper(BusIface);
function Bus() {
    return new BusProxy(Gio.DBus.session, 'org.freedesktop.DBus', '/org/freedesktop/DBus');
}

const FdoNotificationsIface = loadInterfaceXML('org.freedesktop.Notifications');

var NotificationClosedReason = {
    EXPIRED: 1,
    DISMISSED: 2,
    APP_CLOSED: 3,
    UNDEFINED: 4
};

var Urgency = {
    LOW: 0,
    NORMAL: 1,
    CRITICAL: 2
};

const rewriteRules = {
    'XChat': [
        { pattern:     /^XChat: Private message from: (\S*) \(.*\)$/,
          replacement: '<$1>' },
        { pattern:     /^XChat: New public message from: (\S*) \((.*)\)$/,
          replacement: '$2 <$1>' },
        { pattern:     /^XChat: Highlighted message from: (\S*) \((.*)\)$/,
          replacement: '$2 <$1>' }
    ]
};

var FdoNotificationDaemon = new Lang.Class({
    Name: 'FdoNotificationDaemon',

    _init() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(FdoNotificationsIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/freedesktop/Notifications');

        this._sources = [];
        this._senderToPid = {};
        this._notifications = {};
        this._busProxy = new Bus();

        this._nextNotificationId = 1;

        Shell.WindowTracker.get_default().connect('notify::focus-app',
            this._onFocusAppChanged.bind(this));
        Main.overview.connect('hidden',
            this._onFocusAppChanged.bind(this));
    },

    _imageForNotificationData(hints) {
        if (hints['image-data']) {
            let [width, height, rowStride, hasAlpha,
                 bitsPerSample, nChannels, data] = hints['image-data'];
            return Shell.util_create_pixbuf_from_data(data, GdkPixbuf.Colorspace.RGB, hasAlpha,
                                                      bitsPerSample, width, height, rowStride);
        }
        return this._iconForNotificationData(hints['image-path']);
    },

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
    },

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
    },

    _lookupSource(title, pid) {
        for (let i = 0; i < this._sources.length; i++) {
            let source = this._sources[i];
            if (source.pid == pid && source.initialTitle == title)
                return source;
        }
        return null;
    },

    // Returns the source associated with ndata.notification if it is set.
    // If the existing or requested source is associated with a tray icon
    // and passed in pid matches a pid of an existing source, the title
    // match is ignored to enable representing a tray icon and notifications
    // from the same application with a single source.
    //
    // If no existing source is found, a new source is created as long as
    // pid is provided.
    //
    // Either a pid or ndata.notification is needed to retrieve or
    // create a source.
    _getSource(title, pid, ndata, sender) {
        if (!pid && !(ndata && ndata.notification))
            return null;

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

        let appId = ndata ? ndata.hints['desktop-entry'] || null : null;
        source = new FdoNotificationDaemonSource(title, pid, sender, appId);

        this._sources.push(source);
        source.connect('destroy', () => {
            let index = this._sources.indexOf(source);
            if (index >= 0)
                this._sources.splice(index, 1);
        });

        Main.messageTray.add(source);
        return source;
    },

    NotifyAsync(params, invocation) {
        let [appName, replacesId, icon, summary, body, actions, hints, timeout] = params;
        let id;

        for (let hint in hints) {
            // unpack the variants
            hints[hint] = hints[hint].deep_unpack();
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
            let idle_id = Mainloop.idle_add(() => {
                this._emitNotificationClosed(id, NotificationClosedReason.DISMISSED);
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(idle_id, '[gnome-shell] this._emitNotificationClosed');
            return invocation.return_value(GLib.Variant.new('(u)', [id]));
        }

        let rewrites = rewriteRules[appName];
        if (rewrites) {
            for (let i = 0; i < rewrites.length; i++) {
                let rule = rewrites[i];
                if (summary.search(rule.pattern) != -1)
                    summary = summary.replace(rule.pattern, rule.replacement);
            }
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

        let ndata = { appName: appName,
                      icon: icon,
                      summary: summary,
                      body: body,
                      actions: actions,
                      hints: hints,
                      timeout: timeout };
        if (replacesId != 0 && this._notifications[replacesId]) {
            ndata.id = id = replacesId;
            ndata.notification = this._notifications[replacesId].notification;
        } else {
            replacesId = 0;
            ndata.id = id = this._nextNotificationId++;
        }
        this._notifications[id] = ndata;

        let sender = invocation.get_sender();
        let pid = this._senderToPid[sender];

        let source = this._getSource(appName, pid, ndata, sender, null);

        if (source) {
            this._notifyForSource(source, ndata);
            return invocation.return_value(GLib.Variant.new('(u)', [id]));
        }

        if (replacesId) {
            // There's already a pending call to GetConnectionUnixProcessID,
            // which will see the new notification data when it finishes,
            // so we don't have to do anything.
            return invocation.return_value(GLib.Variant.new('(u)', [id]));;
        }

        this._busProxy.GetConnectionUnixProcessIDRemote(sender, (result, excp) => {
            // The app may have updated or removed the notification
            ndata = this._notifications[id];
            if (!ndata)
                return;

            if (excp) {
                logError(excp, 'Call to GetConnectionUnixProcessID failed');
                return;
            }

            let [pid] = result;
            source = this._getSource(appName, pid, ndata, sender, null);

            this._senderToPid[sender] = pid;
            source.connect('destroy', () => {
                delete this._senderToPid[sender];
            });
            this._notifyForSource(source, ndata);
        });

        return invocation.return_value(GLib.Variant.new('(u)', [id]));
    },

    _notifyForSource(source, ndata) {
        let [id, icon, summary, body, actions, hints, notification] =
            [ndata.id, ndata.icon, ndata.summary, ndata.body,
             ndata.actions, ndata.hints, ndata.notification];

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

        let gicon = this._iconForNotificationData(icon, hints);
        let gimage = this._imageForNotificationData(hints);

        // If an icon is not specified, we use 'image-data' or 'image-path' hint for an icon
        // and don't show a large image. There are currently many applications that use
        // notify_notification_set_icon_from_pixbuf() from libnotify, which in turn sets
        // the 'image-data' hint. These applications don't typically pass in 'app_icon'
        // argument to Notify() and actually expect the pixbuf to be shown as an icon.
        // So the logic here does the right thing for this case. If both an icon and either
        // one of 'image-data' or 'image-path' are specified, the icon and takes precedence.
        if (!gicon && gimage)
            gicon = gimage;
        else if (!gicon)
            gicon = this._fallbackIconForNotificationData(hints);

        notification.update(summary, body, { gicon: gicon,
                                             bannerMarkup: true,
                                             clear: true,
                                             soundFile: hints['sound-file'],
                                             soundName: hints['sound-name'] });

        let hasDefaultAction = false;

        if (actions.length) {
            for (let i = 0; i < actions.length - 1; i += 2) {
                let [actionId, label] = [actions[i], actions[i+1]];
                if (actionId == 'default')
                    hasDefaultAction = true;
                else
                    notification.addAction(label, () => {
                        this._emitActionInvoked(ndata.id, actionId);
                    });
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

        let sourceGIcon = source.useNotificationIcon ? gicon : null;
        source.processNotification(notification, sourceGIcon);
    },

    CloseNotification(id) {
        let ndata = this._notifications[id];
        if (ndata) {
            if (ndata.notification)
                ndata.notification.destroy(MessageTray.NotificationDestroyedReason.SOURCE_CLOSED);
            delete this._notifications[id];
        }
    },

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
    },

    GetServerInformation() {
        return [
            Config.PACKAGE_NAME,
            'GNOME',
            Config.PACKAGE_VERSION,
            '1.2'
        ];
    },

    _onFocusAppChanged() {
        let tracker = Shell.WindowTracker.get_default();
        if (!tracker.focus_app)
            return;

        for (let i = 0; i < this._sources.length; i++) {
            let source = this._sources[i];
            if (source.app == tracker.focus_app) {
                source.destroyNonResidentNotifications();
                return;
            }
        }
    },

    _emitNotificationClosed(id, reason) {
        this._dbusImpl.emit_signal('NotificationClosed',
                                   GLib.Variant.new('(uu)', [id, reason]));
    },

    _emitActionInvoked(id, action) {
        this._dbusImpl.emit_signal('ActionInvoked',
                                   GLib.Variant.new('(us)', [id, action]));
    }
});

var FdoNotificationDaemonSource = new Lang.Class({
    Name: 'FdoNotificationDaemonSource',
    Extends: MessageTray.Source,

    _init(title, pid, sender, appId) {
        // Need to set the app before chaining up, so
        // methods called from the parent constructor can find it
        this.pid = pid;
        this.app = this._getApp(appId);

        this.parent(title);

        this.initialTitle = title;

        if (this.app)
            this.title = this.app.get_name();
        else
            this.useNotificationIcon = true;

        if (sender)
            this._nameWatcherId = Gio.DBus.session.watch_name(sender,
                                                              Gio.BusNameWatcherFlags.NONE,
                                                              null,
                                                              this._onNameVanished.bind(this));
        else
            this._nameWatcherId = 0;
    },

    _createPolicy() {
        if (this.app && this.app.get_app_info()) {
            let id = this.app.get_id().replace(/\.desktop$/,'');
            return new MessageTray.NotificationApplicationPolicy(id);
        } else {
            return new MessageTray.NotificationGenericPolicy();
        }
    },

    _onNameVanished() {
        // Destroy the notification source when its sender is removed from DBus.
        // Only do so if this.app is set to avoid removing "notify-send" sources, senders
        // of which аre removed from DBus immediately.
        // Sender being removed from DBus would normally result in a tray icon being removed,
        // so allow the code path that handles the tray icon being removed to handle that case.
        if (this.app)
            this.destroy();
    },

    processNotification(notification, gicon) {
        if (gicon)
            this._gicon = gicon;
        this.iconUpdated();

        let tracker = Shell.WindowTracker.get_default();
        if (notification.resident && this.app && tracker.focus_app == this.app)
            this.pushNotification(notification);
        else
            this.notify(notification);
    },

    _getApp(appId) {
        let app;

        app = Shell.WindowTracker.get_default().get_app_from_pid(this.pid);
        if (app != null)
            return app;

        if (appId) {
            app = Shell.AppSystem.get_default().lookup_app(appId + '.desktop');
            if (app != null)
                return app;
        }

        return null;
    },

    setTitle(title) {
        // Do nothing if .app is set, we don't want to override the
        // app name with whatever is provided through libnotify (usually
        // garbage)
        if (this.app)
            return;

        this.parent(title);
    },

    open() {
        this.openApp();
        this.destroyNonResidentNotifications();
    },

    openApp() {
        if (this.app == null)
            return;

        this.app.activate();
        Main.overview.hide();
        Main.panel.closeCalendar();
    },

    destroy() {
        if (this._nameWatcherId) {
            Gio.DBus.session.unwatch_name(this._nameWatcherId);
            this._nameWatcherId = 0;
        }

        this.parent();
    },

    createIcon(size) {
        if (this.app) {
            return this.app.create_icon_texture(size);
        } else if (this._gicon) {
            return new St.Icon({ gicon: this._gicon,
                                 icon_size: size });
        } else {
            return null;
        }
    }
});

const PRIORITY_URGENCY_MAP = {
    low: MessageTray.Urgency.LOW,
    normal: MessageTray.Urgency.NORMAL,
    high: MessageTray.Urgency.HIGH,
    urgent: MessageTray.Urgency.CRITICAL
};

var GtkNotificationDaemonNotification = new Lang.Class({
    Name: 'GtkNotificationDaemonNotification',
    Extends: MessageTray.Notification,

    _init(source, notification) {
        this.parent(source);
        this._serialized = GLib.Variant.new('a{sv}', notification);

        let { "title": title,
              "body": body,
              "icon": gicon,
              "urgent": urgent,
              "priority": priority,
              "buttons": buttons,
              "default-action": defaultAction,
              "default-action-target": defaultActionTarget,
              "timestamp": time } = notification;

        if (priority) {
            let urgency = PRIORITY_URGENCY_MAP[priority.unpack()];
            this.setUrgency(urgency != undefined ? urgency : MessageTray.Urgency.NORMAL);
        } else if (urgent) {
            this.setUrgency(urgent.unpack() ? MessageTray.Urgency.CRITICAL
                            : MessageTray.Urgency.NORMAL);
        } else {
            this.setUrgency(MessageTray.Urgency.NORMAL);
        }

        if (buttons) {
            buttons.deep_unpack().forEach(button => {
                this.addAction(button.label.unpack(), () => {
                    this._onButtonClicked(button);
                });
            });
        }

        this._defaultAction = defaultAction ? defaultAction.unpack() : null;
        this._defaultActionTarget = defaultActionTarget;

        this.update(title.unpack(), body ? body.unpack() : null,
                    { gicon: gicon ? Gio.icon_deserialize(gicon) : null,
                      datetime : time ? GLib.DateTime.new_from_unix_local(time.unpack()) : null });
    },

    _activateAction(namespacedActionId, target) {
        if (namespacedActionId) {
            if (namespacedActionId.startsWith('app.')) {
                let actionId = namespacedActionId.slice('app.'.length);
                this.source.activateAction(actionId, target);
            }
        } else {
            this.source.open();
        }
    },

    _onButtonClicked(button) {
        let { 'action': action, 'target': actionTarget } = button;
        this._activateAction(action.unpack(), actionTarget);
    },

    activate() {
        this._activateAction(this._defaultAction, this._defaultActionTarget);
        this.parent();
    },

    serialize() {
        return this._serialized;
    },
});

const FdoApplicationIface = loadInterfaceXML('org.freedesktop.Application');
const FdoApplicationProxy = Gio.DBusProxy.makeProxyWrapper(FdoApplicationIface);

function objectPathFromAppId(appId) {
    return '/' + appId.replace(/\./g, '/').replace(/-/g, '_');
}

function getPlatformData() {
    let startupId = GLib.Variant.new('s', '_TIME' + global.get_current_time());
    return { "desktop-startup-id": startupId };
}

function InvalidAppError() {}

var GtkNotificationDaemonAppSource = new Lang.Class({
    Name: 'GtkNotificationDaemonAppSource',
    Extends: MessageTray.Source,

    _init(appId) {
        this._appId = appId;
        this._objectPath = objectPathFromAppId(appId);
        if (!GLib.Variant.is_object_path(this._objectPath))
            throw new InvalidAppError();

        this._app = Shell.AppSystem.get_default().lookup_app(appId + '.desktop');
        if (!this._app)
            throw new InvalidAppError();

        this._notifications = {};
        this._notificationPending = false;

        this.parent(this._app.get_name());
    },

    createIcon(size) {
        return this._app.create_icon_texture(size);
    },

    _createPolicy() {
        return new MessageTray.NotificationApplicationPolicy(this._appId);
    },

    _createApp(callback) {
        return new FdoApplicationProxy(Gio.DBus.session, this._appId, this._objectPath, callback);
    },

    activateAction(actionId, target) {
        this._createApp((app, error) => {
            if (error == null)
                app.ActivateActionRemote(actionId, target ? [target] : [], getPlatformData());
            else
                logError(error, 'Failed to activate application proxy');
        });
        Main.overview.hide();
        Main.panel.closeCalendar();
    },

    open() {
        this._createApp((app, error) => {
            if (error == null)
                app.ActivateRemote(getPlatformData());
            else
                logError(error, 'Failed to open application proxy');
        });
        Main.overview.hide();
        Main.panel.closeCalendar();
    },

    addNotification(notificationId, notificationParams, showBanner) {
        this._notificationPending = true;

        if (this._notifications[notificationId])
            this._notifications[notificationId].destroy();

        let notification = new GtkNotificationDaemonNotification(this, notificationParams);
        notification.connect('destroy', () => {
            delete this._notifications[notificationId];
        });
        this._notifications[notificationId] = notification;

        if (showBanner)
            this.notify(notification);
        else
            this.pushNotification(notification);

        this._notificationPending = false;
    },

    destroy(reason) {
        if (this._notificationPending)
            return;
        this.parent(reason);
    },

    removeNotification(notificationId) {
        if (this._notifications[notificationId])
            this._notifications[notificationId].destroy(MessageTray.NotificationDestroyedReason.SOURCE_CLOSED);
    },

    serialize() {
        let notifications = [];
        for (let notificationId in this._notifications) {
            let notification = this._notifications[notificationId];
            notifications.push([notificationId, notification.serialize()]);
        }
        return [this._appId, notifications];
    },
});

const GtkNotificationsIface = loadInterfaceXML('org.gtk.Notifications');

var GtkNotificationDaemon = new Lang.Class({
    Name: 'GtkNotificationDaemon',

    _init() {
        this._sources = {};

        this._loadNotifications();

        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(GtkNotificationsIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gtk/Notifications');

        Gio.DBus.session.own_name('org.gtk.Notifications', Gio.BusNameOwnerFlags.REPLACE, null, null);
    },

    _ensureAppSource(appId) {
        if (this._sources[appId])
            return this._sources[appId];

        let source = new GtkNotificationDaemonAppSource(appId);

        source.connect('destroy', () => {
            delete this._sources[appId];
            this._saveNotifications();
        });
        source.connect('count-updated', this._saveNotifications.bind(this));
        Main.messageTray.add(source);
        this._sources[appId] = source;
        return source;
    },

    _loadNotifications() {
        this._isLoading = true;

        let value = global.get_persistent_state('a(sa(sv))', 'notifications');
        if (value) {
            let sources = value.deep_unpack();
            sources.forEach(([appId, notifications]) => {
                if (notifications.length == 0)
                    return;

                let source;
                try {
                    source = this._ensureAppSource(appId);
                } catch(e) {
                    if (e instanceof InvalidAppError)
                        return;
                    throw e;
                }

                notifications.forEach(([notificationId, notification]) => {
                    source.addNotification(notificationId, notification.deep_unpack(), false);
                });
            });
        }

        this._isLoading = false;
    },

    _saveNotifications() {
        if (this._isLoading)
            return;

        let sources = [];
        for (let appId in this._sources) {
            let source = this._sources[appId];
            sources.push(source.serialize());
        }

        global.set_persistent_state('notifications', new GLib.Variant('a(sa(sv))', sources));
    },

    AddNotificationAsync(params, invocation) {
        let [appId, notificationId, notification] = params;

        let source;
        try {
            source = this._ensureAppSource(appId);
        } catch(e) {
            if (e instanceof InvalidAppError) {
                invocation.return_dbus_error('org.gtk.Notifications.InvalidApp', 'The app by ID "%s" could not be found'.format(appId));
                return;
            }
            throw e;
        }

        let timestamp = GLib.DateTime.new_now_local().to_unix();
        notification['timestamp'] = new GLib.Variant('x', timestamp);

        source.addNotification(notificationId, notification, true);

        invocation.return_value(null);
    },

    RemoveNotificationAsync(params, invocation) {
        let [appId, notificationId] = params;
        let source = this._sources[appId];
        if (source)
            source.removeNotification(notificationId);

        invocation.return_value(null);
    },
});

var NotificationDaemon = new Lang.Class({
    Name: 'NotificationDaemon',

    _init() {
        this._fdoNotificationDaemon = new FdoNotificationDaemon();
        this._gtkNotificationDaemon = new GtkNotificationDaemon();
    },
});
