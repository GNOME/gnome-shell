// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Mainloop = imports.mainloop;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const A11Y_SCHEMA = 'org.gnome.desktop.a11y.keyboard';
const KEY_STICKY_KEYS_ENABLED = 'stickykeys-enable';
const KEY_BOUNCE_KEYS_ENABLED = 'bouncekeys-enable';
const KEY_SLOW_KEYS_ENABLED   = 'slowkeys-enable';
const KEY_MOUSE_KEYS_ENABLED  = 'mousekeys-enable';

const APPLICATIONS_SCHEMA = 'org.gnome.desktop.a11y.applications';

const DPI_FACTOR_LARGE   = 1.25;

const WM_SCHEMA            = 'org.gnome.desktop.wm.preferences';
const KEY_VISUAL_BELL      = 'visual-bell';

const DESKTOP_INTERFACE_SCHEMA = 'org.gnome.desktop.interface';
const KEY_GTK_THEME      = 'gtk-theme';
const KEY_ICON_THEME     = 'icon-theme';
const KEY_WM_THEME       = 'theme';
const KEY_TEXT_SCALING_FACTOR = 'text-scaling-factor';

const HIGH_CONTRAST_THEME = 'HighContrast';

const ATIndicator = new Lang.Class({
    Name: 'ATIndicator',
    Extends: PanelMenu.SystemStatusButton,

    _init: function() {
        this.parent('preferences-desktop-accessibility-symbolic', _("Accessibility"));

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

        let stickyKeys = this._buildItem(_("Sticky Keys"), A11Y_SCHEMA, KEY_STICKY_KEYS_ENABLED);
        this.menu.addMenuItem(stickyKeys);

        let slowKeys = this._buildItem(_("Slow Keys"), A11Y_SCHEMA, KEY_SLOW_KEYS_ENABLED);
        this.menu.addMenuItem(slowKeys);

        let bounceKeys = this._buildItem(_("Bounce Keys"), A11Y_SCHEMA, KEY_BOUNCE_KEYS_ENABLED);
        this.menu.addMenuItem(bounceKeys);

        let mouseKeys = this._buildItem(_("Mouse Keys"), A11Y_SCHEMA, KEY_MOUSE_KEYS_ENABLED);
        this.menu.addMenuItem(mouseKeys);

        this._syncMenuVisibility();
    },

    _syncMenuVisibility: function() {
        this._syncMenuVisibilityIdle = 0;

        let items = this.menu._getMenuItems();

        this.actor.visible = items.some(function(f) { return !!f.state; });

        return false;
    },

    _queueSyncMenuVisibility: function() {
        if (this._syncMenuVisibilityIdle)
            return;

        this._syncMenuVisbilityIdle = Mainloop.idle_add(Lang.bind(this, this._syncMenuVisibility));
    },

    _buildItemExtended: function(string, initial_value, writable, on_set) {
        let widget = new PopupMenu.PopupSwitchMenuItem(string, initial_value);
        if (!writable)
            widget.actor.reactive = false;
        else
            widget.connect('toggled', function(item) {
                on_set(item.state);
            });
        return widget;
    },

    _buildItem: function(string, schema, key) {
        let settings = new Gio.Settings({ schema: schema });
        let widget = this._buildItemExtended(string,
            settings.get_boolean(key),
            settings.is_writable(key),
            function(enabled) {
                return settings.set_boolean(key, enabled);
            });
        settings.connect('changed::'+key, Lang.bind(this, function() {
            widget.setToggleState(settings.get_boolean(key));

            this._queueSyncMenuVisibility();
        }));
        return widget;
    },

    _buildHCItem: function() {
        let interfaceSettings = new Gio.Settings({ schema: DESKTOP_INTERFACE_SCHEMA });
        let wmSettings = new Gio.Settings({ schema: WM_SCHEMA });
        let gtkTheme = interfaceSettings.get_string(KEY_GTK_THEME);
        let iconTheme = interfaceSettings.get_string(KEY_ICON_THEME);
        let wmTheme = wmSettings.get_string(KEY_WM_THEME);
        let hasHC = (gtkTheme == HIGH_CONTRAST_THEME);
        let highContrast = this._buildItemExtended(
            _("High Contrast"),
            hasHC,
            interfaceSettings.is_writable(KEY_GTK_THEME) &&
            interfaceSettings.is_writable(KEY_ICON_THEME) &&
            wmSettings.is_writable(KEY_WM_THEME),
            function (enabled) {
                if (enabled) {
                    interfaceSettings.set_string(KEY_GTK_THEME, HIGH_CONTRAST_THEME);
                    interfaceSettings.set_string(KEY_ICON_THEME, HIGH_CONTRAST_THEME);
                    wmSettings.set_string(KEY_WM_THEME, HIGH_CONTRAST_THEME);
                } else if(!hasHC) {
                    interfaceSettings.set_string(KEY_GTK_THEME, gtkTheme);
                    interfaceSettings.set_string(KEY_ICON_THEME, iconTheme);
                    wmSettings.set_string(KEY_WM_THEME, wmTheme);
                } else {
                    interfaceSettings.reset(KEY_GTK_THEME);
                    interfaceSettings.reset(KEY_ICON_THEME);
                    wmSettings.reset(KEY_WM_THEME);
                }
            });
        interfaceSettings.connect('changed::' + KEY_GTK_THEME, Lang.bind(this, function() {
            let value = interfaceSettings.get_string(KEY_GTK_THEME);
            if (value == HIGH_CONTRAST_THEME) {
                highContrast.setToggleState(true);
            } else {
                highContrast.setToggleState(false);
                gtkTheme = value;
            }

            this._queueSyncMenuVisibility();
        }));
        interfaceSettings.connect('changed::' + KEY_ICON_THEME, function() {
            let value = interfaceSettings.get_string(KEY_ICON_THEME);
            if (value != HIGH_CONTRAST_THEME)
                iconTheme = value;
        });
        wmSettings.connect('changed::' + KEY_WM_THEME, function() {
            let value = wmSettings.get_string(KEY_WM_THEME);
            if (value != HIGH_CONTRAST_THEME)
                wmTheme = value;
        });
        return highContrast;
    },

    _buildFontItem: function() {
        let settings = new Gio.Settings({ schema: DESKTOP_INTERFACE_SCHEMA });

        let factor = settings.get_double(KEY_TEXT_SCALING_FACTOR);
        let initial_setting = (factor > 1.0);
        let widget = this._buildItemExtended(_("Large Text"),
            initial_setting,
            settings.is_writable(KEY_TEXT_SCALING_FACTOR),
            function (enabled) {
                if (enabled)
                    settings.set_double(KEY_TEXT_SCALING_FACTOR,
                                        DPI_FACTOR_LARGE);
                else
                    settings.reset(KEY_TEXT_SCALING_FACTOR);
            });
        settings.connect('changed::' + KEY_TEXT_SCALING_FACTOR, Lang.bind(this, function() {
            let factor = settings.get_double(KEY_TEXT_SCALING_FACTOR);
            let active = (factor > 1.0);
            widget.setToggleState(active);

            this._queueSyncMenuVisibility();
        }));
        return widget;
    }
});

const ATGreeterIndicator = new Lang.Class({
    Name: 'ATGreeterIndicator',
    Extends: ATIndicator,

    // Override visibility handling to be always visible
    _syncMenuVisibility: function() { },
    _queueSyncMenuVisibility: function() { }
});
