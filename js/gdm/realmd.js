// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const ProviderIface = '<node> \
<interface name="org.freedesktop.realmd.Provider"> \
    <property name="Name" type="s" access="read"/> \
    <property name="Version" type="s" access="read"/> \
    <property name="Realms" type="ao" access="read"/> \
    <method name="Discover"> \
        <arg name="string" type="s" direction="in"/> \
        <arg name="options" type="a{sv}" direction="in"/> \
        <arg name="relevance" type="i" direction="out"/> \
        <arg name="realm" type="ao" direction="out"/> \
    </method> \
</interface> \
</node>';
const Provider = Gio.DBusProxy.makeProxyWrapper(ProviderIface);

const ServiceIface = '<node> \
<interface name="org.freedesktop.realmd.Service"> \
    <method name="Cancel"> \
        <arg name="operation" type="s" direction="in"/> \
    </method> \
    <method name="Release" /> \
    <method name="SetLocale"> \
        <arg name="locale" type="s" direction="in"/> \
    </method> \
    <signal name="Diagnostics"> \
        <arg name="data" type="s"/> \
        <arg name="operation" type="s"/> \
    </signal> \
</interface> \
</node>';
const Service = Gio.DBusProxy.makeProxyWrapper(ServiceIface);

const RealmIface = '<node> \
<interface name="org.freedesktop.realmd.Realm"> \
    <property name="Name" type="s" access="read"/> \
    <property name="Configured" type="s" access="read"/> \
    <property name="Details" type="a(ss)" access="read"/> \
    <property name="LoginFormats" type="as" access="read"/> \
    <property name="LoginPolicy" type="s" access="read"/> \
    <property name="PermittedLogins" type="as" access="read"/> \
    <property name="SupportedInterfaces" type="as" access="read"/> \
    <method name="ChangeLoginPolicy"> \
        <arg name="login_policy" type="s" direction="in"/> \
        <arg name="permitted_add" type="as" direction="in"/> \
        <arg name="permitted_remove" type="as" direction="in"/> \
        <arg name="options" type="a{sv}" direction="in"/> \
    </method> \
    <method name="Deconfigure"> \
        <arg name="options" type="a{sv}" direction="in"/> \
    </method> \
</interface> \
</node>';
const Realm = Gio.DBusProxy.makeProxyWrapper(RealmIface);

const Manager = new Lang.Class({
    Name: 'Manager',

    _init: function(parentActor) {
        this._aggregateProvider = Provider(Gio.DBus.system,
                                           'org.freedesktop.realmd',
                                           '/org/freedesktop/realmd',
                                           Lang.bind(this, this._reloadRealms))
        this._realms = {};

        this._signalId = this._aggregateProvider.connect('g-properties-changed',
                                        Lang.bind(this, function(proxy, properties) {
                                            if ('Realms' in properties.deep_unpack())
                                                this._reloadRealms();
                                        }));
    },

    _reloadRealms: function() {
        let realmPaths = this._aggregateProvider.Realms;

        if (!realmPaths)
            return;

        for (let i = 0; i < realmPaths.length; i++) {
            let realm = Realm(Gio.DBus.system,
                              'org.freedesktop.realmd',
                              realmPaths[i],
                              Lang.bind(this, this._onRealmLoaded));
        }
    },

    _reloadRealm: function(realm) {
        if (!realm.Configured) {
            if (this._realms[realm.get_object_path()])
                delete this._realms[realm.get_object_path()];

            return;
        }

        this._realms[realm.get_object_path()] = realm;

        this._updateLoginFormat();
    },

    _onRealmLoaded: function(realm, error) {
        if (error)
            return;

        this._reloadRealm(realm);

        realm.connect('g-properties-changed',
                      Lang.bind(this, function(proxy, properties) {
                                if ('Configured' in properties.deep_unpack())
                                    this._reloadRealm(realm);
                                }));
    },

    _updateLoginFormat: function() {
        let newLoginFormat;

        for (let realmPath in this._realms) {
            let realm = this._realms[realmPath];
            if (realm.LoginFormats && realm.LoginFormats.length > 0) {
                newLoginFormat = realm.LoginFormats[0];
                break;
            }
        }

        if (this._loginFormat != newLoginFormat) {
            this._loginFormat = newLoginFormat;
            this.emit('login-format-changed', newLoginFormat);
        }
    },

    get loginFormat() {
        if (this._loginFormat !== undefined)
            return this._loginFormat;

        this._updateLoginFormat();

        return this._loginFormat;
    },

    release: function() {
        Service(Gio.DBus.system,
                'org.freedesktop.realmd',
                '/org/freedesktop/realmd',
                function(service) {
                    service.ReleaseRemote();
                });
        this._aggregateProvider.disconnect(this._signalId);
        this._realms = { };
        this._updateLoginFormat();
    }
});
Signals.addSignalMethods(Manager.prototype)
