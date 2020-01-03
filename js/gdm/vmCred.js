const Gio = imports.gi.Gio;
const Signals = imports.signals;
const GLib = imports.gi.GLib;

const env_display = GLib.getenv('DISPLAY');
const display_num = env_display.substr(env_display.indexOf(":")+1);

const dbus_path = '/org/vmware/viewagent/Credentials';
const dbus_interface = 'org.vmware.viewagent.Credentials';

const VmwareCredentialsIface = '<node> \
<interface name="' + dbus_interface + '"> \
<signal name="UserAuthenticated"> \
    <arg type="s" name="token"/> \
    <arg type="s" name="display"/> \
</signal> \
</interface> \
</node>';


const VmwareCredentialsInfo = Gio.DBusInterfaceInfo.new_for_xml(VmwareCredentialsIface);

let _vmwareCredentialsManager = null;

function VmwareCredentials() {
    var self = new Gio.DBusProxy({ g_connection: Gio.DBus.system,
                                   g_interface_name: VmwareCredentialsInfo.name,
                                   g_interface_info: VmwareCredentialsInfo,
                                   g_name: dbus_interface,
                                   g_object_path: dbus_path,
                                   g_flags: (Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES) });
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

    _onUserAuthenticated(proxy, sender, [token, display]) {
        log("VMwareSSO: get token: " + token.substr(0, 5) + "-***-" + token.substr(-16, 5));

        if (display_num == display) {
            log("VMwareSSO: token emitted for DISPLAY " + display);
            this._token = token;
            this.emit('user-authenticated', token);
        }
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
