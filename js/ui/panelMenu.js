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

const ButtonBox = new Lang.Class({
    Name: 'ButtonBox',

    _init: function(params) {
        params = Params.parse(params, { style_class: 'panel-button' }, true);
        this.actor = new Shell.GenericContainer(params);
        this.actor._delegate = this;

        this.container = new St.Bin({ y_fill: true,
                                      x_fill: true,
                                      child: this.actor });

        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));

        this.actor.connect('style-changed', Lang.bind(this, this._onStyleChanged));
        this._minHPadding = this._natHPadding = 0.0;
    },

    _onStyleChanged: function(actor) {
        let themeNode = actor.get_theme_node();

        this._minHPadding = themeNode.get_length('-minimum-hpadding');
        this._natHPadding = themeNode.get_length('-natural-hpadding');
    },

    _getPreferredWidth: function(actor, forHeight, alloc) {
        let children = actor.get_children();
        let child = children.length > 0 ? children[0] : null;

        if (child) {
            [alloc.min_size, alloc.natural_size] = child.get_preferred_width(-1);
        } else {
            alloc.min_size = alloc.natural_size = 0;
        }

        alloc.min_size += 2 * this._minHPadding;
        alloc.natural_size += 2 * this._natHPadding;
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let children = actor.get_children();
        let child = children.length > 0 ? children[0] : null;

        if (child) {
            [alloc.min_size, alloc.natural_size] = child.get_preferred_height(-1);
        } else {
            alloc.min_size = alloc.natural_size = 0;
        }
    },

    _allocate: function(actor, box, flags) {
        let children = actor.get_children();
        if (children.length == 0)
            return;

        let child = children[0];
        let [minWidth, natWidth] = child.get_preferred_width(-1);
        let [minHeight, natHeight] = child.get_preferred_height(-1);

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

        if (natHeight <= availHeight) {
            childBox.y1 = Math.floor((availHeight - natHeight) / 2);
            childBox.y2 = childBox.y1 + natHeight;
        } else {
            childBox.y1 = 0;
            childBox.y2 = availHeight;
        }

        child.allocate(childBox, flags);
    },
});

const Button = new Lang.Class({
    Name: 'PanelMenuButton',
    Extends: ButtonBox,

    _init: function(menuAlignment, nameText, dontCreateMenu) {
        this.parent({ reactive: true,
                      can_focus: true,
                      track_hover: true,
                      accessible_role: Atk.Role.MENU });

        this.actor.connect('button-press-event', Lang.bind(this, this._onButtonPress));
        this.actor.connect('key-press-event', Lang.bind(this, this._onSourceKeyPress));
        this.actor.connect('notify::visible', Lang.bind(this, this._onVisibilityChanged));

        if (dontCreateMenu)
            this.menu = new PopupMenu.PopupDummyMenu(this.actor);
        else
            this.setMenu(new PopupMenu.PopupMenu(this.actor, menuAlignment, St.Side.TOP, 0));

        this.setName(nameText);
    },

    setSensitive: function(sensitive) {
        this.actor.reactive = sensitive;
        this.actor.can_focus = sensitive;
        this.actor.track_hover = sensitive;
    },

    setName: function(text) {
        if (text != null) {
            // This is the easiest way to provide a accessible name to
            // this widget. The label could be also used for other
            // purposes in the future.
            if (!this.label) {
                this.label = new St.Label({ text: text });
                this.actor.label_actor = this.label;
            } else
                this.label.text = text;
        } else {
            this.label = null;
            this.actor.label_actor = null;
        }
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

    _onButtonPress: function(actor, event) {
        if (!this.menu)
            return;

        this.menu.toggle();
    },

    _onSourceKeyPress: function(actor, event) {
        if (!this.menu)
            return false;

        let symbol = event.get_key_symbol();
        if (symbol == Clutter.KEY_space || symbol == Clutter.KEY_Return) {
            this.menu.toggle();
            return true;
        } else if (symbol == Clutter.KEY_Escape && this.menu.isOpen) {
            this.menu.close();
            return true;
        } else if (symbol == Clutter.KEY_Down) {
            if (!this.menu.isOpen)
                this.menu.toggle();
            this.menu.actor.navigate_focus(this.actor, Gtk.DirectionType.DOWN, false);
            return true;
        } else
            return false;
    },

    _onVisibilityChanged: function() {
        if (!this.menu)
            return;

        if (!this.actor.visible)
            this.menu.close();
    },

    _onMenuKeyPress: function(actor, event) {
        if (global.focus_manager.navigate_from_event(event))
            return true;

        let symbol = event.get_key_symbol();
        if (symbol == Clutter.KEY_Left || symbol == Clutter.KEY_Right) {
            let group = global.focus_manager.get_group(this.actor);
            if (group) {
                let direction = (symbol == Clutter.KEY_Left) ? Gtk.DirectionType.LEFT : Gtk.DirectionType.RIGHT;
                group.navigate_focus(this.actor, direction, false);
                return true;
            }
        }
        return false;
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
        this.actor._delegate = null;

        if (this.menu)
            this.menu.destroy();
        this.actor.destroy();

        this.emit('destroy');
    }
});
Signals.addSignalMethods(Button.prototype);

/* SystemStatusButton:
 *
 * This class manages one System Status indicator (network, keyboard,
 * volume, bluetooth...), which is just a PanelMenuButton with an
 * icon.
 */
const SystemStatusButton = new Lang.Class({
    Name: 'SystemStatusButton',
    Extends: Button,

    _init: function(iconName, nameText) {
        this.parent(0.0, nameText);
        this.actor.add_style_class_name('panel-status-button');

        this._box = new St.BoxLayout({ style_class: 'panel-status-button-box' });
        this.actor.add_actor(this._box);

        if (iconName)
            this.setIcon(iconName);
    },

    get icons() {
        return this._box.get_children();
    },

    addIcon: function(gicon) {
        let icon = new St.Icon({ gicon: gicon,
                                 style_class: 'system-status-icon' });
        this._box.add_actor(icon);

        this.emit('icons-changed');

        return icon;
    },

    setIcon: function(iconName) {
        if (!this.mainIcon)
            this.mainIcon = this.addIcon(null);
        this.mainIcon.icon_name = iconName;
    },

    setGIcon: function(gicon) {
        if (this.mainIcon)
            this.mainIcon.gicon = gicon;
        else
            this.mainIcon = this.addIcon(gicon);
    }
});
