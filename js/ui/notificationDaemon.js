/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const DBus = imports.dbus;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Mainloop = imports.mainloop;
const St = imports.gi.St;

const Config = imports.misc.config;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const Params = imports.misc.params;
const Util = imports.misc.util;

let nextNotificationId = 1;

// Should really be defined in dbus.js
const BusIface = {
    name: 'org.freedesktop.DBus',
    methods: [{ name: 'GetConnectionUnixProcessID',
                inSignature: 's',
                outSignature: 'i' }]
};

const Bus = function () {
    this._init();
};

Bus.prototype = {
     _init: function() {
         DBus.session.proxifyObject(this, 'org.freedesktop.DBus', '/org/freedesktop/DBus');
     }
};

DBus.proxifyPrototype(Bus.prototype, BusIface);

const NotificationDaemonIface = {
    name: 'org.freedesktop.Notifications',
    methods: [{ name: 'Notify',
                inSignature: 'susssasa{sv}i',
                outSignature: 'u'
              },
              { name: 'CloseNotification',
                inSignature: 'u',
                outSignature: ''
              },
              { name: 'GetCapabilities',
                inSignature: '',
                outSignature: 'as'
              },
              { name: 'GetServerInformation',
                inSignature: '',
                outSignature: 'ssss'
              }],
    signals: [{ name: 'NotificationClosed',
                inSignature: 'uu' },
              { name: 'ActionInvoked',
                inSignature: 'us' }]
};

const NotificationClosedReason = {
    EXPIRED: 1,
    DISMISSED: 2,
    APP_CLOSED: 3,
    UNDEFINED: 4
};

const Urgency = {
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

function NotificationDaemon() {
    this._init();
}

NotificationDaemon.prototype = {
    _init: function() {
        DBus.session.exportObject('/org/freedesktop/Notifications', this);

        this._sources = {};
        this._senderToPid = {};
        this._notifications = {};
        this._busProxy = new Bus();

        Main.statusIconDispatcher.connect('message-icon-added', Lang.bind(this, this._onTrayIconAdded));
        Main.statusIconDispatcher.connect('message-icon-removed', Lang.bind(this, this._onTrayIconRemoved));

        Shell.WindowTracker.get_default().connect('notify::focus-app',
            Lang.bind(this, this._onFocusAppChanged));
        Main.overview.connect('hidden',
            Lang.bind(this, this._onFocusAppChanged));
    },

    _iconForNotificationData: function(icon, hints, size) {
        let textureCache = St.TextureCache.get_default();

        if (icon) {
            if (icon.substr(0, 7) == 'file://')
                return textureCache.load_uri_async(icon, size, size);
            else if (icon[0] == '/') {
                let uri = GLib.filename_to_uri(icon, null);
                return textureCache.load_uri_async(uri, size, size);
            } else
                return new St.Icon({ icon_name: icon,
                                     icon_type: St.IconType.FULLCOLOR,
                                     icon_size: size });
        } else if (hints['image-data']) {
            let [width, height, rowStride, hasAlpha,
                 bitsPerSample, nChannels, data] = hints['image-data'];
            return textureCache.load_from_raw(data, data.length, hasAlpha,
                                              width, height, rowStride, size);
        } else {
            let stockIcon;
            switch (hints.urgency) {
                case Urgency.LOW:
                case Urgency.NORMAL:
                    stockIcon = 'gtk-dialog-info';
                    break;
                case Urgency.CRITICAL:
                    stockIcon = 'gtk-dialog-error';
                    break;
            }
            return new St.Icon({ icon_name: stockIcon,
                                 icon_type: St.IconType.FULLCOLOR,
                                 icon_size: size });
        }
    },

    // Returns the source associated with ndata.notification if it is set.
    // Otherwise, returns the source associated with the pid if one is
    // stored in this._sources and the notification is not transient.
    // Otherwise, creates a new source as long as pid is provided.
    //
    // Either a pid or ndata.notification is needed to retrieve or
    // create a source.
    _getSource: function(title, pid, ndata) {
        if (!pid && !(ndata && ndata.notification))
            return null;

        // We use notification's source for the notifications we still have
        // around that are getting replaced because we don't keep sources
        // for transient notifications in this._sources, but we still want
        // the notification associated with them to get replaced correctly.
        if (ndata && ndata.notification)
            return ndata.notification.source;

        let isForTransientNotification = (ndata && ndata.hints['transient'] == true);

        // We don't want to override a persistent notification
        // with a transient one from the same sender, so we
        // always create a new source object for new transient notifications
        // and never add it to this._sources .
        if (!isForTransientNotification && this._sources[pid]) {
            let source = this._sources[pid];
            source.setTitle(title);
            return source;
        }

        let source = new Source(title, pid);
        source.setTransient(isForTransientNotification);

        if (!isForTransientNotification) {
            this._sources[pid] = source;
            source.connect('destroy', Lang.bind(this,
                function() {
                    delete this._sources[pid];
                }));
        }

        Main.messageTray.add(source);
        return source;
    },

    Notify: function(appName, replacesId, icon, summary, body,
                     actions, hints, timeout) {
        let id;

        // Filter out chat and presence notifications from Empathy, since we
        // handle that information from telepathyClient.js
        if (appName == 'Empathy' && (hints['category'] == 'im.received' ||
              hints['category'] == 'presence.online' ||
              hints['category'] == 'presence.offline')) {
            // Ignore replacesId since we already sent back a
            // NotificationClosed for that id.
            id = nextNotificationId++;
            Mainloop.idle_add(Lang.bind(this,
                                        function () {
                                            this._emitNotificationClosed(id, NotificationClosedReason.DISMISSED);
                                        }));
            return id;
        }

        let rewrites = rewriteRules[appName];
        if (rewrites) {
            for (let i = 0; i < rewrites.length; i++) {
                let rule = rewrites[i];
                if (summary.search(rule.pattern) != -1)
                    summary = summary.replace(rule.pattern, rule.replacement);
            }
        }

        hints = Params.parse(hints, { urgency: Urgency.NORMAL }, true);

        // Be compatible with the various hints for image data
        // 'image-data' is the latest name of this hint, introduced in 1.2
        if (!hints['image-data']) {
            if (hints['image_data'])
                hints['image-data'] = hints['image_data']; // version 1.1 of the spec
            else if (hints['icon_data'])
                hints['image-data'] = hints['icon_data']; // previous versions of the spec
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
            ndata.id = id = nextNotificationId++;
        }
        this._notifications[id] = ndata;

        let sender = DBus.getCurrentMessageContext().sender;
        let pid = this._senderToPid[sender];

        let source = this._getSource(appName, pid, ndata);

        if (source) {
            this._notifyForSource(source, ndata);
            return id;
        }

        if (replacesId) {
            // There's already a pending call to GetConnectionUnixProcessID,
            // which will see the new notification data when it finishes,
            // so we don't have to do anything.
            return id;
        }

        this._busProxy.GetConnectionUnixProcessIDRemote(sender, Lang.bind(this,
            function (pid, ex) {
                // The app may have updated or removed the notification
                ndata = this._notifications[id];
                if (!ndata)
                    return;

                source = this._getSource(appName, pid, ndata);

                // We only store sender-pid entries for persistent sources.
                // Removing the entries once the source is destroyed
                // would result in the entries associated with transient
                // sources removed once the notification is shown anyway.
                // However, keeping these pairs would mean that we would
                // possibly remove an entry associated with a persistent
                // source when a transient source for the same sender is
                // distroyed.
                if (!source.isTransient) {
                    this._senderToPid[sender] = pid;
                    source.connect('destroy', Lang.bind(this,
                        function() {
                            delete this._senderToPid[sender];
                        }));
                }
                this._notifyForSource(source, ndata);
            }));

        return id;
    },

    _notifyForSource: function(source, ndata) {
        let [id, icon, summary, body, actions, hints, notification] =
            [ndata.id, ndata.icon, ndata.summary, ndata.body,
             ndata.actions, ndata.hints, ndata.notification];

        let iconActor = this._iconForNotificationData(icon, hints, source.ICON_SIZE);

        if (notification == null) {
            notification = new MessageTray.Notification(source, summary, body,
                                                        { icon: iconActor,
                                                          bannerMarkup: true });
            ndata.notification = notification;
            notification.connect('destroy', Lang.bind(this,
                function(n, reason) {
                    delete this._notifications[id];
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
                    this._emitNotificationClosed(id, notificationClosedReason);
                }));
            notification.connect('action-invoked', Lang.bind(this,
                function(n, actionId) {
                    this._emitActionInvoked(id, actionId);
                }));
        } else {
            notification.update(summary, body, { icon: iconActor,
                                                 bannerMarkup: true,
                                                 clear: true });
        }

        if (actions.length) {
            notification.setUseActionIcons(hints['action-icons'] == true);
            for (let i = 0; i < actions.length - 1; i += 2)
                notification.addButton(actions[i], actions[i + 1]);
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
        notification.setResident(hints.resident == true);
        // 'transient' is a reserved keyword in JS, so we have to retrieve the value
        // of the 'transient' hint with hints['transient'] rather than hints.transient
        notification.setTransient(hints['transient'] == true);

        let sourceIconActor = source.useNotificationIcon ? this._iconForNotificationData(icon, hints, source.ICON_SIZE) : null;
        source.processNotification(notification, sourceIconActor);
    },

    CloseNotification: function(id) {
        let ndata = this._notifications[id];
        if (ndata) {
            if (ndata.notification)
                ndata.notification.destroy(MessageTray.NotificationDestroyedReason.SOURCE_CLOSED);
            delete this._notifications[id];
        }
    },

    GetCapabilities: function() {
        return [
            'actions',
            'action-icons',
            'body',
            // 'body-hyperlinks',
            // 'body-images',
            'body-markup',
            // 'icon-multi',
            'icon-static',
            'persistence',
            // 'sound',
        ];
    },

    GetServerInformation: function() {
        return [
            Config.PACKAGE_NAME,
            'GNOME',
            Config.PACKAGE_VERSION,
            '1.2'
        ];
    },

    _onFocusAppChanged: function() {
        let tracker = Shell.WindowTracker.get_default();
        if (!tracker.focus_app)
            return;

        for (let id in this._sources) {
            let source = this._sources[id];
            if (source.app == tracker.focus_app) {
                source.destroyNonResidentNotifications();
                return;
            }
        }
    },

    _emitNotificationClosed: function(id, reason) {
        DBus.session.emit_signal('/org/freedesktop/Notifications',
                                 'org.freedesktop.Notifications',
                                 'NotificationClosed', 'uu',
                                 [id, reason]);
    },

    _emitActionInvoked: function(id, action) {
        DBus.session.emit_signal('/org/freedesktop/Notifications',
                                 'org.freedesktop.Notifications',
                                 'ActionInvoked', 'us',
                                 [id, action]);
    },

    _onTrayIconAdded: function(o, icon) {
        let source = this._getSource(icon.title || icon.wm_class || _("Unknown"), icon.pid, null);
        source.setTrayIcon(icon);
    },

    _onTrayIconRemoved: function(o, icon) {
        let source = this._sources[icon.pid];
        if (source)
            source.destroy();
    }
};

DBus.conformExport(NotificationDaemon.prototype, NotificationDaemonIface);

function Source(title, pid) {
    this._init(title, pid);
}

Source.prototype = {
    __proto__:  MessageTray.Source.prototype,

    _init: function(title, pid) {
        MessageTray.Source.prototype._init.call(this, title);

        this._pid = pid;
        this._appStateChangedId = 0;
        this._setApp();
        if (this.app)
            this.title = this.app.get_name();
        else
            this.useNotificationIcon = true;
        this._trayIcon = null;
    },

    processNotification: function(notification, icon) {
        if (!this.app)
            this._setApp();
        if (!this.app && icon)
            this._setSummaryIcon(icon);

        let tracker = Shell.WindowTracker.get_default();
        if (notification.resident && this.app && tracker.focus_app == this.app)
            this.pushNotification(notification);
        else
            this.notify(notification);
    },

    handleSummaryClick: function() {
        if (!this._trayIcon)
            return false;

        let event = Clutter.get_current_event();
        if (event.type() != Clutter.EventType.BUTTON_RELEASE)
            return false;

        // Left clicks are passed through only where there aren't unacknowledged
        // notifications, so it possible to open them in summary mode; right
        // clicks are always forwarded, as the right click menu is not useful for
        // tray icons
        if (event.get_button() == 1 &&
            this.notifications.length > 0)
            return false;

        if (Main.overview.visible) {
            // We can't just connect to Main.overview's 'hidden' signal,
            // because it's emitted *before* it calls popModal()...
            let id = global.connect('notify::stage-input-mode', Lang.bind(this,
                function () {
                    global.disconnect(id);
                    this._trayIcon.click(event);
                }));
            Main.overview.hide();
        } else {
            this._trayIcon.click(event);
        }
        return true;
    },

    _setApp: function() {
        if (this.app)
            return;

        this.app = Shell.WindowTracker.get_default().get_app_from_pid(this._pid);
        if (!this.app)
            return;

        // We only update the app if this.app is null, so we can't disconnect the old this._appStateChangedId
        // even if it were non-zero for some reason.
        this._appStateChangedId = this.app.connect('notify::state', Lang.bind(this,  this._appStateChanged));

        // Only override the icon if we were previously using
        // notification-based icons (ie, not a trayicon) or if it was unset before
        if (!this._trayIcon) {
            this.useNotificationIcon = false;
            this._setSummaryIcon(this.app.create_icon_texture (this.ICON_SIZE));
        }
    },

    setTrayIcon: function(icon) {
        this._setSummaryIcon(icon);
        this.useNotificationIcon = false;
        this._trayIcon = icon;
    },

    open: function(notification) {
        this.destroyNonResidentNotifications();
        this.openApp();
    },

    _lastNotificationRemoved: function() {
        if (!this._trayIcon)
            this.destroy();
    },

    _appStateChanged: function() {
        // Destroy notification sources when their apps exit.
        // The app exiting would normally result in a tray icon being removed,
        // so the associated source would be destroyed through the code path
        // that handles the tray icon being removed. We should not destroy
        // the source associated with a tray icon when the application state
        // is Shell.AppState.STOPPED because running applications that have
        // no open windows would also have that state. This is often the case
        // for applications that use tray icons.
        if (!this._trayIcon && this.app.get_state() == Shell.AppState.STOPPED)
            this.destroy();
    },

    openApp: function() {
        if (this.app == null)
            return;

        let windows = this.app.get_windows();
        if (windows.length > 0) {
            let mostRecentWindow = windows[0];
            Main.activateWindow(mostRecentWindow);
        }
    },

    destroy: function() {
        if (this.app && this._appStateChangedId) {
            this.app.disconnect(this._appStateChangedId);
            this._appStateChangedId = 0;
        }
        MessageTray.Source.prototype.destroy.call(this);
    }
};
