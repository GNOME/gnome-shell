// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported PermissionStore */

import Gio from 'gi://Gio';

import { loadInterfaceXML } from "./fileUtilsModule.js";

const PermissionStoreIface = loadInterfaceXML('org.freedesktop.impl.portal.PermissionStore');
const PermissionStoreProxy = Gio.DBusProxy.makeProxyWrapper(PermissionStoreIface);

export function PermissionStore(initCallback, cancellable) {
    return PermissionStoreProxy(Gio.DBus.session,
                                'org.freedesktop.impl.portal.PermissionStore',
                                '/org/freedesktop/impl/portal/PermissionStore',
                                initCallback, cancellable);
}
