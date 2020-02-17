// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ATIndicator */

const { Gio, GLib, GObject, St } = imports.gi;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const A11Y_SCHEMA                   = 'org.gnome.desktop.a11y';
const KEY_ALWAYS_SHOW               = 'always-show-universal-access-status';

const A11Y_KEYBOARD_SCHEMA          = 'org.gnome.desktop.a11y.keyboard';
const KEY_STICKY_KEYS_ENABLED       = 'stickykeys-enable';
const KEY_BOUNCE_KEYS_ENABLED       = 'bouncekeys-enable';
const KEY_SLOW_KEYS_ENABLED         = 'slowkeys-enable';
const KEY_MOUSE_KEYS_ENABLED        = 'mousekeys-enable';

const APPLICATIONS_SCHEMA           = 'org.gnome.desktop.a11y.applications';

var DPI_FACTOR_LARGE              = 1.25;

const WM_SCHEMA                     = 'org.gnome.desktop.wm.preferences';
const KEY_VISUAL_BELL               = 'visual-bell';

const DESKTOP_INTERFACE_SCHEMA      = 'org.gnome.desktop.interface';
const KEY_GTK_THEME                 = 'gtk-theme';
const KEY_ICON_THEME                = 'icon-theme';
const KEY_TEXT_SCALING_FACTOR       = 'text-scaling-factor';

const HIGH_CONTRAST_THEME           = 'HighContrast';

var ATIndicator = GObject.registerClass(
class ATIndicator extends PanelMenu.Button {
    _init() {
        super._init(0.5, _("Accessibility"));

        this._hbox = new St.BoxLayout({ style_class: 'panel-status-menu-box' });
        this._hbox.add_child(new St.Icon({ style_class: 'system-status-icon',
                                           icon_name: 'preferences-desktop-accessibility-symbolic' }));
        this._hbox.add_child(PopupMenu.arrowIcon(St.Side.BOTTOM));

        this.add_child(this._hbox);

        this._a11ySettings = new Gio.Settings({ schema_id: A11Y_SCHEMA });
        this._a11ySettings.connect('changed::%s'.format(KEY_ALWAYS_SHOW), this._queueSyncMenuVisibility.bind(this));

        let highContrast = this._buildHCItem();
        this.menu.addMenuItem(highContrast);

        let magnifier = this._buildItem(_("Zoom"), APPLICATIONS_SCHEMA,
                                        'screen-magnifier-enabled');
        this.menu.addMenuItem(magnifier);

        let textZoom = this._buildFontItem();
        this.menu.addMenuItem(textZoom);

        let screenReader = this._buildItem(_("Screen Reader"), APPLICATIONS_SCHEMA,
                                           'screen-reader-enabled');
        this.menu.addMenuItem(screenReader);

        let screenKeyboard = this._buildItem(_("Screen Keyboard"), APPLICATIONS_SCHEMA,
                                             'screen-keyboard-enabled');
        this.menu.addMenuItem(screenKeyboard);

        let visualBell = this._buildItem(_("Visual Alerts"), WM_SCHEMA, KEY_VISUAL_BELL);
        this.menu.addMenuItem(visualBell);

        let stickyKeys = this._buildItem(_("Sticky Keys"), A11Y_KEYBOARD_SCHEMA, KEY_STICKY_KEYS_ENABLED);
        this.menu.addMenuItem(stickyKeys);

        let slowKeys = this._buildItem(_("Slow Keys"), A11Y_KEYBOARD_SCHEMA, KEY_SLOW_KEYS_ENABLED);
        this.menu.addMenuItem(slowKeys);

        let bounceKeys = this._buildItem(_("Bounce Keys"), A11Y_KEYBOARD_SCHEMA, KEY_BOUNCE_KEYS_ENABLED);
        this.menu.addMenuItem(bounceKeys);

        let mouseKeys = this._buildItem(_("Mouse Keys"), A11Y_KEYBOARD_SCHEMA, KEY_MOUSE_KEYS_ENABLED);
        this.menu.addMenuItem(mouseKeys);

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

    _buildItemExtended(string, initialValue, writable, onSet) {
        let widget = new PopupMenu.PopupSwitchMenuItem(string, initialValue);
        if (!writable) {
            widget.reactive = false;
        } else {
            widget.connect('toggled', item => {
                onSet(item.state);
            });
        }
        return widget;
    }

    _buildItem(string, schema, key) {
        let settings = new Gio.Settings({ schema_id: schema });
        let widget = this._buildItemExtended(string,
            settings.get_boolean(key),
            settings.is_writable(key),
            enabled => settings.set_boolean(key, enabled));

        settings.connect('changed::%s'.format(key), () => {
            widget.setToggleState(settings.get_boolean(key));

            this._queueSyncMenuVisibility();
        });

        return widget;
    }

    _buildHCItem() {
        let interfaceSettings = new Gio.Settings({ schema_id: DESKTOP_INTERFACE_SCHEMA });
        let gtkTheme = interfaceSettings.get_string(KEY_GTK_THEME);
        let iconTheme = interfaceSettings.get_string(KEY_ICON_THEME);
        let hasHC = gtkTheme == HIGH_CONTRAST_THEME;
        let highContrast = this._buildItemExtended(
            _("High Contrast"),
            hasHC,
            interfaceSettings.is_writable(KEY_GTK_THEME) &&
            interfaceSettings.is_writable(KEY_ICON_THEME),
            enabled => {
                if (enabled) {
                    interfaceSettings.set_string(KEY_GTK_THEME, HIGH_CONTRAST_THEME);
                    interfaceSettings.set_string(KEY_ICON_THEME, HIGH_CONTRAST_THEME);
                } else if (!hasHC) {
                    interfaceSettings.set_string(KEY_GTK_THEME, gtkTheme);
                    interfaceSettings.set_string(KEY_ICON_THEME, iconTheme);
                } else {
                    interfaceSettings.reset(KEY_GTK_THEME);
                    interfaceSettings.reset(KEY_ICON_THEME);
                }
            });

        interfaceSettings.connect('changed::%s'.format(KEY_GTK_THEME), () => {
            let value = interfaceSettings.get_string(KEY_GTK_THEME);
            if (value == HIGH_CONTRAST_THEME) {
                highContrast.setToggleState(true);
            } else {
                highContrast.setToggleState(false);
                gtkTheme = value;
            }

            this._queueSyncMenuVisibility();
        });

        interfaceSettings.connect('changed::%s'.format(KEY_ICON_THEME), () => {
            let value = interfaceSettings.get_string(KEY_ICON_THEME);
            if (value != HIGH_CONTRAST_THEME)
                iconTheme = value;
        });

        return highContrast;
    }

    _buildFontItem() {
        let settings = new Gio.Settings({ schema_id: DESKTOP_INTERFACE_SCHEMA });
        let factor = settings.get_double(KEY_TEXT_SCALING_FACTOR);
        let initialSetting = factor > 1.0;
        let widget = this._buildItemExtended(_("Large Text"),
            initialSetting,
            settings.is_writable(KEY_TEXT_SCALING_FACTOR),
            enabled => {
                if (enabled) {
                    settings.set_double(
                        KEY_TEXT_SCALING_FACTOR, DPI_FACTOR_LARGE);
                } else {
                    settings.reset(KEY_TEXT_SCALING_FACTOR);
                }
            });

        settings.connect('changed::%s'.format(KEY_TEXT_SCALING_FACTOR), () => {
            factor = settings.get_double(KEY_TEXT_SCALING_FACTOR);
            let active = factor > 1.0;
            widget.setToggleState(active);

            this._queueSyncMenuVisibility();
        });

        return widget;
    }
});
