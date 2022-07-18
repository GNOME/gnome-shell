// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const {Gio, GLib, GObject} = imports.gi;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const {loadInterfaceXML} = imports.misc.fileUtils;

const BUS_NAME = 'org.gnome.SettingsDaemon.Rfkill';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Rfkill';

const RfkillManagerInterface = loadInterfaceXML('org.gnome.SettingsDaemon.Rfkill');
const rfkillManagerInfo = Gio.DBusInterfaceInfo.new_for_xml(RfkillManagerInterface);

const RfkillManager = GObject.registerClass({
    Properties: {
        'airplane-mode': GObject.ParamSpec.boolean(
            'airplane-mode', '', '',
            GObject.ParamFlags.READWRITE,
            false),
        'hw-airplane-mode': GObject.ParamSpec.boolean(
            'hw-airplane-mode', '', '',
            GObject.ParamFlags.READABLE,
            false),
        'show-airplane-mode': GObject.ParamSpec.boolean(
            'show-airplane-mode', '', '',
            GObject.ParamFlags.READABLE,
            false),
    },
}, class RfkillManager extends GObject.Object {
    constructor() {
        super();

        this._proxy = new Gio.DBusProxy({
            g_connection: Gio.DBus.session,
            g_name: BUS_NAME,
            g_object_path: OBJECT_PATH,
            g_interface_name: rfkillManagerInfo.name,
            g_interface_info: rfkillManagerInfo,
        });
        this._proxy.connect('g-properties-changed', this._changed.bind(this));
        this._proxy.init_async(GLib.PRIORITY_DEFAULT, null)
            .catch(e => console.error(e.message));
    }

    /* eslint-disable camelcase */
    get airplane_mode() {
        return this._proxy.AirplaneMode;
    }

    set airplane_mode(v) {
        this._proxy.AirplaneMode = v;
    }

    get hw_airplane_mode() {
        return this._proxy.HardwareAirplaneMode;
    }

    get show_airplane_mode() {
        return this._proxy.ShouldShowAirplaneMode;
    }
    /* eslint-enable camelcase */

    _changed(proxy, properties) {
        for (const prop in properties.deep_unpack()) {
            switch (prop) {
            case 'AirplaneMode':
                this.notify('airplane-mode');
                break;
            case 'HardwareAirplaneMode':
                this.notify('hw-airplane-mode');
                break;
            case 'ShouldShowAirplaneMode':
                this.notify('show-airplane-mode');
                break;
            }
        }
    }
});

var _manager;
function getRfkillManager() {
    if (_manager != null)
        return _manager;

    _manager = new RfkillManager();
    return _manager;
}

var Indicator = GObject.registerClass(
class Indicator extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        this._manager = getRfkillManager();
        this._manager.connect('notify', () => this._sync());

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'airplane-mode-symbolic';
        this._indicator.hide();

        // The menu only appears when airplane mode is on, so just
        // statically build it as if it was on, rather than dynamically
        // changing the menu contents.
        this._item = new PopupMenu.PopupSubMenuMenuItem(_("Airplane Mode On"), true);
        this._item.icon.icon_name = 'airplane-mode-symbolic';
        this._offItem = this._item.menu.addAction(_("Turn Off"), () => {
            this._manager.airplaneMode = false;
        });
        this._item.menu.addSettingsAction(_("Network Settings"), 'gnome-network-panel.desktop');
        this.menu.addMenuItem(this._item);

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();

        this._sync();
    }

    _sessionUpdated() {
        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        this.menu.setSensitive(sensitive);
    }

    _sync() {
        const {airplaneMode, hwAirplaneMode, showAirplaneMode} = this._manager;

        this._indicator.visible = airplaneMode && showAirplaneMode;
        this._item.visible = airplaneMode && showAirplaneMode;
        this._offItem.setSensitive(!hwAirplaneMode);

        if (hwAirplaneMode)
            this._offItem.label.text = _("Use hardware switch to turn off");
        else
            this._offItem.label.text = _("Turn Off");
    }
});
