import Gio from 'gi://Gio';
import * as Credential from './credentialManager.js';

export const SERVICE_NAME = 'gdm-ovirtcred';

const OVirtCredentialsIface = `
<node>
<interface name="org.ovirt.vdsm.Credentials">
<signal name="UserAuthenticated">
    <arg type="s" name="token"/>
</signal>
</interface>
</node>`;

const OVirtCredentialsInfo = Gio.DBusInterfaceInfo.new_for_xml(OVirtCredentialsIface);

let _oVirtCredentialsManager = null;

function OVirtCredentials() {
    var self = new Gio.DBusProxy({
        g_connection: Gio.DBus.system,
        g_interface_name: OVirtCredentialsInfo.name,
        g_interface_info: OVirtCredentialsInfo,
        g_name: 'org.ovirt.vdsm.Credentials',
        g_object_path: '/org/ovirt/vdsm/Credentials',
        g_flags: Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES,
    });
    self.init(null);
    return self;
}

class OVirtCredentialsManager extends Credential.CredentialManager {
    constructor() {
        super(SERVICE_NAME);
        this._credentials = new OVirtCredentials();
        this._credentials.connectSignal('UserAuthenticated',
            (proxy, sender, [token]) => {
                this.token = token;
            });
    }
}

/**
 * @returns {OVirtCredentialsManager}
 */
export function getOVirtCredentialsManager() {
    if (!_oVirtCredentialsManager)
        _oVirtCredentialsManager = new OVirtCredentialsManager();

    return _oVirtCredentialsManager;
}
