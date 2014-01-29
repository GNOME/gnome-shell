// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Signals = imports.signals;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const BUS_NAME = 'org.gnome.SettingsDaemon.Rfkill';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Rfkill';

const RfkillManagerInterface = '<node> \
<interface name="org.gnome.SettingsDaemon.Rfkill"> \
<property name="AirplaneMode" type="b" access="readwrite" /> \
<property name="HardwareAirplaneMode" type="b" access="read" /> \
</interface> \
</node>';

const RfkillManagerProxy = Gio.DBusProxy.makeProxyWrapper(RfkillManagerInterface);

const RfkillManager = new Lang.Class({
    Name: 'RfkillManager',

    _init: function() {
        this._proxy = new RfkillManagerProxy(Gio.DBus.session, BUS_NAME, OBJECT_PATH,
                                             Lang.bind(this, function(proxy, error) {
                                                 if (error) {
                                                     log(error.message);
                                                     return;
                                                 }
                                                 this._proxy.connect('g-properties-changed',
                                                                     Lang.bind(this, this._changed));
                                                 this._changed();
                                             }));
    },

    get airplaneMode() {
        return this._proxy.AirplaneMode;
    },

    set airplaneMode(v) {
        this._proxy.AirplaneMode = v;
    },

    get hwAirplaneMode() {
        return this._proxy.HardwareAirplaneMode;
    },

    _changed: function() {
        this.emit('airplane-mode-changed');
    }
});
Signals.addSignalMethods(RfkillManager.prototype);

var _manager;
function getRfkillManager() {
    if (_manager != null)
        return _manager;

    _manager = new RfkillManager();
    return _manager;
}

const Indicator = new Lang.Class({
    Name: 'RfkillIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init: function() {
        this.parent();

        this._manager = getRfkillManager();
        this._manager.connect('airplane-mode-changed', Lang.bind(this, this._sync));

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'airplane-mode-symbolic';
        this._indicator.hide();

        // The menu only appears when airplane mode is on, so just
        // statically build it as if it was on, rather than dynamically
        // changing the menu contents.
        this._item = new PopupMenu.PopupSubMenuMenuItem(_("Airplane Mode"), true);
        this._item.icon.icon_name = 'airplane-mode-symbolic';
        this._item.status.text = _("On");
        this._offItem = this._item.menu.addAction(_("Turn Off"), Lang.bind(this, function() {
            this._proxy.AirplaneMode = false;
        }));
        this._item.menu.addSettingsAction(_("Network Settings"), 'gnome-network-panel.desktop');
        this.menu.addMenuItem(this._item);
    },

    _sync: function() {
        let airplaneMode = this._manager.airplaneMode;
        let hwAirplaneMode = this._manager.hwAirplaneMode;
        let changed = (airplaneMode != this._indicator.visible) ||
            (hwAirplaneMode != this._offItem.actor.visible);

        this._indicator.visible = airplaneMode;
        this._item.actor.visible = airplaneMode;
        this._offItem.setSensitive(!hwAirplaneMode);

        if (hwAirplaneMode)
            this._offItem.label.text = _("Use hardware switch to turn off");
        else
            this._offItem.label.text = _("Turn Off");
    },
});
