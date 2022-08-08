// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const {Gio, GLib, GnomeBluetooth, GObject} = imports.gi;

const {QuickToggle, SystemIndicator} = imports.ui.quickSettings;

const {loadInterfaceXML} = imports.misc.fileUtils;

const BUS_NAME = 'org.gnome.SettingsDaemon.Rfkill';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Rfkill';

const RfkillManagerInterface = loadInterfaceXML('org.gnome.SettingsDaemon.Rfkill');
const rfkillManagerInfo = Gio.DBusInterfaceInfo.new_for_xml(RfkillManagerInterface);

const HAD_BLUETOOTH_DEVICES_SETUP = 'had-bluetooth-devices-setup';

const BtClient = GObject.registerClass({
    Properties: {
        'available': GObject.ParamSpec.boolean('available', '', '',
            GObject.ParamFlags.READABLE,
            false),
        'active': GObject.ParamSpec.boolean('active', '', '',
            GObject.ParamFlags.READABLE,
            false),
    },
    Signals: {
        'devices-changed': {},
    },
}, class BtClient extends GObject.Object {
    _init() {
        super._init();

        this._hadSetupDevices = global.settings.get_boolean(HAD_BLUETOOTH_DEVICES_SETUP);

        this._client = new GnomeBluetooth.Client();
        this._client.connect('notify::default-adapter-powered', () => {
            this.notify('active');
            this.notify('available');
        });
        this._client.connect('notify::default-adapter', () => {
            const newAdapter = this._client.default_adapter ?? null;

            if (newAdapter && this._adapter)
                this._setHadSetupDevices([...this.getDevices()].length > 0);

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
            if ('BluetoothHardwareAirplaneMode' in properties.unpack())
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
            this.emit('devices-changed');
        });
        this._client.connect('device-added', (c, device) => {
            this._connectDeviceNotify(device);
            this.emit('devices-changed');
        });
    }

    get available() {
        // If there were set up devices, assume there is an adapter
        // that can be powered on as long as we're not hard blocked
        return this._hadSetupDevices
            ? !this._proxy.BluetoothHardwareAirplaneMode
            : this.active;
    }

    get active() {
        return this._client.default_adapter_powered;
    }

    toggleActive() {
        this._proxy.BluetoothAirplaneMode = this.active;
        if (!this._client.default_adapter_powered)
            this._client.default_adapter_powered = true;
    }

    *getDevices() {
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

    _setHadSetupDevices(value) {
        if (this._hadSetupDevices === value)
            return;

        this._hadSetupDevices = value;
        global.settings.set_boolean(
            HAD_BLUETOOTH_DEVICES_SETUP, this._hadSetupDevices);
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

const BluetoothToggle = GObject.registerClass(
class BluetoothToggle extends QuickToggle {
    _init(client) {
        super._init({label: _('Bluetooth')});

        this._client = client;

        this._client.bind_property('available',
            this, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        this._client.bind_property('active',
            this, 'checked',
            GObject.BindingFlags.SYNC_CREATE);
        this._client.bind_property_full('active',
            this, 'icon-name',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, source) => [true, source ? 'bluetooth-active-symbolic' : 'bluetooth-disabled-symbolic'],
            null);

        this.connect('clicked', () => this._client.toggleActive());
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
