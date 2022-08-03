// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported NMApplet */
const { Clutter, Gio, GLib, GObject, Meta, NM, Polkit, St } = imports.gi;
const Signals = imports.misc.signals;

const Animation = imports.ui.animation;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const MessageTray = imports.ui.messageTray;
const ModalDialog = imports.ui.modalDialog;
const ModemManager = imports.misc.modemManager;
const Rfkill = imports.ui.status.rfkill;
const Util = imports.misc.util;

const {loadInterfaceXML} = imports.misc.fileUtils;
const {registerDestroyableType} = imports.misc.signalTracker;

Gio._promisify(Gio.DBusConnection.prototype, 'call');
Gio._promisify(NM.Client, 'new_async');
Gio._promisify(NM.Client.prototype, 'check_connectivity_async');

const NMConnectionCategory = {
    INVALID: 'invalid',
    WIRED: 'wired',
    WIRELESS: 'wireless',
    BLUETOOTH: 'bluetooth',
    WWAN: 'wwan',
    VPN: 'vpn',
};

var MAX_DEVICE_ITEMS = 4;

// small optimization, to avoid using [] all the time
const NM80211Mode = NM['80211Mode'];

var PortalHelperResult = {
    CANCELLED: 0,
    COMPLETED: 1,
    RECHECK: 2,
};

const PortalHelperIface = loadInterfaceXML('org.gnome.Shell.PortalHelper');
const PortalHelperInfo = Gio.DBusInterfaceInfo.new_for_xml(PortalHelperIface);

function signalToIcon(value) {
    if (value < 20)
        return 'none';
    else if (value < 40)
        return 'weak';
    else if (value < 50)
        return 'ok';
    else if (value < 80)
        return 'good';
    else
        return 'excellent';
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

function launchSettingsPanel(panel, ...args) {
    const param = new GLib.Variant('(sav)',
        [panel, args.map(s => new GLib.Variant('s', s))]);
    const platformData = {
        'desktop-startup-id': new GLib.Variant('s',
            `_TIME${global.get_current_time()}`),
    };
    try {
        Gio.DBus.session.call(
            'org.gnome.Settings',
            '/org/gnome/Settings',
            'org.freedesktop.Application',
            'ActivateAction',
            new GLib.Variant('(sava{sv})',
                ['launch-panel', [param], platformData]),
            null,
            Gio.DBusCallFlags.NONE,
            -1,
            null);
    } catch (e) {
        log(`Failed to launch Settings panel: ${e.message}`);
    }
}

var NMConnectionItem = class extends Signals.EventEmitter {
    constructor(section, connection) {
        super();

        this._section = section;
        this._connection = connection;
        this._activeConnection = null;

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
        this._activeConnection?.disconnectObject(this);
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
        this._activeConnection?.disconnectObject(this);

        this._activeConnection = activeConnection;

        this._activeConnection?.connectObject('notify::state',
            this._connectionStateChanged.bind(this), this);

        this._sync();
    }
};

var NMConnectionSection = class NMConnectionSection extends Signals.EventEmitter {
    constructor(client) {
        super();

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

        this._client.connectObject('notify::connectivity',
            this._iconChanged.bind(this), this);
    }

    destroy() {
        this._client.disconnectObject(this);
        this.item.destroy();
    }

    _iconChanged() {
        this._sync();
        this.emit('icon-changed');
    }

    _sync() {
        let nItems = this._connectionItems.size;

        this._radioSection.actor.visible = nItems > 1;
        this._labelSection.actor.visible = nItems == 1;

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
        item.connect('activation-failed', () => this.emit('activation-failed'));
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

var NMDeviceItem = class NMDeviceItem extends NMConnectionSection {
    constructor(client, device) {
        super(client);

        if (this.constructor === NMDeviceItem)
            throw new TypeError(`Cannot instantiate abstract type ${this.constructor.name}`);

        this._device = device;
        this._deviceName = '';

        this._autoConnectItem = this.item.menu.addAction(_("Connect"), this._autoConnect.bind(this));
        this._deactivateItem = this._radioSection.addAction(_("Turn Off"), this.deactivateConnection.bind(this));

        this._client.connectObject(
            'notify::primary-connection', () => this._iconChanged(),
            this);

        this._device.connectObject(
            'state-changed', this._deviceStateChanged.bind(this),
            'notify::active-connection', this._activeConnectionChanged.bind(this),
            this);
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
        this._device.disconnectObject(this);

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
            reason != NM.DeviceStateReason.NO_SECRETS)
            this.emit('activation-failed');

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

    setDeviceName(name) {
        this._deviceName = name;
        this._sync();
    }

    _getDescription() {
        return this._deviceName;
    }

    _sync() {
        let nItems = this._connectionItems.size;
        this._autoConnectItem.visible = nItems === 0;
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
        return this._getDescription();
    }
};

var NMWiredDeviceItem = class extends NMDeviceItem {
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

var NMModemDeviceItem = class extends NMDeviceItem {
    constructor(client, device) {
        super(client, device);

        const settingsPanel = this._useWwanPanel()
            ? 'gnome-wwan-panel.desktop'
            : 'gnome-network-panel.desktop';

        this.item.menu.addSettingsAction(_('Mobile Broadband Settings'), settingsPanel);

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

        this._mobileDevice?.connectObject(
            'notify::operator-name', this._sync.bind(this),
            'notify::signal-quality', () => this._iconChanged(), this);

        Main.sessionMode.connectObject('updated',
            this._sessionUpdated.bind(this), this);
        this._sessionUpdated();
    }

    get category() {
        return NMConnectionCategory.WWAN;
    }

    _useWwanPanel() {
        // Currently, wwan panel doesn't support CDMA_EVDO modems
        const supportedCaps =
            NM.DeviceModemCapabilities.GSM_UMTS |
            NM.DeviceModemCapabilities.LTE;
        return this._device.current_capabilities & supportedCaps;
    }

    _autoConnect() {
        if (this._useWwanPanel())
            launchSettingsPanel('wwan', 'show-device', this._device.udi);
        else
            launchSettingsPanel('network', 'connect-3g', this._device.get_path());
    }

    _sessionUpdated() {
        this._autoConnectItem.sensitive = Main.sessionMode.hasWindows;
    }

    destroy() {
        this._mobileDevice?.disconnectObject(this);
        Main.sessionMode.disconnectObject(this);

        super.destroy();
    }

    _getStatus() {
        return this._mobileDevice?.operator_name || this._getDescription();
    }

    _getMenuIcon() {
        if (!this._device.active_connection)
            return 'network-cellular-disabled-symbolic';

        return this.getIndicatorIcon();
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

var NMBluetoothDeviceItem = class extends NMDeviceItem {
    constructor(client, device) {
        super(client, device);

        this.item.menu.addSettingsAction(_("Bluetooth Settings"), 'gnome-network-panel.desktop');
    }

    get category() {
        return NMConnectionCategory.BLUETOOTH;
    }

    _getDescription() {
        return this._device.name;
    }

    getConnectLabel() {
        return _("Connect to Internet");
    }

    _getMenuIcon() {
        if (!this._device.active_connection)
            return 'network-cellular-disabled-symbolic';

        return this.getIndicatorIcon();
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

const WirelessNetwork = GObject.registerClass({
    Properties: {
        'name': GObject.ParamSpec.string(
            'name', '', '',
            GObject.ParamFlags.READABLE,
            ''),
        'icon-name': GObject.ParamSpec.string(
            'icon-name', '', '',
            GObject.ParamFlags.READABLE,
            ''),
        'secure': GObject.ParamSpec.boolean(
            'secure', '', '',
            GObject.ParamFlags.READABLE,
            false),
        'is-active': GObject.ParamSpec.boolean(
            'is-active', '', '',
            GObject.ParamFlags.READABLE,
            false),
    },
    Signals: {
        'destroy': {},
    },
}, class WirelessNetwork extends GObject.Object {
    static _securityTypes =
        Object.values(NM.UtilsSecurityType).sort((a, b) => b - a);

    _init(device) {
        super._init();

        this._device = device;

        this._device.connectObject(
            'notify::active-access-point', () => this.notify('is-active'),
            this);

        this._accessPoints = new Set();
        this._connections = [];
        this._name = '';
        this._ssid = null;
        this._bestAp = null;
        this._mode = 0;
        this._securityType = NM.UtilsSecurityType.NONE;
    }

    get _strength() {
        return this._bestAp?.strength ?? 0;
    }

    get name() {
        return this._name;
    }

    get icon_name() {
        if (this._mode === NM80211Mode.ADHOC)
            return 'network-workgroup-symbolic';

        if (!this._bestAp)
            return '';

        return `network-wireless-signal-${signalToIcon(this._bestAp.strength)}-symbolic`;
    }

    get secure() {
        return this._securityType !== NM.UtilsSecurityType.NONE;
    }

    get is_active() {
        return this._accessPoints.has(this._device.activeAccessPoint);
    }

    hasAccessPoint(ap) {
        return this._accessPoints.has(ap);
    }

    hasAccessPoints() {
        return this._accessPoints.size > 0;
    }

    checkAccessPoint(ap) {
        if (!ap.get_ssid())
            return false;

        const secType = this._getApSecurityType(ap);
        if (secType === NM.UtilsSecurityType.INVALID)
            return false;

        if (this._accessPoints.size === 0)
            return true;

        return this._ssid.equal(ap.ssid) &&
            this._mode === ap.mode &&
            this._securityType === secType;
    }

    /**
     * @param {NM.AccessPoint} ap - an access point
     * @returns {bool} - whether the access point was added
     */
    addAccessPoint(ap) {
        if (!this.checkAccessPoint(ap))
            return false;

        if (this._accessPoints.size === 0) {
            this._ssid = ap.get_ssid();
            this._mode = ap.mode;
            this._securityType = this._getApSecurityType(ap);
            this._name = NM.utils_ssid_to_utf8(this._ssid.get_data()) || '<unknown>';

            this.notify('name');
            this.notify('secure');
        }

        const wasActive = this.is_active;
        this._accessPoints.add(ap);

        ap.connectObject(
            'notify::strength', () => this._updateBestAp(),
            this);
        this._updateBestAp();

        if (wasActive !== this.is_active)
            this.notify('is-active');

        return true;
    }

    /**
     * @param {NM.AccessPoint} ap - an access point
     * @returns {bool} - whether the access point was removed
     */
    removeAccessPoint(ap) {
        const wasActive = this.is_active;
        if (!this._accessPoints.delete(ap))
            return false;

        this._updateBestAp();

        if (wasActive !== this.is_active)
            this.notify('is-active');

        return true;
    }

    /**
     * @param {WirelessNetwork} other - network to compare with
     * @returns {number} - the sort order
     */
    compare(other) {
        // place known connections first
        const cmpConnections = other.hasConnections() - this.hasConnections();
        if (cmpConnections !== 0)
            return cmpConnections;

        const cmpAps = other.hasAccessPoints() - this.hasAccessPoints();
        if (cmpAps !== 0)
            return cmpAps;

        // place stronger connections first
        const cmpStrength = other._strength - this._strength;
        if (cmpStrength !== 0)
            return cmpStrength;

        // place secure connections first
        const cmpSec = other.secure - this.secure;
        if (cmpSec !== 0)
            return cmpSec;

        // sort alphabetically
        return GLib.utf8_collate(this._name, other._name);
    }

    hasConnections() {
        return this._connections.length > 0;
    }

    checkConnections(connections) {
        const aps = [...this._accessPoints];
        this._connections = connections.filter(
            c => aps.some(ap => ap.connection_valid(c)));
    }

    canAutoconnect() {
        const canAutoconnect =
            this._securityTypes !== NM.UtilsSecurityType.WPA_ENTERPRISE &&
            this._securityTypes !== NM.UtilsSecurityType.WPA2_ENTERPRISE;
        return canAutoconnect;
    }

    activate() {
        const [ap] = this._accessPoints;
        let [conn] = this._connections;
        if (conn) {
            this._device.client.activate_connection_async(conn, this._device, null, null, null);
        } else if (!this.canAutoconnect()) {
            launchSettingsPanel('wifi', 'connect-8021x-wifi',
                this._getDeviceDBusPath(), ap.get_path());
        } else {
            conn = new NM.SimpleConnection();
            this._device.client.add_and_activate_connection_async(
                conn, this._device, ap.get_path(), null, null);
        }
    }

    destroy() {
        this.emit('destroy');
    }

    _getDeviceDBusPath() {
        // nm_object_get_path() is shadowed by nm_device_get_path()
        return NM.Object.prototype.get_path.call(this._device);
    }

    _getApSecurityType(ap) {
        const {wirelessCapabilities: caps} = this._device;
        const {flags, wpaFlags, rsnFlags} = ap;
        const haveAp = true;
        const adHoc = ap.mode === NM80211Mode.ADHOC;
        const bestType = WirelessNetwork._securityTypes
            .find(t => NM.utils_security_valid(t, caps, haveAp, adHoc, flags, wpaFlags, rsnFlags));
        return bestType ?? NM.UtilsSecurityType.INVALID;
    }

    _updateBestAp() {
        const [bestAp] =
            [...this._accessPoints].sort((a, b) => b.strength - a.strength);

        if (this._bestAp === bestAp)
            return;

        this._bestAp = bestAp;
        this.notify('icon-name');
    }
});
registerDestroyableType(WirelessNetwork);

var NMWirelessDialogItem = GObject.registerClass({
    Signals: {
        'selected': {},
    },
}, class NMWirelessDialogItem extends St.BoxLayout {
    _init(network) {
        this._network = network;

        super._init({
            style_class: 'nm-dialog-item',
            can_focus: true,
            reactive: true,
        });

        let action = new Clutter.ClickAction();
        action.connect('clicked', () => this.grab_key_focus());
        this.add_action(action);

        this._label = new St.Label({
            x_expand: true,
        });

        this.label_actor = this._label;
        this.add_child(this._label);

        this._selectedIcon = new St.Icon({
            style_class: 'nm-dialog-icon nm-dialog-network-selected',
            icon_name: 'object-select-symbolic',
        });
        this.add(this._selectedIcon);

        this._icons = new St.BoxLayout({
            style_class: 'nm-dialog-icons',
            x_align: Clutter.ActorAlign.END,
        });
        this.add_child(this._icons);

        this._secureIcon = new St.Icon({
            style_class: 'nm-dialog-icon',
        });
        this._icons.add_actor(this._secureIcon);

        this._signalIcon = new St.Icon({
            style_class: 'nm-dialog-icon',
        });
        this._icons.add_actor(this._signalIcon);

        this._network.bind_property('icon-name',
            this._signalIcon, 'icon-name',
            GObject.BindingFlags.SYNC_CREATE);
        this._network.bind_property('name',
            this._label, 'text',
            GObject.BindingFlags.SYNC_CREATE);
        this._network.bind_property('is-active',
            this._selectedIcon, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        this._network.bind_property_full('secure',
            this._secureIcon, 'icon-name',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, source) => [true, source ? 'network-wireless-encrypted-symbolic' : ''],
            null);
    }

    get network() {
        return this._network;
    }

    vfunc_key_focus_in() {
        this.emit('selected');
    }
});

var NMWirelessDialog = GObject.registerClass(
class NMWirelessDialog extends ModalDialog.ModalDialog {
    _init(client, device) {
        super._init({ styleClass: 'nm-dialog' });

        this._client = client;
        this._device = device;

        this._client.connectObject('notify::wireless-enabled',
            this._syncView.bind(this), this);

        this._rfkill = Rfkill.getRfkillManager();
        this._rfkill.connectObject(
            'notify::airplane-mode', () => this._syncView(),
            'notify::hw-airplane-mode', () => this._syncView(),
            this);

        this._networkItems = new Map();
        this._buildLayout();

        let connections = client.get_connections();
        this._connections = connections.filter(
            connection => device.connection_valid(connection));

        device.connectObject(
            'notify::active-access-point', () => this._updateSensitivity(),
            'notify::available-connections', () => this._availableConnectionsChanged(),
            'access-point-added', (d, ap) => {
                this._addAccessPoint(ap);
                this._syncNetworksList();
                this._syncView();
            },
            'access-point-removed', (d, ap) => {
                this._removeAccessPoint(ap);
                this._syncNetworksList();
                this._syncView();
            },
            this);

        for (const ap of this._device.get_access_points())
            this._addAccessPoint(ap);

        this._selectedNetwork = null;
        this._availableConnectionsChanged();
        this._updateSensitivity();
        this._syncView();

        this._scanTimeoutId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 15, this._onScanTimeout.bind(this));
        GLib.Source.set_name_by_id(this._scanTimeoutId, '[gnome-shell] this._onScanTimeout');
        this._onScanTimeout();

        let id = Main.sessionMode.connect('updated', () => {
            if (Main.sessionMode.allowSettings)
                return;

            Main.sessionMode.disconnect(id);
            this.close();
        });

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onDestroy() {
        if (this._scanTimeoutId) {
            GLib.source_remove(this._scanTimeoutId);
            this._scanTimeoutId = 0;
        }

        if (this._syncVisibilityId) {
            Meta.later_remove(this._syncVisibilityId);
            this._syncVisibilityId = 0;
        }
    }

    _onScanTimeout() {
        this._device.request_scan_async(null, null);
        return GLib.SOURCE_CONTINUE;
    }

    _availableConnectionsChanged() {
        const connections = this._device.get_available_connections();
        for (const net of this._networkItems.keys())
            net.checkConnections(connections);
        this._syncNetworksList();
    }

    _updateSensitivity() {
        const connectSensitive =
            this._client.wireless_enabled && this._selectedNetwork && !this._selectedNetwork.isActive;
        this._connectButton.reactive = connectSensitive;
        this._connectButton.can_focus = connectSensitive;
    }

    _syncView() {
        if (this._rfkill.airplaneMode) {
            this._airplaneBox.show();

            this._airplaneIcon.icon_name = 'airplane-mode-symbolic';
            this._airplaneHeadline.text = _("Airplane Mode is On");
            this._airplaneText.text = _("Wi-Fi is disabled when airplane mode is on.");
            this._airplaneButton.label = _("Turn Off Airplane Mode");

            this._airplaneButton.visible = !this._rfkill.hwAirplaneMode;
            this._airplaneInactive.visible = this._rfkill.hwAirplaneMode;
            this._noNetworksBox.hide();
        } else if (!this._client.wireless_enabled) {
            this._airplaneBox.show();

            this._airplaneIcon.icon_name = 'dialog-information-symbolic';
            this._airplaneHeadline.text = _("Wi-Fi is Off");
            this._airplaneText.text = _("Wi-Fi needs to be turned on in order to connect to a network.");
            this._airplaneButton.label = _("Turn On Wi-Fi");

            this._airplaneButton.show();
            this._airplaneInactive.hide();
            this._noNetworksBox.hide();
        } else {
            this._airplaneBox.hide();

            this._noNetworksBox.visible = this._networkItems.size === 0;
        }

        if (this._noNetworksBox.visible)
            this._noNetworksSpinner.play();
        else
            this._noNetworksSpinner.stop();
    }

    _buildLayout() {
        let headline = new St.BoxLayout({ style_class: 'nm-dialog-header-hbox' });

        const icon = new St.Icon({
            style_class: 'nm-dialog-header-icon',
            icon_name: 'network-wireless-symbolic',
        });

        let titleBox = new St.BoxLayout({ vertical: true });
        const title = new St.Label({
            style_class: 'nm-dialog-header',
            text: _('Wi-Fi Networks'),
        });
        const subtitle = new St.Label({
            style_class: 'nm-dialog-subheader',
            text: _('Select a network'),
        });
        titleBox.add(title);
        titleBox.add(subtitle);

        headline.add(icon);
        headline.add(titleBox);

        this.contentLayout.style_class = 'nm-dialog-content';
        this.contentLayout.add(headline);

        this._stack = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            y_expand: true,
        });

        this._itemBox = new St.BoxLayout({ vertical: true });
        this._scrollView = new St.ScrollView({ style_class: 'nm-dialog-scroll-view' });
        this._scrollView.set_x_expand(true);
        this._scrollView.set_y_expand(true);
        this._scrollView.set_policy(St.PolicyType.NEVER,
                                    St.PolicyType.AUTOMATIC);
        this._scrollView.add_actor(this._itemBox);
        this._stack.add_child(this._scrollView);

        this._noNetworksBox = new St.BoxLayout({
            vertical: true,
            style_class: 'no-networks-box',
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this._noNetworksSpinner = new Animation.Spinner(16);
        this._noNetworksBox.add_actor(this._noNetworksSpinner);
        this._noNetworksBox.add_actor(new St.Label({
            style_class: 'no-networks-label',
            text: _('No Networks'),
        }));
        this._stack.add_child(this._noNetworksBox);

        this._airplaneBox = new St.BoxLayout({
            vertical: true,
            style_class: 'nm-dialog-airplane-box',
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._airplaneIcon = new St.Icon({ icon_size: 48 });
        this._airplaneHeadline = new St.Label({ style_class: 'nm-dialog-airplane-headline headline' });
        this._airplaneText = new St.Label({ style_class: 'nm-dialog-airplane-text' });

        let airplaneSubStack = new St.Widget({ layout_manager: new Clutter.BinLayout() });
        this._airplaneButton = new St.Button({ style_class: 'modal-dialog-button button' });
        this._airplaneButton.connect('clicked', () => {
            if (this._rfkill.airplaneMode)
                this._rfkill.airplaneMode = false;
            else
                this._client.wireless_enabled = true;
        });
        airplaneSubStack.add_actor(this._airplaneButton);
        this._airplaneInactive = new St.Label({
            style_class: 'nm-dialog-airplane-text',
            text: _('Use hardware switch to turn off'),
        });
        airplaneSubStack.add_actor(this._airplaneInactive);

        this._airplaneBox.add_child(this._airplaneIcon);
        this._airplaneBox.add_child(this._airplaneHeadline);
        this._airplaneBox.add_child(this._airplaneText);
        this._airplaneBox.add_child(airplaneSubStack);
        this._stack.add_child(this._airplaneBox);

        this.contentLayout.add_child(this._stack);

        this._disconnectButton = this.addButton({
            action: () => this.close(),
            label: _('Cancel'),
            key: Clutter.KEY_Escape,
        });
        this._connectButton = this.addButton({
            action: this._connect.bind(this),
            label: _('Connect'),
            key: Clutter.KEY_Return,
        });
    }

    _connect() {
        this._selectedNetwork?.activate();
        this.close();
    }

    _addAccessPoint(ap) {
        if (ap.get_ssid() == null) {
            // This access point is not visible yet
            // Wait for it to get a ssid
            ap.connectObject('notify::ssid', () => {
                if (!ap.ssid)
                    return;
                ap.disconnectObject(this);
                this._addAccessPoint(ap);
            }, this);
            return;
        }

        let network = [...this._networkItems.keys()]
            .find(n => n.checkAccessPoint(ap));

        if (!network) {
            network = new WirelessNetwork(this._device);
            network.connectObject(
                'notify::icon-name', () => this._syncNetworksList(),
                'notify::is-active', () => this._syncNetworksList(),
                this);
            const item = this._createNetworkItem(network);
            this._itemBox.add_child(item);
            this._networkItems.set(network, item);
        }

        network.addAccessPoint(ap);
    }

    _removeAccessPoint(ap) {
        const network = [...this._networkItems.keys()]
            .find(n => n.removeAccessPoint(ap));

        if (!network || network.hasAccessPoints())
            return;

        this._networkItems.get(network)?.destroy();
        this._networkItems.delete(network);
        network.destroy();
    }

    _syncNetworksList() {
        const {hasWindows} = Main.sessionMode;
        const sortedItems = [...this._networkItems.values()]
            .sort((one, two) => one.network.compare(two.network));

        for (const [index, item] of sortedItems.entries())
            this._itemBox.set_child_at_index(item, index);

        for (const [net, item] of this._networkItems) {
            item.visible =
                hasWindows || net.hasConnections() || net.canAutoconnect();
        }
    }

    _selectNetwork(network) {
        this._networkItems.get(this._selectedNetwork)?.remove_style_pseudo_class('selected');
        this._selectedNetwork = network;
        this._updateSensitivity();
        this._networkItems.get(this._selectedNetwork)?.add_style_pseudo_class('selected');
    }

    _createNetworkItem(network) {
        const item = new NMWirelessDialogItem(network);
        item.connect('selected', () => {
            Util.ensureActorVisibleInScrollView(this._scrollView, item);
            this._selectNetwork(network);
        });
        item.connect('destroy', () => {
            let keyFocus = global.stage.key_focus;
            if (keyFocus && keyFocus.contains(item))
                this._itemBox.grab_key_focus();
        });
        return item;
    }
});

var NMWirelessDeviceItem = class extends Signals.EventEmitter {
    constructor(client, device) {
        super();

        this._client = client;
        this._device = device;

        this._deviceName = '';

        this.item = new PopupMenu.PopupSubMenuMenuItem('', true);
        this.item.menu.addAction(_("Select Network"), this._showDialog.bind(this));

        this._toggleItem = new PopupMenu.PopupMenuItem('');
        this._toggleItem.connect('activate', this._toggleWifi.bind(this));
        this.item.menu.addMenuItem(this._toggleItem);

        this.item.menu.addSettingsAction(_("Wi-Fi Settings"), 'gnome-wifi-panel.desktop');

        this._client.connectObject(
            'notify::wireless-enabled', this._sync.bind(this),
            'notify::wireless-hardware-enabled', this._sync.bind(this),
            'notify::connectivity', () => this._iconChanged(),
            'notify::primary-connection', () => this._iconChanged(),
            this);

        this._device.connectObject(
            'notify::active-access-point', this._activeApChanged.bind(this),
            'state-changed', this._deviceStateChanged.bind(this), this);

        this._activeApChanged();
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
        this._device.disconnectObject(this);
        this._activeAccessPoint?.disconnectObject(this);
        this._client.disconnectObject(this);

        if (this._dialog) {
            this._dialog.destroy();
            this._dialog = null;
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
            reason != NM.DeviceStateReason.NO_SECRETS)
            this.emit('activation-failed');

        this._sync();
    }

    _toggleWifi() {
        this._client.wireless_enabled = !this._client.wireless_enabled;
    }

    _showDialog() {
        this._dialog = new NMWirelessDialog(this._client, this._device);
        this._dialog.connect('closed', this._dialogClosed.bind(this));
        this._dialog.open();
    }

    _dialogClosed() {
        this._dialog = null;
    }

    _activeApChanged() {
        this._activeAccessPoint?.disconnectObject(this);
        this._activeAccessPoint = this._device.active_access_point;
        this._activeAccessPoint?.connectObject(
            'notify::strength', () => this._iconChanged(),
            this);

        this._iconChanged();
    }

    _sync() {
        this._toggleItem.label.text = this._client.wireless_enabled ? _("Turn Off") : _("Turn On");
        this._toggleItem.visible = this._client.wireless_hardware_enabled;

        this.item.icon.icon_name = this._getMenuIcon();
        this.item.label.text = this._getStatus();
    }

    setDeviceName(name) {
        this._deviceName = name;
        this._sync();
    }

    _getStatus() {
        if (this._isHotSpotMaster())
            /* Translators: %s is a network identifier */
            return _('%s Hotspot').format(this._deviceName);

        if (this._activeAccessPoint)
            return ssidToLabel(this._activeAccessPoint.get_ssid());

        return this._deviceName;
    }

    _getMenuIcon() {
        if (!this._client.wireless_enabled)
            return 'network-wireless-disabled-symbolic';

        if (this._device.active_connection)
            return this.getIndicatorIcon();
        else
            return 'network-wireless-signal-none-symbolic';
    }

    _canReachInternet() {
        if (this._client.primary_connection !== this._device.active_connection)
            return true;

        return this._client.connectivity === NM.ConnectivityState.FULL;
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

        return ip4config.get_method() === NM.SETTING_IP4_CONFIG_METHOD_SHARED;
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

var NMVpnConnectionItem = class extends NMConnectionItem {
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
        this.emit('icon-changed');
    }

    _connectionStateChanged() {
        const state = this._activeConnection?.get_state();
        const reason = this._activeConnection?.get_state_reason();

        if (state === NM.ActiveConnectionState.DEACTIVATED &&
            reason !== NM.ActiveConnectionStateReason.NO_SECRETS &&
            reason !== NM.ActiveConnectionStateReason.USER_DISCONNECTED)
            this.emit('activation-failed');

        this.emit('icon-changed');
        super._connectionStateChanged();
    }

    getIndicatorIcon() {
        if (this._activeConnection) {
            if (this._activeConnection.state < NM.ActiveConnectionState.ACTIVATED)
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
        this.item.visible = nItems > 0;

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
        return this.getIndicatorIcon() || 'network-vpn-disabled-symbolic';
    }

    activateConnection(connection) {
        this._client.activate_connection_async(connection, null, null, null, null);
    }

    deactivateConnection(activeConnection) {
        this._client.deactivate_connection(activeConnection, null);
    }

    setActiveConnections(vpnConnections) {
        let connections = this._connectionItems.values();
        for (let item of connections)
            item.setActiveConnection(null);

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

var NMDeviceSection = class extends PopupMenu.PopupMenuSection {
    constructor(deviceType) {
        super();

        this._deviceType = deviceType;

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
        throw new GObject.NotImplementedError();
    }

    _getSummaryLabel() {
        throw new GObject.NotImplementedError();
    }
};

class NMWirelessSection extends NMDeviceSection {
    constructor() {
        super(NM.DeviceType.WIFI);
    }

    _getSummaryIcon() {
        return 'network-wireless-symbolic';
    }

    _getSummaryLabel(nDevices) {
        return ngettext(
            '%s Wi-Fi Connection',
            '%s Wi-Fi Connections',
            nDevices).format(nDevices);
    }
}

class NMWiredSection extends NMDeviceSection {
    constructor() {
        super(NM.DeviceType.ETHERNET);
    }

    _getSummaryIcon() {
        return 'network-wired-symbolic';
    }

    _getSummaryLabel(nDevices) {
        return ngettext(
            '%s Wired Connection',
            '%s Wired Connections',
            nDevices).format(nDevices);
    }
}

class NMBluetoothSection extends NMDeviceSection {
    constructor() {
        super(NM.DeviceType.BT);
    }

    _getSummaryIcon() {
        return 'network-wireless-symbolic';
    }

    _getSummaryLabel(nDevices) {
        return ngettext(
            '%s Bluetooth Connection',
            '%s Bluetooth Connections',
            nDevices).format(nDevices);
    }
}

class NMModemSection extends NMDeviceSection {
    constructor() {
        super(NM.DeviceType.MODEM);
    }

    _getSummaryIcon() {
        return 'network-wireless-symbolic';
    }

    _getSummaryLabel(nDevices) {
        return ngettext(
            '%s Modem Connection',
            '%s Modem Connections',
            nDevices).format(nDevices);
    }
}

var NMApplet = GObject.registerClass(
class Indicator extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        this._primaryIndicator = this._addIndicator();
        this._vpnIndicator = this._addIndicator();

        // Device types
        this._dtypes = { };
        this._dtypes[NM.DeviceType.ETHERNET] = NMWiredDeviceItem;
        this._dtypes[NM.DeviceType.WIFI] = NMWirelessDeviceItem;
        this._dtypes[NM.DeviceType.MODEM] = NMModemDeviceItem;
        this._dtypes[NM.DeviceType.BT] = NMBluetoothDeviceItem;

        // Connection types
        this._ctypes = { };
        this._ctypes[NM.SETTING_WIRED_SETTING_NAME] = NMConnectionCategory.WIRED;
        this._ctypes[NM.SETTING_WIRELESS_SETTING_NAME] = NMConnectionCategory.WIRELESS;
        this._ctypes[NM.SETTING_BLUETOOTH_SETTING_NAME] = NMConnectionCategory.BLUETOOTH;
        this._ctypes[NM.SETTING_CDMA_SETTING_NAME] = NMConnectionCategory.WWAN;
        this._ctypes[NM.SETTING_GSM_SETTING_NAME] = NMConnectionCategory.WWAN;
        this._ctypes[NM.SETTING_VPN_SETTING_NAME] = NMConnectionCategory.VPN;
        this._ctypes[NM.SETTING_WIREGUARD_SETTING_NAME] = NMConnectionCategory.VPN;

        this._getClient().catch(logError);
    }

    async _getClient() {
        this._client = await NM.Client.new_async(null);

        this._activeConnections = [];
        this._connections = [];
        this._connectivityQueue = new Set();

        this._mainConnection = null;

        this._notification = null;

        this._nmDevices = [];

        this._wiredSection = new NMWiredSection();
        this._wirelessSection = new NMWirelessSection();
        this._modemSection = new NMModemSection();
        this._btSection = new NMBluetoothSection();

        this._deviceSections = new Map([
            [NMConnectionCategory.WIRED, this._wiredSection],
            [NMConnectionCategory.WIRELESS, this._wirelessSection],
            [NMConnectionCategory.WWAN, this._modemSection],
            [NMConnectionCategory.BLUETOOTH, this._btSection],
        ]);
        for (const section of this._deviceSections.values())
            this.menu.addMenuItem(section);

        this._vpnSection = new NMVpnSection(this._client);
        this._vpnSection.connect('activation-failed', this._onActivationFailed.bind(this));
        this._vpnSection.connect('icon-changed', this._updateIcon.bind(this));
        this.menu.addMenuItem(this._vpnSection.item);

        this._readConnections();
        this._readDevices();
        this._syncMainConnection();
        this._syncVpnConnections();

        this._client.bind_property('nm-running',
            this, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        this._client.bind_property('networking-enabled',
            this.menu.actor, 'visible',
            GObject.BindingFlags.SYNC_CREATE);

        this._client.connectObject(
            'notify::state', () => this._updateIcon(),
            'notify::primary-connection', () => this._syncMainConnection(),
            'notify::activating-connection', () => this._syncMainConnection(),
            'notify::active-connections', () => this._syncVpnConnections(),
            'notify::connectivity', () => this._syncConnectivity(),
            'device-added', this._deviceAdded.bind(this),
            'device-removed', this._deviceRemoved.bind(this),
            'connection-added', this._connectionAdded.bind(this),
            'connection-removed', this._connectionRemoved.bind(this),
            this);

        try {
            this._configPermission = await Polkit.Permission.new(
                'org.freedesktop.NetworkManager.network-control', null, null);
        } catch (e) {
            log(`No permission to control network connections: ${e}`);
            this._configPermission = null;
        }

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
    }

    _sessionUpdated() {
        const sensitive =
            !Main.sessionMode.isLocked &&
            this._configPermission && this._configPermission.allowed;
        this.menu.setSensitive(sensitive);
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

    _onActivationFailed() {
        this._notification?.destroy();

        const source = new MessageTray.Source(
            _('Network Manager'), 'network-error-symbolic');
        source.policy =
            new MessageTray.NotificationApplicationPolicy('gnome-network-panel');

        this._notification = new MessageTray.Notification(source,
            _('Connection failed'),
            _('Activation of network connection failed'));
        this._notification.setUrgency(MessageTray.Urgency.HIGH);
        this._notification.setTransient(true);
        this._notification.connect('destroy',
            () => (this._notification = null));

        Main.messageTray.add(source);
        source.showNotification(this._notification);
    }

    _syncDeviceNames() {
        let names = NM.Device.disambiguate_names(this._nmDevices);
        for (let i = 0; i < this._nmDevices.length; i++) {
            let device = this._nmDevices[i];
            let name = names[i];
            if (device._delegate)
                device._delegate.setDeviceName(name);
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
        wrapper.connectObject('activation-failed',
            this._onActivationFailed.bind(this), this);

        const {section} = this._deviceSections.get(wrapper.category);
        section.addMenuItem(wrapper.item);

        const {devices} = this._deviceSections.get(wrapper.category);
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
        wrapper.disconnectObject(this);
        wrapper.destroy();

        const {devices} = this._deviceSections.get(wrapper.category);
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
        this._mainConnection?._primaryDevice?.disconnectObject(this);
        this._mainConnection?.disconnectObject(this);

        this._mainConnection = this._getMainConnection();

        if (this._mainConnection) {
            this._mainConnection._primaryDevice?.connectObject('icon-changed',
                this._updateIcon.bind(this), this);
            this._mainConnection.connectObject('notify::state',
                this._mainConnectionStateChanged.bind(this), this);
            this._mainConnectionStateChanged();
        }

        this._updateIcon();
        this._syncConnectivity();
    }

    _syncVpnConnections() {
        let activeConnections = this._client.get_active_connections() || [];
        let vpnConnections = activeConnections.filter(
            a => a instanceof NM.VpnConnection || a.get_connection_type() === 'wireguard');
        vpnConnections.forEach(a => {
            ensureActiveConnectionProps(a);
        });
        this._vpnSection.setActiveConnections(vpnConnections);

        this._updateIcon();
    }

    _mainConnectionStateChanged() {
        if (this._mainConnection.state === NM.ActiveConnectionState.ACTIVATED)
            this._notification?.destroy();
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
        if (this._connections.includes(connection)) {
            // connection was already seen
            return;
        }

        connection.connectObject('changed',
            this._updateConnection.bind(this), this);

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
            const {devices} = this._deviceSections.get(section);
            for (let i = 0; i < devices.length; i++) {
                if (devices[i] instanceof NMConnectionSection)
                    devices[i].removeConnection(connection);
            }
        }

        connection.disconnectObject(this);
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
            const {devices} = this._deviceSections.get(section);
            devices.forEach(wrapper => {
                if (wrapper instanceof NMConnectionSection)
                    wrapper.checkConnection(connection);
            });
        }
    }

    _flushConnectivityQueue() {
        for (let item of this._connectivityQueue)
            this._portalHelperProxy?.CloseAsync(item);
        this._connectivityQueue.clear();
    }

    _closeConnectivityCheck(path) {
        if (this._connectivityQueue.delete(path))
            this._portalHelperProxy?.CloseAsync(path);
    }

    async _portalHelperDone(proxy, emitter, parameters) {
        let [path, result] = parameters;

        if (result == PortalHelperResult.CANCELLED) {
            // Keep the connection in the queue, so the user is not
            // spammed with more logins until we next flush the queue,
            // which will happen once they choose a better connection
            // or we get to full connectivity through other means
        } else if (result == PortalHelperResult.COMPLETED) {
            this._closeConnectivityCheck(path);
        } else if (result == PortalHelperResult.RECHECK) {
            try {
                const state = await this._client.check_connectivity_async(null);
                if (state >= NM.ConnectivityState.FULL)
                    this._closeConnectivityCheck(path);
            } catch (e) { }
        } else {
            log(`Invalid result from portal helper: ${result}`);
        }
    }

    async _syncConnectivity() {
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
            isPortal ||= this._client.connectivity < NM.ConnectivityState.FULL;
        if (!isPortal || Main.sessionMode.isGreeter)
            return;

        let path = this._mainConnection.get_path();
        if (this._connectivityQueue.has(path))
            return;

        let timestamp = global.get_current_time();
        if (!this._portalHelperProxy) {
            this._portalHelperProxy = new Gio.DBusProxy({
                g_connection: Gio.DBus.session,
                g_name: 'org.gnome.Shell.PortalHelper',
                g_object_path: '/org/gnome/Shell/PortalHelper',
                g_interface_name: PortalHelperInfo.name,
                g_interface_info: PortalHelperInfo,
            });
            this._portalHelperProxy.connectSignal('Done',
                () => this._portalHelperDone().catch(logError));

            try {
                await this._portalHelperProxy.init_async(
                    GLib.PRIORITY_DEFAULT, null);
            } catch (e) {
                console.error(`Error launching the portal helper: ${e.message}`);
            }
        }

        this._portalHelperProxy?.AuthenticateAsync(path, '', timestamp).catch(logError);

        this._connectivityQueue.add(path);
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
        this._vpnIndicator.visible = this._vpnIndicator.icon_name !== null;
    }
});
