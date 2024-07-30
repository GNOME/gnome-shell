import Gio from 'gi://Gio';

import {loadInterfaceXML} from './fileUtils.js';

const PresenceIface = loadInterfaceXML('org.gnome.SessionManager.Presence');

/** @enum {number} */
export const PresenceStatus = {
    AVAILABLE: 0,
    INVISIBLE: 1,
    BUSY: 2,
    IDLE: 3,
};

const PresenceProxy = Gio.DBusProxy.makeProxyWrapper(PresenceIface);

/**
 * @param {Function} initCallback
 * @param {Gio.Cancellable} cancellable
 * @returns {Gio.DBusProxy}
 */
export function Presence(initCallback, cancellable) {
    return new PresenceProxy(Gio.DBus.session,
        'org.gnome.SessionManager',
        '/org/gnome/SessionManager/Presence',
        initCallback, cancellable);
}

// Note inhibitors are immutable objects, so they don't
// change at runtime (changes always come in the form
// of new inhibitors)
const InhibitorIface = loadInterfaceXML('org.gnome.SessionManager.Inhibitor');
const InhibitorProxy = Gio.DBusProxy.makeProxyWrapper(InhibitorIface);

/**
 * @param {string} objectPath
 * @param {Function} initCallback
 * @param {Gio.Cancellable} cancellable
 * @returns {Gio.DBusProxy}
 */
export function Inhibitor(objectPath, initCallback, cancellable) {
    return new InhibitorProxy(Gio.DBus.session, 'org.gnome.SessionManager', objectPath, initCallback, cancellable);
}

// Not the full interface, only the methods we use
const SessionManagerIface = loadInterfaceXML('org.gnome.SessionManager');
const SessionManagerProxy = Gio.DBusProxy.makeProxyWrapper(SessionManagerIface);

/**
 * @param {Function} initCallback
 * @param {Gio.Cancellable} cancellable
 * @returns {Gio.DBusProxy}
 */
export function SessionManager(initCallback, cancellable) {
    return new SessionManagerProxy(Gio.DBus.session, 'org.gnome.SessionManager', '/org/gnome/SessionManager', initCallback, cancellable);
}

export const InhibitFlags = {
    LOGOUT: 1 << 0,
    SWITCH: 1 << 1,
    SUSPEND: 1 << 2,
    IDLE: 1 << 3,
    AUTOMOUNT: 1 << 4,
};
