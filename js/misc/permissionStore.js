// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported PermissionStore */

const Gio = imports.gi.Gio;

const { loadInterfaceXML } = imports.misc.fileUtils;

const PermissionStoreIface = loadInterfaceXML('org.freedesktop.impl.portal.PermissionStore');
const PermissionStoreProxy = Gio.DBusProxy.makeProxyWrapper(PermissionStoreIface);

function PermissionStore(initCallback, cancellable) {
    return new PermissionStoreProxy(Gio.DBus.session,
                                    'org.freedesktop.impl.portal.PermissionStore',
                                    '/org/freedesktop/impl/portal/PermissionStore',
                                    initCallback, cancellable);
}
