// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

/* exported HotCorner */

const { Clutter, Gdk, Gio, GObject } = imports.gi;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;

var HOT_CORNER_ENABLED_KEY = 'hot-corner-enabled';

var HotCorner = GObject.registerClass(
class HotCorner extends PanelMenu.SingleIconButton {
    _init() {
        super._init(_('Hot Corner'), Clutter.ActorAlign.END, Clutter.ActorAlign.END);
        this.add_style_class_name('hot-corner');

        let iconFile;
        if (this.get_text_direction() === Clutter.TextDirection.RTL)
            iconFile = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/hot-corner-rtl-symbolic.svg');
        else
            iconFile = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/hot-corner-symbolic.svg');
        this.setIcon(new Gio.FileIcon({ file: iconFile }));

        this._enableMenuItem = this.menu.addAction(_('Enable Hot Corner'), () => {
            global.settings.set_boolean(HOT_CORNER_ENABLED_KEY, true);
        });

        this._disableMenuItem = this.menu.addAction(_('Disable Hot Corner'), () => {
            global.settings.set_boolean(HOT_CORNER_ENABLED_KEY, false);
        });

        if (global.settings.get_boolean(HOT_CORNER_ENABLED_KEY))
            this._enableMenuItem.actor.visible = false;
        else
            this._disableMenuItem.actor.visible = false;

        this.menu.connect('menu-closed', () => {
            let isEnabled = global.settings.get_boolean(HOT_CORNER_ENABLED_KEY);
            this._enableMenuItem.actor.visible = !isEnabled;
            this._disableMenuItem.actor.visible = isEnabled;
        });
    }

    // overrides default implementation from PanelMenu.Button
    vfunc_event(event) {
        if (this.menu &&
            (event.type() === Clutter.EventType.TOUCH_BEGIN ||
             event.type() === Clutter.EventType.BUTTON_PRESS)) {
            let button = event.get_button();
            if (button === Gdk.BUTTON_PRIMARY && Main.overview.shouldToggleByCornerOrButton())
                Main.overview.toggleWindows();
            else if (button === Gdk.BUTTON_SECONDARY)
                this.menu.toggle();
        }

        return Clutter.EVENT_PROPAGATE;
    }
});
