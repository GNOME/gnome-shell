// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const {Gio, GLib, GnomeBluetooth, GObject, Pango, St} = imports.gi;

const {Spinner} = imports.ui.animation;
const PopupMenu = imports.ui.popupMenu;
const {QuickMenuToggle, SystemIndicator} = imports.ui.quickSettings;

const {loadInterfaceXML} = imports.misc.fileUtils;

const {AdapterState} = GnomeBluetooth;

const BUS_NAME = 'org.gnome.SettingsDaemon.Rfkill';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Rfkill';

const RfkillManagerInterface = loadInterfaceXML('org.gnome.SettingsDaemon.Rfkill');
const rfkillManagerInfo = Gio.DBusInterfaceInfo.new_for_xml(RfkillManagerInterface);

Gio._promisify(GnomeBluetooth.Client.prototype, 'connect_service');

const BtClient = GObject.registerClass({
    Properties: {
        'available': GObject.ParamSpec.boolean('available', '', '',
            GObject.ParamFlags.READABLE,
            false),
        'active': GObject.ParamSpec.boolean('active', '', '',
            GObject.ParamFlags.READABLE,
            false),
        'adapter-state': GObject.ParamSpec.enum('adapter-state', '', '',
            GObject.ParamFlags.READABLE,
            AdapterState, AdapterState.ABSENT),
    },
    Signals: {
        'devices-changed': {},
        'device-removed': {param_types: [GObject.TYPE_STRING]},
    },
}, class BtClient extends GObject.Object {
    _init() {
        super._init();

        this._client = new GnomeBluetooth.Client();
        this._client.connect('notify::default-adapter-powered', () => {
            this.notify('active');
            this.notify('available');
        });
        this._client.connect('notify::default-adapter-state',
            () => this.notify('adapter-state'));
        this._client.connect('notify::default-adapter', () => {
            const newAdapter = this._client.default_adapter ?? null;

            this._adapter = newAdapter;
            this._deviceNotifyConnected.clear();
            this.emit('devices-changed');

            this.notify('active');
            this.notify('available');
        });

        this._proxy = new Gio.DBusProxy({
            g_connection: Gio.DBus.session,
            g_name: BUS_NAME,
            g_object_path: OBJECT_PATH,
            g_interface_name: rfkillManagerInfo.name,
            g_interface_info: rfkillManagerInfo,
        });
        this._proxy.connect('g-properties-changed', (p, properties) => {
            const changedProperties = properties.unpack();
            if ('BluetoothHardwareAirplaneMode' in changedProperties)
                this.notify('available');
            else if ('BluetoothHasAirplaneMode' in changedProperties)
                this.notify('available');
        });
        this._proxy.init_async(GLib.PRIORITY_DEFAULT, null)
            .catch(e => console.error(e.message));

        this._adapter = null;

        this._deviceNotifyConnected = new Set();

        const deviceStore = this._client.get_devices();
        for (let i = 0; i < deviceStore.get_n_items(); i++)
            this._connectDeviceNotify(deviceStore.get_item(i));

        this._client.connect('device-removed', (c, path) => {
            this._deviceNotifyConnected.delete(path);
            this.emit('device-removed', path);
            this.emit('devices-changed');
        });
        this._client.connect('device-added', (c, device) => {
            this._connectDeviceNotify(device);
            this.emit('devices-changed');
        });
    }

    get available() {
        // If we have an rfkill switch, make sure it's not a hardware
        // one as we can't get out of it in software
        return this._proxy.BluetoothHasAirplaneMode
            ? !this._proxy.BluetoothHardwareAirplaneMode
            : this.active;
    }

    get active() {
        return this._client.default_adapter_powered;
    }

    get adapter_state() {
        return this._client.default_adapter_state;
    }

    toggleActive() {
        this._proxy.BluetoothAirplaneMode = this.active;
        if (!this._client.default_adapter_powered)
            this._client.default_adapter_powered = true;
    }

    async toggleDevice(device) {
        const connect = !device.connected;
        console.debug(`${connect
            ? 'Connect' : 'Disconnect'} device "${device.name}"`);

        try {
            await this._client.connect_service(
                device.get_object_path(),
                connect,
                null);
            console.debug(`Device "${device.name}" ${
                connect ? 'connected' : 'disconnected'}`);
        } catch (e) {
            console.error(`Failed to ${connect
                ? 'connect' : 'disconnect'} device "${device.name}": ${e.message}`);
        }
    }

    *getDevices() {
        // Ignore any lingering device references when turned off
        if (!this.active)
            return;

        const deviceStore = this._client.get_devices();

        for (let i = 0; i < deviceStore.get_n_items(); i++) {
            const device = deviceStore.get_item(i);

            if (device.paired || device.trusted)
                yield device;
        }
    }

    _queueDevicesChanged() {
        if (this._devicesChangedId)
            return;
        this._devicesChangedId = GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            delete this._devicesChangedId;
            this.emit('devices-changed');
            return GLib.SOURCE_REMOVE;
        });
    }

    _connectDeviceNotify(device) {
        const path = device.get_object_path();

        if (this._deviceNotifyConnected.has(path))
            return;

        device.connect('notify::alias', () => this._queueDevicesChanged());
        device.connect('notify::paired', () => this._queueDevicesChanged());
        device.connect('notify::trusted', () => this._queueDevicesChanged());
        device.connect('notify::connected', () => this._queueDevicesChanged());

        this._deviceNotifyConnected.add(path);
    }
});

const BluetoothDeviceItem = GObject.registerClass(
class BluetoothDeviceItem extends PopupMenu.PopupBaseMenuItem {
    constructor(device, client) {
        super({
            style_class: 'bt-device-item',
        });

        this._device = device;
        this._client = client;

        this._icon = new St.Icon({
            style_class: 'popup-menu-icon',
        });
        this.add_child(this._icon);

        this._label = new St.Label({
            x_expand: true,
        });
        this.add_child(this._label);

        this._subtitle = new St.Label({
            style_class: 'device-subtitle',
        });
        this.add_child(this._subtitle);

        this._spinner = new Spinner(16, {hideOnStop: true});
        this.add_child(this._spinner);

        this._spinner.bind_property('visible',
            this._subtitle, 'visible',
            GObject.BindingFlags.SYNC_CREATE |
            GObject.BindingFlags.INVERT_BOOLEAN);

        this._device.bind_property('connectable',
            this, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        this._device.bind_property('icon',
            this._icon, 'icon-name',
            GObject.BindingFlags.SYNC_CREATE);
        this._device.bind_property('alias',
            this._label, 'text',
            GObject.BindingFlags.SYNC_CREATE);
        this._device.bind_property_full('connected',
            this._subtitle, 'text',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, source) => [true, source ? _('Disconnect') : _('Connect')],
            null);

        this.connect('destroy', () => (this._spinner = null));
        this.connect('activate', () => this._toggleConnected().catch(logError));
    }

    async _toggleConnected() {
        this._spinner.play();
        await this._client.toggleDevice(this._device);
        this._spinner?.stop();
    }
});

const BluetoothToggle = GObject.registerClass(
class BluetoothToggle extends QuickMenuToggle {
    _init(client) {
        super._init({title: _('Bluetooth')});

        this.menu.setHeader('bluetooth-active-symbolic', _('Bluetooth'));

        this._deviceItems = new Map();
        this._deviceSection = new PopupMenu.PopupMenuSection();
        this.menu.addMenuItem(this._deviceSection);

        this._placeholderItem = new PopupMenu.PopupMenuItem('', {
            style_class: 'bt-menu-placeholder',
            reactive: false,
            can_focus: false,
        });
        this._placeholderItem.label.clutter_text.set({
            ellipsize: Pango.EllipsizeMode.NONE,
            line_wrap: true,
        });
        this._placeholderItem.setOrnament(PopupMenu.Ornament.HIDDEN);
        this.menu.addMenuItem(this._placeholderItem);

        this._deviceSection.actor.bind_property('visible',
            this._placeholderItem, 'visible',
            GObject.BindingFlags.SYNC_CREATE |
            GObject.BindingFlags.INVERT_BOOLEAN);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.menu.addSettingsAction(_('Bluetooth Settings'),
            'gnome-bluetooth-panel.desktop');

        this._client = client;

        this._client.bind_property('available',
            this, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        this._client.bind_property('active',
            this, 'checked',
            GObject.BindingFlags.SYNC_CREATE);
        this._client.bind_property_full('adapter-state',
            this, 'icon-name',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, source) => [true, this._getIconNameFromState(source)],
            null);

        this._client.connectObject(
            'notify::active', () => this._onActiveChanged(),
            'devices-changed', () => this._sync(),
            'device-removed', (c, path) => this._removeDevice(path),
            this);

        this.menu.connect('open-state-changed', isOpen => {
            // We don't reorder the list while the menu is open,
            // so do it now to start with the proper order
            if (isOpen)
                this._reorderDeviceItems();
        });

        this.connect('clicked', () => this._client.toggleActive());

        this._updatePlaceholder();
        this._sync();
    }

    _onActiveChanged() {
        this._updatePlaceholder();

        this._deviceItems.forEach(item => item.destroy());
        this._deviceItems.clear();

        this._sync();
    }

    _updatePlaceholder() {
        this._placeholderItem.label.text = this._client.active
            ? _('No available or connected devices')
            : _('Turn on Bluetooth to connect to devices');
    }

    _updateDeviceVisibility() {
        this._deviceSection.actor.visible =
            [...this._deviceItems.values()].some(item => item.visible);
    }

    _getSortedDevices() {
        return [...this._client.getDevices()].sort((dev1, dev2) => {
            if (dev1.connected !== dev2.connected)
                return dev2.connected - dev1.connected;
            return dev1.alias.localeCompare(dev2.alias);
        });
    }

    _removeDevice(path) {
        this._deviceItems.get(path)?.destroy();
        this._deviceItems.delete(path);

        this._updateDeviceVisibility();
    }

    _reorderDeviceItems() {
        const devices = this._getSortedDevices();
        for (const [i, dev] of devices.entries()) {
            const item = this._deviceItems.get(dev.get_object_path());
            if (!item)
                continue;

            this._deviceSection.moveMenuItem(item, i);
        }
    }

    _sync() {
        const devices = this._getSortedDevices();

        for (const dev of devices) {
            const path = dev.get_object_path();
            if (this._deviceItems.has(path))
                continue;

            const item = new BluetoothDeviceItem(dev, this._client);
            item.connect('notify::visible', () => this._updateDeviceVisibility());

            this._deviceSection.addMenuItem(item);
            this._deviceItems.set(path, item);
        }

        const connectedDevices = devices.filter(dev => dev.connected);
        const nConnected = connectedDevices.length;

        if (nConnected > 1)
            /* Translators: This is the number of connected bluetooth devices */
            this.subtitle = ngettext('%d Connected', '%d Connected', nConnected).format(nConnected);
        else if (nConnected === 1)
            this.subtitle = connectedDevices[0].alias;
        else
            this.subtitle = null;

        this._updateDeviceVisibility();
    }

    _getIconNameFromState(state) {
        switch (state) {
        case AdapterState.ON:
            return 'bluetooth-active-symbolic';
        case AdapterState.OFF:
        case AdapterState.ABSENT:
            return 'bluetooth-disabled-symbolic';
        case AdapterState.TURNING_ON:
        case AdapterState.TURNING_OFF:
            return 'bluetooth-acquiring-symbolic';
        default:
            console.warn(`Unexpected state ${
                GObject.enum_to_string(AdapterState, state)}`);
            return '';
        }
    }
});

var Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this._client = new BtClient();
        this._client.connect('devices-changed', () => this._sync());

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'bluetooth-active-symbolic';

        this.quickSettingsItems.push(new BluetoothToggle(this._client));

        this._sync();
    }

    _sync() {
        const devices = [...this._client.getDevices()];
        const connectedDevices = devices.filter(dev => dev.connected);
        const nConnectedDevices = connectedDevices.length;

        this._indicator.visible = nConnectedDevices > 0;
    }
});
