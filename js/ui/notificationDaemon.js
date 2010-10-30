/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const DBus = imports.dbus;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Mainloop = imports.mainloop;
const St = imports.gi.St;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const Params = imports.misc.params;

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
          replacement: '&lt;$1&gt;' },
        { pattern:     /^XChat: New public message from: (\S*) \((.*)\)$/,
          replacement: '$2 &lt;$1&gt;' },
        { pattern:     /^XChat: Highlighted message from: (\S*) \((.*)\)$/,
          replacement: '$2 &lt;$1&gt;' }
    ]
};

function NotificationDaemon() {
    this._init();
}

NotificationDaemon.prototype = {
    _init: function() {
        DBus.session.exportObject('/org/freedesktop/Notifications', this);

        this._everAcquiredName = false;
        DBus.session.acquire_name('org.freedesktop.Notifications',
                                  // We pass MANY_INSTANCES so that if
                                  // notification-daemon is running, we'll
                                  // get queued behind it and then get the
                                  // name after killing it below
                                  DBus.MANY_INSTANCES,
                                  Lang.bind(this, this._acquiredName),
                                  Lang.bind(this, this._lostName));

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

    _acquiredName: function() {
        this._everAcquiredName = true;
    },

    _lostName: function() {
        if (this._everAcquiredName)
            log('Lost name org.freedesktop.Notifications!');
        else if (GLib.getenv('GNOME_SHELL_NO_REPLACE'))
            log('Failed to acquire org.freedesktop.Notifications');
        else {
            log('Failed to acquire org.freedesktop.Notifications; trying again');

            // kill the notification-daemon. pkill is more portable
            // than killall, but on Linux at least it won't match if
            // you pass more than 15 characters of the process name...
            // However, if you use the '-f' flag to match the entire
            // command line, it will work, but we have to be careful
            // in that case that we don't match 'gedit
            // notification-daemon.c' or whatever...
            let p = new Shell.Process({ args: ['pkill', '-f',
                                               '^([^ ]*/)?(notification-daemon|notify-osd)$']});
            p.run();
        }
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
                return textureCache.load_icon_name(icon, St.IconType.FULLCOLOR, size);
        } else if (hints.icon_data) {
            let [width, height, rowStride, hasAlpha,
                 bitsPerSample, nChannels, data] = hints.icon_data;
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
            return textureCache.load_icon_name(stockIcon, St.IconType.FULLCOLOR, size);
        }
    },

    _newSource: function(title, pid) {
        let source = new Source(title, pid);
        this._sources[pid] = source;

        source.connect('destroy', Lang.bind(this,
            function() {
                delete this._sources[pid];
            }));

        Main.messageTray.add(source);
        return source;
    },

    Notify: function(appName, replacesId, icon, summary, body,
                     actions, hints, timeout) {
        let id;

        // Filter out notifications from Empathy, since we
        // handle that information from telepathyClient.js
        if (appName == 'Empathy') {
            // Ignore replacesId since we already sent back a
            // NotificationClosed for that id.
            id = nextNotificationId++;
            Mainloop.idle_add(Lang.bind(this,
                                        function () {
                                            this._emitNotificationClosed(id, NotificationClosedReason.DISMISSED);
                                        }));
            return id;
        }

        summary = GLib.markup_escape_text(summary, -1);

        let rewrites = rewriteRules[appName];
        if (rewrites) {
            for (let i = 0; i < rewrites.length; i++) {
                let rule = rewrites[i];
                if (summary.search(rule.pattern) != -1)
                    summary = summary.replace(rule.pattern, rule.replacement);
            }
        }

        hints = Params.parse(hints, { urgency: Urgency.NORMAL }, true);

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
        let source = pid ? this._sources[pid] : null;

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

                this._senderToPid[sender] = pid;
                source = this._sources[pid];

                if (!source)
                    source = this._newSource(appName, pid);
                source.connect('destroy', Lang.bind(this,
                    function() {
                        delete this._senderToPid[sender];
                    }));

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
            notification = new MessageTray.Notification(source, summary, body, { icon: iconActor });
            ndata.notification = notification;
            notification.connect('clicked', Lang.bind(this,
                function(n) {
                    this._emitNotificationClosed(id, NotificationClosedReason.DISMISSED);
                }));
            notification.connect('destroy', Lang.bind(this,
                function(n) {
                    delete this._notifications[id];
                }));
            notification.connect('action-invoked', Lang.bind(this, this._actionInvoked, source, id));
        } else {
            notification.update(summary, body, { icon: iconActor,
                                                 clear: true });
        }

        if (actions.length) {
            notification.setUseActionIcons(hints['action-icons'] == true);
            for (let i = 0; i < actions.length - 1; i += 2)
                notification.addButton(actions[i], actions[i + 1]);
        }

        notification.setUrgent(hints.urgency == Urgency.CRITICAL);

        let sourceIconActor = source.useNotificationIcon ? this._iconForNotificationData(icon, hints, source.ICON_SIZE) : null;
        source.notify(notification, sourceIconActor);
    },

    CloseNotification: function(id) {
        let ndata = this._notifications[id];
        if (ndata) {
            if (ndata.notification)
                ndata.notification.destroy();
            delete this._notifications[id];
        }
        this._emitNotificationClosed(id, NotificationClosedReason.APP_CLOSED);
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
            'GNOME Shell',
            'GNOME',
            '0.1', // FIXME, get this from somewhere
            '1.0'
        ];
    },

    _onFocusAppChanged: function() {
        let tracker = Shell.WindowTracker.get_default();
        if (!tracker.focus_app)
            return;

        for (let id in this._sources) {
            let source = this._sources[id];
            if (source.app == tracker.focus_app) {
                source.activated();
                return;
            }
        }
    },

    _actionInvoked: function(notification, action, source, id) {
        source.activated();
        this._emitActionInvoked(id, action);
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
        let source = this._sources[icon.pid];
        if (!source)
            source = this._newSource(icon.title || icon.wm_class || _("Unknown"), icon.pid);
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
        this._setApp();
        if (this.app)
            this.title = this.app.get_name();
        else
            this.useNotificationIcon = true;
        this._isTrayIcon = false;
    },

    notify: function(notification, icon) {
        if (!this.app)
            this._setApp();
        if (!this.app && icon)
            this._setSummaryIcon(icon);
        MessageTray.Source.prototype.notify.call(this, notification);
    },

    _setApp: function() {
        this.app = Shell.WindowTracker.get_default().get_app_from_pid(this._pid);
        if (!this.app)
            return;

        // Only override the icon if we were previously using
        // notification-based icons (ie, not a trayicon)
        if (this.useNotificationIcon) {
            this.useNotificationIcon = false;
            this._setSummaryIcon(this.app.create_icon_texture (this.ICON_SIZE));
        }
    },

    setTrayIcon: function(icon) {
        this._setSummaryIcon(icon);
        this.useNotificationIcon = false;
        this._isTrayIcon = true;
    },

    _notificationClicked: function(notification) {
        notification.destroy();
        this.openApp();
        this.activated();
    },

    activated: function() {
        if (!this._isTrayIcon)
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
    }
};
