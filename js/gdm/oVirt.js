// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported getOVirtCredentialsManager */

const Gio = imports.gi.Gio;
const Signals = imports.signals;

const GLib = imports.gi.GLib;
const env_display = GLib.getenv('DISPLAY');
const display = env_display.substr(env_display.indexOf(":")+1);

const dbus_path = '/org/vmware/viewagent/Credentials/D' + display;
const dbus_interface = 'org.vmware.viewagent.Credentials.D' + display;

const VCredentialsIface = '<node> \
<interface name="' + dbus_interface + '"> \
<signal name="UserAuthenticated"> \
    <arg type="s" name="token"/> \
</signal> \
</interface> \
</node>';

const VGreeterIface = '<node> \
<interface name="org.vmware.viewagent.Greeter"> \
<signal name="GreeterStarted"> \
    <arg type="s" name="token"/> \
</signal> \
</interface> \
</node>';

const VCredentialsInfo = Gio.DBusInterfaceInfo.new_for_xml(VCredentialsIface);

function VCredentials() {
    var self = new Gio.DBusProxy({ g_connection: Gio.DBus.system,
                                   g_interface_name: VCredentialsInfo.name,
                                   g_interface_info: VCredentialsInfo,
                                   g_name: dbus_interface,
                                   g_object_path: dbus_path,
                                   g_flags: (Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES) });
    self.init(null);
    return self;
}

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
    var self = new Gio.DBusProxy({ g_connection: Gio.DBus.system,
                                   g_interface_name: OVirtCredentialsInfo.name,
                                   g_interface_info: OVirtCredentialsInfo,
                                   g_name: 'org.ovirt.vdsm.Credentials',
                                   g_object_path: '/org/ovirt/vdsm/Credentials',
                                   g_flags: Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES });
    self.init(null);
    return self;
}

var OVirtCredentialsManager = class {
    constructor() {
        this._token = null;

        this._credentials = new OVirtCredentials();
        this._credentials.connectSignal('UserAuthenticated',
                                        this._onUserAuthenticated.bind(this));

        this._VMcredentials = new VCredentials();
        this._VMcredentials.connectSignal('UserAuthenticated',
                                        this._onUserAuthenticated.bind(this));

        try {
            if (display >= 100) {
                this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(VGreeterIface, this);
                this._dbusImpl.export(Gio.DBus.system, '/org/vmware/viewagent/Greeter');
                this._dbusImpl.emit_signal('GreeterStarted', GLib.Variant.new('(s)', [display]));
                log("Greeter started with DISPLAY: " + display);
            }
        } catch (e) {
            log("Error: " + e.message);
        }
    }

    _onUserAuthenticated(proxy, sender, [token]) {
        this._token = token;
        this.emit('user-authenticated', token);
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
Signals.addSignalMethods(OVirtCredentialsManager.prototype);

function getOVirtCredentialsManager() {
    if (!_oVirtCredentialsManager)
        _oVirtCredentialsManager = new OVirtCredentialsManager();

    return _oVirtCredentialsManager;
}
