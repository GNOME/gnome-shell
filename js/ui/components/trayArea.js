// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// Copyright (C) phocean <jc@phocean.net>
// Copyright (C) 2017 Endless Mobile, Inc.
//
// This is a GNOME Shell component implemented to keep providing a way
// for apps the still rely in the old-fashioned tray area present in
// GNOME Shell < 3.26 to keep showing up somewhere in Endless OS.
//
// Since this is the same goal achieved by the -now unmaintained- extensions
// TopIcons and TopIcons Plus, we are pretty much forking the code in the
// latter here and integrating it as a component in our shell, so that our
// users can keep enjoying the ability to actually close certain applications
// that would keep running in the background otherwise (e.g. Steam, Dropbox..).
//
// The code from the original TopIcons Plus extension used as the source for
// this component can be checked in https://github.com/phocean/TopIcons-plus.
//
// Licensed under the GNU General Public License Version 2
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

const { Clutter, Gio, GLib, GObject, Shell, St } = imports.gi;

const Main = imports.ui.main;
const System = imports.system;
const PanelMenu = imports.ui.panelMenu;
const ExtensionUtils = imports.misc.extensionUtils;

// The original TopIcons Plus extension allowed configuring these values via a
// GSettings schema, but we don't need that for now, let's hardcode those values.
const ICON_BRIGHTNESS = 0.0;   // range: [-1.0, 1.0]
const ICON_CONTRAST = 0.0;     // range: [-1.0, 1.0]
const ICON_OPACITY = 153;      // range: [0, 255]
const ICON_DESATURATION = 1.0; // range: [0.0, 1.0]
const ICON_SIZE = 16;

// We don't want to show icons in the tray bar for apps that are already
// handled by other extensions, such as in the case of Skype.
const EXTENSIONS_BLACKLIST = [
    ["skype","SkypeNotification@chrisss404.gmail.com"]
];

const SETTINGS_SCHEMA = 'org.gnome.shell';
const SETTING_ENABLE_TRAY_AREA = 'enable-tray-area';

var TrayArea = class TrayArea {
    constructor() {
        this._icons = [];
        this._iconsBoxLayout = null;
        this._iconsContainer = null;
        this._trayManager = null;

        this._settings = new Gio.Settings({ schema_id: SETTINGS_SCHEMA });

        // The original extension allowed to configure the target side of
        // the bottom panel but we will always be using the right one.
        this._targetPanel = Main.panel._rightBox;

        this._trayAreaEnabled = this._settings.get_boolean(SETTING_ENABLE_TRAY_AREA);

        global.settings.connect('changed::' + SETTING_ENABLE_TRAY_AREA, () => {
            this._trayAreaEnabled = this._settings.get_boolean(SETTING_ENABLE_TRAY_AREA);

            if (this._trayManager)
                this._destroyTray();

            if (this._trayAreaEnabled)
                this._createTray();
        });
    }

    _updateIconStyle(icon) {
        // Size
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        if (arguments.length == 1) {
            icon.get_parent().set_size(ICON_SIZE * scaleFactor, ICON_SIZE * scaleFactor);
            icon.set_size(ICON_SIZE * scaleFactor, ICON_SIZE * scaleFactor);
        } else {
            for (let icon of this._icons) {
                icon.get_parent().set_size(ICON_SIZE * scaleFactor, ICON_SIZE * scaleFactor);
                icon.set_size(ICON_SIZE * scaleFactor, ICON_SIZE * scaleFactor);
            }
        }

        // Opacity
        if (arguments.length == 1) {
            icon.get_parent().connect('enter-event', (actor, event) => icon.opacity = 255);
            icon.get_parent().connect('leave-event', (actor, event) => icon.opacity = ICON_OPACITY);
            icon.opacity = ICON_OPACITY;
        } else {
            for (let icon of this._icons) {
                icon.get_parent().connect('enter-event', (actor, event) => icon.opacity = 255);
                icon.get_parent().connect('leave-event', (actor, event) => icon.opacity = ICON_OPACITY);
                icon.opacity = ICON_OPACITY;
            }
        }

        // Saturation
        if (arguments.length == 1) {
            let desat_effect = new Clutter.DesaturateEffect({ factor : ICON_DESATURATION });
            desat_effect.set_factor(ICON_DESATURATION);
            icon.add_effect_with_name('desaturate', desat_effect);
        } else {
            for (let icon of this._icons) {
                let effect = icon.get_effect('desaturate');
                if (effect)
                    effect.set_factor(ICON_DESATURATION);
            }
        }

        // Brightness & Contrast
        if (arguments.length == 1) {
            let bright_effect = new Clutter.BrightnessContrastEffect({});
            bright_effect.set_brightness(ICON_BRIGHTNESS);
            bright_effect.set_contrast(ICON_CONTRAST);
            icon.add_effect_with_name('brightness-contrast', bright_effect);
        } else {
            for (let icon of this._icons) {
                let effect = icon.get_effect('brightness-contrast')
                effect.set_brightness(ICON_BRIGHTNESS);
                effect.set_contrast(ICON_CONTRAST);
            }
        }

        icon.reactive = true;
    }

    _onTrayIconAdded(o, icon, role, delay=1000) {
        // Loop through the array and hide the extension if extension X is
        // enabled and corresponding application is running
        let iconWmClass = icon.wm_class ? icon.wm_class.toLowerCase() : '';
        for (let [wmClass, uuid] of EXTENSIONS_BLACKLIST) {
            if (ExtensionUtils.extensions[uuid] !== undefined &&
                ExtensionUtils.extensions[uuid].state === 1 &&
                iconWmClass === wmClass)
                return;
        }

        let iconContainer = new St.Button({
            child: icon,
            visible: false,
        });

        icon.connect("destroy", () => {
            icon.clear_effects();
            iconContainer.destroy();
        });

        iconContainer.connect('button-release-event', (actor, event) => {
            icon.click(event);
        });

        GLib.timeout_add(GLib.PRIORITY_DEFAULT, delay, () => {
            iconContainer.visible = true;
            return GLib.SOURCE_REMOVE;
        });

        this._iconsBoxLayout.insert_child_at_index(iconContainer, 0);
        this._updateIconStyle(icon);
        this._icons.push(icon);
    }

    _onTrayIconRemoved(o, icon) {
        if (this._icons.indexOf(icon) == -1)
            return;

        icon.get_parent().destroy();
        this._icons.splice(this._icons.indexOf(icon), 1);
    }

    _createTray() {
        this._iconsBoxLayout = new St.BoxLayout({ style_class: 'tray-area' });

        // An empty ButtonBox will still display padding,therefore create it without visibility.
        this._iconsContainer = new PanelMenu.ButtonBox({ visible: false });
        this._iconsContainer.add_actor(this._iconsBoxLayout);
        this._iconsContainer.show();

        this._trayManager = new Shell.TrayManager();
        this._trayManager.connect('tray-icon-added', this._onTrayIconAdded.bind(this));
        this._trayManager.connect('tray-icon-removed', this._onTrayIconRemoved.bind(this));
        this._trayManager.manage_screen(Main.panel);

        // The actor for a PanelMenu.ButtonBox is already inside a StBin
        // container, so remove that association before re-adding it below.
        let parent = this._iconsContainer.get_parent();
        if (parent)
            parent.remove_actor(this._iconsContainer);

        // Place our tray at the end of the panel (e.g. leftmost side for right panel).
        this._targetPanel.insert_child_at_index(this._iconsContainer, 0);

    }

    _destroyTray() {
        this._iconsContainer.destroy();

        this._trayManager = null;
        this._iconsContainer = null;
        this._iconsBoxLayout = null;
        this._icons = [];

        // Force finalizing tray to unmanage screen
        System.gc();
    }

    enable() {
        Main.trayArea = this;
        if (this._trayAreaEnabled)
            this._createTray();
    }

    disable() {
        this._destroyTray();
        Main.trayArea = null;
    }
};
var Component = TrayArea;
