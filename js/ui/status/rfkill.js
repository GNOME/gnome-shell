// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import * as Signals from '../../misc/signals.js';

import Main from '../main.js';
import * as PanelMenu from '../panelMenu.js';
import * as PopupMenu from '../popupMenu.js';

import { loadInterfaceXML } from '../../misc/fileUtilsModule.js';

const BUS_NAME = 'org.gnome.SettingsDaemon.Rfkill';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Rfkill';

const RfkillManagerInterface = loadInterfaceXML('org.gnome.SettingsDaemon.Rfkill');
const RfkillManagerProxy = Gio.DBusProxy.makeProxyWrapper(RfkillManagerInterface);

var RfkillManager = class extends Signals.EventEmitter {
    constructor() {
        super();

        this._proxy = new RfkillManagerProxy(Gio.DBus.session, BUS_NAME, OBJECT_PATH,
                                         (proxy, error) => {
                                             if (error) {
                                                 log(error.message);
                                                 return;
                                             }
                                             this._proxy.connect('g-properties-changed',
                                                                 this._changed.bind(this));
                                             this._changed();
                                         });
    }

    get airplaneMode() {
        return this._proxy.AirplaneMode;
    }

    set airplaneMode(v) {
        this._proxy.AirplaneMode = v;
    }

    get hwAirplaneMode() {
        return this._proxy.HardwareAirplaneMode;
    }

    get shouldShowAirplaneMode() {
        return this._proxy.ShouldShowAirplaneMode;
    }

    _changed() {
        this.emit('airplane-mode-changed');
    }
};

var _manager;
export function getRfkillManager() {
    if (_manager != null)
        return _manager;

    _manager = new RfkillManager();
    return _manager;
}

export const Indicator = GObject.registerClass(
class Indicator extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        this._manager = getRfkillManager();
        this._manager.connect('airplane-mode-changed', this._sync.bind(this));

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
        let airplaneMode = this._manager.airplaneMode;
        let hwAirplaneMode = this._manager.hwAirplaneMode;
        let showAirplaneMode = this._manager.shouldShowAirplaneMode;

        this._indicator.visible = airplaneMode && showAirplaneMode;
        this._item.visible = airplaneMode && showAirplaneMode;
        this._offItem.setSensitive(!hwAirplaneMode);

        if (hwAirplaneMode)
            this._offItem.label.text = _("Use hardware switch to turn off");
        else
            this._offItem.label.text = _("Turn Off");
    }
});
