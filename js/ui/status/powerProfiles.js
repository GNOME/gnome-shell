import Gio from 'gi://Gio';
import GObject from 'gi://GObject';


import {QuickMenuToggle, SystemIndicator} from '../quickSettings.js';

import * as PopupMenu from '../popupMenu.js';

import {loadInterfaceXML} from '../../misc/fileUtils.js';

const BUS_NAME = 'org.freedesktop.UPower.PowerProfiles';
const OBJECT_PATH = '/org/freedesktop/UPower/PowerProfiles';

const PowerProfilesIface = loadInterfaceXML('org.freedesktop.UPower.PowerProfiles');
const PowerProfilesProxy = Gio.DBusProxy.makeProxyWrapper(PowerProfilesIface);

const PROFILE_PARAMS = {
    'performance': {
        name: C_('Power profile', 'Performance'),
        iconName: 'power-profile-performance-symbolic',
    },

    'balanced': {
        name: C_('Power profile', 'Balanced'),
        iconName: 'power-profile-balanced-symbolic',
    },

    'power-saver': {
        name: C_('Power profile', 'Power Saver'),
        iconName: 'power-profile-power-saver-symbolic',
    },
};

const FALLBACK_PARAMS = {
    name: C_('Power profile', 'Custom'),
    iconName: 'gnome-power-manager-symbolic',
};

const LAST_PROFILE_KEY = 'last-selected-power-profile';

const PowerProfilesToggle = GObject.registerClass(
class PowerProfilesToggle extends QuickMenuToggle {
    _init() {
        super._init({
            title: C_('Quick settings button title', 'Power Mode'),
            menuButtonAccessibleName: _('Open power profiles menu'),
        });

        this._profileItems = new Map();

        this.connect('clicked', () => {
            this._proxy.ActiveProfile = this.checked
                ? 'balanced'
                : global.settings.get_string(LAST_PROFILE_KEY);
        });

        this._proxy = new PowerProfilesProxy(Gio.DBus.system, BUS_NAME, OBJECT_PATH,
            (proxy, error) => {
                if (error) {
                    log(error.message);
                } else {
                    this._proxy.connect('g-properties-changed', (p, properties) => {
                        const profilesChanged = !!properties.lookup_value('Profiles', null);
                        if (profilesChanged)
                            this._syncProfiles();
                        this._sync();
                    });

                    if (this._proxy.g_name_owner)
                        this._syncProfiles();
                }
                this._sync();
            });

        this._profileSection = new PopupMenu.PopupMenuSection();
        this.menu.addMenuItem(this._profileSection);
        this.menu.setHeader('power-profile-balanced-symbolic', C_('Quick settings menu header', 'Power Mode'));
        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.menu.addSettingsAction(_('Power Settings'),
            'gnome-power-panel.desktop');

        this._sync();
    }

    _syncProfiles() {
        this._profileSection.removeAll();
        this._profileItems.clear();

        const profiles = this._proxy.Profiles
            .map(p => p.Profile.unpack())
            .reverse();
        for (const profile of profiles) {
            if (!PROFILE_PARAMS[profile])
                continue;

            const {name, iconName} = PROFILE_PARAMS[profile];
            if (!name)
                continue;

            const item = new PopupMenu.PopupImageMenuItem(name, iconName);
            item.connect('activate',
                () => (this._proxy.ActiveProfile = profile));
            this._profileItems.set(profile, item);
            this._profileSection.addMenuItem(item);
        }

        this.menuEnabled = this._profileItems.size > 2;
    }

    _sync() {
        this.visible = this._proxy.g_name_owner !== null;

        if (!this.visible)
            return;

        const {ActiveProfile: activeProfile} = this._proxy;

        for (const [profile, item] of this._profileItems) {
            item.setOrnament(profile === activeProfile
                ? PopupMenu.Ornament.CHECK
                : PopupMenu.Ornament.NONE);
        }

        const {name: subtitle, iconName} = PROFILE_PARAMS[activeProfile] ?? FALLBACK_PARAMS;
        this.set({subtitle, iconName});

        this.checked = activeProfile !== 'balanced';

        if (this.checked)
            global.settings.set_string(LAST_PROFILE_KEY, activeProfile);
    }
});

export const Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this.quickSettingsItems.push(new PowerProfilesToggle());
    }
});
