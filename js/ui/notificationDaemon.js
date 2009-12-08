/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const DBus = imports.dbus;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Mainloop = imports.mainloop;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

let nextNotificationId = 1;

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

function NotificationDaemon() {
    this._init();
}

NotificationDaemon.prototype = {
    _init: function() {
        DBus.session.exportObject('/org/freedesktop/Notifications', this);

        let acquiredName = false;
        DBus.session.acquire_name('org.freedesktop.Notifications', DBus.SINGLE_INSTANCE,
            function(name) {
                log("Acquired name " + name);
                acquiredName = true;
            },
            function(name) {
                if (acquiredName)
                    log("Lost name " + name);
                else
                    log("Could not get name " + name);
            });
    },

    _sourceId: function(id) {
        return 'notification-' + id;
    },

    Notify: function(appName, replacesId, icon, summary, body,
                     actions, hints, timeout) {
        let id, source = null;

        if (replacesId != 0) {
            id = replacesId;
            source = Main.messageTray.getSource(this._sourceId(id));
            // source may be null if the current source was destroyed
            // right as the client sent the new notification
        }

        if (source == null) {
            id = nextNotificationId++;

            source = new MessageTray.Source(this._sourceId(id), Lang.bind(this,
                function (size) {
                    if (icon != '')
                        return Shell.TextureCache.get_default().load_icon_name(icon, size);
                    else {
                        // FIXME: load icon data from hints
                        // FIXME: better fallback icon
                        return Shell.TextureCache.get_default().load_icon_name('gtk-dialog-info', size);
                    }
                }));
            Main.messageTray.add(source);

            source.connect('clicked', Lang.bind(this,
                function() {
                    source.destroy();
                    this._emitNotificationClosed(id, NotificationClosedReason.DISMISSED);
                }));
        }

        source.notify(summary);
        return id;
    },

    CloseNotification: function(id) {
        let source = Main.messageTray.getSource(this._sourceId(id));
        if (source)
            source.destroy();
        this._emitNotificationClosed(id, NotificationClosedReason.APP_CLOSED);
    },

    GetCapabilities: function() {
        return [
            // 'actions',
            'body',
            // 'body-hyperlinks',
            // 'body-images',
            // 'body-markup',
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

    _emitNotificationClosed: function(id, reason) {
        DBus.session.emit_signal('/org/freedesktop/Notifications',
                                 'org.freedesktop.Notifications',
                                 'NotificationClosed', 'uu',
                                 [id, reason]);
    }
};

DBus.conformExport(NotificationDaemon.prototype, NotificationDaemonIface);

