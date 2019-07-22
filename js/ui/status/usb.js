// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const { loadInterfaceXML } = imports.misc.fileUtils;

const PRIVACY_SCHEMA = 'org.gnome.desktop.privacy';
const USB_PROTECTION = 'usb-protection';
const USB_PROTECTION_LEVEL = 'usb-protection-level';

const USBGUARD_DBUS_IFACE = 'org.usbguard';
const USBGUARD_DBUS_NAME = 'org.usbguard';
const USBGUARD_DBUS_PATH = '/org/usbguard';

const USBGuardInterface = loadInterfaceXML(USBGUARD_DBUS_IFACE);

var Indicator = new Lang.Class({
    Name: 'USBIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init() {
        this.parent();

        this._proxy = null;
        this._proxyWorking = false;

        let nodeInfo = Gio.DBusNodeInfo.new_for_xml(USBGuardInterface);
        Gio.DBusProxy.new(Gio.DBus.system,
                          Gio.DBusProxyFlags.DO_NOT_AUTO_START,
                          nodeInfo.lookup_interface(USBGUARD_DBUS_IFACE),
                          USBGUARD_DBUS_NAME,
                          USBGUARD_DBUS_PATH,
                          USBGUARD_DBUS_IFACE,
                          null,
                          this._onProxyReady.bind(this));

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'drive-removable-media-symbolic';

        this._protectionSetting = false;
        this._protectionLevelSetting = 0;

        this._settings = new Gio.Settings({ schema_id: PRIVACY_SCHEMA });
        this._settings.connect('changed::' + USB_PROTECTION,
                               this._onProtectionSettingChanged.bind(this));
        this._settings.connect('changed::' + USB_PROTECTION_LEVEL,
                               this._onProtectionLevelSettingChanged.bind(this));
        this._onProtectionSettingChanged();
        this._onProtectionLevelSettingChanged();

        this._item = new PopupMenu.PopupSubMenuMenuItem(_("USB Protection"), true);
        this._item.icon.icon_name = 'drive-removable-media-symbolic';
        this._item.menu.addAction(_("Turn Off"), () => {
            this._settings.set_boolean(USB_PROTECTION, false);
        });
        this._item.menu.addSettingsAction(_("Privacy Settings"), 'gnome-privacy-panel.desktop');
        this.menu.addMenuItem(this._item);

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
    },

    _onProxyReady(o, res) {
        try {
            this._proxy = Gio.DBusProxy.new_finish(res);
        } catch(e) {
            log('error creating USBGuard proxy: %s'.format(e.message));
            return;
        }

        this._proxy.connect('notify::g-name-owner', this._onProxyOwnerChanged.bind(this));
        this._onProxyOwnerChanged();
    },

    _onProxyOwnerChanged() {
        if (!this._proxy.g_name_owner) {
            this._proxyWorking = false;
        } else {
            this._proxyWorking = true;
        }
        this._sessionUpdated();
    },

    _onProtectionSettingChanged() {
        this._protectionSetting = this._settings.get_boolean(USB_PROTECTION);
        this._sessionUpdated();
    },

    _onProtectionLevelSettingChanged() {
        this._protectionLevelSetting = this._settings.get_uint(USB_PROTECTION_LEVEL);
        this._sessionUpdated();
    },

    _isProtectionActive() {
        return this._protectionSetting && this._proxyWorking;
    },

    _sessionUpdated() {
        let unlocked = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        let visible = unlocked && this._isProtectionActive();
        this._indicator.visible = visible;
    }
});
