/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const DBus = imports.dbus;
const GConf = imports.gi.GConf;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const KEY_A11Y_DIR            = "/desktop/gnome/accessibility";
const KEY_STICKY_KEYS_ENABLED = KEY_A11Y_DIR + "/keyboard/stickykeys_enable";
const KEY_BOUNCE_KEYS_ENABLED = KEY_A11Y_DIR + "/keyboard/bouncekeys_enable";
const KEY_SLOW_KEYS_ENABLED   = KEY_A11Y_DIR + "/keyboard/slowkeys_enable";
const KEY_MOUSE_KEYS_ENABLED  = KEY_A11Y_DIR + "/keyboard/mousekeys_enable";

const AT_SCREEN_KEYBOARD_SCHEMA = "org.gnome.desktop.default-applications.at.mobility";
const AT_SCREEN_READER_SCHEMA   = "org.gnome.desktop.default-applications.at.visual";

const KEY_FONT_DPI    = "/desktop/gnome/font_rendering/dpi";
const DPI_LOW_REASONABLE_VALUE  = 50;
const DPI_HIGH_REASONABLE_VALUE = 500;

const DPI_FACTOR_LARGE   = 1.25;
const DPI_FACTOR_LARGER  = 1.5;
const DPI_FACTOR_LARGEST = 2.0;
const DPI_DEFAULT        = 96;

const KEY_META_DIR       = "/apps/metacity/general";
const KEY_VISUAL_BELL = KEY_META_DIR + "/visual_bell";

const DESKTOP_INTERFACE_SCHEMA = "org.gnome.desktop.interface";
const KEY_GTK_THEME      = "gtk-theme";
const KEY_ICON_THEME     = "icon-theme";

const HIGH_CONTRAST_THEME = "HighContrast";

function getDPIFromX() {
    let screen = global.get_gdk_screen();
    if (screen) {
        let width_dpi = (screen.get_width() / (screen.get_width_mm() / 25.4));
        let height_dpi = (screen.get_height() / (screen.get_height_mm() / 25.4));
        if (width_dpi < DPI_LOW_REASONABLE_VALUE
            || width_dpi > DPI_HIGH_REASONABLE_VALUE
            || height_dpi < DPI_LOW_REASONABLE_VALUE
            || height_dpi > DPI_HIGH_REASONABLE_VALUE)
            return DPI_DEFAULT;
        else
            return (width_dpi + height_dpi) / 2;
    }
    return DPI_DEFAULT;
}

function ATIndicator() {
    this._init.apply(this, arguments);
}

ATIndicator.prototype = {
    __proto__: PanelMenu.SystemStatusButton.prototype,

    _init: function() {
        PanelMenu.SystemStatusButton.prototype._init.call(this, 'preferences-desktop-accessibility', null);

        let client = GConf.Client.get_default();
        client.add_dir(KEY_A11Y_DIR, GConf.ClientPreloadType.PRELOAD_ONELEVEL, null);
        client.notify_add(KEY_A11Y_DIR, Lang.bind(this, this._keyChanged), null, null);
        client.add_dir(KEY_META_DIR, GConf.ClientPreloadType.PRELOAD_ONELEVEL, null);
        client.notify_add(KEY_META_DIR, Lang.bind(this, this._keyChanged), null, null);

        let highContrast = this._buildHCItem();
        this.menu.addMenuItem(highContrast);

        let magnifier = this._buildMagItem();
        this.menu.addMenuItem(magnifier);

        let textZoom = this._buildFontItem(client);
        this.menu.addMenuItem(textZoom);

        let screenReader = this._buildItem(_("Screen reader"), AT_SCREEN_READER_SCHEMA, 'startup');
        this.menu.addMenuItem(screenReader);

        let screenKeyboard = this._buildItem(_("Screen keyboard"), AT_SCREEN_KEYBOARD_SCHEMA, 'startup');
        this.menu.addMenuItem(screenKeyboard);

        let visualBell = this._buildItemGConf(_("Visual alerts"), client, KEY_VISUAL_BELL);
        this.menu.addMenuItem(visualBell);

        let stickyKeys = this._buildItemGConf(_("Sticky keys"), client, KEY_STICKY_KEYS_ENABLED);
        this.menu.addMenuItem(stickyKeys);

        let slowKeys = this._buildItemGConf(_("Slow keys"), client, KEY_SLOW_KEYS_ENABLED);
        this.menu.addMenuItem(slowKeys);

        let bounceKeys = this._buildItemGConf(_("Bounce keys"), client, KEY_BOUNCE_KEYS_ENABLED);
        this.menu.addMenuItem(bounceKeys);

        let mouseKeys = this._buildItemGConf(_("Mouse keys"), client, KEY_MOUSE_KEYS_ENABLED);
        this.menu.addMenuItem(mouseKeys);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.menu.addAction(_("Universal access settings"), function() {
            let p = new Shell.Process({ args: ['gnome-control-center','universal-access'] });
            p.run();
        });
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

    _buildItemGConf: function(string, client, key) {
        function on_get() {
            return client.get_bool(key);
        }
        let widget = this._buildItemExtended(string,
            client.get_bool(key),
            client.key_is_writable(key),
            function(enabled) {
                client.set_bool(key, enabled);
            });
        this.connect('gconf-changed', function() {
            widget.setToggleState(client.get_bool(key));
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
        settings.connect('changed::'+key, function() {
            widget.setToggleState(settings.get_boolean(key));
        });
        return widget;
    },

    _buildHCItem: function() {
        let settings = new Gio.Settings({ schema: DESKTOP_INTERFACE_SCHEMA });
        let gtkTheme = settings.get_string(KEY_GTK_THEME);
        let iconTheme = settings.get_string(KEY_ICON_THEME);
        let hasHC = (gtkTheme == HIGH_CONTRAST_THEME);
        let highContrast = this._buildItemExtended(
            _("High Contrast"),
            hasHC,
            settings.is_writable(KEY_GTK_THEME) && settings.is_writable(KEY_ICON_THEME),
            function (enabled) {
                if (enabled) {
                    settings.set_string(KEY_GTK_THEME, HIGH_CONTRAST_THEME);
                    settings.set_string(KEY_ICON_THEME, HIGH_CONTRAST_THEME);
                } else {
                    settings.set_string(KEY_GTK_THEME, gtkTheme);
                    settings.set_string(KEY_ICON_THEME, iconTheme);
                }
            });
        settings.connect('changed::' + KEY_GTK_THEME, function() {
            let value = settings.get_string(KEY_GTK_THEME);
            if (value == HIGH_CONTRAST_THEME) {
                highContrast.setToggleState(true);
            } else {
                highContrast.setToggleState(false);
                gtkTheme = value;
            }
        });
        settings.connect('changed::' + KEY_ICON_THEME, function() {
            let value = settings.get_string(KEY_ICON_THEME);
            if (value != HIGH_CONTRAST_THEME)
                iconTheme = value;
        });
        return highContrast;
    },

    _buildFontItem: function(client) {
        let first_gconf_value = client.get_without_default(KEY_FONT_DPI);
        let default_value = getDPIFromX();
        let first_value = first_gconf_value ? first_gconf_value.get_float() : default_value;
        log('default_value = %f, first_value = %f'.format(default_value,first_value));
        function on_get() {
            let u_dpi = client.get_float(KEY_FONT_DPI);
            let x_dpi = getDPIFromX();
            return (u_dpi - (DPI_FACTOR_LARGE * x_dpi) > -1);
        }
        let initial_setting = on_get();
        let widget = this._buildItemExtended(_("Large text"),
            initial_setting,
            client.key_is_writable(KEY_FONT_DPI),
            function (enabled) {
                if (enabled)
                    client.set_float(KEY_FONT_DPI, DPI_FACTOR_LARGE * getDPIFromX());
                else
                    client.set_float(KEY_FONT_DPI, (first_value && !initial_setting) ? first_value : default_value);
            });
        this.connect('gconf-changed', function() {
            let active = on_get();
            if (!active)
                // setting was modified manually, update it
                first_value = client.get_value(KEY_FONT_DPI);
            widget.setToggleState(on_get());
        });
        return widget;
    },

    _buildMagItem: function() {
        let mag = Main.magnifier;
        let widget = this._buildItemExtended(_("Zoom screen"),
            mag.isActive(),
            true,
            Lang.bind(mag, mag.setActive));
        mag.connect('active-changed', function(magnifier, active) {
            widget.setToggleState(active);
        });
        return widget;
    },

    _keyChanged: function() {
        this.emit('gconf-changed');
    }
};
Signals.addSignalMethods(ATIndicator.prototype);
