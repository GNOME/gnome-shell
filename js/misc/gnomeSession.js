/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const DBus = imports.dbus;
const Lang = imports.lang;

const PresenceIface = {
    name: 'org.gnome.SessionManager.Presence',
    methods: [{ name: 'SetStatus',
                inSignature: 'u' }],
    properties: [{ name: 'status',
                   signature: 'u',
                   access: 'readwrite' }],
    signals: [{ name: 'StatusChanged',
                inSignature: 'u' }]
};

const PresenceStatus = {
    AVAILABLE: 0,
    INVISIBLE: 1,
    BUSY: 2,
    IDLE: 3
};

function Presence() {
    this._init();
}

Presence.prototype = {
    _init: function() {
        DBus.session.proxifyObject(this, 'org.gnome.SessionManager', '/org/gnome/SessionManager/Presence', this);
    },

    getStatus: function(callback) {
        this.GetRemote('status', Lang.bind(this,
            function(status, ex) {
                if (!ex)
                    callback(this, status);
            }));
    },

    setStatus: function(status) {
        this.SetStatusRemote(status);
    }
};
DBus.proxifyPrototype(Presence.prototype, PresenceIface);
