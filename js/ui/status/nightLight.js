// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const BUS_NAME = 'org.gnome.SettingsDaemon.Color';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Color';

const ColorInterface = `
<node>
<interface name="org.gnome.SettingsDaemon.Color">
  <property name="DisabledUntilTomorrow" type="b" access="readwrite"/>
  <property name="NightLightActive" type="b" access="read"/>
</interface>
</node>`;

const ColorProxy = Gio.DBusProxy.makeProxyWrapper(ColorInterface);

var Indicator = new Lang.Class({
    Name: 'NightLightIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init() {
        this.parent();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'night-light-symbolic';
        this._proxy = new ColorProxy(Gio.DBus.session, BUS_NAME, OBJECT_PATH,
                                     (proxy, error) => {
                                         if (error) {
                                             log(error.message);
                                             return;
                                         }
                                         this._proxy.connect('g-properties-changed',
                                                             this._sync.bind(this));
                                         this._sync();
                                     });

        this._item = new PopupMenu.PopupSubMenuMenuItem("", true);
        this._item.icon.icon_name = 'night-light-symbolic';
        this._disableItem = this._item.menu.addAction('', () => {
            this._proxy.DisabledUntilTomorrow = !this._proxy.DisabledUntilTomorrow;
        });
        this._item.menu.addAction(_("Turn Off"), () => {
            let settings = new Gio.Settings({ schema_id: 'org.gnome.settings-daemon.plugins.color' });
            settings.set_boolean('night-light-enabled', false);
        });
        this._item.menu.addSettingsAction(_("Display Settings"), 'gnome-display-panel.desktop');
        this.menu.addMenuItem(this._item);

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
        this._sync();
    },

    _sessionUpdated() {
        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        this.menu.setSensitive(sensitive);
    },

    _sync() {
        let visible = this._proxy.NightLightActive;
        let disabled = this._proxy.DisabledUntilTomorrow;

        this._item.label.text = disabled ? _("Night Light Disabled")
                                         : _("Night Light On");
        this._disableItem.label.text = disabled ? _("Resume")
                                                : _("Disable Until Tomorrow");
        this._item.actor.visible = this._indicator.visible = visible;
    }
});
