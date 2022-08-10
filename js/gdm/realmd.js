// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Manager */

const Gio = imports.gi.Gio;
const Signals = imports.misc.signals;

const { loadInterfaceXML } = imports.misc.fileUtils;

const ProviderIface = loadInterfaceXML("org.freedesktop.realmd.Provider");
const Provider = Gio.DBusProxy.makeProxyWrapper(ProviderIface);

const ServiceIface = loadInterfaceXML("org.freedesktop.realmd.Service");
const Service = Gio.DBusProxy.makeProxyWrapper(ServiceIface);

const RealmIface = loadInterfaceXML("org.freedesktop.realmd.Realm");
const Realm = Gio.DBusProxy.makeProxyWrapper(RealmIface);

var Manager = class extends Signals.EventEmitter {
    constructor() {
        super();

        this._aggregateProvider = Provider(Gio.DBus.system,
                                           'org.freedesktop.realmd',
                                           '/org/freedesktop/realmd',
                                           this._reloadRealms.bind(this));
        this._realms = {};
        this._loginFormat = null;

        this._aggregateProvider.connectObject('g-properties-changed',
            (proxy, properties) => {
                const realmsChanged = !!properties.lookup_value('Realms', null);
                if (realmsChanged)
                    this._reloadRealms();
            }, this);
    }

    _reloadRealms() {
        let realmPaths = this._aggregateProvider.Realms;

        if (!realmPaths)
            return;

        for (let i = 0; i < realmPaths.length; i++) {
            Realm(Gio.DBus.system,
                  'org.freedesktop.realmd',
                  realmPaths[i],
                  this._onRealmLoaded.bind(this));
        }
    }

    _reloadRealm(realm) {
        if (!realm.Configured) {
            if (this._realms[realm.get_object_path()])
                delete this._realms[realm.get_object_path()];

            return;
        }

        this._realms[realm.get_object_path()] = realm;

        this._updateLoginFormat();
    }

    _onRealmLoaded(realm, error) {
        if (error)
            return;

        this._reloadRealm(realm);

        realm.connect('g-properties-changed', (proxy, properties) => {
            const configuredChanged = !!properties.lookup_value('Configured', null);
            if (configuredChanged)
                this._reloadRealm(realm);
        });
    }

    _updateLoginFormat() {
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
    }

    get loginFormat() {
        if (this._loginFormat)
            return this._loginFormat;

        this._updateLoginFormat();

        return this._loginFormat;
    }

    release() {
        Service(Gio.DBus.system,
            'org.freedesktop.realmd',
            '/org/freedesktop/realmd',
            service => service.ReleaseAsync().catch(logError));
        this._aggregateProvider.disconnectObject(this);
        this._realms = { };
        this._updateLoginFormat();
    }
};
