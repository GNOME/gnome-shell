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

var ButtonBox = new Lang.Class({
    Name: 'ButtonBox',
    Extends: St.Widget,

    _init(params) {
        params = Params.parse(params, { style_class: 'panel-button' }, true);

        this.parent(params);

        this.actor = this;
        this._delegate = this;

        this.container = new St.Bin({ y_fill: true,
                                      x_fill: true,
                                      child: this.actor });

        this.connect('style-changed', this._onStyleChanged.bind(this));
        this.connect('destroy', this._onDestroy.bind(this));

        this._minHPadding = this._natHPadding = 0.0;
    },

    _onStyleChanged(actor) {
        let themeNode = actor.get_theme_node();

        this._minHPadding = themeNode.get_length('-minimum-hpadding');
        this._natHPadding = themeNode.get_length('-natural-hpadding');
    },

    vfunc_get_preferred_width(forHeight) {
        let child = this.get_first_child();
        let minimumSize, naturalSize;

        if (child)
            [minimumSize, naturalSize] = child.get_preferred_width(-1);
        else
            minimumSize = naturalSize = 0;

        minimumSize += 2 * this._minHPadding;
        naturalSize += 2 * this._natHPadding;

        return [minimumSize, naturalSize];
    },

    vfunc_get_preferred_height(forWidth) {
        let child = this.get_first_child();

        if (child)
            return child.get_preferred_height(-1);

        return [0, 0];
    },

    vfunc_allocate(box, flags) {
        this.set_allocation(box, flags);

        let child = this.get_first_child();
        if (!child)
            return;

        let [minWidth, natWidth] = child.get_preferred_width(-1);

        let availWidth = box.x2 - box.x1;
        let availHeight = box.y2 - box.y1;

        let childBox = new Clutter.ActorBox();
        if (natWidth + 2 * this._natHPadding <= availWidth) {
            childBox.x1 = this._natHPadding;
            childBox.x2 = availWidth - this._natHPadding;
        } else {
            childBox.x1 = this._minHPadding;
            childBox.x2 = availWidth - this._minHPadding;
        }

        childBox.y1 = 0;
        childBox.y2 = availHeight;

        child.allocate(childBox, flags);
    },

    _onDestroy() {
        this.container.child = null;
        this.container.destroy();
    },
});

var Button = new Lang.Class({
    Name: 'PanelMenuButton',
    Extends: ButtonBox,
    Signals: {'menu-set': {} },

    _init(menuAlignment, nameText, dontCreateMenu) {
        this.parent({ reactive: true,
                      can_focus: true,
                      track_hover: true,
                      accessible_name: nameText ? nameText : "",
                      accessible_role: Atk.Role.MENU });

        this.connect('event', this._onEvent.bind(this));
        this.connect('notify::visible', this._onVisibilityChanged.bind(this));

        if (dontCreateMenu)
            this.menu = new PopupMenu.PopupDummyMenu(this.actor);
        else
            this.setMenu(new PopupMenu.PopupMenu(this.actor, menuAlignment, St.Side.TOP, 0));
    },

    setSensitive(sensitive) {
        this.reactive = sensitive;
        this.can_focus = sensitive;
        this.track_hover = sensitive;
    },

    setMenu(menu) {
        if (this.menu)
            this.menu.destroy();

        this.menu = menu;
        if (this.menu) {
            this.menu.actor.add_style_class_name('panel-menu');
            this.menu.connect('open-state-changed', this._onOpenStateChanged.bind(this));
            this.menu.actor.connect('key-press-event', this._onMenuKeyPress.bind(this));

            Main.uiGroup.add_actor(this.menu.actor);
            this.menu.actor.hide();
        }
        this.emit('menu-set');
    },

    _onEvent(actor, event) {
        if (this.menu &&
            (event.type() == Clutter.EventType.TOUCH_BEGIN ||
             event.type() == Clutter.EventType.BUTTON_PRESS))
            this.menu.toggle();

        return Clutter.EVENT_PROPAGATE;
    },

    _onVisibilityChanged() {
        if (!this.menu)
            return;

        if (!this.actor.visible)
            this.menu.close();
    },

    _onMenuKeyPress(actor, event) {
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

    _onOpenStateChanged(menu, open) {
        if (open)
            this.actor.add_style_pseudo_class('active');
        else
            this.actor.remove_style_pseudo_class('active');

        // Setting the max-height won't do any good if the minimum height of the
        // menu is higher then the screen; it's useful if part of the menu is
        // scrollable so the minimum height is smaller than the natural height
        let workArea = Main.layoutManager.getWorkAreaForMonitor(Main.layoutManager.primaryIndex);
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        let verticalMargins = this.menu.actor.margin_top + this.menu.actor.margin_bottom;

        // The workarea and margin dimensions are in physical pixels, but CSS
        // measures are in logical pixels, so make sure to consider the scale
        // factor when computing max-height
        let maxHeight = Math.round((workArea.height - verticalMargins) / scaleFactor);
        this.menu.actor.style = ('max-height: %spx;').format(maxHeight);
    },

    _onDestroy() {
        this.parent();

        if (this.menu)
            this.menu.destroy();
    }
});

/* SystemIndicator:
 *
 * This class manages one system indicator, which are the icons
 * that you see at the top right. A system indicator is composed
 * of an icon and a menu section, which will be composed into the
 * aggregate menu.
 */
var SystemIndicator = new Lang.Class({
    Name: 'SystemIndicator',

    _init() {
        this.indicators = new St.BoxLayout({ style_class: 'panel-status-indicators-box',
                                             reactive: true });
        this.indicators.hide();
        this.menu = new PopupMenu.PopupMenuSection();
    },

    _syncIndicatorsVisible() {
        this.indicators.visible = this.indicators.get_children().some(a => a.visible);
    },

    _addIndicator() {
        let icon = new St.Icon({ style_class: 'system-status-icon' });
        this.indicators.add_actor(icon);
        icon.connect('notify::visible', this._syncIndicatorsVisible.bind(this));
        this._syncIndicatorsVisible();
        return icon;
    }
});
Signals.addSignalMethods(SystemIndicator.prototype);
