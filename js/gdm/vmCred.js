// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported getVmwareCredentialsManager */

const Gio = imports.gi.Gio;
const Signals = imports.signals;
const GLib = imports.gi.GLib;

const dbusPath = '/org/vmware/viewagent/Credentials';
const dbusInterface = 'org.vmware.viewagent.Credentials';

const VmwareCredentialsIface = '<node> \
<interface name="' + dbusInterface + '"> \
<signal name="UserAuthenticated"> \
    <arg type="s" name="token"/> \
</signal> \
</interface> \
</node>';


const VmwareCredentialsInfo = Gio.DBusInterfaceInfo.new_for_xml(VmwareCredentialsIface);

let _vmwareCredentialsManager = null;

function VmwareCredentials() {
    var self = new Gio.DBusProxy({ g_connection: Gio.DBus.session,
                                   g_interface_name: VmwareCredentialsInfo.name,
                                   g_interface_info: VmwareCredentialsInfo,
                                   g_name: dbusInterface,
                                   g_object_path: dbusPath,
                                   g_flags: Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES });
    self.init(null);
    return self;
}

var VmwareCredentialsManager = class {
    constructor() {
        this._token = null;

        this._credentials = new VmwareCredentials();
        this._credentials.connectSignal('UserAuthenticated',
            this._onUserAuthenticated.bind(this));

    }

    _onUserAuthenticated(proxy, sender, [token]) {
        log("VMwareSSO: userAuthenticated get token.");

        this._token = token;
        this.emit('vm-authenticated', token);
    }

    hasToken() {
        return this._token != null;
    }

    getToken() {
        return this._token;
    }

    resetToken() {
        this._token = null;
    }
};

Signals.addSignalMethods(VmwareCredentialsManager.prototype);

function getVmwareCredentialsManager() {
    if (!_vmwareCredentialsManager)
        _vmwareCredentialsManager = new VmwareCredentialsManager();

    return _vmwareCredentialsManager;
}
