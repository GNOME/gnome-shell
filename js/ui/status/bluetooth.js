// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const { Gio, GnomeBluetooth, GObject } = imports.gi;

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

        this._proxy = new RfkillManagerProxy(Gio.DBus.session, BUS_NAME, OBJECT_PATH,
                                             (proxy, error) => {
                                                 if (error) {
                                                     log(error.message);
                                                     return;
                                                 }

                                                 this._sync();
                                             });
        this._proxy.connect('g-properties-changed', this._sync.bind(this));

        this._item = new PopupMenu.PopupSubMenuMenuItem(_("Bluetooth"), true);
        this._item.icon.icon_name = 'bluetooth-active-symbolic';

        this._toggleItem = new PopupMenu.PopupMenuItem('');
        this._toggleItem.connect('activate', () => {
            this._proxy.BluetoothAirplaneMode = !this._proxy.BluetoothAirplaneMode;
        });
        this._item.menu.addMenuItem(this._toggleItem);

        this._item.menu.addSettingsAction(_("Bluetooth Settings"), 'gnome-bluetooth-panel.desktop');
        this.menu.addMenuItem(this._item);

        this._client = new GnomeBluetooth.Client();
        this._model = this._client.get_model();
        this._model.connect('row-changed', this._sync.bind(this));
        this._model.connect('row-deleted', this._sync.bind(this));
        this._model.connect('row-inserted', this._sync.bind(this));
        Main.sessionMode.connect('updated', this._sync.bind(this));
        this._sync();
    }

    _getDefaultAdapter() {
        let [ret, iter] = this._model.get_iter_first();
        while (ret) {
            let isDefault = this._model.get_value(iter,
                                                  GnomeBluetooth.Column.DEFAULT);
            let isPowered = this._model.get_value(iter,
                                                  GnomeBluetooth.Column.POWERED);
            if (isDefault && isPowered)
                return iter;
            ret = this._model.iter_next(iter);
        }
        return null;
    }

    // nDevices is the number of devices setup for the current default
    // adapter if one exists and is powered. If unpowered or unavailable,
    // nDevice is "1" if it had setup devices associated to it the last
    // time it was seen, and "-1" if not.
    //
    // nConnectedDevices is the number of devices connected to the default
    // adapter if one exists and is powered, or -1 if it's not available.
    _getNDevices() {
        let adapter = this._getDefaultAdapter();
        if (!adapter)
            return [this._hadSetupDevices ? 1 : -1, -1];

        let nConnectedDevices = 0;
        let nDevices = 0;
        let [ret, iter] = this._model.iter_children(adapter);
        while (ret) {
            let isConnected = this._model.get_value(iter,
                                                    GnomeBluetooth.Column.CONNECTED);
            if (isConnected)
                nConnectedDevices++;

            let isPaired = this._model.get_value(iter,
                                                 GnomeBluetooth.Column.PAIRED);
            let isTrusted = this._model.get_value(iter,
                                                  GnomeBluetooth.Column.TRUSTED);
            if (isPaired || isTrusted)
                nDevices++;
            ret = this._model.iter_next(iter);
        }

        if (this._hadSetupDevices != (nDevices > 0)) {
            this._hadSetupDevices = !this._hadSetupDevices;
            global.settings.set_boolean(HAD_BLUETOOTH_DEVICES_SETUP, this._hadSetupDevices);
        }

        return [nDevices, nConnectedDevices];
    }

    _sync() {
        let [nDevices, nConnectedDevices] = this._getNDevices();
        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;

        this.menu.setSensitive(sensitive);
        this._indicator.visible = nConnectedDevices > 0;

        // Remember if there were setup devices and show the menu
        // if we've seen setup devices and we're not hard blocked
        if (nDevices > 0)
            this._item.visible = !this._proxy.BluetoothHardwareAirplaneMode;
        else
            this._item.visible = this._proxy.BluetoothHasAirplaneMode && !this._proxy.BluetoothAirplaneMode;

        if (nConnectedDevices > 0)
            /* Translators: this is the number of connected bluetooth devices */
            this._item.label.text = ngettext("%d Connected", "%d Connected", nConnectedDevices).format(nConnectedDevices);
        else if (nConnectedDevices == -1)
            this._item.label.text = _("Off");
        else
            this._item.label.text = _("On");

        this._toggleItem.label.text = this._proxy.BluetoothAirplaneMode ? _("Turn On") : _("Turn Off");
    }
});
