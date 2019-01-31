// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Gio, NMA } = imports.gi;
const Signals = imports.signals;

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

var ModemGsm = class {
    constructor(path) {
        this._proxy = new ModemGsmNetworkProxy(Gio.DBus.system, 'org.freedesktop.ModemManager', path);

        this.signal_quality = 0;
        this.operator_name = null;

        // Code is duplicated because the function have different signatures
        this._proxy.connectSignal('SignalQuality', (proxy, sender, [quality]) => {
            this.signal_quality = quality;
            this.emit('notify::signal-quality');
        });
        this._proxy.connectSignal('RegistrationInfo', (proxy, sender, [status, code, name]) => {
            this.operator_name = _findProviderForMccMnc(name, code);
            this.emit('notify::operator-name');
        });
        this._proxy.GetRegistrationInfoRemote(([result], err) => {
            if (err) {
                log(err);
                return;
            }

            let [status, code, name] = result;
            this.operator_name = _findProviderForMccMnc(name, code);
            this.emit('notify::operator-name');
        });
        this._proxy.GetSignalQualityRemote((result, err) => {
            if (err) {
                // it will return an error if the device is not connected
                this.signal_quality = 0;
            } else {
                let [quality] = result;
                this.signal_quality = quality;
            }
            this.emit('notify::signal-quality');
        });
    }
};
Signals.addSignalMethods(ModemGsm.prototype);

var ModemCdma = class {
    constructor(path) {
        this._proxy = new ModemCdmaProxy(Gio.DBus.system, 'org.freedesktop.ModemManager', path);

        this.signal_quality = 0;
        this.operator_name = null;
        this._proxy.connectSignal('SignalQuality', (proxy, sender, params) => {
            this.signal_quality = params[0];
            this.emit('notify::signal-quality');

            // receiving this signal means the device got activated
            // and we can finally call GetServingSystem
            if (this.operator_name == null)
                this._refreshServingSystem();
        });
        this._proxy.GetSignalQualityRemote((result, err) => {
            if (err) {
                // it will return an error if the device is not connected
                this.signal_quality = 0;
            } else {
                let [quality] = result;
                this.signal_quality = quality;
            }
            this.emit('notify::signal-quality');
        });
    }

    _refreshServingSystem() {
        this._proxy.GetServingSystemRemote(([result], err) => {
            if (err) {
                // it will return an error if the device is not connected
                this.operator_name = null;
            } else {
                let [bandClass, band, sid] = result;

                this.operator_name = _findProviderForSid(sid);
            }
            this.emit('notify::operator-name');
        });
    }
};
Signals.addSignalMethods(ModemCdma.prototype);


//------------------------------------------------------------------------------
// Support for the new ModemManager1 interface (MM >= 0.7)
//------------------------------------------------------------------------------

const BroadbandModemInterface = loadInterfaceXML('org.freedesktop.ModemManager1.Modem');
const BroadbandModemProxy = Gio.DBusProxy.makeProxyWrapper(BroadbandModemInterface);

const BroadbandModem3gppInterface = loadInterfaceXML('org.freedesktop.ModemManager1.Modem.Modem3gpp');
const BroadbandModem3gppProxy = Gio.DBusProxy.makeProxyWrapper(BroadbandModem3gppInterface);

const BroadbandModemCdmaInterface = loadInterfaceXML('org.freedesktop.ModemManager1.Modem.ModemCdma');
const BroadbandModemCdmaProxy = Gio.DBusProxy.makeProxyWrapper(BroadbandModemCdmaInterface);

var BroadbandModem = class {
    constructor(path, capabilities) {
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
        let [quality, recent] = this._proxy.SignalQuality;
        this.signal_quality = quality;
        this.emit('notify::signal-quality');
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

        this.operator_name = newName;
        this.emit('notify::operator-name');
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
};
Signals.addSignalMethods(BroadbandModem.prototype);
