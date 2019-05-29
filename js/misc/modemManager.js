// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Gio, GObject, NMA } = imports.gi;

const { loadInterfaceXML } = imports.misc.fileUtils;

// _getMobileProvidersDatabase:
//
// Gets the database of mobile providers, with references between MCCMNC/SID and
// operator name
//
let _mpd;
function _getMobileProvidersDatabase() {
    if (_mpd == null) {
        try {
            _mpd = new NMA.MobileProvidersDatabase();
            _mpd.init(null);
        } catch (e) {
            log(e.message);
            _mpd = null;
        }
    }

    return _mpd;
}

// _findProviderForMccMnc:
// @operatorName: operator name
// @operatorCode: operator code
//
// Given an operator name string (which may not be a real operator name) and an
// operator code string, tries to find a proper operator name to display.
//
function _findProviderForMccMnc(operatorName, operatorCode) {
    if (operatorName) {
        if (operatorName.length != 0 &&
            (operatorName.length > 6 || operatorName.length < 5)) {
            // this looks like a valid name, i.e. not an MCCMNC (that some
            // devices return when not yet connected
            return operatorName;
        }

        if (isNaN(parseInt(operatorName))) {
            // name is definitely not a MCCMNC, so it may be a name
            // after all; return that
            return operatorName;
        }
    }

    let needle;
    if ((!operatorName || operatorName.length == 0) && operatorCode)
        needle = operatorCode;
    else if (operatorName && (operatorName.length == 6 || operatorName.length == 5))
        needle = operatorName;
    else // nothing to search
        return null;

    let mpd = _getMobileProvidersDatabase();
    if (mpd) {
        let provider = mpd.lookup_3gpp_mcc_mnc(needle);
        if (provider)
            return provider.get_name();
    }
    return null;
}

// _findProviderForSid:
// @sid: System Identifier of the serving CDMA network
//
// Tries to find the operator name corresponding to the given SID
//
function _findProviderForSid(sid) {
    if (!sid)
        return null;

    let mpd = _getMobileProvidersDatabase();
    if (mpd) {
        let provider = mpd.lookup_cdma_sid(sid);
        if (provider)
            return provider.get_name();
    }
    return null;
}


//------------------------------------------------------------------------------
// Support for the old ModemManager interface (MM < 0.7)
//------------------------------------------------------------------------------


// The following are not the complete interfaces, just the methods we need
// (or may need in the future)

const ModemGsmNetworkInterface = loadInterfaceXML('org.freedesktop.ModemManager.Modem.Gsm.Network');
const ModemGsmNetworkProxy = Gio.DBusProxy.makeProxyWrapper(ModemGsmNetworkInterface);

const ModemCdmaInterface = loadInterfaceXML('org.freedesktop.ModemManager.Modem.Cdma');
const ModemCdmaProxy = Gio.DBusProxy.makeProxyWrapper(ModemCdmaInterface);

var ModemInterface = GObject.registerClass({
    Requires: [GObject.Object],
    Properties: {
        'operator-name': GObject.ParamSpec.string('operator-name',
                                                  'operator-name',
                                                  'operator-name',
                                                  GObject.ParamFlags.READABLE,
                                                  null),
        'signal-quality': GObject.ParamSpec.int('signal-quality',
                                                'signal-quality',
                                                'signal-quality',
                                                GObject.ParamFlags.READABLE,
                                                0, 100, 0),
    },
}, class ModemManager_ModemInterface extends GObject.Interface {
    get operator_name() { return this._operator_name || null; }
    get signal_quality() { return this._signal_quality || 0; }

    _setOperatorName(operator_name) {
        if (this._operator_name == operator_name)
            return;
        this._operator_name = operator;
        this.notify('operator-name');
    }

    _setSignalQuality(signal_quality) {
        if (this._signal_quality == signal_quality)
            return;
        this._signal_quality = signal_quality;
        this.notify('signal-quality');
    }
});

var ModemGsm = GObject.registerClass({
    Implements: [ModemInterface],
    Properties: {
        'operator-name': GObject.ParamSpec.override('operator-name',
                                                    ModemInterface),
        'signal-quality': GObject.ParamSpec.override('signal-quality',
                                                     ModemInterface),
    }
}, class ModemManager_ModemGsm extends GObject.Object {
    _init(path) {
        this._proxy = new ModemGsmNetworkProxy(Gio.DBus.system, 'org.freedesktop.ModemManager', path);

        // Code is duplicated because the function have different signatures
        this._proxy.connectSignal('SignalQuality', (proxy, sender, [quality]) => {
            this._setSignalQuality(quality);
        });
        this._proxy.connectSignal('RegistrationInfo', (proxy, sender, [status, code, name]) => {
            this._setOperatorName(_findProviderForMccMnc(name, code));
        });
        this._proxy.GetRegistrationInfoRemote(([result], err) => {
            if (err) {
                log(err);
                return;
            }

            let [status, code, name] = result;
            this._setOperatorName(_findProviderForMccMnc(name, code));
        });
        this._proxy.GetSignalQualityRemote((result, err) => {
            if (err) {
                // it will return an error if the device is not connected
                this._setSignalQuality(0);
            } else {
                let [quality] = result;
                this._setSignalQuality(quality);
            }
        });
    }
});

var ModemCdma = GObject.registerClass({
    Implements: [ModemInterface],
    Properties: {
        'operator-name': GObject.ParamSpec.override('operator-name',
                                                    ModemInterface),
        'signal-quality': GObject.ParamSpec.override('signal-quality',
                                                     ModemInterface),
    }
}, class ModemManager_ModemCdma extends GObject.Object {
    _init(path) {
        this._proxy = new ModemCdmaProxy(Gio.DBus.system, 'org.freedesktop.ModemManager', path);

        this._proxy.connectSignal('SignalQuality', (proxy, sender, params) => {
            this._setSignalQuality(params[0]);

            // receiving this signal means the device got activated
            // and we can finally call GetServingSystem
            if (this.operator_name == null)
                this._refreshServingSystem();
        });
        this._proxy.GetSignalQualityRemote((result, err) => {
            if (err) {
                // it will return an error if the device is not connected
                this._setSignalQuality(0);
            } else {
                let [quality] = result;
                this._setSignalQuality(quality);
            }
        });
    }

    _refreshServingSystem() {
        this._proxy.GetServingSystemRemote(([result], err) => {
            if (err) {
                // it will return an error if the device is not connected
                this._setOperatorName(null);
            } else {
                let [bandClass, band, sid] = result;
                this._setOperatorName(_findProviderForSid(sid));
            }
        });
    }
});


//------------------------------------------------------------------------------
// Support for the new ModemManager1 interface (MM >= 0.7)
//------------------------------------------------------------------------------

const BroadbandModemInterface = loadInterfaceXML('org.freedesktop.ModemManager1.Modem');
const BroadbandModemProxy = Gio.DBusProxy.makeProxyWrapper(BroadbandModemInterface);

const BroadbandModem3gppInterface = loadInterfaceXML('org.freedesktop.ModemManager1.Modem.Modem3gpp');
const BroadbandModem3gppProxy = Gio.DBusProxy.makeProxyWrapper(BroadbandModem3gppInterface);

const BroadbandModemCdmaInterface = loadInterfaceXML('org.freedesktop.ModemManager1.Modem.ModemCdma');
const BroadbandModemCdmaProxy = Gio.DBusProxy.makeProxyWrapper(BroadbandModemCdmaInterface);

var BroadbandModem = GObject.registerClass({
    Implements: [ModemInterface],
    Properties: {
        'operator-name': GObject.ParamSpec.override('operator-name',
                                                    ModemInterface),
        'signal-quality': GObject.ParamSpec.override('signal-quality',
                                                     ModemInterface),
    }
}, class ModemManager_BroadbandModem extends GObject.Object {
    _init(path, capabilities) {
        this._proxy = new BroadbandModemProxy(Gio.DBus.system, 'org.freedesktop.ModemManager1', path);
        this._proxy_3gpp = new BroadbandModem3gppProxy(Gio.DBus.system, 'org.freedesktop.ModemManager1', path);
        this._proxy_cdma = new BroadbandModemCdmaProxy(Gio.DBus.system, 'org.freedesktop.ModemManager1', path);
        this._capabilities = capabilities;

        this._proxy.connect('g-properties-changed', (proxy, properties) => {
            if ('SignalQuality' in properties.deep_unpack())
                this._reloadSignalQuality();
        });
        this._reloadSignalQuality();

        this._proxy_3gpp.connect('g-properties-changed', (proxy, properties) => {
            let unpacked = properties.deep_unpack();
            if ('OperatorName' in unpacked || 'OperatorCode' in unpacked)
                this._reload3gppOperatorName();
        });
        this._reload3gppOperatorName();

        this._proxy_cdma.connect('g-properties-changed', (proxy, properties) => {
            let unpacked = properties.deep_unpack();
            if ('Nid' in unpacked || 'Sid' in unpacked)
                this._reloadCdmaOperatorName();
        });
        this._reloadCdmaOperatorName();
    }

    _reloadSignalQuality() {
        let [quality, recent] = this.SignalQuality;
        this._setSignalQuality(quality);
    }

    _reloadOperatorName() {
        let newName = "";
        if (this.operator_name_3gpp && this.operator_name_3gpp.length > 0)
            newName += this.operator_name_3gpp;

        if (this.operator_name_cdma && this.operator_name_cdma.length > 0) {
            if (newName != "")
                newName += ", ";
            newName += this.operator_name_cdma;
        }

        this._setOperatorName(newName);
    }

    _reload3gppOperatorName() {
        let name = this._proxy_3gpp.OperatorName;
        let code = this._proxy_3gpp.OperatorCode;
        this.operator_name_3gpp = _findProviderForMccMnc(name, code);
        this._reloadOperatorName();
    }

    _reloadCdmaOperatorName() {
        let sid = this._proxy_cdma.Sid;
        this.operator_name_cdma = _findProviderForSid(sid);
        this._reloadOperatorName();
    }
});
