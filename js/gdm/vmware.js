// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported getVmwareCredentialsManager */

const Gio = imports.gi.Gio;
const Credential = imports.gdm.credentialManager;

const dbusPath = '/org/vmware/viewagent/Credentials';
const dbusInterface = 'org.vmware.viewagent.Credentials';

var SERVICE_NAME = 'gdm-vmwcred';

const VmwareCredentialsIface = `<node>
<interface name="${dbusInterface}">
<signal name="UserAuthenticated">
    <arg type="s" name="token"/>
</signal>
</interface>
</node>`;


const VmwareCredentialsInfo = Gio.DBusInterfaceInfo.new_for_xml(VmwareCredentialsIface);

let _vmwareCredentialsManager = null;

function VmwareCredentials() {
    var self = new Gio.DBusProxy({
        g_connection: Gio.DBus.session,
        g_interface_name: VmwareCredentialsInfo.name,
        g_interface_info: VmwareCredentialsInfo,
        g_name: dbusInterface,
        g_object_path: dbusPath,
        g_flags: Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES,
    });
    self.init(null);
    return self;
}

var VmwareCredentialsManager = class VmwareCredentialsManager extends Credential.CredentialManager {
    constructor() {
        super(SERVICE_NAME);
        this._credentials = new VmwareCredentials();
        this._credentials.connectSignal('UserAuthenticated',
            (proxy, sender, [token]) => {
                this.token = token;
            });
    }
};

function getVmwareCredentialsManager() {
    if (!_vmwareCredentialsManager)
        _vmwareCredentialsManager = new VmwareCredentialsManager();

    return _vmwareCredentialsManager;
}
