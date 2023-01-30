// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const {Gio, GLib, GObject} = imports.gi;

const {QuickToggle, SystemIndicator} = imports.ui.quickSettings;

const {loadInterfaceXML} = imports.misc.fileUtils;

const BUS_NAME = 'org.gnome.SettingsDaemon.Color';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Color';

const ColorInterface = loadInterfaceXML('org.gnome.SettingsDaemon.Color');
const colorInfo = Gio.DBusInterfaceInfo.new_for_xml(ColorInterface);

const NightLightToggle = GObject.registerClass(
class NightLightToggle extends QuickToggle {
    _init() {
        super._init({
            title: _('Night Light'),
            iconName: 'night-light-symbolic',
            toggleMode: true,
        });

        const monitorManager = global.backend.get_monitor_manager();
        monitorManager.bind_property('night-light-supported',
            this, 'visible',
            GObject.BindingFlags.SYNC_CREATE);

        this._settings = new Gio.Settings({
            schema_id: 'org.gnome.settings-daemon.plugins.color',
        });
        this._settings.bind('night-light-enabled',
            this, 'checked',
            Gio.SettingsBindFlags.DEFAULT);
    }
});

var Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'night-light-symbolic';

        this.quickSettingsItems.push(new NightLightToggle());

        this._proxy = new Gio.DBusProxy({
            g_connection: Gio.DBus.session,
            g_name: BUS_NAME,
            g_object_path: OBJECT_PATH,
            g_interface_name: colorInfo.name,
            g_interface_info: colorInfo,
        });
        this._proxy.connect('g-properties-changed', (p, properties) => {
            const nightLightActiveChanged = !!properties.lookup_value('NightLightActive', null);
            if (nightLightActiveChanged)
                this._sync();
        });
        this._proxy.init_async(GLib.PRIORITY_DEFAULT, null)
            .catch(e => console.error(e.message));

        this._sync();
    }

    _sync() {
        this._indicator.visible = this._proxy.NightLightActive;
    }
});
