// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Signals = imports.signals;

const PresenceIface = '<node> \
<interface name="org.gnome.SessionManager.Presence"> \
<method name="SetStatus"> \
    <arg type="u" direction="in"/> \
</method> \
<property name="status" type="u" access="readwrite"/> \
<signal name="StatusChanged"> \
    <arg type="u" direction="out"/> \
</signal> \
</interface> \
</node>';

const PresenceStatus = {
    AVAILABLE: 0,
    INVISIBLE: 1,
    BUSY: 2,
    IDLE: 3
};

var PresenceProxy = Gio.DBusProxy.makeProxyWrapper(PresenceIface);
function Presence(initCallback, cancellable) {
    return new PresenceProxy(Gio.DBus.session, 'org.gnome.SessionManager',
                             '/org/gnome/SessionManager/Presence', initCallback, cancellable);
}

// Note inhibitors are immutable objects, so they don't
// change at runtime (changes always come in the form
// of new inhibitors)
const InhibitorIface = '<node> \
<interface name="org.gnome.SessionManager.Inhibitor"> \
<method name="GetAppId"> \
    <arg type="s" direction="out" /> \
</method> \
<method name="GetReason"> \
    <arg type="s" direction="out" /> \
</method> \
</interface> \
</node>';

var InhibitorProxy = Gio.DBusProxy.makeProxyWrapper(InhibitorIface);
function Inhibitor(objectPath, initCallback, cancellable) {
    return new InhibitorProxy(Gio.DBus.session, 'org.gnome.SessionManager', objectPath, initCallback, cancellable);
}

// Not the full interface, only the methods we use
const SessionManagerIface = '<node> \
<interface name="org.gnome.SessionManager"> \
<method name="Logout"> \
    <arg type="u" direction="in" /> \
</method> \
<method name="Shutdown" /> \
<method name="Reboot" /> \
<method name="CanShutdown"> \
    <arg type="b" direction="out" /> \
</method> \
<method name="IsInhibited"> \
    <arg type="u" direction="in" /> \
    <arg type="b" direction="out" /> \
</method> \
<property name="SessionIsActive" type="b" access="read"/> \
<signal name="InhibitorAdded"> \
    <arg type="o" direction="out"/> \
</signal> \
<signal name="InhibitorRemoved"> \
    <arg type="o" direction="out"/> \
</signal> \
</interface> \
</node>';

var SessionManagerProxy = Gio.DBusProxy.makeProxyWrapper(SessionManagerIface);
function SessionManager(initCallback, cancellable) {
    return new SessionManagerProxy(Gio.DBus.session, 'org.gnome.SessionManager', '/org/gnome/SessionManager', initCallback, cancellable);
}
