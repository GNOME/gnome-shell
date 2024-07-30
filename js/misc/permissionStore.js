import Gio from 'gi://Gio';

import {loadInterfaceXML} from './fileUtils.js';

const PermissionStoreIface = loadInterfaceXML('org.freedesktop.impl.portal.PermissionStore');
const PermissionStoreProxy = Gio.DBusProxy.makeProxyWrapper(PermissionStoreIface);

/**
 * @param {Function} initCallback
 * @param {Gio.Cancellable} cancellable
 * @returns {Gio.DBusProxy}
 */
export function PermissionStore(initCallback, cancellable) {
    return new PermissionStoreProxy(Gio.DBus.session,
        'org.freedesktop.impl.portal.PermissionStore',
        '/org/freedesktop/impl/portal/PermissionStore',
        initCallback, cancellable);
}
