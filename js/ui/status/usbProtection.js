// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Gio } = imports.gi;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const PRIVACY_SCHEMA = 'org.gnome.desktop.privacy';
const USB_PROTECTION = 'usb-protection';

const USBGUARD_DBUS_NAME = 'org.usbguard';

var Indicator = class extends PanelMenu.SystemIndicator {
    constructor() {
        super();

        this._usbGuardWorking = false;

        Gio.bus_watch_name(Gio.BusType.SYSTEM,
                           USBGUARD_DBUS_NAME,
                           0,
                           this._onUSBGuardPresent.bind(this),
                           this._onUSBGuardVanished.bind(this));

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'drive-removable-media-symbolic';

        this._protectionActive = false;

        this._item = new PopupMenu.PopupSubMenuMenuItem(_("USB Protection"), true);
        this._item.icon.icon_name = 'drive-removable-media-symbolic';
        this._item.menu.addAction(_("Turn Off"), () => {
            this._settings.set_boolean(USB_PROTECTION, false);
        });
        this._item.menu.addSettingsAction(_("Privacy Settings"), 'gnome-privacy-panel.desktop');
        this.menu.addMenuItem(this._item);

        this._settings = new Gio.Settings({ schema_id: PRIVACY_SCHEMA });
        this._settings.connect(`changed::${USB_PROTECTION}`,
                               this._onProtectionSettingChanged.bind(this));
        this._onProtectionSettingChanged();

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
    }

    _onUSBGuardPresent() {
        this._usbGuardWorking = true;
        this._sessionUpdated();
    }

    _onUSBGuardVanished() {
        this._usbGuardWorking = false;
        this._sessionUpdated();
    }

    _onProtectionSettingChanged() {
        this._protectionActive = this._settings.get_boolean(USB_PROTECTION);
        this._sessionUpdated();
    }

    _isProtectionActive() {
        return this._protectionActive && this._usbGuardWorking;
    }

    _sessionUpdated() {
        let unlocked = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        let visible = unlocked && this._isProtectionActive();
        this._item.actor.visible = this._indicator.visible = visible;
    }
};
