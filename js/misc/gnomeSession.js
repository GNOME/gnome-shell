// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported PresenceStatus, Presence, Inhibitor, SessionManager, InhibitFlags */

const Gio = imports.gi.Gio;

const { loadInterfaceXML } = imports.misc.fileUtils;

const PresenceIface = loadInterfaceXML('org.gnome.SessionManager.Presence');

var PresenceStatus = {
    AVAILABLE: 0,
    INVISIBLE: 1,
    BUSY: 2,
    IDLE: 3,
};

var PresenceProxy = Gio.DBusProxy.makeProxyWrapper(PresenceIface);
function Presence(initCallback, cancellable) {
    return new PresenceProxy(Gio.DBus.session, 'org.gnome.SessionManager',
                             '/org/gnome/SessionManager/Presence', initCallback, cancellable);
}

// Note inhibitors are immutable objects, so they don't
// change at runtime (changes always come in the form
// of new inhibitors)
const InhibitorIface = loadInterfaceXML('org.gnome.SessionManager.Inhibitor');
var InhibitorProxy = Gio.DBusProxy.makeProxyWrapper(InhibitorIface);
function Inhibitor(objectPath, initCallback, cancellable) {
    return new InhibitorProxy(Gio.DBus.session, 'org.gnome.SessionManager', objectPath, initCallback, cancellable);
}

// Not the full interface, only the methods we use
const SessionManagerIface = loadInterfaceXML('org.gnome.SessionManager');
var SessionManagerProxy = Gio.DBusProxy.makeProxyWrapper(SessionManagerIface);
function SessionManager(initCallback, cancellable) {
    return new SessionManagerProxy(Gio.DBus.session, 'org.gnome.SessionManager', '/org/gnome/SessionManager', initCallback, cancellable);
}

var InhibitFlags = {
    LOGOUT: 1 << 0,
    SWITCH: 1 << 1,
    SUSPEND: 1 << 2,
    IDLE: 1 << 3,
    AUTOMOUNT: 1 << 4,
};
