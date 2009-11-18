/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const DBus = imports.dbus;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Mainloop = imports.mainloop;

const Main = imports.ui.main;

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
              }]
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

    Notify: function(appName, replacesId, icon, summary, body,
                     actions, hints, timeout) {
        let iconActor = null;

        if (icon != '')
            iconActor = Shell.TextureCache.get_default().load_icon_name(icon, 24);
        else {
            // FIXME: load icon data from hints
        }            

        Main.notificationPopup.show(iconActor, summary);
    },

    CloseNotification: function(id) {
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
    }
};

DBus.conformExport(NotificationDaemon.prototype, NotificationDaemonIface);

