import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import St from 'gi://St';

import * as PanelMenu from '../panelMenu.js';
import * as PopupMenu from '../popupMenu.js';
import {QuickToggle} from '../quickSettings.js';

const A11Y_SCHEMA                   = 'org.gnome.desktop.a11y';
const KEY_ALWAYS_SHOW               = 'always-show-universal-access-status';

const A11Y_KEYBOARD_SCHEMA          = 'org.gnome.desktop.a11y.keyboard';
const KEY_STICKY_KEYS_ENABLED       = 'stickykeys-enable';
const KEY_BOUNCE_KEYS_ENABLED       = 'bouncekeys-enable';
const KEY_SLOW_KEYS_ENABLED         = 'slowkeys-enable';
const KEY_MOUSE_KEYS_ENABLED        = 'mousekeys-enable';

const APPLICATIONS_SCHEMA           = 'org.gnome.desktop.a11y.applications';

const DPI_FACTOR_LARGE              = 1.25;

const WM_SCHEMA                     = 'org.gnome.desktop.wm.preferences';
const KEY_VISUAL_BELL               = 'visual-bell';

const DESKTOP_INTERFACE_SCHEMA      = 'org.gnome.desktop.interface';
const KEY_TEXT_SCALING_FACTOR       = 'text-scaling-factor';

const A11Y_INTERFACE_SCHEMA         = 'org.gnome.desktop.a11y.interface';
const KEY_HIGH_CONTRAST             = 'high-contrast';

export const ATIndicator = GObject.registerClass(
class ATIndicator extends PanelMenu.Button {
    _init() {
        super._init(0.5, _('Accessibility'));

        this.add_child(new St.Icon({
            style_class: 'system-status-icon',
            icon_name: 'accessibility-menu-symbolic',
        }));

        this._a11ySettings = new Gio.Settings({schema_id: A11Y_SCHEMA});
        this._a11ySettings.connect(`changed::${KEY_ALWAYS_SHOW}`, this._queueSyncMenuVisibility.bind(this));

        let highContrast = this._buildItem(_('High Contrast'), A11Y_INTERFACE_SCHEMA, KEY_HIGH_CONTRAST);
        this.menu.addMenuItem(highContrast);

        const magnifier = this._buildItem(_('Zoom'),
            APPLICATIONS_SCHEMA, 'screen-magnifier-enabled');
        this.menu.addMenuItem(magnifier);

        let textZoom = this._buildFontItem();
        this.menu.addMenuItem(textZoom);

        const screenReader = this._buildItem(_('Screen Reader'),
            APPLICATIONS_SCHEMA, 'screen-reader-enabled');
        this.menu.addMenuItem(screenReader);

        const screenKeyboard = this._buildItem(_('Screen Keyboard'),
            APPLICATIONS_SCHEMA, 'screen-keyboard-enabled');
        this.menu.addMenuItem(screenKeyboard);

        const visualBell = this._buildItem(_('Visual Alerts'),
            WM_SCHEMA, KEY_VISUAL_BELL);
        this.menu.addMenuItem(visualBell);

        const stickyKeys = this._buildItem(_('Sticky Keys'),
            A11Y_KEYBOARD_SCHEMA, KEY_STICKY_KEYS_ENABLED);
        this.menu.addMenuItem(stickyKeys);

        const slowKeys = this._buildItem(_('Slow Keys'),
            A11Y_KEYBOARD_SCHEMA, KEY_SLOW_KEYS_ENABLED);
        this.menu.addMenuItem(slowKeys);

        const bounceKeys = this._buildItem(_('Bounce Keys'),
            A11Y_KEYBOARD_SCHEMA, KEY_BOUNCE_KEYS_ENABLED);
        this.menu.addMenuItem(bounceKeys);

        const mouseKeys = this._buildItem(_('Mouse Keys'),
            A11Y_KEYBOARD_SCHEMA, KEY_MOUSE_KEYS_ENABLED);
        this.menu.addMenuItem(mouseKeys);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.menu.addSettingsAction(_('Accessibility Settings'),
            'gnome-universal-access-panel.desktop');

        this._syncMenuVisibility();
    }

    _syncMenuVisibility() {
        this._syncMenuVisibilityIdle = 0;

        let alwaysShow = this._a11ySettings.get_boolean(KEY_ALWAYS_SHOW);
        let items = this.menu._getMenuItems();

        this.visible = alwaysShow || items.some(f => !!f.state);

        return GLib.SOURCE_REMOVE;
    }

    _queueSyncMenuVisibility() {
        if (this._syncMenuVisibilityIdle)
            return;

        this._syncMenuVisibilityIdle = GLib.idle_add(GLib.PRIORITY_DEFAULT, this._syncMenuVisibility.bind(this));
        GLib.Source.set_name_by_id(this._syncMenuVisibilityIdle, '[gnome-shell] this._syncMenuVisibility');
    }

    _buildItem(string, schema, key) {
        const settings = new Gio.Settings({schema_id: schema});
        const widget = new PopupMenu.PopupSwitchMenuItem(string, false);
        settings.bind(key, widget, 'state', Gio.SettingsBindFlags.DEFAULT);

        widget.connect('toggled',
            () => this._queueSyncMenuVisibility());

        return widget;
    }

    _buildFontItem() {
        const settings = new Gio.Settings({schema_id: DESKTOP_INTERFACE_SCHEMA});
        let factor = settings.get_double(KEY_TEXT_SCALING_FACTOR);
        const widget =
            new PopupMenu.PopupSwitchMenuItem(_('Large Text'), factor > 1.0);

        const toggledId = widget.connect('toggled', item => {
            if (item.state)
                settings.set_double(KEY_TEXT_SCALING_FACTOR, DPI_FACTOR_LARGE);
            else
                settings.reset(KEY_TEXT_SCALING_FACTOR);
        });

        settings.connect(`changed::${KEY_TEXT_SCALING_FACTOR}`, () => {
            factor = settings.get_double(KEY_TEXT_SCALING_FACTOR);
            let active = factor > 1.0;

            widget.block_signal_handler(toggledId);
            widget.setToggleState(active);
            widget.unblock_signal_handler(toggledId);

            this._queueSyncMenuVisibility();
        });
        settings.bind_writable(KEY_TEXT_SCALING_FACTOR,
            widget, 'sensitive',
            false);

        return widget;
    }
});

export const HighContrastToggle = GObject.registerClass(
class HighContrastToggle extends QuickToggle {
    constructor() {
        super({
            title: _('High Contrast'),
            iconName: 'accessibility-high-contrast-symbolic',
            toggleMode: true,
        });

        const settings = new Gio.Settings({schemaId: A11Y_INTERFACE_SCHEMA});
        settings.bind(KEY_HIGH_CONTRAST,
            this, 'checked',
            Gio.SettingsBindFlags.DEFAULT);
    }
});

export const MagnifierToggle = GObject.registerClass(
class MagnifierToggle extends QuickToggle {
    constructor() {
        super({
            title: _('Zoom'),
            iconName: 'accessibility-zoom-symbolic',
            toggleMode: true,
        });

        const settings = new Gio.Settings({schemaId: APPLICATIONS_SCHEMA});
        settings.bind('screen-magnifier-enabled',
            this, 'checked',
            Gio.SettingsBindFlags.DEFAULT);
    }
});

export const LargeTextToggle = GObject.registerClass(
class LargeTextToggle extends QuickToggle {
    constructor() {
        super({
            title: _('Large Text'),
            iconName: 'accessibility-large-text-symbolic',
            toggleMode: true,
        });

        this._settings = new Gio.Settings({schemaId: DESKTOP_INTERFACE_SCHEMA});
        this._updateChecked();

        const toggledId = this.connect('notify::checked', () => {
            if (this.checked)
                this._settings.set_double(KEY_TEXT_SCALING_FACTOR, DPI_FACTOR_LARGE);
            else
                this._settings.reset(KEY_TEXT_SCALING_FACTOR);
        });

        this._settings.connectObject(`changed::${KEY_TEXT_SCALING_FACTOR}`, () => {
            this.block_signal_handler(toggledId);
            this._updateChecked();
            this.unblock_signal_handler(toggledId);
        });
        this._settings.bind_writable(KEY_TEXT_SCALING_FACTOR,
            this, 'reactive',
            false);
    }

    _updateChecked() {
        const factor = this._settings.get_double(KEY_TEXT_SCALING_FACTOR);
        const checked = factor > 1.0;
        this.set({checked});
    }
});

export const ScreenReaderToggle = GObject.registerClass(
class ScreenReaderToggle extends QuickToggle {
    constructor() {
        super({
            title: _('Screen Reader'),
            iconName: 'accessibility-screen-reader-symbolic',
            toggleMode: true,
        });

        const settings = new Gio.Settings({schemaId: APPLICATIONS_SCHEMA});
        settings.bind('screen-reader-enabled',
            this, 'checked',
            Gio.SettingsBindFlags.DEFAULT);
    }
});

export const ScreenKeyboardToggle = GObject.registerClass(
class ScreenKeyboardToggle extends QuickToggle {
    constructor() {
        super({
            title: _('Screen Keyboard'),
            iconName: 'input-keyboard-symbolic',
            toggleMode: true,
        });

        const settings = new Gio.Settings({schemaId: APPLICATIONS_SCHEMA});
        settings.bind('screen-keyboard-enabled',
            this, 'checked',
            Gio.SettingsBindFlags.DEFAULT);
    }
});

export const VisualBellToggle = GObject.registerClass(
class VisualBellToggle extends QuickToggle {
    constructor() {
        super({
            title: _('Visual Alerts'),
            iconName: 'accessibility-visual-alerts-symbolic',
            toggleMode: true,
        });

        const settings = new Gio.Settings({schemaId: WM_SCHEMA});
        settings.bind(KEY_VISUAL_BELL,
            this, 'checked',
            Gio.SettingsBindFlags.DEFAULT);
    }
});

export const StickyKeysToggle = GObject.registerClass(
class StickyKeysToggle extends QuickToggle {
    constructor() {
        super({
            title: _('Sticky Keys'),
            iconName: 'accessibility-sticky-keys-symbolic',
            toggleMode: true,
        });

        const settings = new Gio.Settings({schemaId: A11Y_KEYBOARD_SCHEMA});
        settings.bind(KEY_STICKY_KEYS_ENABLED,
            this, 'checked',
            Gio.SettingsBindFlags.DEFAULT);
    }
});

export const SlowKeysToggle = GObject.registerClass(
class SlowKeysToggle extends QuickToggle {
    constructor() {
        super({
            title: _('Slow Keys'),
            iconName: 'accessibility-slow-keys-symbolic',
            toggleMode: true,
        });

        const settings = new Gio.Settings({schemaId: A11Y_KEYBOARD_SCHEMA});
        settings.bind(KEY_SLOW_KEYS_ENABLED,
            this, 'checked',
            Gio.SettingsBindFlags.DEFAULT);
    }
});

export const BounceKeysToggle = GObject.registerClass(
class BounceKeysToggle extends QuickToggle {
    constructor() {
        super({
            title: _('Bounce Keys'),
            iconName: 'accessibility-bounce-keys-symbolic',
            toggleMode: true,
        });

        const settings = new Gio.Settings({schemaId: A11Y_KEYBOARD_SCHEMA});
        settings.bind(KEY_BOUNCE_KEYS_ENABLED,
            this, 'checked',
            Gio.SettingsBindFlags.DEFAULT);
    }
});

export const MouseKeysToggle = GObject.registerClass(
class MouseKeysToggle extends QuickToggle {
    constructor() {
        super({
            title: _('Mouse Keys'),
            iconName: 'accessibility-mouse-keys-symbolic',
            toggleMode: true,
        });

        const settings = new Gio.Settings({schemaId: A11Y_KEYBOARD_SCHEMA});
        settings.bind(KEY_MOUSE_KEYS_ENABLED,
            this, 'checked',
            Gio.SettingsBindFlags.DEFAULT);
    }
});
