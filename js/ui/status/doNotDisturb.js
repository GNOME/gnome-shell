import Gio from 'gi://Gio';
import GObject from 'gi://GObject';

import {QuickToggle, SystemIndicator} from '../quickSettings.js';

const DoNotDisturbToggle = GObject.registerClass(
class DoNotDisturbToggle extends QuickToggle {
    constructor() {
        super({
            title: _('Do Not Disturb'),
            iconName: 'notifications-disabled-symbolic',
            toggleMode: true,
        });

        this._settings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.notifications',
        });
        this._settings.bind('show-banners',
            this, 'checked',
            Gio.SettingsBindFlags.INVERT_BOOLEAN);
    }
});

export const Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    constructor() {
        super();
        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'notifications-disabled-symbolic';

        const toggle = new DoNotDisturbToggle();
        toggle.bind_property('checked',
            this._indicator, 'visible',
            GObject.BindingFlags.SYNC_CREATE);

        this.quickSettingsItems.push(toggle);
    }
});

