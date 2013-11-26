// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const GnomeBluetoothApplet = imports.gi.GnomeBluetoothApplet;
const GnomeBluetooth = imports.gi.GnomeBluetooth;
const Lang = imports.lang;
const St = imports.gi.St;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const BUS_NAME = 'org.gnome.SettingsDaemon.Rfkill';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Rfkill';

const RfkillManagerInterface = '<node> \
<interface name="org.gnome.SettingsDaemon.Rfkill"> \
<property name="BluetoothAirplaneMode" type="b" access="readwrite" /> \
</interface> \
</node>';

const RfkillManagerProxy = Gio.DBusProxy.makeProxyWrapper(RfkillManagerInterface);

const Indicator = new Lang.Class({
    Name: 'BTIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init: function() {
        this.parent();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'bluetooth-active-symbolic';

        this._proxy = new RfkillManagerProxy(Gio.DBus.session, BUS_NAME, OBJECT_PATH,
                                             Lang.bind(this, function(proxy, error) {
                                                 if (error) {
                                                     log(error.message);
                                                     return;
                                                 }
                                             }));

        // The Bluetooth menu only appears when Bluetooth is in use,
        // so just statically build it with a "Turn Off" menu item.
        this._item = new PopupMenu.PopupSubMenuMenuItem(_("Bluetooth"), true);
        this._item.icon.icon_name = 'bluetooth-active-symbolic';
        this._item.menu.addAction(_("Turn Off"), Lang.bind(this, function() {
            this._proxy.BluetoothAirplaneMode = true;
        }));
        this._item.menu.addSettingsAction(_("Bluetooth Settings"), 'gnome-bluetooth-panel.desktop');
        this.menu.addMenuItem(this._item);

        this._applet = new GnomeBluetoothApplet.Applet();
        this._applet.connect('devices-changed', Lang.bind(this, this._sync));
        this._sync();
    },

    _sync: function() {
        let connectedDevices = this._applet.get_devices().filter(function(device) {
            return device.connected;
        });
        let nDevices = connectedDevices.length;

        let on = nDevices > 0;
        this._indicator.visible = on;
        this._item.actor.visible = on;

        if (on)
            this._item.status.text = ngettext("%d Connected Device", "%d Connected Devices", nDevices).format(nDevices);
    },
});
