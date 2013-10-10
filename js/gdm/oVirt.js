// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Signals = imports.signals;

const OVirtCredentialsIface = <interface name='org.ovirt.vdsm.Credentials'>
<signal name="UserAuthenticated">
    <arg type="s" name="token"/>
</signal>
</interface>;

const OVirtCredentialsInfo = Gio.DBusInterfaceInfo.new_for_xml(OVirtCredentialsIface);

let _oVirtCredentialsManager = null;

function OVirtCredentials() {
    var self = new Gio.DBusProxy({ g_connection: Gio.DBus.system,
                                   g_interface_name: OVirtCredentialsInfo.name,
                                   g_interface_info: OVirtCredentialsInfo,
                                   g_name: 'org.ovirt.vdsm.Credentials',
                                   g_object_path: '/org/ovirt/vdsm/Credentials',
                                   g_flags: (Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES) });
    self.init(null);
    return self;
}

const OVirtCredentialsManager = new Lang.Class({
    Name: 'OVirtCredentialsManager',
    _init: function() {
        this._token = null;

        this._credentials = new OVirtCredentials();
        this._credentials.connectSignal('UserAuthenticated',
                                        Lang.bind(this, this._onUserAuthenticated));
    },

    _onUserAuthenticated: function(proxy, sender, [token]) {
        this._token = token;
        this.emit('user-authenticated', token);
    },

    hasToken: function() {
        return this._token != null;
    },

    getToken: function() {
        return this._token;
    },

    resetToken: function() {
        this._token = null;
    }
});
Signals.addSignalMethods(OVirtCredentialsManager.prototype);

function getOVirtCredentialsManager() {
    if (!_oVirtCredentialsManager)
        _oVirtCredentialsManager = new OVirtCredentialsManager();

    return _oVirtCredentialsManager;
}
