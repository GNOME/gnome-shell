// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const BUS_NAME = 'org.gnome.SettingsDaemon.Rfkill';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Rfkill';

const RfkillManagerInterface = '<node> \
<interface name="org.gnome.SettingsDaemon.Rfkill"> \
<property name="AirplaneMode" type="b" access="readwrite" /> \
</interface> \
</node>';

const RfkillManagerProxy = Gio.DBusProxy.makeProxyWrapper(RfkillManagerInterface);

const Indicator = new Lang.Class({
    Name: 'RfkillIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init: function() {
        this.parent();

        this._proxy = new RfkillManagerProxy(Gio.DBus.session, BUS_NAME, OBJECT_PATH,
                                             Lang.bind(this, function(proxy, error) {
                                                 if (error) {
                                                     log(error.message);
                                                     return;
                                                 }
                                                 this._proxy.connect('g-properties-changed',
                                                                     Lang.bind(this, this._sync));
                                                 this._sync();
                                             }));

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'airplane-mode-symbolic';
        this._indicator.hide();

        // The menu only appears when airplane mode is on, so just
        // statically build it as if it was on, rather than dynamically
        // changing the menu contents.
        this._item = new PopupMenu.PopupSubMenuMenuItem(_("Airplane Mode"), true);
        this._item.icon.icon_name = 'airplane-mode-symbolic';
        this._item.status.text = _("On");
        this._item.menu.addAction(_("Turn Off"), Lang.bind(this, function() {
            this._proxy.AirplaneMode = false;
        }));
        this._item.menu.addSettingsAction(_("Network Settings"), 'gnome-network-panel.desktop');
        this.menu.addMenuItem(this._item);
    },

    _sync: function() {
        let airplaneMode = this._proxy.AirplaneMode;
        this._indicator.visible = airplaneMode;
        this._item.actor.visible = airplaneMode;
    },
});
