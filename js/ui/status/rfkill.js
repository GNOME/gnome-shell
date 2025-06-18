import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';

import {QuickToggle, SystemIndicator} from '../quickSettings.js';

import {loadInterfaceXML} from '../../misc/fileUtils.js';

const BUS_NAME = 'org.gnome.SettingsDaemon.Rfkill';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Rfkill';

const RfkillManagerInterface = loadInterfaceXML('org.gnome.SettingsDaemon.Rfkill');
const rfkillManagerInfo = Gio.DBusInterfaceInfo.new_for_xml(RfkillManagerInterface);

const RfkillManager = GObject.registerClass({
    Properties: {
        'airplane-mode': GObject.ParamSpec.boolean(
            'airplane-mode', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'hw-airplane-mode': GObject.ParamSpec.boolean(
            'hw-airplane-mode', null, null,
            GObject.ParamFlags.READABLE,
            false),
        'show-airplane-mode': GObject.ParamSpec.boolean(
            'show-airplane-mode', null, null,
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
        return this._proxy.HasAirplaneMode && this._proxy.ShouldShowAirplaneMode;
    }

    _changed(proxy, properties) {
        for (const prop in properties.deepUnpack()) {
            switch (prop) {
            case 'AirplaneMode':
                this.notify('airplane-mode');
                break;
            case 'HardwareAirplaneMode':
                this.notify('hw-airplane-mode');
                break;
            case 'HasAirplaneMode':
            case 'ShouldShowAirplaneMode':
                this.notify('show-airplane-mode');
                break;
            }
        }
    }
});

let _manager;

/**
 * @returns {RfkillManager}
 */
export function getRfkillManager() {
    if (_manager != null)
        return _manager;

    _manager = new RfkillManager();
    return _manager;
}

const RfkillToggle = GObject.registerClass(
class RfkillToggle extends QuickToggle {
    _init() {
        super._init({
            title: _('Airplane Mode'),
            iconName: 'airplane-mode-symbolic',
        });

        this._manager = getRfkillManager();
        this._manager.bind_property('show-airplane-mode',
            this, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        this._manager.bind_property('airplane-mode',
            this, 'checked',
            GObject.BindingFlags.SYNC_CREATE);

        this.connect('clicked',
            () => (this._manager.airplaneMode = !this._manager.airplaneMode));
    }
});

export const Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'airplane-mode-symbolic';

        this._rfkillToggle = new RfkillToggle();
        this._rfkillToggle.connectObject(
            'notify::visible', () => this._sync(),
            'notify::checked', () => this._sync(),
            this);
        this.quickSettingsItems.push(this._rfkillToggle);

        this._sync();
    }

    _sync() {
        // Only show indicator when airplane mode is on
        const {visible, checked} = this._rfkillToggle;
        this._indicator.visible = visible && checked;
    }
});
