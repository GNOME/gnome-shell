// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const { Gio, GLib, GnomeBluetooth, GObject } = imports.gi;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const { loadInterfaceXML } = imports.misc.fileUtils;

const BUS_NAME = 'org.gnome.SettingsDaemon.Rfkill';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Rfkill';

const RfkillManagerInterface = loadInterfaceXML('org.gnome.SettingsDaemon.Rfkill');
const RfkillManagerProxy = Gio.DBusProxy.makeProxyWrapper(RfkillManagerInterface);

const HAD_BLUETOOTH_DEVICES_SETUP = 'had-bluetooth-devices-setup';

var Indicator = GObject.registerClass(
class Indicator extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'bluetooth-active-symbolic';
        this._hadSetupDevices = global.settings.get_boolean(HAD_BLUETOOTH_DEVICES_SETUP);
        this._client = new GnomeBluetooth.Client();

        this._proxy = new RfkillManagerProxy(Gio.DBus.session, BUS_NAME, OBJECT_PATH,
                                             (proxy, error) => {
                                                 if (error) {
                                                     log(error.message);
                                                     return;
                                                 }

                                                 this._sync();
                                             });
        this._proxy.connect('g-properties-changed', this._queueSync.bind(this));

        this._item = new PopupMenu.PopupSubMenuMenuItem(_("Bluetooth"), true);
        this._item.icon.icon_name = 'bluetooth-active-symbolic';

        this._toggleItem = new PopupMenu.PopupMenuItem('');
        this._toggleItem.connect('activate', () => {
            this._proxy.BluetoothAirplaneMode = !this._proxy.BluetoothAirplaneMode;
            if (!this._proxy.BluetoothAirplaneMode)
                this._client.default_adapter_powered = true;
        });
        this._item.menu.addMenuItem(this._toggleItem);

        this._item.menu.addSettingsAction(_("Bluetooth Settings"), 'gnome-bluetooth-panel.desktop');
        this.menu.addMenuItem(this._item);

        this._syncId = 0;
        this._adapter = null;

        this._store = this._client.get_devices();
        this._deviceNotifyConnected = [];
        this._client.connect('device-removed', (c, path) => {
            this._deviceNotifyConnected[path] = false;
            this._queueSync.bind(this);
        });
        this._client.connect('device-added', (c, device) => {
            this._connectDeviceNotify(device);
            this._sync();
        });
        Main.sessionMode.connect('updated', this._sync.bind(this));
        this._sync();
    }

    _setHadSetupDevices(value) {
        if (this._hadSetupDevices === value)
            return;

        this._hadSetupDevices = value;
        global.settings.set_boolean(
            HAD_BLUETOOTH_DEVICES_SETUP, this._hadSetupDevices);
    }

    _connectDeviceNotify(device) {
        if (this._deviceNotifyConnected[device.get_object_path()] !== true)
            return;
        device.connect('notify::alias', this._queueSync.bind(this));
        device.connect('notify::paired', this._queueSync.bind(this));
        device.connect('notify::trusted', this._queueSync.bind(this));
        device.connect('notify::connected', this._queueSync.bind(this));
        this._deviceNotifyConnected.push([device.get_object_path(), true]);
    }

    _getDeviceInfos() {
        let deviceInfos = [];
        const numDevices = this._store.get_n_items();

        for (let i = 0; i < numDevices; i++) {
            const device = this._store.get_item(i);

            if (device.paired || device.trusted) {
                deviceInfos.push({
                    connected: device.connected,
                    name: device.alias,
                });
            }
            this._connectDeviceNotify(device);
        }

        return deviceInfos;
    }

    _queueSync() {
        if (this._syncId)
            return;
        this._syncId = GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            this._syncId = 0;
            this._sync();
            return GLib.SOURCE_REMOVE;
        });
    }

    _sync() {
        let devices = this._getDeviceInfos();
        const connectedDevices = devices.filter(dev => dev.connected);
        const nConnectedDevices = connectedDevices.length;

        if (this._client.default_adapter && this._adapter)
            this._setHadSetupDevices(devices.length > 0);
        this._adapter = this._client.default_adapter ?? null;

        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;

        this.menu.setSensitive(sensitive);
        this._indicator.visible = nConnectedDevices > 0;

        // Remember if there were setup devices and show the menu
        // if we've seen setup devices and we're not hard blocked
        if (this._hadSetupDevices)
            this._item.visible = !this._proxy.BluetoothHardwareAirplaneMode;
        else
            this._item.visible = this._proxy.BluetoothHasAirplaneMode && !this._proxy.BluetoothAirplaneMode;

        if (nConnectedDevices > 1)
            /* Translators: this is the number of connected bluetooth devices */
            this._item.label.text = ngettext('%d Connected', '%d Connected', nConnectedDevices).format(nConnectedDevices);
        else if (nConnectedDevices === 1)
            this._item.label.text = connectedDevices[0].name;
        else if (this._adapter === null)
            this._item.label.text = _('Bluetooth Off');
        else
            this._item.label.text = _('Bluetooth On');

        this._toggleItem.label.text = this._client.default_adapter_powered ? _('Turn Off') : _('Turn On');
    }
});
