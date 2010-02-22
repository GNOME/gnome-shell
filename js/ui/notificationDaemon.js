/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const DBus = imports.dbus;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Mainloop = imports.mainloop;

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
}

const Bus = function () {
    this._init();
}

Bus.prototype = {
     _init: function() {
         DBus.session.proxifyObject(this, 'org.freedesktop.DBus', '/org/freedesktop/DBus');
     }
}

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

        this._currentNotifications = {};
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
            // However, if you use the "-f" flag to match the entire
            // command line, it will work, but we have to be careful
            // in that case that we don't match "gedit
            // notification-daemon.c" or whatever...
            let p = new Shell.Process({ args: ['pkill', '-f',
                                               '^([^ ]*/)?(notification-daemon|notify-osd)$']});
            p.run();
        }
    },

    _sourceId: function(id) {
        return 'source-' + id;
    },

    Notify: function(appName, replacesId, icon, summary, body,
                     actions, hints, timeout) {
        let source = Main.messageTray.getSource(this._sourceId(appName));
        let id = null;

        // Source may be null if we have never received a notification from
        // this app or if all notifications from this app have been acknowledged.
        if (source == null) {
            source = new Source(this._sourceId(appName), icon, hints);
            Main.messageTray.add(source);

            source.connect('clicked', Lang.bind(this,
                function() {
                    source.destroy();
                }));

            let sender = DBus.getCurrentMessageContext().sender;
            let busProxy = new Bus();
            busProxy.GetConnectionUnixProcessIDRemote(sender, function (result, excp) {
                let app = Shell.WindowTracker.get_default().get_app_from_pid(result);
                source.setApp(app);
            });
        } else {
            source.update(icon, hints);
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

        let notification;
        if (replacesId != 0) {
            id = replacesId;
            notification = this._currentNotifications[id];
        }

        if (notification == null) {
            id = nextNotificationId++;
            notification = new MessageTray.Notification(id, source, summary, body, true);
            this._currentNotifications[id] = notification;
            notification.connect('dismissed', Lang.bind(this,
                function(n) {
                    n.destroy();
                    this._emitNotificationClosed(n.id, NotificationClosedReason.DISMISSED);
                }));
        } else {
            // passing in true as the last parameter will clear out extra actors,
            // such as actions
            notification.update(summary, body, true);
        }

        if (actions.length) {
            for (let i = 0; i < actions.length - 1; i += 2)
                notification.addAction(actions[i], actions[i + 1]);
            notification.connect('action-invoked', Lang.bind(this, this._actionInvoked, source, id));
        }

        source.notify(notification);
        return id;
    },

    CloseNotification: function(id) {
        let notification = this._currentNotifications[id];
        if (notification)
            notification.destroy();
        this._emitNotificationClosed(id, NotificationClosedReason.APP_CLOSED);
    },

    GetCapabilities: function() {
        return [
            'actions',
            'body',
            // 'body-hyperlinks',
            // 'body-images',
            'body-markup',
            // 'icon-multi',
            'icon-static'
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

    _actionInvoked: function(notification, action, source, id) {
        this._emitActionInvoked(id, action);
        source.destroy();
    },

    _emitNotificationClosed: function(id, reason) {
        delete this._currentNotifications[id];
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
    }
};

DBus.conformExport(NotificationDaemon.prototype, NotificationDaemonIface);

function Source(sourceId, icon, hints) {
    this._init(sourceId, icon, hints);
}

Source.prototype = {
    __proto__:  MessageTray.Source.prototype,

    _init: function(sourceId, icon, hints) {
        MessageTray.Source.prototype._init.call(this, sourceId);

        this.update(icon, hints);
    },

    update: function(icon, hints) {
        hints = Params.parse(hints, { urgency: Urgency.NORMAL }, true);

        this._icon = icon;
        this._iconData = hints.icon_data;
        this._urgency = hints.urgency;

        this.app = null;
        this._openAppRequested = false;
    },

    createIcon: function(size) {
        let textureCache = Shell.TextureCache.get_default();

        if (this._icon) {
            if (this._icon.substr(0, 7) == 'file://')
                return textureCache.load_uri_async(this._icon, size, size);
            else if (this._icon[0] == '/') {
                let uri = GLib.filename_to_uri(this._icon, null);
                return textureCache.load_uri_async(uri, size, size);
            } else
                return textureCache.load_icon_name(this._icon, size);
        } else if (this._iconData) {
            let [width, height, rowStride, hasAlpha,
                 bitsPerSample, nChannels, data] = this._iconData;
            return textureCache.load_from_raw(data, data.length, hasAlpha,
                                              width, height, rowStride, size);
        } else {
            let stockIcon;
            switch (this._urgency) {
                case Urgency.LOW:
                case Urgency.NORMAL:
                    stockIcon = 'gtk-dialog-info';
                    break;
                case Urgency.CRITICAL:
                    stockIcon = 'gtk-dialog-error';
                    break;
            }
            return textureCache.load_icon_name(stockIcon, size);
        }
    },

    clicked: function() {
        this.openApp();
        MessageTray.Source.prototype.clicked.call(this);
    },

    setApp: function(app) {
        this.app = app;
        if (this._openAppRequested)
            this.openApp();
    },

    openApp: function() {
        if (this.app == null) {
            this._openAppRequested = true;
            return;
        }
        let windows = this.app.get_windows();
        if (windows.length > 0) {
            let mostRecentWindow = windows[0];
            Main.activateWindow(mostRecentWindow);
        }
        this._openAppRequested = false;
    }
};
