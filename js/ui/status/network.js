// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const NetworkManager = imports.gi.NetworkManager;
const NMClient = imports.gi.NMClient;
const NMGtk = imports.gi.NMGtk;
const Signals = imports.signals;
const St = imports.gi.St;

const Hash = imports.misc.hash;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const MessageTray = imports.ui.messageTray;
const NotificationDaemon = imports.ui.notificationDaemon;
const ModalDialog = imports.ui.modalDialog;
const ModemManager = imports.misc.modemManager;
const Util = imports.misc.util;

const NMConnectionCategory = {
    INVALID: 'invalid',
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

// small optimization, to avoid using [] all the time
const NM80211Mode = NetworkManager['80211Mode'];
const NM80211ApFlags = NetworkManager['80211ApFlags'];
const NM80211ApSecurityFlags = NetworkManager['80211ApSecurityFlags'];

function ssidCompare(one, two) {
    if (!one || !two)
        return false;
    if (one.length != two.length)
        return false;
    for (let i = 0; i < one.length; i++) {
        if (one[i] != two[i])
            return false;
    }
    return true;
}

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
    let label = NetworkManager.utils_ssid_to_utf8(ssid);
    if (!label)
        label = _("<unknown>");
    return label;
}

const NMConnectionItem = new Lang.Class({
    Name: 'NMConnectionItem',

    _init: function(section, connection) {
        this._section = section;
        this._connection = connection;
        this._activeConnection = null;
        this._activeConnectionChangedId = 0;

        this.labelItem = new PopupMenu.PopupMenuItem('');
        this.labelItem.connect('activate', Lang.bind(this, this._toggle));

        this.switchItem = new PopupMenu.PopupSwitchMenuItem(connection.get_id(), false);
        this.switchItem.connect('toggled', Lang.bind(this, this._toggle));

        this._sync();
    },

    destroy: function() {
        this.labelItem.destroy();
        this.switchItem.destroy();
    },

    getName: function() {
        return this.connection.get_id();
    },

    isActive: function() {
        if (this._activeConnection == null)
            return false;

        return this._activeConnection.state == NetworkManager.ActiveConnectionState.ACTIVATED;
    },

    _sync: function() {
        let isActive = this.isActive();
        this.labelItem.label.text = isActive ? _("Turn Off") : _("Connect");
        this.switchItem.setToggleState(isActive);
        this.switchItem.setStatus(this._getStatus());
        this.emit('icon-changed');
    },

    _toggle: function() {
        if (this._activeConnection == null)
            this._section.activateConnection(this._connection);
        else
            this._section.deactivateConnection(this._activeConnection);

        this._sync();
    },

    _getStatus: function() {
        return null;
    },

    _connectionStateChanged: function(ac, newstate, reason) {
        this._sync();
    },

    setActiveConnection: function(activeConnection) {
        if (this._activeConnectionChangedId > 0) {
            this._activeConnection.disconnect(this._activeConnectionChangedId);
            this._activeConnectionChangedId = 0;
        }

        this._activeConnection = activeConnection;

        if (this._activeConnection)
            this._activeConnectionChangedId = this._activeConnection.connect('state-changed',
                                                                             Lang.bind(this, this._connectionStateChanged));

        this._sync();
    },
});
Signals.addSignalMethods(NMConnectionItem.prototype);

const NMConnectionSection = new Lang.Class({
    Name: 'NMConnectionSection',
    Abstract: true,

    _init: function(client) {
        this._client = client;

        this._connectionItems = new Hash.Map();
        this._connections = [];

        this._labelSection = new PopupMenu.PopupMenuSection();
        this._switchSection = new PopupMenu.PopupMenuSection();

        this.item = new PopupMenu.PopupSubMenuMenuItem('', true);
        this.item.menu.addMenuItem(this._labelSection);
        this.item.menu.addMenuItem(this._switchSection);

        this.connect('icon-changed', Lang.bind(this, this._sync));
    },

    destroy: function() {
        this.statusItem.destroy();
        this.section.destroy();
    },

    _sync: function() {
        let nItems = this._connectionItems.size();

        this._switchSection.actor.visible = (nItems > 1);
        this._labelSection.actor.visible = (nItems == 1);

        this.item.status.text = this._getStatus();
        this.item.icon.icon_name = this._getMenuIcon();
        this.item.label.text = this._getDescription();
    },

    _getStatus: function() {
        let values = this._connectionItems.values();
        for (let i = 0; i < values; i++) {
            let item = values[i];
            if (item.isActive())
                return item.getName();
        }

        return _("Off");
    },

    _hasConnection: function(connection) {
        return this._connectionItems.has(connection.get_uuid());
    },

    _connectionValid: function(connection) {
        return true;
    },

    _connectionSortFunction: function(one, two) {
        if (one._timestamp == two._timestamp)
            return GLib.utf8_collate(one.get_id(), two.get_id());

        return two._timestamp - one._timestamp;
    },

    _makeConnectionItem: function(connection) {
        return new NMConnectionItem(this, connection);
    },

    checkConnection: function(connection) {
        if (!this._connectionValid(connection))
            return;

        if (this._hasConnection(connection))
            return;

        this._addConnection(connection);
    },

    _addConnection: function(connection) {
        let item = this._makeConnectionItem(connection);
        if (!item)
            return;

        item.connect('icon-changed', Lang.bind(this, function() {
            this.emit('icon-changed');
        }));
        item.connect('activation-failed', Lang.bind(this, function(item, reason) {
            this.emit('activation-failed', reason);
        }));

        let pos = Util.insertSorted(this._connections, connection, this._connectionSortFunction);
        this._labelSection.addMenuItem(item.labelItem, pos);
        this._switchSection.addMenuItem(item.switchItem, pos);
        this._connectionItems.set(connection.get_uuid(), item);
        this._sync();
    },

    removeConnection: function(connection) {
        this._connectionItems.get(connection.get_uuid()).destroy();
        this._connectionItems.delete(connection.get_uuid());

        let pos = this._connections.indexOf(connection);
        this._connections.splice(pos, 1);

        this._sync();
    },
});
Signals.addSignalMethods(NMConnectionSection.prototype);

const NMConnectionDevice = new Lang.Class({
    Name: 'NMConnectionDevice',
    Extends: NMConnectionSection,
    Abstract: true,

    _init: function(client, device) {
        this.parent(client);
        this._device = device;

        this._autoConnectItem = this.item.menu.addAction(_("Connect"), Lang.bind(this, this._autoConnect));
        this.item.menu.addSettingsAction(_("Network Settings"), 'gnome-network-panel.desktop');

        this._stateChangedId = this._device.connect('state-changed', Lang.bind(this, this._deviceStateChanged));
        this._activeConnectionChangedId = this._device.connect('notify::active-connection', Lang.bind(this, this._activeConnectionChanged));
    },

    destroy: function() {
        if (this._stateChangedId) {
            GObject.Object.prototype.disconnect.call(this._device, this._stateChangedId);
            this._stateChangedId = 0;
        }
        if (this._activeConnectionChangedId) {
            GObject.Object.prototype.disconnect.call(this._device, this._activeConnectionChangedId);
            this._stateChangedId = 0;
        }

        this.parent();
    },

    _activeConnectionChanged: function() {
        if (this._activeConnection) {
            let item = this._connectionItems.get(this._activeConnection._connection.get_uuid());
            item.setActiveConnection(null);
        }

        this._activeConnection = this._device.active_connection;

        if (this._activeConnection) {
            let item = this._connectionItems.get(this._activeConnection._connection.get_uuid());
            item.setActiveConnection(this._activeConnection);
        }
    },

    _deviceStateChanged: function(device, newstate, oldstate, reason) {
        if (newstate == oldstate) {
            log('device emitted state-changed without actually changing state');
            return;
        }

        /* Emit a notification if activation fails, but don't do it
           if the reason is no secrets, as that indicates the user
           cancelled the agent dialog */
        if (newstate == NetworkManager.DeviceState.FAILED &&
            reason != NetworkManager.DeviceStateReason.NO_SECRETS) {
            this.emit('activation-failed', reason);
        }

        this._sync();
    },

    _connectionValid: function(connection) {
        return this._device.connection_valid(connection);
    },

    activateConnection: function(connection) {
        this._client.activate_connection(connection, this._device, null, null);
    },

    deactivateConnection: function(activeConnection) {
        this._device.disconnect(null);
    },

    setDeviceDescription: function(desc) {
        this._description = desc;
        this._sync();
    },

    _getDescription: function() {
        return this._description;
    },

    _sync: function() {
        let nItems = this._connectionItems.size();
        this._autoConnectItem.actor.visible = (nItems == 0);
        this.parent();
    },

    _getStatus: function() {
        if (!this._device)
            return null;

        switch(this._device.state) {
        case NetworkManager.DeviceState.DISCONNECTED:
        case NetworkManager.DeviceState.ACTIVATED:
            return null;
        case NetworkManager.DeviceState.UNMANAGED:
            /* Translators: this is for network devices that are physically present but are not
               under NetworkManager's control (and thus cannot be used in the menu) */
            return _("unmanaged");
        case NetworkManager.DeviceState.DEACTIVATING:
            return _("disconnecting...");
        case NetworkManager.DeviceState.PREPARE:
        case NetworkManager.DeviceState.CONFIG:
        case NetworkManager.DeviceState.IP_CONFIG:
        case NetworkManager.DeviceState.IP_CHECK:
        case NetworkManager.DeviceState.SECONDARIES:
            return _("connecting...");
        case NetworkManager.DeviceState.NEED_AUTH:
            /* Translators: this is for network connections that require some kind of key or password */
            return _("authentication required");
        case NetworkManager.DeviceState.UNAVAILABLE:
            // This state is actually a compound of various states (generically unavailable,
            // firmware missing), that are exposed by different properties (whose state may
            // or may not updated when we receive state-changed).
            if (this._device.firmware_missing) {
                /* Translators: this is for devices that require some kind of firmware or kernel
                   module, which is missing */
                return _("firmware missing");
            }
            /* Translators: this is for a network device that cannot be activated (for example it
               is disabled by rfkill, or it has no coverage */
            return _("unavailable");
        case NetworkManager.DeviceState.FAILED:
            return _("connection failed");
        default:
            log('Device state invalid, is %d'.format(this._device.state));
            return 'invalid';
        }
    },
});

const NMDeviceModem = new Lang.Class({
    Name: 'NMDeviceModem',
    Extends: NMConnectionDevice,
    category: NMConnectionCategory.WWAN,

    _init: function(client, device) {
        this.parent(client, device);
        this._mobileDevice = null;

        let capabilities = device.current_capabilities;
        if (device.udi.indexOf('/org/freedesktop/ModemManager1/Modem') == 0)
            this._mobileDevice = new ModemManager.BroadbandModem(device.udi, capabilities);
        else if (capabilities & NetworkManager.DeviceModemCapabilities.GSM_UMTS)
            this._mobileDevice = new ModemManager.ModemGsm(device.udi);
        else if (capabilities & NetworkManager.DeviceModemCapabilities.CDMA_EVDO)
            this._mobileDevice = new ModemManager.ModemCdma(device.udi);
        else if (capabilities & NetworkManager.DeviceModemCapabilities.LTE)
            this._mobileDevice = new ModemManager.ModemGsm(device.udi);

        if (this._mobileDevice) {
            this._operatorNameId = this._mobileDevice.connect('notify::operator-name', Lang.bind(this, function() {
                if (this._operatorItem) {
                    let name = this._mobileDevice.operator_name;
                    if (name) {
                        this._operatorItem.label.text = name;
                        this._operatorItem.actor.show();
                    } else
                        this._operatorItem.actor.hide();
                }
            }));
            this._signalQualityId = this._mobileDevice.connect('notify::signal-quality', Lang.bind(this, function() {
                this.emit('icon-changed');
            }));
        }
    },

    _autoConnect: function() {
        Util.spawn(['gnome-control-center', 'network',
                    'connect-3g', this._device.get_path()]);
    },

    destroy: function() {
        if (this._operatorNameId) {
            this._mobileDevice.disconnect(this._operatorNameId);
            this._operatorNameId = 0;
        }
        if (this._signalQualityId) {
            this._mobileDevice.disconnect(this._signalQualityId);
            this._signalQualityId = 0;
        }

        this.parent();
    },

    _getMenuIcon: function() {
        if (this._device.active_connection)
            return this.getIndicatorIcon();
        else
            return 'network-cellular-signal-none-symbolic';
    },

    _getSignalIcon: function() {
        return 'network-cellular-signal-' + signalToIcon(this._mobileDevice.signal_quality) + '-symbolic';
    },

    getIndicatorIcon: function() {
        if (this._device.active_connection.state == NetworkManager.ActiveConnectionState.ACTIVATING)
            return 'network-cellular-acquiring-symbolic';

        if (!this._mobileDevice) {
            // this can happen for bluetooth in PAN mode
            return 'network-cellular-connected-symbolic';
        }

        return this._getSignalIcon();
    }
});

const NMDeviceBluetooth = new Lang.Class({
    Name: 'NMDeviceBluetooth',
    Extends: NMConnectionDevice,
    category: NMConnectionCategory.WWAN,

    _autoConnect: function() {
        // FIXME: DUN devices are configured like modems, so
        // We need to spawn the mobile wizard
        // but the network panel doesn't support bluetooth at the moment
        // so we just create an empty connection and hope
        // that this phone supports PAN

        let connection = new NetworkManager.Connection();
        this._client.add_and_activate_connection(connection, this._device, null, null);
        return true;
    },

    _getMenuIcon: function() {
        if (this._device.active_connection)
            return this.getIndicatorIcon();
        else
            return 'network-cellular-signal-none-symbolic';
    },

    getIndicatorIcon: function() {
        let state = this._device.active_connection.state;
        if (state == NetworkManager.ActiveConnectionState.ACTIVATING)
            return 'network-cellular-acquiring-symbolic';
        else if (state == NetworkManager.ActiveConnectionState.ACTIVATED)
            return 'network-cellular-connected-symbolic';
        else
            return 'network-cellular-signal-none-symbolic';
    }
});

const NMWirelessDialogItem = new Lang.Class({
    Name: 'NMWirelessDialogItem',

    _init: function(network) {
        this._network = network;
        this._ap = network.accessPoints[0];

        this.actor = new St.Button({ style_class: 'nm-dialog-item',
                                     can_focus: true,
                                     x_fill: true });
        this.actor.connect('key-focus-in', Lang.bind(this, function() {
            this.emit('selected');
        }));
        this.actor.connect('clicked', Lang.bind(this, function() {
            this.actor.grab_key_focus();
        }));

        this._content = new St.BoxLayout({ style_class: 'nm-dialog-item-box' });
        this.actor.set_child(this._content);

        let title = ssidToLabel(this._ap.get_ssid());
        this._label = new St.Label({ text: title });

        this.actor.label_actor = this._label;
        this._content.add(this._label, { expand: true, x_align: St.Align.START });

        this._icons = new St.BoxLayout({ style_class: 'nm-dialog-icons' });
        this._content.add(this._icons, { x_fill: false, x_align: St.Align.END });

        this._secureIcon = new St.Icon({ style_class: 'nm-dialog-icon' });
        if (this._ap._secType != NMAccessPointSecurity.NONE)
            this._secureIcon.icon_name = 'network-wireless-encrypted-symbolic';
        this._icons.add_actor(this._secureIcon);

        this._signalIcon = new St.Icon({ icon_name: this._getIcon(),
                                         style_class: 'nm-dialog-icon' });
        this._icons.add_actor(this._signalIcon);
    },

    updateBestAP: function(ap) {
        this._ap = ap;
        this._signalIcon.icon_name = this._getIcon();
    },

    _getIcon: function() {
        if (this._ap.mode == NM80211Mode.ADHOC)
            return 'network-workgroup-symbolic';
        else
            return 'network-wireless-signal-' + signalToIcon(this._ap.strength) + '-symbolic';
    }
});
Signals.addSignalMethods(NMWirelessDialogItem.prototype);

const NMWirelessDialog = new Lang.Class({
    Name: 'NMWirelessDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function(client, device, settings) {
        this.parent({ styleClass: 'nm-dialog' });

        this._client = client;
        this._device = device;

        this._networks = [];
        this._buildLayout();

        let connections = settings.list_connections();
        this._connections = connections.filter(Lang.bind(this, function(connection) {
            return device.connection_valid(connection);
        }));

        this._apAddedId = device.connect('access-point-added', Lang.bind(this, this._accessPointAdded));
        this._apRemovedId = device.connect('access-point-removed', Lang.bind(this, this._accessPointRemoved));

        // accessPointAdded will also create dialog items
        let accessPoints = device.get_access_points() || [ ];
        accessPoints.forEach(Lang.bind(this, function(ap) {
            this._accessPointAdded(this._device, ap);
        }));

        this._selectedNetwork = null;
        this._updateSensitivity();
    },

    destroy: function() {
        if (this._apAddedId) {
            GObject.Object.prototype.disconnect.call(this._device, this._apAddedId);
            this._apAddedId = 0;
        }

        if (this._apRemovedId) {
            GObject.Object.prototype.disconnect.call(this._device, this._apRemovedId);
            this._apRemovedId = 0;
        }

        this.parent();
    },

    _updateSensitivity: function() {
        let connectSensitive = this._selectedNetwork != null;
        this._connectButton.reactive = connectSensitive;
        this._connectButton.can_focus = connectSensitive;
    },

    _updateVisibility: function() {
        this._noNetworksLabel.visible = (this._networks.length == 0);
    },

    _buildLayout: function() {
        let headline = new St.BoxLayout({ style_class: 'nm-dialog-header-hbox' });

        let icon = new St.Icon({ style_class: 'nm-dialog-header-icon',
                                 icon_name: 'network-wireless-signal-excellent-symbolic' });

        let titleBox = new St.BoxLayout({ vertical: true });
        let title = new St.Label({ style_class: 'nm-dialog-header',
                                   text: _("Wi-Fi Networks") });
        let subtitle = new St.Label({ style_class: 'nm-dialog-subheader',
                                      text: _("Select a network") });
        titleBox.add(title);
        titleBox.add(subtitle);

        headline.add(icon);
        headline.add(titleBox);

        this.contentLayout.style_class = 'nm-dialog-content';
        this.contentLayout.add(headline);

        this._stack = new St.Widget({ layout_manager: new Clutter.BinLayout() });

        this._itemBox = new St.BoxLayout({ vertical: true });
        this._scrollView = new St.ScrollView({ style_class: 'nm-dialog-scroll-view' });
        this._scrollView.set_x_expand(true);
        this._scrollView.set_y_expand(true);
        this._scrollView.set_policy(Gtk.PolicyType.NEVER,
                                    Gtk.PolicyType.AUTOMATIC);
        this._scrollView.add_actor(this._itemBox);
        this._stack.add_child(this._scrollView);

        this._noNetworksLabel = new St.Label({ style_class: 'no-networks-label',
                                               x_align: Clutter.ActorAlign.CENTER,
                                               y_align: Clutter.ActorAlign.CENTER,
                                               text: _("No Networks") });
        this._stack.add_child(this._noNetworksLabel);

        this.contentLayout.add(this._stack, { expand: true });

        this._disconnectButton = this.addButton({ action: Lang.bind(this, this.close),
                                                  label: _("Cancel"),
                                                  key: Clutter.Escape });
        this._connectButton = this.addButton({ action: Lang.bind(this, this._connect),
                                               label: _("Connect"),
                                               key: Clutter.Return },
                                             { expand: true,
                                               x_fill: false,
                                               x_align: St.Align.END });
    },

    _connect: function() {
        let network = this._selectedNetwork;
        let accessPoints = network.accessPoints;
        if (network.connections.length > 0) {
            let connection = network.connections[0];
            for (let i = 0; i < accessPoints.length; i++) {
                if (accessPoints[i].connection_valid(connection)) {
                    this._client.activate_connection(connection, this._device, accessPoints[i].dbus_path, null);
                    break;
                }
            }
        } else {
            if ((accessPoints[0]._secType == NMAccessPointSecurity.WPA2_ENT)
                || (accessPoints[0]._secType == NMAccessPointSecurity.WPA_ENT)) {
                // 802.1x-enabled APs require further configuration, so they're
                // handled in gnome-control-center
                Util.spawn(['gnome-control-center', 'network', 'connect-8021x-wifi',
                            this._device.get_path(), accessPoints[0].dbus_path]);
            } else {
                let connection = new NetworkManager.Connection();
                this._client.add_and_activate_connection(connection, this._device, accessPoints[0].dbus_path, null)
            }
        }

        this.close();
    },

    _notifySsidCb: function(accessPoint) {
        if (accessPoint.get_ssid() != null) {
            accessPoint.disconnect(accessPoint._notifySsidId);
            accessPoint._notifySsidId = 0;
            this._accessPointAdded(this._device, accessPoint);
        }
    },

    _getApSecurityType: function(accessPoint) {
        if (accessPoint._secType)
            return accessPoint._secType;

        let flags = accessPoint.flags;
        let wpa_flags = accessPoint.wpa_flags;
        let rsn_flags = accessPoint.rsn_flags;
        let type;
        if (rsn_flags != NM80211ApSecurityFlags.NONE) {
            /* RSN check first so that WPA+WPA2 APs are treated as RSN/WPA2 */
            if (rsn_flags & NM80211ApSecurityFlags.KEY_MGMT_802_1X)
	        type = NMAccessPointSecurity.WPA2_ENT;
	    else if (rsn_flags & NM80211ApSecurityFlags.KEY_MGMT_PSK)
	        type = NMAccessPointSecurity.WPA2_PSK;
        } else if (wpa_flags != NM80211ApSecurityFlags.NONE) {
            if (wpa_flags & NM80211ApSecurityFlags.KEY_MGMT_802_1X)
                type = NMAccessPointSecurity.WPA_ENT;
            else if (wpa_flags & NM80211ApSecurityFlags.KEY_MGMT_PSK)
	        type = NMAccessPointSecurity.WPA_PSK;
        } else {
            if (flags & NM80211ApFlags.PRIVACY)
                type = NMAccessPointSecurity.WEP;
            else
                type = NMAccessPointSecurity.NONE;
        }

        // cache the found value to avoid checking flags all the time
        accessPoint._secType = type;
        return type;
    },

    _networkSortFunction: function(one, two) {
        let oneHasConnection = one.connections.length != 0;
        let twoHasConnection = two.connections.length != 0;

        // place known connections first
        // (-1 = good order, 1 = wrong order)
        if (oneHasConnection && !twoHasConnection)
            return -1;
        else if (!oneHasConnection && twoHasConnection)
            return 1;

        let oneStrength = one.accessPoints[0].strength;
        let twoStrength = two.accessPoints[0].strength;

        // place stronger connections first
        if (oneStrength != twoStrength)
            return oneStrength < twoStrength ? 1 : -1;

        let oneHasSecurity = one.security != NMAccessPointSecurity.NONE;
        let twoHasSecurity = two.security != NMAccessPointSecurity.NONE;

        // place secure connections first
        // (we treat WEP/WPA/WPA2 the same as there is no way to
        // take them apart from the UI)
        if (oneHasSecurity && !twoHasSecurity)
            return -1;
        else if (!oneHasSecurity && twoHasSecurity)
            return 1;

        // sort alphabetically
        return GLib.utf8_collate(one.ssidText, two.ssidText);
    },

    _networkCompare: function(network, accessPoint) {
        if (!ssidCompare(network.ssid, accessPoint.get_ssid()))
            return false;
        if (network.mode != accessPoint.mode)
            return false;
        if (network.security != this._getApSecurityType(accessPoint))
            return false;

        return true;
    },

    _findExistingNetwork: function(accessPoint) {
        for (let i = 0; i < this._networks.length; i++) {
            let network = this._networks[i];
            for (let j = 0; j < network.accessPoints.length; j++) {
                if (network.accessPoints[j] == accessPoint)
                    return { network: i, ap: j };
            }
        }

        return null;
    },

    _findNetwork: function(accessPoint) {
        if (accessPoint.get_ssid() == null)
            return -1;

        for (let i = 0; i < this._networks.length; i++) {
            if (this._networkCompare(this._networks[i], accessPoint))
                return i;
        }
        return -1;
    },

    _checkConnections: function(network, accessPoint) {
        this._connections.forEach(function(connection) {
            if (accessPoint.connection_valid(connection) &&
                network.connections.indexOf(connection) == -1) {
                network.connections.push(connection);
            }
        });
    },

    _accessPointAdded: function(device, accessPoint) {
        if (accessPoint.get_ssid() == null) {
            // This access point is not visible yet
            // Wait for it to get a ssid
            accessPoint._notifySsidId = accessPoint.connect('notify::ssid', Lang.bind(this, this._notifySsidCb));
            return;
        }

        let pos = this._findNetwork(accessPoint);
        let network;

        if (pos != -1) {
            network = this._networks[pos];
            if (network.accessPoints.indexOf(accessPoint) != -1) {
                log('Access point was already seen, not adding again');
                return;
            }

            Util.insertSorted(network.accessPoints, accessPoint, function(one, two) {
                return two.strength - one.strength;
            });
            network.item.updateBestAP(network.accessPoints[0]);
            this._checkConnections(network, accessPoint);

            this._resortItems();
        } else {
            network = { ssid: accessPoint.get_ssid(),
                        mode: accessPoint.mode,
                        security: this._getApSecurityType(accessPoint),
                        connections: [ ],
                        item: null,
                        accessPoints: [ accessPoint ]
                      };
            network.ssidText = ssidToLabel(network.ssid);
            this._checkConnections(network, accessPoint);

            let newPos = Util.insertSorted(this._networks, network, this._networkSortFunction);
            this._createNetworkItem(network);
            this._itemBox.insert_child_at_index(network.item.actor, newPos);
        }

        this._updateVisibility();
    },

    _accessPointRemoved: function(device, accessPoint) {
        let res = this._findExistingNetwork(accessPoint);

        if (res == null) {
            log('Removing an access point that was never added');
            return;
        }

        let network = this._networks[res.network];
        network.accessPoints.splice(res.ap, 1);

        if (network.accessPoints.length == 0) {
            network.item.destroy();
            this._networks.splice(res.network, 1);
        } else {
            network.item.updateBestAP(network.accessPoints[0]);
            this._resortItems();
        }

        this._updateVisibility();
    },

    _resortItems: function() {
        let adjustment = this._scrollView.vscroll.adjustment;
        let scrollValue = adjustment.value;

        this._itemBox.remove_all_children();
        this._networks.forEach(Lang.bind(this, function(network) {
            this._itemBox.add_child(network.item.actor);
        }));

        adjustment.value = scrollValue;
    },

    _selectNetwork: function(network) {
        if (this._selectedNetwork)
            this._selectedNetwork.item.actor.checked = false;

        this._selectedNetwork = network;
        this._updateSensitivity();

        if (this._selectedNetwork)
            this._selectedNetwork.item.actor.checked = true;
    },

    _createNetworkItem: function(network) {
        network.item = new NMWirelessDialogItem(network);
        network.item.connect('selected', Lang.bind(this, function() {
            Util.ensureActorVisibleInScrollView(this._scrollView, network.item.actor);
            this._selectNetwork(network);
        }));
    },
});

const NMDeviceWireless = new Lang.Class({
    Name: 'NMDeviceWireless',
    category: NMConnectionCategory.WIRELESS,

    _init: function(client, device, settings) {
        this._client = client;
        this._device = device;
        this._settings = settings;

        this._description = '';

        this.item = new PopupMenu.PopupSubMenuMenuItem('', true);
        this.item.menu.addAction(_("Select Network"), Lang.bind(this, this._showDialog));

        this._toggleItem = new PopupMenu.PopupMenuItem('');
        this._toggleItem.connect('activate', Lang.bind(this, this._toggleWifi));
        this.item.menu.addMenuItem(this._toggleItem);

        this.item.menu.addSettingsAction(_("Network Settings"), 'gnome-network-panel.desktop');

        this._wirelessEnabledChangedId = this._device.connect('notify::wireless-enabled', Lang.bind(this, this._sync));
        this._activeApChangedId = this._device.connect('notify::active-access-point', Lang.bind(this, this._activeApChanged));
        this._stateChangedId = this._device.connect('state-changed', Lang.bind(this, this._deviceStateChanged));

        this._sync();
    },

    destroy: function() {
        if (this._activeApChangedId) {
            GObject.Object.prototype.disconnect.call(this._device, this._activeApChangedId);
            this._activeApChangedId = 0;
        }
        if (this._stateChangedId) {
            GObject.Object.prototype.disconnect.call(this._device, this._stateChangedId);
            this._stateChangedId = 0;
        }
        if (this._strengthChangedId > 0) {
            this._activeAccessPoint.disconnect(this._strengthChangedId);
            this._strengthChangedId = 0;
        }

        this.item.destroy();
    },

    _deviceStateChanged: function(device, newstate, oldstate, reason) {
        if (newstate == oldstate) {
            log('device emitted state-changed without actually changing state');
            return;
        }

        /* Emit a notification if activation fails, but don't do it
           if the reason is no secrets, as that indicates the user
           cancelled the agent dialog */
        if (newstate == NetworkManager.DeviceState.FAILED &&
            reason != NetworkManager.DeviceStateReason.NO_SECRETS) {
            this.emit('activation-failed', reason);
        }

        this._sync();
    },

    _toggleWifi: function() {
        this._client.wireless_enabled = !this._client.wireless_enabled;
    },

    _showDialog: function() {
        this._dialog = new NMWirelessDialog(this._client, this._device, this._settings);
        this._dialog.connect('closed', Lang.bind(this, this._dialogClosed));
        this._dialog.open();
    },

    _dialogClosed: function() {
        this._dialog.destroy();
        this._dialog = null;
    },

    _strengthChanged: function() {
        this.emit('icon-changed');
    },

    _activeApChanged: function() {
        if (this._activeAccessPoint) {
            this._activeAccessPoint.disconnect(this._strengthChangedId);
            this._strengthChangedId = 0;
        }

        this._activeAccessPoint = this._device.active_access_point;

        if (this._activeAccessPoint) {
            this._strengthChangedId = this._activeAccessPoint.connect('notify::strength',
                                                                      Lang.bind(this, this._strengthChanged));
        }

        this._sync();
    },

    _sync: function() {
        this._toggleItem.label.text = this._client.wireless_enabled ? _("Turn Off") : _("Turn On");

        this.item.status.text = this._getStatus();
        this.item.icon.icon_name = this._getMenuIcon();
        this.item.label.text = this._description;
    },

    setDeviceDescription: function(desc) {
        this._description = desc;
        this._sync();
    },

    _getStatus: function() {
        let ap = this._device.active_access_point;
        if (!ap)
            return _("Off"); // XXX -- interpret actual status

        return ssidToLabel(ap.get_ssid());
    },

    _getMenuIcon: function() {
        if (this._device.active_connection)
            return this.getIndicatorIcon();
        else
            return 'network-wireless-signal-none-symbolic';
    },

    getIndicatorIcon: function() {
        if (this._device.active_connection.state == NetworkManager.ActiveConnectionState.ACTIVATING)
            return 'network-wireless-acquiring-symbolic';

        let ap = this._device.active_access_point;
        if (!ap) {
            if (this._device.mode != NM80211Mode.ADHOC)
                log('An active wireless connection, in infrastructure mode, involves no access point?');

            return 'network-wireless-connected-symbolic';
        }

        return 'network-wireless-signal-' + signalToIcon(ap.strength) + '-symbolic';
    },
});
Signals.addSignalMethods(NMDeviceWireless.prototype);

const NMVPNConnectionItem = new Lang.Class({
    Name: 'NMVPNConnectionItem',
    Extends: NMConnectionItem,

    isActive: function() {
        if (this._activeConnection == null)
            return false;

        return this._activeConnection.vpn_state == NetworkManager.VPNConnectionState.ACTIVATED;
    },

    _getStatus: function() {
        if (this._activeConnection == null)
            return null;

        switch(this._activeConnection.vpn_state) {
        case NetworkManager.VPNConnectionState.DISCONNECTED:
        case NetworkManager.VPNConnectionState.ACTIVATED:
            return null;
        case NetworkManager.VPNConnectionState.PREPARE:
        case NetworkManager.VPNConnectionState.CONNECT:
        case NetworkManager.VPNConnectionState.IP_CONFIG_GET:
            return _("connecting...");
        case NetworkManager.VPNConnectionState.NEED_AUTH:
            /* Translators: this is for network connections that require some kind of key or password */
            return _("authentication required");
        case NetworkManager.VPNConnectionState.FAILED:
            return _("connection failed");
        default:
            return 'invalid';
        }
    },

    _connectionStateChanged: function(ac, newstate, reason) {
        if (newstate == NetworkManager.VPNConnectionState.FAILED &&
            reason != NetworkManager.VPNConnectionStateReason.NO_SECRETS) {
            // FIXME: if we ever want to show something based on reason,
            // we need to convert from NetworkManager.VPNConnectionStateReason
            // to NetworkManager.DeviceStateReason
            this.emit('activation-failed', reason);
        }

        this.parent();
    },

    setActiveConnection: function(activeConnection) {
        if (this._activeConnectionChangedId > 0) {
            this._activeConnection.disconnect(this._activeConnectionChangedId);
            this._activeConnectionChangedId = 0;
        }

        this._activeConnection = activeConnection;

        if (this._activeConnection)
            this._activeConnectionChangedId = this._activeConnection.connect('vpn-state-changed',
                                                                             Lang.bind(this, this._connectionStateChanged));

        this._sync();
    },

    getIndicatorIcon: function() {
        if (this._activeConnection) {
            if (this._activeConnection.state == NetworkManager.ActiveConnectionState.ACTIVATING)
                return 'network-vpn-acquiring-symbolic';
            else
                return 'network-vpn-symbolic';
        } else {
            return '';
        }
    },
});

const NMVPNSection = new Lang.Class({
    Name: 'NMVPNSection',
    Extends: NMConnectionSection,
    category: NMConnectionCategory.VPN,

    _init: function(client) {
        this.parent(client);
        this._sync();
    },

    _sync: function() {
        let nItems = this._connectionItems.size();
        this.item.actor.visible = (nItems > 0);
        this.parent();
    },

    _getDescription: function() {
        return _("VPN");
    },

    _getMenuIcon: function() {
        return this.getIndicatorIcon() || 'network-vpn-symbolic';
    },

    activateConnection: function(connection) {
        this._client.activate_connection(connection, null, null, null);
    },

    deactivateConnection: function(activeConnection) {
        this._client.deactivate_connection(activeConnection);
    },

    addActiveConnection: function(activeConnection) {
        let item = this._connectionItems.get(activeConnection._connection.get_uuid());
        item.setActiveConnection(activeConnection);
    },

    removeActiveConnection: function(activeConnection) {
        let item = this._connectionItems.get(activeConnection._connection.get_uuid());
        item.setActiveConnection(null);
    },

    _makeConnectionItem: function(connection) {
        return new NMVPNConnectionItem(this, connection);
    },

    getIndicatorIcon: function() {
        let items = this._connectionItems.values();
        for (let i = 0; i < items.length; i++) {
            let item = items[i];
            let icon = item.getIndicatorIcon();
            if (icon)
                return icon;
        }
        return '';
    },
});
Signals.addSignalMethods(NMVPNSection.prototype);

const NMApplet = new Lang.Class({
    Name: 'NMApplet',
    Extends: PanelMenu.SystemIndicator,

    _init: function() {
        this.parent();

        this._primaryIndicator = this._addIndicator();
        this._vpnIndicator = this._addIndicator();

        // Device types
        this._dtypes = { };
        this._dtypes[NetworkManager.DeviceType.WIFI] = NMDeviceWireless;
        this._dtypes[NetworkManager.DeviceType.MODEM] = NMDeviceModem;
        this._dtypes[NetworkManager.DeviceType.BT] = NMDeviceBluetooth;
        // TODO: WiMax support

        // Connection types
        this._ctypes = { };
        this._ctypes[NetworkManager.SETTING_WIRELESS_SETTING_NAME] = NMConnectionCategory.WIRELESS;
        this._ctypes[NetworkManager.SETTING_BLUETOOTH_SETTING_NAME] = NMConnectionCategory.WWAN;
        this._ctypes[NetworkManager.SETTING_CDMA_SETTING_NAME] = NMConnectionCategory.WWAN;
        this._ctypes[NetworkManager.SETTING_GSM_SETTING_NAME] = NMConnectionCategory.WWAN;
        this._ctypes[NetworkManager.SETTING_VPN_SETTING_NAME] = NMConnectionCategory.VPN;

        NMClient.Client.new_async(null, Lang.bind(this, this._clientGot));
        NMClient.RemoteSettings.new_async(null, null, Lang.bind(this, this._remoteSettingsGot));
    },

    _clientGot: function(obj, result) {
        this._client = NMClient.Client.new_finish(result);

        this._tryLateInit();
    },

    _remoteSettingsGot: function(obj, result) {
        this._settings = NMClient.RemoteSettings.new_finish(result);

        this._tryLateInit();
    },

    _tryLateInit: function() {
        if (!this._client || !this._settings)
            return;

        this._activeConnections = [ ];
        this._connections = [ ];

        this._mainConnection = null;
        this._mainConnectionIconChangedId = 0;

        this._nmDevices = [];
        this._devices = { };

        this._devices.wireless = {
            section: new PopupMenu.PopupMenuSection(),
            devices: [ ],
        };
        this.menu.addMenuItem(this._devices.wireless.section);

        this._devices.wwan = {
            section: new PopupMenu.PopupMenuSection(),
            devices: [ ],
        };
        this.menu.addMenuItem(this._devices.wwan.section);

        this._vpnSection = new NMVPNSection(this._client);
        this._vpnSection.connect('activation-failed', Lang.bind(this, this._onActivationFailed));
        this._vpnSection.connect('icon-changed', Lang.bind(this, this._updateIcon));
        this.menu.addMenuItem(this._vpnSection.item);

        this._readConnections();
        this._readDevices();
        this._syncNMState();

        this._client.connect('notify::manager-running', Lang.bind(this, this._syncNMState));
        this._client.connect('notify::networking-enabled', Lang.bind(this, this._syncNMState));
        this._client.connect('notify::state', Lang.bind(this, this._syncNMState));
        this._client.connect('notify::active-connections', Lang.bind(this, this._syncActiveConnections));
        this._client.connect('device-added', Lang.bind(this, this._deviceAdded));
        this._client.connect('device-removed', Lang.bind(this, this._deviceRemoved));
        this._settings.connect('new-connection', Lang.bind(this, this._newConnection));

        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
        this._sessionUpdated();
    },

    _sessionUpdated: function() {
        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        this.menu.setSensitive(sensitive);
    },

    _ensureSource: function() {
        if (!this._source) {
            this._source = new MessageTray.Source(_("Network Manager"),
                                                  'network-transmit-receive');
            this._source.policy = new NotificationDaemon.NotificationApplicationPolicy('gnome-network-panel');

            this._source.connect('destroy', Lang.bind(this, function() {
                this._source = null;
            }));
            Main.messageTray.add(this._source);
        }
    },

    _readDevices: function() {
        let devices = this._client.get_devices() || [ ];
        for (let i = 0; i < devices.length; ++i) {
            this._deviceAdded(this._client, devices[i], true);
        }
        this._syncDeviceNames();
    },

    _notifyForDevice: function(device, iconName, title, text, urgency) {
        if (device._notification)
            device._notification.destroy();

        /* must call after destroying previous notification,
           or this._source will be cleared */
        this._ensureSource();

        let gicon = new Gio.ThemedIcon({ name: iconName });
        device._notification = new MessageTray.Notification(this._source, title, text,
                                                            { gicon: gicon });
        device._notification.setUrgency(urgency);
        device._notification.setTransient(true);
        device._notification.connect('destroy', function() {
            device._notification = null;
        });
        this._source.notify(device._notification);
    },

    _onActivationFailed: function(device, reason) {
        // XXX: nm-applet has no special text depending on reason
        // but I'm not sure of this generic message
        this._notifyForDevice(device, 'network-error-symbolic',
                              _("Connection failed"),
                              _("Activation of network connection failed"),
                              MessageTray.Urgency.HIGH);
    },

    _syncDeviceNames: function() {
        let names = NMGtk.utils_disambiguate_device_names(this._nmDevices);
        for (let i = 0; i < this._nmDevices.length; i++) {
            let device = this._nmDevices[i];
            let description = names[i];
            if (device._delegate)
                device._delegate.setDeviceDescription(description);
        }
    },

    _deviceAdded: function(client, device, skipSyncDeviceNames) {
        if (device._delegate) {
            // already seen, not adding again
            return;
        }

        let wrapperClass = this._dtypes[device.get_device_type()];
        if (wrapperClass) {
            let wrapper = new wrapperClass(this._client, device, this._settings);
            device._delegate = wrapper;
            this._addDeviceWrapper(wrapper);

            this._nmDevices.push(device);
            if (!skipSyncDeviceNames)
                this._syncDeviceNames();

            if (wrapper instanceof NMConnectionSection) {
                this._connections.forEach(function(connection) {
                    wrapper.checkConnection(connection);
                });
            }
        }
    },

    _addDeviceWrapper: function(wrapper) {
        wrapper._activationFailedId = wrapper.connect('activation-failed',
                                                      Lang.bind(this, this._onActivationFailed));

        let section = this._devices[wrapper.category].section;
        section.addMenuItem(wrapper.item);

        let devices = this._devices[wrapper.category].devices;
        devices.push(wrapper);
    },

    _deviceRemoved: function(client, device) {
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
    },

    _removeDeviceWrapper: function(wrapper) {
        wrapper.disconnect(wrapper._activationFailedId);
        wrapper.destroy();

        let devices = this._devices[wrapper.category].devices;
        let pos = devices.indexOf(wrapper);
        devices.splice(pos, 1);
    },

    _getSupportedActiveConnections: function() {
        let activeConnections = this._client.get_active_connections() || [ ];
        let supportedConnections = [];

        for (let i = 0; i < activeConnections.length; i++) {
            let devices = activeConnections[i].get_devices();
            if (!devices || !devices[0])
                continue;
            // Ignore connections via unrecognized device types
            if (!this._dtypes[devices[0].device_type])
                continue;

            // Ignore slave connections
            let connectionPath = activeConnections[i].connection;
            let connection = this._settings.get_connection_by_path(connectionPath);

            // connection might be null, if libnm-glib fails to create
            // the object due to version incompatibility, or if the
            // connection is not visible to the current user
            if (connection && this._ignoreConnection(connection))
                continue;

            supportedConnections.push(activeConnections[i]);
        }
        return supportedConnections;
    },

    _syncActiveConnections: function() {
        let closedConnections = [ ];
        let newActiveConnections = this._getSupportedActiveConnections();
        for (let i = 0; i < this._activeConnections.length; i++) {
            let a = this._activeConnections[i];
            if (newActiveConnections.indexOf(a) == -1) // connection is removed
                closedConnections.push(a);
        }

        for (let i = 0; i < closedConnections.length; i++) {
            let a = closedConnections[i];
            if (a._type == NetworkManager.SETTING_VPN_SETTING_NAME)
                this._vpnSection.removeActiveConnection(a);
            if (a._inited) {
                a.disconnect(a._notifyStateId);
                a.disconnect(a._notifyDefaultId);
                a.disconnect(a._notifyDefault6Id);
                a._inited = false;
            }
        }

        if (this._mainConnectionIconChangedId > 0) {
            this._mainConnection._primaryDevice.disconnect(this._mainConnectionIconChangedId);
            this._mainConnectionIconChangedId = 0;
        }

        this._activeConnections = newActiveConnections;
        this._mainConnection = null;

        let activating = null;
        let default_ip4 = null;
        let default_ip6 = null;
        let active_any = null;
        for (let i = 0; i < this._activeConnections.length; i++) {
            let a = this._activeConnections[i];

            if (!a._inited) {
                a._notifyDefaultId = a.connect('notify::default', Lang.bind(this, this._syncActiveConnections));
                a._notifyDefault6Id = a.connect('notify::default6', Lang.bind(this, this._syncActiveConnections));
                a._notifyStateId = a.connect('notify::state', Lang.bind(this, this._notifyActivated));

                a._inited = true;
            }

            if (!a._connection) {
                a._connection = this._settings.get_connection_by_path(a.connection);

                if (a._connection) {
                    a._type = a._connection._type;
                    a._section = this._ctypes[a._type];
                } else {
                    a._connection = null;
                    a._type = null;
                    a._section = null;
                    log('Cannot find connection for active (or connection cannot be read)');
                }
            }

            if (a['default'])
                default_ip4 = a;
            if (a.default6)
                default_ip6 = a;

            if (a.state == NetworkManager.ActiveConnectionState.ACTIVATING)
                activating = a;
            else if (a.state == NetworkManager.ActiveConnectionState.ACTIVATED)
                active_any = a;

            if (!a._primaryDevice) {
                if (a._type != NetworkManager.SETTING_VPN_SETTING_NAME) {
                    // This list is guaranteed to have one device in it.
                    a._primaryDevice = a.get_devices()[0]._delegate;
                } else {
                    a._primaryDevice = this._vpnSection;
                    this._vpnSection.addActiveConnection(a);
                }

                if (a.state == NetworkManager.ActiveConnectionState.ACTIVATED
                    && a._primaryDevice && a._primaryDevice._notification) {
                    a._primaryDevice._notification.destroy();
                    a._primaryDevice._notification = null;
                }
            }
        }

        this._mainConnection = default_ip4 || default_ip6 || active_any || activating || null;

        if (this._mainConnection) {
            let dev = this._mainConnection._primaryDevice;
            this._mainConnectionIconChangedId = dev.connect('icon-changed', Lang.bind(this, this._updateIcon));
            this._updateIcon();
        }
    },

    _notifyActivated: function(activeConnection) {
        if (activeConnection.state == NetworkManager.ActiveConnectionState.ACTIVATED
            && activeConnection._primaryDevice && activeConnection._primaryDevice._notification) {
            activeConnection._primaryDevice._notification.destroy();
            activeConnection._primaryDevice._notification = null;
        }

        this._syncActiveConnections();
    },

    _ignoreConnection: function(connection) {
        let setting = connection.get_setting_connection();
        if (!setting)
            return true;

        // Ignore slave connections
        if (setting.get_master())
            return true;

        return false;
    },

    _addConnection: function(connection) {
        if (this._ignoreConnection(connection))
            return;
        if (connection._updatedId) {
            // connection was already seen
            return;
        }

        connection._removedId = connection.connect('removed', Lang.bind(this, this._connectionRemoved));
        connection._updatedId = connection.connect('updated', Lang.bind(this, this._updateConnection));

        this._updateConnection(connection);
        this._connections.push(connection);
    },

    _readConnections: function() {
        let connections = this._settings.list_connections();
        connections.forEach(Lang.bind(this, this._addConnection));
    },

    _newConnection: function(settings, connection) {
        this._addConnection(connection);
        this._syncActiveConnections();
    },

    _connectionRemoved: function(connection) {
        let pos = this._connections.indexOf(connection);
        if (pos != -1)
            this._connections.splice(connection, 1);

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

        connection.disconnect(connection._removedId);
        connection.disconnect(connection._updatedId);
        connection._removedId = connection._updatedId = 0;
    },

    _updateConnection: function(connection) {
        let connectionSettings = connection.get_setting_by_name(NetworkManager.SETTING_CONNECTION_SETTING_NAME);
        connection._type = connectionSettings.type;
        connection._section = this._ctypes[connection._type] || NMConnectionCategory.INVALID;
        connection._timestamp = connectionSettings.timestamp;

        let section = connection._section;

        if (section == NMConnectionCategory.INVALID)
            return;

        if (section == NMConnectionCategory.VPN) {
            this._vpnSection.checkConnection(connection);
        } else {
            let devices = this._devices[section].devices;
            devices.forEach(function(wrapper) {
                if (wrapper instanceof NMConnectionSection)
                    wrapper.checkConnection(connection);
            });
        }
    },

    _syncNMState: function() {
        this._syncActiveConnections();

        this.indicators.visible = this._client.manager_running;
        this.menu.actor.visible = this._client.networking_enabled;
    },

    _updateIcon: function() {
        let mc = this._mainConnection;

        if (!this._client.networking_enabled || !mc) {
            this._primaryIndicator.icon_name = 'network-offline-symbolic';
        } else {
            let dev = this._mainConnection._primaryDevice;
            if (!dev) {
                log('Active connection with no primary device?');
                return;
            }
            this._primaryIndicator.icon_name = dev.getIndicatorIcon(mc);
        }

        this._vpnIndicator.icon_name = this._vpnSection.getIndicatorIcon();
        this._vpnIndicator.visible = (this._vpnIndicator.icon_name != '');
    }
});
