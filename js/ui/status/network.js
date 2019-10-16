// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported NMApplet */
const { Clutter, Gio, GLib, GObject, NM, St } = imports.gi;
const Signals = imports.signals;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const MessageTray = imports.ui.messageTray;
const ModemManager = imports.misc.modemManager;
const Util = imports.misc.util;

const { loadInterfaceXML } = imports.misc.fileUtils;

const NMConnectionCategory = {
    INVALID: 'invalid',
    WIRED: 'wired',
    WIRELESS: 'wireless',
    WWAN: 'wwan',
    VPN: 'vpn'
};

const NMAccessPointSecurity = {
    NONE: 1,
    WEP: 2,
    WPA_PSK: 3,
    WPA2_PSK: 4,
    WPA_ENT: 5,
    WPA2_ENT: 6
};

var MAX_DEVICE_ITEMS = 4;

// small optimization, to avoid using [] all the time
const NM80211Mode = NM['80211Mode'];
const NM80211ApFlags = NM['80211ApFlags'];
const NM80211ApSecurityFlags = NM['80211ApSecurityFlags'];

var PortalHelperResult = {
    CANCELLED: 0,
    COMPLETED: 1,
    RECHECK: 2
};

const PortalHelperIface = loadInterfaceXML('org.gnome.Shell.PortalHelper');
const PortalHelperProxy = Gio.DBusProxy.makeProxyWrapper(PortalHelperIface);

function signalToIcon(value) {
    if (value > 80)
        return 'excellent';
    if (value > 55)
        return 'good';
    if (value > 30)
        return 'ok';
    if (value > 5)
        return 'weak';
    return 'none';
}

function ssidToLabel(ssid) {
    let label = NM.utils_ssid_to_utf8(ssid.get_data());
    if (!label)
        label = _("<unknown>");
    return label;
}

function ensureActiveConnectionProps(active) {
    if (!active._primaryDevice) {
        let devices = active.get_devices();
        if (devices.length > 0) {
            // This list is guaranteed to have at most one device in it.
            let device = devices[0]._delegate;
            active._primaryDevice = device;
        }
    }
}

var NMConnectionItem = class {
    constructor(section, connection) {
        this._section = section;
        this._connection = connection;
        this._activeConnection = null;
        this._activeConnectionChangedId = 0;

        this._buildUI();
        this._sync();
    }

    _buildUI() {
        this.labelItem = new PopupMenu.PopupMenuItem('');
        this.labelItem.connect('activate', this._toggle.bind(this));

        this.radioItem = new PopupMenu.PopupMenuItem(this._connection.get_id(), false);
        this.radioItem.connect('activate', this._activate.bind(this));
    }

    destroy() {
        this.labelItem.destroy();
        this.radioItem.destroy();
    }

    updateForConnection(connection) {
        // connection should always be the same object
        // (and object path) as this._connection, but
        // this can be false if NetworkManager was restarted
        // and picked up connections in a different order
        // Just to be safe, we set it here again

        this._connection = connection;
        this.radioItem.label.text = connection.get_id();
        this._sync();
        this.emit('name-changed');
    }

    getName() {
        return this._connection.get_id();
    }

    isActive() {
        if (this._activeConnection == null)
            return false;

        return this._activeConnection.state <= NM.ActiveConnectionState.ACTIVATED;
    }

    _sync() {
        let isActive = this.isActive();
        this.labelItem.label.text = isActive ? _("Turn Off") : this._section.getConnectLabel();
        this.radioItem.setOrnament(isActive ? PopupMenu.Ornament.DOT : PopupMenu.Ornament.NONE);
        this.emit('icon-changed');
    }

    _toggle() {
        if (this._activeConnection == null)
            this._section.activateConnection(this._connection);
        else
            this._section.deactivateConnection(this._activeConnection);

        this._sync();
    }

    _activate() {
        if (this._activeConnection == null)
            this._section.activateConnection(this._connection);

        this._sync();
    }

    _connectionStateChanged(_ac, _newstate, _reason) {
        this._sync();
    }

    setActiveConnection(activeConnection) {
        if (this._activeConnectionChangedId > 0) {
            this._activeConnection.disconnect(this._activeConnectionChangedId);
            this._activeConnectionChangedId = 0;
        }

        this._activeConnection = activeConnection;

        if (this._activeConnection)
            this._activeConnectionChangedId = this._activeConnection.connect('notify::state',
                                                                             this._connectionStateChanged.bind(this));

        this._sync();
    }
};
Signals.addSignalMethods(NMConnectionItem.prototype);

var NMConnectionSection = class NMConnectionSection {
    constructor(client) {
        if (this.constructor === NMConnectionSection)
            throw new TypeError(`Cannot instantiate abstract type ${this.constructor.name}`);

        this._client = client;

        this._connectionItems = new Map();
        this._connections = [];

        this._labelSection = new PopupMenu.PopupMenuSection();
        this._radioSection = new PopupMenu.PopupMenuSection();

        this.item = new PopupMenu.PopupSubMenuMenuItem('', true);
        this.item.menu.addMenuItem(this._labelSection);
        this.item.menu.addMenuItem(this._radioSection);

        this._notifyConnectivityId = this._client.connect('notify::connectivity', this._iconChanged.bind(this));
    }

    destroy() {
        if (this._notifyConnectivityId != 0) {
            this._client.disconnect(this._notifyConnectivityId);
            this._notifyConnectivityId = 0;
        }

        this.item.destroy();
    }

    _iconChanged() {
        this._sync();
        this.emit('icon-changed');
    }

    _sync() {
        let nItems = this._connectionItems.size;

        this._radioSection.actor.visible = (nItems > 1);
        this._labelSection.actor.visible = (nItems == 1);

        this.item.label.text = this._getStatus();
        this.item.icon.icon_name = this._getMenuIcon();
    }

    _getMenuIcon() {
        return this.getIndicatorIcon();
    }

    getConnectLabel() {
        return _("Connect");
    }

    _connectionValid(_connection) {
        return true;
    }

    _connectionSortFunction(one, two) {
        return GLib.utf8_collate(one.get_id(), two.get_id());
    }

    _makeConnectionItem(connection) {
        return new NMConnectionItem(this, connection);
    }

    checkConnection(connection) {
        if (!this._connectionValid(connection))
            return;

        // This function is called every time the connection is added or updated.
        // In the usual case, we already added this connection and UUID
        // didn't change. So we need to check if we already have an item,
        // and update it for properties in the connection that changed
        // (the only one we care about is the name)
        // But it's also possible we didn't know about this connection
        // (eg, during coldplug, or because it was updated and suddenly
        // it's valid for this device), in which case we add a new item.

        let item = this._connectionItems.get(connection.get_uuid());
        if (item)
            this._updateForConnection(item, connection);
        else
            this._addConnection(connection);
    }

    _updateForConnection(item, connection) {
        let pos = this._connections.indexOf(connection);

        this._connections.splice(pos, 1);
        pos = Util.insertSorted(this._connections, connection, this._connectionSortFunction.bind(this));
        this._labelSection.moveMenuItem(item.labelItem, pos);
        this._radioSection.moveMenuItem(item.radioItem, pos);

        item.updateForConnection(connection);
    }

    _addConnection(connection) {
        let item = this._makeConnectionItem(connection);
        if (!item)
            return;

        item.connect('icon-changed', () => this._iconChanged());
        item.connect('activation-failed', (item, reason) => {
            this.emit('activation-failed', reason);
        });
        item.connect('name-changed', this._sync.bind(this));

        let pos = Util.insertSorted(this._connections, connection, this._connectionSortFunction.bind(this));
        this._labelSection.addMenuItem(item.labelItem, pos);
        this._radioSection.addMenuItem(item.radioItem, pos);
        this._connectionItems.set(connection.get_uuid(), item);
        this._sync();
    }

    removeConnection(connection) {
        let uuid = connection.get_uuid();
        let item = this._connectionItems.get(uuid);
        if (item == undefined)
            return;

        item.destroy();
        this._connectionItems.delete(uuid);

        let pos = this._connections.indexOf(connection);
        this._connections.splice(pos, 1);

        this._sync();
    }
};
Signals.addSignalMethods(NMConnectionSection.prototype);

var NMConnectionDevice = class NMConnectionDevice extends NMConnectionSection {
    constructor(client, device) {
        super(client);

        if (this.constructor === NMConnectionDevice)
            throw new TypeError(`Cannot instantiate abstract type ${this.constructor.name}`);

        this._device = device;
        this._description = '';

        this._autoConnectItem = this.item.menu.addAction(_("Connect"), this._autoConnect.bind(this));
        this._deactivateItem = this._radioSection.addAction(_("Turn Off"), this.deactivateConnection.bind(this));

        this._stateChangedId = this._device.connect('state-changed', this._deviceStateChanged.bind(this));
        this._activeConnectionChangedId = this._device.connect('notify::active-connection', this._activeConnectionChanged.bind(this));
    }

    _canReachInternet() {
        if (this._client.primary_connection != this._device.active_connection)
            return true;

        return this._client.connectivity == NM.ConnectivityState.FULL;
    }

    _autoConnect() {
        let connection = new NM.SimpleConnection();
        this._client.add_and_activate_connection_async(connection, this._device, null, null, null);
    }

    destroy() {
        if (this._stateChangedId) {
            GObject.signal_handler_disconnect(this._device, this._stateChangedId);
            this._stateChangedId = 0;
        }
        if (this._activeConnectionChangedId) {
            GObject.signal_handler_disconnect(this._device, this._activeConnectionChangedId);
            this._activeConnectionChangedId = 0;
        }

        super.destroy();
    }

    _activeConnectionChanged() {
        if (this._activeConnection) {
            let item = this._connectionItems.get(this._activeConnection.connection.get_uuid());
            item.setActiveConnection(null);
            this._activeConnection = null;
        }

        this._sync();
    }

    _deviceStateChanged(device, newstate, oldstate, reason) {
        if (newstate == oldstate) {
            log('device emitted state-changed without actually changing state');
            return;
        }

        /* Emit a notification if activation fails, but don't do it
           if the reason is no secrets, as that indicates the user
           cancelled the agent dialog */
        if (newstate == NM.DeviceState.FAILED &&
            reason != NM.DeviceStateReason.NO_SECRETS) {
            this.emit('activation-failed', reason);
        }

        this._sync();
    }

    _connectionValid(connection) {
        return this._device.connection_valid(connection);
    }

    activateConnection(connection) {
        this._client.activate_connection_async(connection, this._device, null, null, null);
    }

    deactivateConnection(_activeConnection) {
        this._device.disconnect(null);
    }

    setDeviceDescription(desc) {
        this._description = desc;
        this._sync();
    }

    _getDescription() {
        return this._description;
    }

    _sync() {
        let nItems = this._connectionItems.size;
        this._autoConnectItem.visible = (nItems == 0);
        this._deactivateItem.visible = this._device.state > NM.DeviceState.DISCONNECTED;

        if (this._activeConnection == null) {
            let activeConnection = this._device.active_connection;
            if (activeConnection && activeConnection.connection) {
                let item = this._connectionItems.get(activeConnection.connection.get_uuid());
                if (item) {
                    this._activeConnection = activeConnection;
                    ensureActiveConnectionProps(this._activeConnection);
                    item.setActiveConnection(this._activeConnection);
                }
            }
        }

        super._sync();
    }

    _getStatus() {
        if (!this._device)
            return '';

        switch (this._device.state) {
        case NM.DeviceState.DISCONNECTED:
            /* Translators: %s is a network identifier */
            return _("%s Off").format(this._getDescription());
        case NM.DeviceState.ACTIVATED:
            /* Translators: %s is a network identifier */
            return _("%s Connected").format(this._getDescription());
        case NM.DeviceState.UNMANAGED:
            /* Translators: this is for network devices that are physically present but are not
               under NetworkManager's control (and thus cannot be used in the menu);
               %s is a network identifier */
            return _("%s Unmanaged").format(this._getDescription());
        case NM.DeviceState.DEACTIVATING:
            /* Translators: %s is a network identifier */
            return _("%s Disconnecting").format(this._getDescription());
        case NM.DeviceState.PREPARE:
        case NM.DeviceState.CONFIG:
        case NM.DeviceState.IP_CONFIG:
        case NM.DeviceState.IP_CHECK:
        case NM.DeviceState.SECONDARIES:
            /* Translators: %s is a network identifier */
            return _("%s Connecting").format(this._getDescription());
        case NM.DeviceState.NEED_AUTH:
            /* Translators: this is for network connections that require some kind of key or password; %s is a network identifier */
            return _("%s Requires Authentication").format(this._getDescription());
        case NM.DeviceState.UNAVAILABLE:
            // This state is actually a compound of various states (generically unavailable,
            // firmware missing), that are exposed by different properties (whose state may
            // or may not updated when we receive state-changed).
            if (this._device.firmware_missing) {
                /* Translators: this is for devices that require some kind of firmware or kernel
                   module, which is missing; %s is a network identifier */
                return _("Firmware Missing For %s").format(this._getDescription());
            }
            /* Translators: this is for a network device that cannot be activated (for example it
               is disabled by rfkill, or it has no coverage; %s is a network identifier */
            return _("%s Unavailable").format(this._getDescription());
        case NM.DeviceState.FAILED:
            /* Translators: %s is a network identifier */
            return _("%s Connection Failed").format(this._getDescription());
        default:
            log('Device state invalid, is %d'.format(this._device.state));
            return 'invalid';
        }
    }
};

var NMDeviceWired = class extends NMConnectionDevice {
    constructor(client, device) {
        super(client, device);

        this.item.menu.addSettingsAction(_("Wired Settings"), 'gnome-network-panel.desktop');
    }

    get category() {
        return NMConnectionCategory.WIRED;
    }

    _hasCarrier() {
        if (this._device instanceof NM.DeviceEthernet)
            return this._device.carrier;
        else
            return true;
    }

    _sync() {
        this.item.visible = this._hasCarrier();
        super._sync();
    }

    getIndicatorIcon() {
        if (this._device.active_connection) {
            let state = this._device.active_connection.state;

            if (state == NM.ActiveConnectionState.ACTIVATING) {
                return 'network-wired-acquiring-symbolic';
            } else if (state == NM.ActiveConnectionState.ACTIVATED) {
                if (this._canReachInternet())
                    return 'network-wired-symbolic';
                else
                    return 'network-wired-no-route-symbolic';
            } else {
                return 'network-wired-disconnected-symbolic';
            }
        } else {
            return 'network-wired-disconnected-symbolic';
        }
    }
};

var NMDeviceModem = class extends NMConnectionDevice {
    constructor(client, device) {
        super(client, device);

        this.item.menu.addSettingsAction(_("Mobile Broadband Settings"), 'gnome-network-panel.desktop');

        this._mobileDevice = null;

        let capabilities = device.current_capabilities;
        if (device.udi.indexOf('/org/freedesktop/ModemManager1/Modem') == 0)
            this._mobileDevice = new ModemManager.BroadbandModem(device.udi, capabilities);
        else if (capabilities & NM.DeviceModemCapabilities.GSM_UMTS)
            this._mobileDevice = new ModemManager.ModemGsm(device.udi);
        else if (capabilities & NM.DeviceModemCapabilities.CDMA_EVDO)
            this._mobileDevice = new ModemManager.ModemCdma(device.udi);
        else if (capabilities & NM.DeviceModemCapabilities.LTE)
            this._mobileDevice = new ModemManager.ModemGsm(device.udi);

        if (this._mobileDevice) {
            this._operatorNameId = this._mobileDevice.connect('notify::operator-name', this._sync.bind(this));
            this._signalQualityId = this._mobileDevice.connect('notify::signal-quality', () => {
                this._iconChanged();
            });
        }
    }

    get category() {
        return NMConnectionCategory.WWAN;
    }

    _autoConnect() {
        Util.spawn(['gnome-control-center', 'network',
                    'connect-3g', this._device.get_path()]);
    }

    destroy() {
        if (this._operatorNameId) {
            this._mobileDevice.disconnect(this._operatorNameId);
            this._operatorNameId = 0;
        }
        if (this._signalQualityId) {
            this._mobileDevice.disconnect(this._signalQualityId);
            this._signalQualityId = 0;
        }

        super.destroy();
    }

    _getStatus() {
        if (!this._client.wwan_hardware_enabled)
            /* Translators: %s is a network identifier */
            return _("%s Hardware Disabled").format(this._getDescription());
        else if (!this._client.wwan_enabled)
            /* Translators: this is for a network device that cannot be activated
               because it's disabled by rfkill (airplane mode); %s is a network identifier */
            return _("%s Disabled").format(this._getDescription());
        else if (this._device.state == NM.DeviceState.ACTIVATED &&
                 this._mobileDevice && this._mobileDevice.operator_name)
            return this._mobileDevice.operator_name;
        else
            return super._getStatus();
    }

    getIndicatorIcon() {
        if (this._device.active_connection) {
            if (this._device.active_connection.state == NM.ActiveConnectionState.ACTIVATING)
                return 'network-cellular-acquiring-symbolic';

            return this._getSignalIcon();
        } else {
            return 'network-cellular-signal-none-symbolic';
        }
    }

    _getSignalIcon() {
        return `network-cellular-signal-${signalToIcon(this._mobileDevice.signal_quality)}-symbolic`;
    }
};

var NMDeviceBluetooth = class extends NMConnectionDevice {
    constructor(client, device) {
        super(client, device);

        this.item.menu.addSettingsAction(_("Bluetooth Settings"), 'gnome-network-panel.desktop');
    }

    get category() {
        return NMConnectionCategory.WWAN;
    }

    _getDescription() {
        return this._device.name;
    }

    getConnectLabel() {
        return _("Connect to Internet");
    }

    getIndicatorIcon() {
        if (this._device.active_connection) {
            let state = this._device.active_connection.state;
            if (state == NM.ActiveConnectionState.ACTIVATING)
                return 'network-cellular-acquiring-symbolic';
            else if (state == NM.ActiveConnectionState.ACTIVATED)
                return 'network-cellular-connected-symbolic';
            else
                return 'network-cellular-signal-none-symbolic';
        } else {
            return 'network-cellular-signal-none-symbolic';
        }
    }
};

var NMDeviceWireless = class {
    constructor(client, device) {
        this._client = client;
        this._device = device;

        this._description = '';

        this.item = new PopupMenu.PopupSubMenuMenuItem('', true);

        this._toggleItem = new PopupMenu.PopupMenuItem('');
        this._toggleItem.connect('activate', this._toggleWifi.bind(this));
        this.item.menu.addMenuItem(this._toggleItem);

        this.item.menu.addSettingsAction(_("Wi-Fi Settings"), 'gnome-wifi-panel.desktop');

        this._wirelessEnabledChangedId = this._client.connect('notify::wireless-enabled', this._sync.bind(this));
        this._wirelessHwEnabledChangedId = this._client.connect('notify::wireless-hardware-enabled', this._sync.bind(this));
        this._activeApChangedId = this._device.connect('notify::active-access-point', this._activeApChanged.bind(this));
        this._stateChangedId = this._device.connect('state-changed', this._deviceStateChanged.bind(this));
        this._notifyConnectivityId = this._client.connect('notify::connectivity', this._iconChanged.bind(this));

        this._sync();
    }

    get category() {
        return NMConnectionCategory.WIRELESS;
    }

    _iconChanged() {
        this._sync();
        this.emit('icon-changed');
    }

    destroy() {
        if (this._activeApChangedId) {
            GObject.signal_handler_disconnect(this._device, this._activeApChangedId);
            this._activeApChangedId = 0;
        }
        if (this._stateChangedId) {
            GObject.signal_handler_disconnect(this._device, this._stateChangedId);
            this._stateChangedId = 0;
        }
        if (this._strengthChangedId > 0) {
            this._activeAccessPoint.disconnect(this._strengthChangedId);
            this._strengthChangedId = 0;
        }
        if (this._wirelessEnabledChangedId) {
            this._client.disconnect(this._wirelessEnabledChangedId);
            this._wirelessEnabledChangedId = 0;
        }
        if (this._wirelessHwEnabledChangedId) {
            this._client.disconnect(this._wirelessHwEnabledChangedId);
            this._wirelessHwEnabledChangedId = 0;
        }
        if (this._notifyConnectivityId) {
            this._client.disconnect(this._notifyConnectivityId);
            this._notifyConnectivityId = 0;
        }

        this.item.destroy();
    }

    _deviceStateChanged(device, newstate, oldstate, reason) {
        if (newstate == oldstate) {
            log('device emitted state-changed without actually changing state');
            return;
        }

        /* Emit a notification if activation fails, but don't do it
           if the reason is no secrets, as that indicates the user
           cancelled the agent dialog */
        if (newstate == NM.DeviceState.FAILED &&
            reason != NM.DeviceStateReason.NO_SECRETS) {
            this.emit('activation-failed', reason);
        }

        this._sync();
    }

    _toggleWifi() {
        this._client.wireless_enabled = !this._client.wireless_enabled;
    }

    _strengthChanged() {
        this._iconChanged();
    }

    _activeApChanged() {
        if (this._activeAccessPoint) {
            this._activeAccessPoint.disconnect(this._strengthChangedId);
            this._strengthChangedId = 0;
        }

        this._activeAccessPoint = this._device.active_access_point;

        if (this._activeAccessPoint) {
            this._strengthChangedId = this._activeAccessPoint.connect('notify::strength',
                                                                      this._strengthChanged.bind(this));
        }

        this._sync();
    }

    _sync() {
        this._toggleItem.label.text = this._client.wireless_enabled ? _("Turn Off") : _("Turn On");
        this._toggleItem.visible = this._client.wireless_hardware_enabled;

        this.item.icon.icon_name = this._getMenuIcon();
        this.item.label.text = this._getStatus();
    }

    setDeviceDescription(desc) {
        this._description = desc;
        this._sync();
    }

    _getStatus() {
        let ap = this._device.active_access_point;

        if (this._isHotSpotMaster())
            /* Translators: %s is a network identifier */
            return _("%s Hotspot Active").format(this._description);
        else if (this._device.state >= NM.DeviceState.PREPARE &&
                 this._device.state < NM.DeviceState.ACTIVATED)
            /* Translators: %s is a network identifier */
            return _("%s Connecting").format(this._description);
        else if (ap)
            return ssidToLabel(ap.get_ssid());
        else if (!this._client.wireless_hardware_enabled)
            /* Translators: %s is a network identifier */
            return _("%s Hardware Disabled").format(this._description);
        else if (!this._client.wireless_enabled)
            /* Translators: %s is a network identifier */
            return _("%s Off").format(this._description);
        else if (this._device.state == NM.DeviceState.DISCONNECTED)
            /* Translators: %s is a network identifier */
            return _("%s Not Connected").format(this._description);
        else
            return '';
    }

    _getMenuIcon() {
        if (this._device.active_connection)
            return this.getIndicatorIcon();
        else
            return 'network-wireless-signal-none-symbolic';
    }

    _canReachInternet() {
        if (this._client.primary_connection != this._device.active_connection)
            return true;

        return this._client.connectivity == NM.ConnectivityState.FULL;
    }

    _isHotSpotMaster() {
        if (!this._device.active_connection)
            return false;

        let connection = this._device.active_connection.connection;
        if (!connection)
            return false;

        let ip4config = connection.get_setting_ip4_config();
        if (!ip4config)
            return false;

        return ip4config.get_method() == NM.SETTING_IP4_CONFIG_METHOD_SHARED;
    }

    getIndicatorIcon() {
        if (this._device.state < NM.DeviceState.PREPARE)
            return 'network-wireless-disconnected-symbolic';
        if (this._device.state < NM.DeviceState.ACTIVATED)
            return 'network-wireless-acquiring-symbolic';

        if (this._isHotSpotMaster())
            return 'network-wireless-hotspot-symbolic';

        let ap = this._device.active_access_point;
        if (!ap) {
            if (this._device.mode != NM80211Mode.ADHOC)
                log('An active wireless connection, in infrastructure mode, involves no access point?');

            if (this._canReachInternet())
                return 'network-wireless-connected-symbolic';
            else
                return 'network-wireless-no-route-symbolic';
        }

        if (this._canReachInternet())
            return `network-wireless-signal-${signalToIcon(ap.strength)}-symbolic`;
        else
            return 'network-wireless-no-route-symbolic';
    }
};
Signals.addSignalMethods(NMDeviceWireless.prototype);

var NMVpnConnectionItem = class extends NMConnectionItem {
    isActive() {
        if (this._activeConnection == null)
            return false;

        return this._activeConnection.vpn_state != NM.VpnConnectionState.DISCONNECTED;
    }

    _buildUI() {
        this.labelItem = new PopupMenu.PopupMenuItem('');
        this.labelItem.connect('activate', this._toggle.bind(this));

        this.radioItem = new PopupMenu.PopupSwitchMenuItem(this._connection.get_id(), false);
        this.radioItem.connect('toggled', this._toggle.bind(this));
    }

    _sync() {
        let isActive = this.isActive();
        this.labelItem.label.text = isActive ? _("Turn Off") : this._section.getConnectLabel();
        this.radioItem.setToggleState(isActive);
        this.radioItem.setStatus(this._getStatus());
        this.emit('icon-changed');
    }

    _getStatus() {
        if (this._activeConnection == null)
            return null;

        switch (this._activeConnection.vpn_state) {
        case NM.VpnConnectionState.DISCONNECTED:
        case NM.VpnConnectionState.ACTIVATED:
            return null;
        case NM.VpnConnectionState.PREPARE:
        case NM.VpnConnectionState.CONNECT:
        case NM.VpnConnectionState.IP_CONFIG_GET:
            return _("connecting…");
        case NM.VpnConnectionState.NEED_AUTH:
            /* Translators: this is for network connections that require some kind of key or password */
            return _("authentication required");
        case NM.VpnConnectionState.FAILED:
            return _("connection failed");
        default:
            return 'invalid';
        }
    }

    _connectionStateChanged(ac, newstate, reason) {
        if (newstate == NM.VpnConnectionState.FAILED &&
            reason != NM.VpnConnectionStateReason.NO_SECRETS) {
            // FIXME: if we ever want to show something based on reason,
            // we need to convert from NM.VpnConnectionStateReason
            // to NM.DeviceStateReason
            this.emit('activation-failed', reason);
        }

        this.emit('icon-changed');
        super._connectionStateChanged();
    }

    setActiveConnection(activeConnection) {
        if (this._activeConnectionChangedId > 0) {
            this._activeConnection.disconnect(this._activeConnectionChangedId);
            this._activeConnectionChangedId = 0;
        }

        this._activeConnection = activeConnection;

        if (this._activeConnection)
            this._activeConnectionChangedId = this._activeConnection.connect('vpn-state-changed',
                                                                             this._connectionStateChanged.bind(this));

        this._sync();
    }

    getIndicatorIcon() {
        if (this._activeConnection) {
            if (this._activeConnection.vpn_state < NM.VpnConnectionState.ACTIVATED)
                return 'network-vpn-acquiring-symbolic';
            else
                return 'network-vpn-symbolic';
        } else {
            return '';
        }
    }
};

var NMVpnSection = class extends NMConnectionSection {
    constructor(client) {
        super(client);

        this.item.menu.addSettingsAction(_("VPN Settings"), 'gnome-network-panel.desktop');

        this._sync();
    }

    _sync() {
        let nItems = this._connectionItems.size;
        this.item.visible = (nItems > 0);

        super._sync();
    }

    get category() {
        return NMConnectionCategory.VPN;
    }

    _getDescription() {
        return _("VPN");
    }

    _getStatus() {
        let values = this._connectionItems.values();
        for (let item of values) {
            if (item.isActive())
                return item.getName();
        }

        return _("VPN Off");
    }

    _getMenuIcon() {
        return this.getIndicatorIcon() || 'network-vpn-symbolic';
    }

    activateConnection(connection) {
        this._client.activate_connection_async(connection, null, null, null, null);
    }

    deactivateConnection(activeConnection) {
        this._client.deactivate_connection(activeConnection, null);
    }

    setActiveConnections(vpnConnections) {
        let connections = this._connectionItems.values();
        for (let item of connections) {
            item.setActiveConnection(null);
        }
        vpnConnections.forEach(a => {
            if (a.connection) {
                let item = this._connectionItems.get(a.connection.get_uuid());
                item.setActiveConnection(a);
            }
        });
    }

    _makeConnectionItem(connection) {
        return new NMVpnConnectionItem(this, connection);
    }

    getIndicatorIcon() {
        let items = this._connectionItems.values();
        for (let item of items) {
            let icon = item.getIndicatorIcon();
            if (icon)
                return icon;
        }
        return '';
    }
};
Signals.addSignalMethods(NMVpnSection.prototype);

var DeviceCategory = class extends PopupMenu.PopupMenuSection {
    constructor(category) {
        super();

        this._category = category;

        this.devices = [];

        this.section = new PopupMenu.PopupMenuSection();
        this.section.box.connect('actor-added', this._sync.bind(this));
        this.section.box.connect('actor-removed', this._sync.bind(this));
        this.addMenuItem(this.section);

        this._summaryItem = new PopupMenu.PopupSubMenuMenuItem('', true);
        this._summaryItem.icon.icon_name = this._getSummaryIcon();
        this.addMenuItem(this._summaryItem);

        this._summaryItem.menu.addSettingsAction(_('Network Settings'),
                                                 'gnome-network-panel.desktop');
        this._summaryItem.hide();

    }

    _sync() {
        let nDevices = this.section.box.get_children().reduce(
            (prev, child) => prev + (child.visible ? 1 : 0), 0);
        this._summaryItem.label.text = this._getSummaryLabel(nDevices);
        let shouldSummarize = nDevices > MAX_DEVICE_ITEMS;
        this._summaryItem.visible = shouldSummarize;
        this.section.actor.visible = !shouldSummarize;
    }

    _getSummaryIcon() {
        switch (this._category) {
        case NMConnectionCategory.WIRED:
            return 'network-wired-symbolic';
        case NMConnectionCategory.WIRELESS:
        case NMConnectionCategory.WWAN:
            return 'network-wireless-symbolic';
        }
        return '';
    }

    _getSummaryLabel(nDevices) {
        switch (this._category) {
        case NMConnectionCategory.WIRED:
            return ngettext("%s Wired Connection",
                            "%s Wired Connections",
                            nDevices).format(nDevices);
        case NMConnectionCategory.WIRELESS:
            return ngettext("%s Wi-Fi Connection",
                            "%s Wi-Fi Connections",
                            nDevices).format(nDevices);
        case NMConnectionCategory.WWAN:
            return ngettext("%s Modem Connection",
                            "%s Modem Connections",
                            nDevices).format(nDevices);
        }
        return '';
    }
};

var NMApplet = GObject.registerClass({
    GTypeName: 'Network_Indicator'
}, class Indicator extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        this._primaryIndicator = this._addIndicator();
        this._vpnIndicator = this._addIndicator();

        // Device types
        this._dtypes = { };
        this._dtypes[NM.DeviceType.ETHERNET] = NMDeviceWired;
        this._dtypes[NM.DeviceType.WIFI] = NMDeviceWireless;
        this._dtypes[NM.DeviceType.MODEM] = NMDeviceModem;
        this._dtypes[NM.DeviceType.BT] = NMDeviceBluetooth;

        // Connection types
        this._ctypes = { };
        this._ctypes[NM.SETTING_WIRED_SETTING_NAME] = NMConnectionCategory.WIRED;
        this._ctypes[NM.SETTING_WIRELESS_SETTING_NAME] = NMConnectionCategory.WIRELESS;
        this._ctypes[NM.SETTING_BLUETOOTH_SETTING_NAME] = NMConnectionCategory.WWAN;
        this._ctypes[NM.SETTING_CDMA_SETTING_NAME] = NMConnectionCategory.WWAN;
        this._ctypes[NM.SETTING_GSM_SETTING_NAME] = NMConnectionCategory.WWAN;
        this._ctypes[NM.SETTING_VPN_SETTING_NAME] = NMConnectionCategory.VPN;

        NM.Client.new_async(null, this._clientGot.bind(this));
    }

    _clientGot(obj, result) {
        this._client = NM.Client.new_finish(result);

        this._activeConnections = [];
        this._connections = [];
        this._connectivityQueue = [];

        this._mainConnection = null;
        this._mainConnectionIconChangedId = 0;
        this._mainConnectionStateChangedId = 0;

        this._notification = null;

        this._nmDevices = [];
        this._devices = { };

        let categories = [NMConnectionCategory.WIRED,
                          NMConnectionCategory.WIRELESS,
                          NMConnectionCategory.WWAN];
        for (let category of categories) {
            this._devices[category] = new DeviceCategory(category);
            this.menu.addMenuItem(this._devices[category]);
        }

        this._vpnSection = new NMVpnSection(this._client);
        this._vpnSection.connect('activation-failed', this._onActivationFailed.bind(this));
        this._vpnSection.connect('icon-changed', this._updateIcon.bind(this));
        this.menu.addMenuItem(this._vpnSection.item);

        this._readConnections();
        this._readDevices();
        this._syncNMState();
        this._syncMainConnection();
        this._syncVpnConnections();

        this._client.connect('notify::nm-running', this._syncNMState.bind(this));
        this._client.connect('notify::networking-enabled', this._syncNMState.bind(this));
        this._client.connect('notify::state', this._syncNMState.bind(this));
        this._client.connect('notify::primary-connection', this._syncMainConnection.bind(this));
        this._client.connect('notify::activating-connection', this._syncMainConnection.bind(this));
        this._client.connect('notify::active-connections', this._syncVpnConnections.bind(this));
        this._client.connect('notify::connectivity', this._syncConnectivity.bind(this));
        this._client.connect('device-added', this._deviceAdded.bind(this));
        this._client.connect('device-removed', this._deviceRemoved.bind(this));
        this._client.connect('connection-added', this._connectionAdded.bind(this));
        this._client.connect('connection-removed', this._connectionRemoved.bind(this));

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
    }

    _sessionUpdated() {
        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        this.menu.setSensitive(sensitive);
    }

    _ensureSource() {
        if (!this._source) {
            this._source = new MessageTray.Source(_("Network Manager"),
                                                  'network-transmit-receive');
            this._source.policy = new MessageTray.NotificationApplicationPolicy('gnome-network-panel');

            this._source.connect('destroy', () => (this._source = null));
            Main.messageTray.add(this._source);
        }
    }

    _readDevices() {
        let devices = this._client.get_devices() || [];
        for (let i = 0; i < devices.length; ++i) {
            try {
                this._deviceAdded(this._client, devices[i], true);
            } catch (e) {
                log(`Failed to add device ${devices[i]}: ${e}`);
            }
        }
        this._syncDeviceNames();
    }

    _notify(iconName, title, text, urgency) {
        if (this._notification)
            this._notification.destroy();

        this._ensureSource();

        let gicon = new Gio.ThemedIcon({ name: iconName });
        this._notification = new MessageTray.Notification(this._source, title, text, { gicon: gicon });
        this._notification.setUrgency(urgency);
        this._notification.setTransient(true);
        this._notification.connect('destroy', () => {
            this._notification = null;
        });
        this._source.showNotification(this._notification);
    }

    _onActivationFailed(_device, _reason) {
        // XXX: nm-applet has no special text depending on reason
        // but I'm not sure of this generic message
        this._notify('network-error-symbolic',
                     _("Connection failed"),
                     _("Activation of network connection failed"),
                     MessageTray.Urgency.HIGH);
    }

    _syncDeviceNames() {
        let names = NM.Device.disambiguate_names(this._nmDevices);
        for (let i = 0; i < this._nmDevices.length; i++) {
            let device = this._nmDevices[i];
            let description = names[i];
            if (device._delegate)
                device._delegate.setDeviceDescription(description);
        }
    }

    _deviceAdded(client, device, skipSyncDeviceNames) {
        if (device._delegate) {
            // already seen, not adding again
            return;
        }

        let wrapperClass = this._dtypes[device.get_device_type()];
        if (wrapperClass) {
            let wrapper = new wrapperClass(this._client, device);
            device._delegate = wrapper;
            this._addDeviceWrapper(wrapper);

            this._nmDevices.push(device);
            this._deviceChanged(device, skipSyncDeviceNames);

            device.connect('notify::interface', () => {
                this._deviceChanged(device, false);
            });
        }
    }

    _deviceChanged(device, skipSyncDeviceNames) {
        let wrapper = device._delegate;

        if (!skipSyncDeviceNames)
            this._syncDeviceNames();

        if (wrapper instanceof NMConnectionSection) {
            this._connections.forEach(connection => {
                wrapper.checkConnection(connection);
            });
        }
    }

    _addDeviceWrapper(wrapper) {
        wrapper._activationFailedId = wrapper.connect('activation-failed',
                                                      this._onActivationFailed.bind(this));

        let section = this._devices[wrapper.category].section;
        section.addMenuItem(wrapper.item);

        let devices = this._devices[wrapper.category].devices;
        devices.push(wrapper);
    }

    _deviceRemoved(client, device) {
        let pos = this._nmDevices.indexOf(device);
        if (pos != -1) {
            this._nmDevices.splice(pos, 1);
            this._syncDeviceNames();
        }

        let wrapper = device._delegate;
        if (!wrapper) {
            log('Removing a network device that was not added');
            return;
        }

        this._removeDeviceWrapper(wrapper);
    }

    _removeDeviceWrapper(wrapper) {
        wrapper.disconnect(wrapper._activationFailedId);
        wrapper.destroy();

        let devices = this._devices[wrapper.category].devices;
        let pos = devices.indexOf(wrapper);
        devices.splice(pos, 1);
    }

    _getMainConnection() {
        let connection;

        connection = this._client.get_primary_connection();
        if (connection) {
            ensureActiveConnectionProps(connection);
            return connection;
        }

        connection = this._client.get_activating_connection();
        if (connection) {
            ensureActiveConnectionProps(connection);
            return connection;
        }

        return null;
    }

    _syncMainConnection() {
        if (this._mainConnectionIconChangedId > 0) {
            this._mainConnection._primaryDevice.disconnect(this._mainConnectionIconChangedId);
            this._mainConnectionIconChangedId = 0;
        }

        if (this._mainConnectionStateChangedId > 0) {
            this._mainConnection.disconnect(this._mainConnectionStateChangedId);
            this._mainConnectionStateChangedId = 0;
        }

        this._mainConnection = this._getMainConnection();

        if (this._mainConnection) {
            if (this._mainConnection._primaryDevice)
                this._mainConnectionIconChangedId = this._mainConnection._primaryDevice.connect('icon-changed', this._updateIcon.bind(this));
            this._mainConnectionStateChangedId = this._mainConnection.connect('notify::state', this._mainConnectionStateChanged.bind(this));
            this._mainConnectionStateChanged();
        }

        this._updateIcon();
        this._syncConnectivity();
    }

    _syncVpnConnections() {
        let activeConnections = this._client.get_active_connections() || [];
        let vpnConnections = activeConnections.filter(
            a => (a instanceof NM.VpnConnection)
        );
        vpnConnections.forEach(a => {
            ensureActiveConnectionProps(a);
        });
        this._vpnSection.setActiveConnections(vpnConnections);

        this._updateIcon();
    }

    _mainConnectionStateChanged() {
        if (this._mainConnection.state == NM.ActiveConnectionState.ACTIVATED && this._notification)
            this._notification.destroy();
    }

    _ignoreConnection(connection) {
        let setting = connection.get_setting_connection();
        if (!setting)
            return true;

        // Ignore slave connections
        if (setting.get_master())
            return true;

        return false;
    }

    _addConnection(connection) {
        if (this._ignoreConnection(connection))
            return;
        if (connection._updatedId) {
            // connection was already seen
            return;
        }

        connection._updatedId = connection.connect('changed', this._updateConnection.bind(this));

        this._updateConnection(connection);
        this._connections.push(connection);
    }

    _readConnections() {
        let connections = this._client.get_connections();
        connections.forEach(this._addConnection.bind(this));
    }

    _connectionAdded(client, connection) {
        this._addConnection(connection);
    }

    _connectionRemoved(client, connection) {
        let pos = this._connections.indexOf(connection);
        if (pos != -1)
            this._connections.splice(pos, 1);

        let section = connection._section;

        if (section == NMConnectionCategory.INVALID)
            return;

        if (section == NMConnectionCategory.VPN) {
            this._vpnSection.removeConnection(connection);
        } else {
            let devices = this._devices[section].devices;
            for (let i = 0; i < devices.length; i++) {
                if (devices[i] instanceof NMConnectionSection)
                    devices[i].removeConnection(connection);
            }
        }

        connection.disconnect(connection._updatedId);
        connection._updatedId = 0;
    }

    _updateConnection(connection) {
        let connectionSettings = connection.get_setting_by_name(NM.SETTING_CONNECTION_SETTING_NAME);
        connection._type = connectionSettings.type;
        connection._section = this._ctypes[connection._type] || NMConnectionCategory.INVALID;

        let section = connection._section;

        if (section == NMConnectionCategory.INVALID)
            return;

        if (section == NMConnectionCategory.VPN) {
            this._vpnSection.checkConnection(connection);
        } else {
            let devices = this._devices[section].devices;
            devices.forEach(wrapper => {
                if (wrapper instanceof NMConnectionSection)
                    wrapper.checkConnection(connection);
            });
        }
    }

    _syncNMState() {
        this.visible = this._client.nm_running;
        this.menu.actor.visible = this._client.networking_enabled;

        this._updateIcon();
        this._syncConnectivity();
    }

    _flushConnectivityQueue() {
        if (this._portalHelperProxy) {
            for (let item of this._connectivityQueue)
                this._portalHelperProxy.CloseRemote(item);
        }

        this._connectivityQueue = [];
    }

    _closeConnectivityCheck(path) {
        let index = this._connectivityQueue.indexOf(path);

        if (index >= 0) {
            if (this._portalHelperProxy)
                this._portalHelperProxy.CloseRemote(path);

            this._connectivityQueue.splice(index, 1);
        }
    }

    _portalHelperDone(proxy, emitter, parameters) {
        let [path, result] = parameters;

        if (result == PortalHelperResult.CANCELLED) {
            // Keep the connection in the queue, so the user is not
            // spammed with more logins until we next flush the queue,
            // which will happen once he chooses a better connection
            // or we get to full connectivity through other means
        } else if (result == PortalHelperResult.COMPLETED) {
            this._closeConnectivityCheck(path);
        } else if (result == PortalHelperResult.RECHECK) {
            this._client.check_connectivity_async(null, (client, result) => {
                try {
                    let state = client.check_connectivity_finish(result);
                    if (state >= NM.ConnectivityState.FULL)
                        this._closeConnectivityCheck(path);
                } catch (e) { }
            });
        } else {
            log(`Invalid result from portal helper: ${result}`);
        }
    }

    _syncConnectivity() {
        if (this._mainConnection == null ||
            this._mainConnection.state != NM.ActiveConnectionState.ACTIVATED) {
            this._flushConnectivityQueue();
            return;
        }

        let isPortal = this._client.connectivity == NM.ConnectivityState.PORTAL;
        // For testing, allow interpreting any value != FULL as PORTAL, because
        // LIMITED (no upstream route after the default gateway) is easy to obtain
        // with a tethered phone
        // NONE is also possible, with a connection configured to force no default route
        // (but in general we should only prompt a portal if we know there is a portal)
        if (GLib.getenv('GNOME_SHELL_CONNECTIVITY_TEST') != null)
            isPortal = isPortal || this._client.connectivity < NM.ConnectivityState.FULL;
        if (!isPortal || Main.sessionMode.isGreeter)
            return;

        let path = this._mainConnection.get_path();
        for (let item of this._connectivityQueue) {
            if (item == path)
                return;
        }

        let timestamp = global.get_current_time();
        if (this._portalHelperProxy) {
            this._portalHelperProxy.AuthenticateRemote(path, '', timestamp);
        } else {
            new PortalHelperProxy(Gio.DBus.session, 'org.gnome.Shell.PortalHelper',
                                  '/org/gnome/Shell/PortalHelper', (proxy, error) => {
                                      if (error) {
                                          log(`Error launching the portal helper: ${error}`);
                                          return;
                                      }

                                      this._portalHelperProxy = proxy;
                                      proxy.connectSignal('Done', this._portalHelperDone.bind(this));

                                      proxy.AuthenticateRemote(path, '', timestamp);
                                  });
        }

        this._connectivityQueue.push(path);
    }

    _updateIcon() {
        if (!this._client.networking_enabled) {
            this._primaryIndicator.visible = false;
        } else {
            let dev = null;
            if (this._mainConnection)
                dev = this._mainConnection._primaryDevice;

            let state = this._client.get_state();
            let connected = state == NM.State.CONNECTED_GLOBAL;
            this._primaryIndicator.visible = (dev != null) || connected;
            if (dev) {
                this._primaryIndicator.icon_name = dev.getIndicatorIcon();
            } else if (connected) {
                if (this._client.connectivity == NM.ConnectivityState.FULL)
                    this._primaryIndicator.icon_name = 'network-wired-symbolic';
                else
                    this._primaryIndicator.icon_name = 'network-wired-no-route-symbolic';
            }
        }

        this._vpnIndicator.icon_name = this._vpnSection.getIndicatorIcon();
        this._vpnIndicator.visible = (this._vpnIndicator.icon_name != '');
    }
});
