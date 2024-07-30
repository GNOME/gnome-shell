import Gio from 'gi://Gio';
import * as Credential from './credentialManager.js';

const dbusPath = '/org/vmware/viewagent/Credentials';
const dbusInterface = 'org.vmware.viewagent.Credentials';

export const SERVICE_NAME = 'gdm-vmwcred';

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

class VmwareCredentialsManager extends Credential.CredentialManager {
    constructor() {
        super(SERVICE_NAME);
        this._credentials = new VmwareCredentials();
        this._credentials.connectSignal('UserAuthenticated',
            (proxy, sender, [token]) => {
                this.token = token;
            });
    }
}

/**
 * @returns {VmwareCredentialsManager}
 */
export function getVmwareCredentialsManager() {
    if (!_vmwareCredentialsManager)
        _vmwareCredentialsManager = new VmwareCredentialsManager();

    return _vmwareCredentialsManager;
}
