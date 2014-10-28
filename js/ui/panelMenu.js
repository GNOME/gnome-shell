// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const Atk = imports.gi.Atk;

const Main = imports.ui.main;
const Params = imports.misc.params;
const PopupMenu = imports.ui.popupMenu;

const Button = new Lang.Class({
    Name: 'PanelMenuButton',

    _init: function(menuAlignment, nameText, dontCreateMenu) {
        this.actor = new St.Bin({ style_class: 'panel-button',
                                  reactive: true,
                                  can_focus: true,
                                  track_hover: true,
                                  accessible_name: nameText ? nameText : "",
                                  accessible_role: Atk.Role.MENU });

        this.actor.connect('event', Lang.bind(this, this._onEvent));
        this.actor.connect('notify::visible', Lang.bind(this, this._onVisibilityChanged));

        if (dontCreateMenu)
            this.menu = new PopupMenu.PopupDummyMenu(this.actor);
        else
            this.setMenu(new PopupMenu.PopupMenu(this.actor, menuAlignment, St.Side.TOP, 0));
    },

    setSensitive: function(sensitive) {
        this.actor.reactive = sensitive;
        this.actor.can_focus = sensitive;
        this.actor.track_hover = sensitive;
    },

    setMenu: function(menu) {
        if (this.menu)
            this.menu.destroy();

        this.menu = menu;
        if (this.menu) {
            this.menu.actor.add_style_class_name('panel-menu');
            this.menu.connect('open-state-changed', Lang.bind(this, this._onOpenStateChanged));
            this.menu.actor.connect('key-press-event', Lang.bind(this, this._onMenuKeyPress));

            Main.uiGroup.add_actor(this.menu.actor);
            this.menu.actor.hide();
        }
    },

    _onEvent: function(actor, event) {
        if (this.menu &&
            (event.type() == Clutter.EventType.TOUCH_BEGIN ||
             event.type() == Clutter.EventType.BUTTON_PRESS))
            this.menu.toggle();

        return Clutter.EVENT_PROPAGATE;
    },

    _onVisibilityChanged: function() {
        if (!this.menu)
            return;

        if (!this.actor.visible)
            this.menu.close();
    },

    _onMenuKeyPress: function(actor, event) {
        if (global.focus_manager.navigate_from_event(event))
            return Clutter.EVENT_STOP;

        let symbol = event.get_key_symbol();
        if (symbol == Clutter.KEY_Left || symbol == Clutter.KEY_Right) {
            let group = global.focus_manager.get_group(this.actor);
            if (group) {
                let direction = (symbol == Clutter.KEY_Left) ? Gtk.DirectionType.LEFT : Gtk.DirectionType.RIGHT;
                group.navigate_focus(this.actor, direction, false);
                return Clutter.EVENT_STOP;
            }
        }
        return Clutter.EVENT_PROPAGATE;
    },

    _onOpenStateChanged: function(menu, open) {
        if (open)
            this.actor.add_style_pseudo_class('active');
        else
            this.actor.remove_style_pseudo_class('active');

        // Setting the max-height won't do any good if the minimum height of the
        // menu is higher then the screen; it's useful if part of the menu is
        // scrollable so the minimum height is smaller than the natural height
        let workArea = Main.layoutManager.getWorkAreaForMonitor(Main.layoutManager.primaryIndex);
        this.menu.actor.style = ('max-height: ' + Math.round(workArea.height) + 'px;');
    },

    destroy: function() {
        if (this.menu)
            this.menu.destroy();
        this.actor.destroy();

        this.emit('destroy');
    }
});
Signals.addSignalMethods(Button.prototype);

/* SystemIndicator:
 *
 * This class manages one system indicator, which are the icons
 * that you see at the top right. A system indicator is composed
 * of an icon and a menu section, which will be composed into the
 * aggregate menu.
 */
const SystemIndicator = new Lang.Class({
    Name: 'SystemIndicator',

    _init: function() {
        this.indicators = new St.BoxLayout({ style_class: 'panel-status-indicators-box',
                                             reactive: true });
        this.indicators.hide();
        this.menu = new PopupMenu.PopupMenuSection();
    },

    _syncIndicatorsVisible: function() {
        this.indicators.visible = this.indicators.get_children().some(function(actor) {
            return actor.visible;
        });
    },

    _addIndicator: function() {
        let icon = new St.Icon({ style_class: 'system-status-icon' });
        this.indicators.add_actor(icon);
        icon.connect('notify::visible', Lang.bind(this, this._syncIndicatorsVisible));
        this._syncIndicatorsVisible();
        return icon;
    }
});
Signals.addSignalMethods(SystemIndicator.prototype);
