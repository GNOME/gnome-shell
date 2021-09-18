// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const { Gio, GObject } = imports.gi;

import Main from '../main.js';
import * as PanelMenu from '../panelMenu.js';
import * as PopupMenu from '../popupMenu.js';

import { loadInterfaceXML } from '../../misc/fileUtilsModule.js';

const BUS_NAME = 'net.hadess.PowerProfiles';
const OBJECT_PATH = '/net/hadess/PowerProfiles';

const PowerProfilesIface = loadInterfaceXML('net.hadess.PowerProfiles');
const PowerProfilesProxy = Gio.DBusProxy.makeProxyWrapper(PowerProfilesIface);

const PROFILE_LABELS = {
    'performance': C_('Power profile', 'Performance'),
    'balanced': C_('Power profile', 'Balanced'),
    'power-saver': C_('Power profile', 'Power Saver'),
};
const PROFILE_ICONS = {
    'performance': 'power-profile-performance-symbolic',
    'balanced': 'power-profile-balanced-symbolic',
    'power-saver': 'power-profile-power-saver-symbolic',
};

export const Indicator = GObject.registerClass(
class Indicator extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        this._profileItems = new Map();
        this._updateProfiles = true;

        this._proxy = new PowerProfilesProxy(Gio.DBus.system, BUS_NAME, OBJECT_PATH,
            (proxy, error) => {
                if (error) {
                    log(error.message);
                } else {
                    this._proxy.connect('g-properties-changed',
                        (p, properties) => {
                            const propertyNames = properties.deep_unpack();
                            this._updateProfiles = 'Profiles' in propertyNames;
                            this._sync();
                        });
                }
                this._sync();
            });

        this._item = new PopupMenu.PopupSubMenuMenuItem('', true);

        this._profileSection = new PopupMenu.PopupMenuSection();
        this._item.menu.addMenuItem(this._profileSection);
        this._item.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this._item.menu.addSettingsAction(_('Power Settings'),
            'gnome-power-panel.desktop');
        this.menu.addMenuItem(this._item);

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
        this._sync();
    }

    _sessionUpdated() {
        const sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        this.menu.setSensitive(sensitive);
    }

    _sync() {
        this._item.visible = this._proxy.g_name_owner !== null;

        if (!this._item.visible)
            return;

        if (this._updateProfiles) {
            this._profileSection.removeAll();
            this._profileItems.clear();

            const profiles = this._proxy.Profiles
                .map(p => p.Profile.unpack())
                .reverse();
            for (const profile of profiles) {
                const label = PROFILE_LABELS[profile];
                if (!label)
                    continue;

                const item = new PopupMenu.PopupMenuItem(label);
                item.connect('activate',
                    () => (this._proxy.ActiveProfile = profile));
                this._profileItems.set(profile, item);
                this._profileSection.addMenuItem(item);
            }
            this._updateProfiles = false;
        }

        for (const [profile, item] of this._profileItems) {
            item.setOrnament(profile === this._proxy.ActiveProfile
                ? PopupMenu.Ornament.DOT
                : PopupMenu.Ornament.NONE);
        }

        const perfItem = this._profileItems.get('performance');
        if (perfItem)
            perfItem.sensitive = this._proxy.PerformanceInhibited === '';

        this._item.label.text = PROFILE_LABELS[this._proxy.ActiveProfile];
        this._item.icon.icon_name = PROFILE_ICONS[this._proxy.ActiveProfile];
    }
});
