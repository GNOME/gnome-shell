// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const Atk = imports.gi.Atk;

const BoxPointer = imports.ui.boxpointer;
const GrabHelper = imports.ui.grabHelper;
const Main = imports.ui.main;
const Params = imports.misc.params;
const Separator = imports.ui.separator;
const Slider = imports.ui.slider;
const Tweener = imports.ui.tweener;

const Ornament = {
    NONE: 0,
    DOT: 1,
    CHECK: 2,
};

function _ensureStyle(actor) {
    if (actor.get_children) {
        let children = actor.get_children();
        for (let i = 0; i < children.length; i++)
            _ensureStyle(children[i]);
    }

    if (actor instanceof St.Widget)
        actor.ensure_style();
}

const PopupBaseMenuItem = new Lang.Class({
    Name: 'PopupBaseMenuItem',

    _init: function (params) {
        params = Params.parse (params, { reactive: true,
                                         activate: true,
                                         hover: true,
                                         sensitive: true,
                                         style_class: null,
                                         can_focus: true
                                       });
        this.actor = new Shell.GenericContainer({ style_class: 'popup-menu-item',
                                                  reactive: params.reactive,
                                                  track_hover: params.reactive,
                                                  can_focus: params.can_focus,
                                                  accessible_role: Atk.Role.MENU_ITEM});
        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));
        this.actor.connect('style-changed', Lang.bind(this, this._onStyleChanged));
        this.actor._delegate = this;

        this._children = [];
        this._ornament = Ornament.NONE;
        this._ornamentLabel = new St.Label({ style_class: 'popup-menu-ornament' });
        this.actor.add_actor(this._ornamentLabel);
        this._columnWidths = null;
        this._spacing = 0;
        this.active = false;
        this._activatable = params.reactive && params.activate;
        this.sensitive = this._activatable && params.sensitive;

        this.setSensitive(this.sensitive);

        if (!this._activatable)
            this.actor.add_style_class_name('popup-inactive-menu-item');

        if (params.style_class)
            this.actor.add_style_class_name(params.style_class);

        if (this._activatable) {
            this.actor.connect('button-release-event', Lang.bind(this, this._onButtonReleaseEvent));
            this.actor.connect('key-press-event', Lang.bind(this, this._onKeyPressEvent));
        }
        if (params.reactive && params.hover)
            this.actor.connect('notify::hover', Lang.bind(this, this._onHoverChanged));

        this.actor.connect('key-focus-in', Lang.bind(this, this._onKeyFocusIn));
        this.actor.connect('key-focus-out', Lang.bind(this, this._onKeyFocusOut));
    },

    _onStyleChanged: function (actor) {
        this._spacing = Math.round(actor.get_theme_node().get_length('spacing'));
    },

    _onButtonReleaseEvent: function (actor, event) {
        this.activate(event);
        return true;
    },

    _onKeyPressEvent: function (actor, event) {
        let symbol = event.get_key_symbol();

        if (symbol == Clutter.KEY_space || symbol == Clutter.KEY_Return) {
            this.activate(event);
            return true;
        }
        return false;
    },

    _onKeyFocusIn: function (actor) {
        this.setActive(true);
    },

    _onKeyFocusOut: function (actor) {
        this.setActive(false);
    },

    _onHoverChanged: function (actor) {
        this.setActive(actor.hover);
    },

    activate: function (event) {
        this.emit('activate', event);
    },

    setActive: function (active, params) {
        let activeChanged = active != this.active;
        params = Params.parse (params, { grabKeyboard: true });

        if (activeChanged) {
            this.active = active;
            if (active) {
                this.actor.add_style_pseudo_class('active');
                if (params.grabKeyboard)
                    this.actor.grab_key_focus();
            } else
                this.actor.remove_style_pseudo_class('active');
            this.emit('active-changed', active);
        }
    },

    setSensitive: function(sensitive) {
        if (!this._activatable)
            return;
        if (this.sensitive == sensitive)
            return;

        this.sensitive = sensitive;
        this.actor.reactive = sensitive;
        this.actor.can_focus = sensitive;

        this.emit('sensitive-changed', sensitive);
    },

    destroy: function() {
        this.actor.destroy();
        this.emit('destroy');
    },

    // adds an actor to the menu item; @params can contain %span
    // (column span; defaults to 1, -1 means "all the remaining width"),
    // %expand (defaults to #false), and %align (defaults to
    // #St.Align.START)
    addActor: function(child, params) {
        params = Params.parse(params, { span: 1,
                                        expand: false,
                                        align: St.Align.START });
        params.actor = child;
        this._children.push(params);
        this.actor.connect('destroy', Lang.bind(this, function () { this._removeChild(child); }));
        this.actor.add_actor(child);
    },

    _removeChild: function(child) {
        for (let i = 0; i < this._children.length; i++) {
            if (this._children[i].actor == child) {
                this._children.splice(i, 1);
                return;
            }
        }
    },

    removeActor: function(child) {
        this.actor.remove_actor(child);
        this._removeChild(child);
    },

    setOrnament: function(ornament) {
        if (ornament == this._ornament)
            return;

        this._ornament = ornament;

        if (ornament == Ornament.DOT) {
            this._ornamentLabel.text = '\u2022';
            this.actor.add_accessible_state(Atk.StateType.CHECKED);
        } else if (ornament == Ornament.CHECK) {
            this._ornamentLabel.text = '\u2713';
            this.actor.add_accessible_state(Atk.StateType.CHECKED);
        } else if (ornament == Ornament.NONE) {
            this._ornamentLabel.text = '';
            this.actor.remove_accessible_state(Atk.StateType.CHECKED);
        }
    },

    // This returns column widths in logical order (i.e. from the dot
    // to the image), not in visual order (left to right)
    getColumnWidths: function() {
        let widths = [];
        for (let i = 0, col = 0; i < this._children.length; i++) {
            let child = this._children[i];
            let [min, natural] = child.actor.get_preferred_width(-1);
            widths[col++] = natural;
            if (child.span > 1) {
                for (let j = 1; j < child.span; j++)
                    widths[col++] = 0;
            }
        }
        return widths;
    },

    setColumnWidths: function(widths) {
        this._columnWidths = widths;
    },

    _getPreferredWidth: function(actor, forHeight, alloc) {
        let width = 0;
        if (this._columnWidths) {
            for (let i = 0; i < this._columnWidths.length; i++) {
                if (i > 0)
                    width += this._spacing;
                width += this._columnWidths[i];
            }
        } else {
            for (let i = 0; i < this._children.length; i++) {
                let child = this._children[i];
                if (i > 0)
                    width += this._spacing;
                let [min, natural] = child.actor.get_preferred_width(-1);
                width += natural;
            }
        }
        alloc.min_size = alloc.natural_size = width;
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let height = 0, x = 0, minWidth, childWidth;
        for (let i = 0; i < this._children.length; i++) {
            let child = this._children[i];
            if (this._columnWidths) {
                if (child.span == -1) {
                    childWidth = 0;
                    for (let j = i; j < this._columnWidths.length; j++)
                        childWidth += this._columnWidths[j]
                } else
                    childWidth = this._columnWidths[i];
            } else {
                if (child.span == -1)
                    childWidth = forWidth - x;
                else
                    [minWidth, childWidth] = child.actor.get_preferred_width(-1);
            }
            x += childWidth;

            let [min, natural] = child.actor.get_preferred_height(childWidth);
            if (natural > height)
                height = natural;
        }
        alloc.min_size = alloc.natural_size = height;
    },

    _allocate: function(actor, box, flags) {
        let height = box.y2 - box.y1;
        let direction = this.actor.get_text_direction();

        // The ornament is placed outside box
        // one quarter of padding from the border of the container
        // (so 3/4 from the inner border)
        // (padding is box.x1)
        let ornamentBox = new Clutter.ActorBox();
        let ornamentWidth = box.x1;

        ornamentBox.x1 = 0;
        ornamentBox.x2 = ornamentWidth;
        ornamentBox.y1 = box.y1;
        ornamentBox.y2 = box.y2;

        if (direction == Clutter.TextDirection.RTL) {
            ornamentBox.x1 += box.x2;
            ornamentBox.x2 += box.x2;
        }

        this._ornamentLabel.allocate(ornamentBox, flags);

        let x;
        if (direction == Clutter.TextDirection.LTR)
            x = box.x1;
        else
            x = box.x2;
        // if direction is ltr, x is the right edge of the last added
        // actor, and it's constantly increasing, whereas if rtl, x is
        // the left edge and it decreases
        for (let i = 0, col = 0; i < this._children.length; i++) {
            let child = this._children[i];
            let childBox = new Clutter.ActorBox();

            let [minWidth, naturalWidth] = child.actor.get_preferred_width(-1);
            let availWidth, extraWidth;
            if (this._columnWidths) {
                if (child.span == -1) {
                    if (direction == Clutter.TextDirection.LTR)
                        availWidth = box.x2 - x;
                    else
                        availWidth = x - box.x1;
                } else {
                    availWidth = 0;
                    for (let j = 0; j < child.span; j++)
                        availWidth += this._columnWidths[col++];
                }
                extraWidth = availWidth - naturalWidth;
            } else {
                if (child.span == -1) {
                    if (direction == Clutter.TextDirection.LTR)
                        availWidth = box.x2 - x;
                    else
                        availWidth = x - box.x1;
                } else {
                    availWidth = naturalWidth;
                }
                extraWidth = 0;
            }

            if (direction == Clutter.TextDirection.LTR) {
                if (child.expand) {
                    childBox.x1 = x;
                    childBox.x2 = x + availWidth;
                } else if (child.align === St.Align.MIDDLE) {
                    childBox.x1 = x + Math.round(extraWidth / 2);
                    childBox.x2 = childBox.x1 + naturalWidth;
                } else if (child.align === St.Align.END) {
                    childBox.x2 = x + availWidth;
                    childBox.x1 = childBox.x2 - naturalWidth;
                } else {
                    childBox.x1 = x;
                    childBox.x2 = x + naturalWidth;
                }
            } else {
                if (child.expand) {
                    childBox.x1 = x - availWidth;
                    childBox.x2 = x;
                } else if (child.align === St.Align.MIDDLE) {
                    childBox.x1 = x - Math.round(extraWidth / 2);
                    childBox.x2 = childBox.x1 + naturalWidth;
                } else if (child.align === St.Align.END) {
                    // align to the left
                    childBox.x1 = x - availWidth;
                    childBox.x2 = childBox.x1 + naturalWidth;
                } else {
                    // align to the right
                    childBox.x2 = x;
                    childBox.x1 = x - naturalWidth;
                }
            }

            let [minHeight, naturalHeight] = child.actor.get_preferred_height(childBox.x2 - childBox.x1);
            childBox.y1 = Math.round(box.y1 + (height - naturalHeight) / 2);
            childBox.y2 = childBox.y1 + naturalHeight;

            child.actor.allocate(childBox, flags);

            if (direction == Clutter.TextDirection.LTR)
                x += availWidth + this._spacing;
            else
                x -= availWidth + this._spacing;
        }
    }
});
Signals.addSignalMethods(PopupBaseMenuItem.prototype);

const PopupMenuItem = new Lang.Class({
    Name: 'PopupMenuItem',
    Extends: PopupBaseMenuItem,

    _init: function (text, params) {
        this.parent(params);

        this.label = new St.Label({ text: text });
        this.addActor(this.label);
        this.actor.label_actor = this.label
    }
});

const PopupSeparatorMenuItem = new Lang.Class({
    Name: 'PopupSeparatorMenuItem',
    Extends: PopupBaseMenuItem,

    _init: function (text) {
        this.parent({ reactive: false,
                      can_focus: false});

        this._box = new St.BoxLayout();
        this.addActor(this._box, { span: -1, expand: true });

        this.label = new St.Label({ text: text || '' });
        this._box.add(this.label);
        this.actor.label_actor = this.label;

        this._separator = new Separator.HorizontalSeparator({ style_class: 'popup-separator-menu-item' });
        this._box.add(this._separator.actor, { expand: true });
    }
});

const PopupAlternatingMenuItemState = {
    DEFAULT: 0,
    ALTERNATIVE: 1
}

const PopupAlternatingMenuItem = new Lang.Class({
    Name: 'PopupAlternatingMenuItem',
    Extends: PopupBaseMenuItem,

    _init: function(text, alternateText, params) {
        this.parent(params);
        this.actor.add_style_class_name('popup-alternating-menu-item');

        this._text = text;
        this._alternateText = alternateText;
        this.label = new St.Label({ text: text });
        this.state = PopupAlternatingMenuItemState.DEFAULT;
        this.addActor(this.label);
        this.actor.label_actor = this.label;

        this.actor.connect('notify::mapped', Lang.bind(this, this._onMapped));
    },

    _onMapped: function() {
        if (this.actor.mapped) {
            this._capturedEventId = global.stage.connect('captured-event',
                                                         Lang.bind(this, this._onCapturedEvent));
            this._updateStateFromModifiers();
        } else {
            if (this._capturedEventId != 0) {
                global.stage.disconnect(this._capturedEventId);
                this._capturedEventId = 0;
            }
        }
    },

    _setState: function(state) {
        if (this.state != state) {
            if (state == PopupAlternatingMenuItemState.ALTERNATIVE && !this._canAlternate())
                return;

            this.state = state;
            this._updateLabel();
        }
    },

    _updateStateFromModifiers: function() {
        let [x, y, mods] = global.get_pointer();
        let state;

        if ((mods & Clutter.ModifierType.MOD1_MASK) == 0) {
            state = PopupAlternatingMenuItemState.DEFAULT;
        } else {
            state = PopupAlternatingMenuItemState.ALTERNATIVE;
        }

        this._setState(state);
    },

    _onCapturedEvent: function(actor, event) {
        if (event.type() != Clutter.EventType.KEY_PRESS &&
            event.type() != Clutter.EventType.KEY_RELEASE)
            return false;

        let key = event.get_key_symbol();

        if (key == Clutter.KEY_Alt_L || key == Clutter.KEY_Alt_R)
            this._updateStateFromModifiers();

        return false;
    },

    _updateLabel: function() {
        if (this.state == PopupAlternatingMenuItemState.ALTERNATIVE) {
            this.actor.add_style_pseudo_class('alternate');
            this.label.set_text(this._alternateText);
        } else {
            this.actor.remove_style_pseudo_class('alternate');
            this.label.set_text(this._text);
        }
    },

    _canAlternate: function() {
        if (this.state == PopupAlternatingMenuItemState.DEFAULT && !this._alternateText)
            return false;
        return true;
    },

    updateText: function(text, alternateText) {
        this._text = text;
        this._alternateText = alternateText;

        if (!this._canAlternate())
            this._setState(PopupAlternatingMenuItemState.DEFAULT);

        this._updateLabel();
    }
});

const PopupSliderMenuItem = new Lang.Class({
    Name: 'PopupSliderMenuItem',
    Extends: PopupBaseMenuItem,

    _init: function(value) {
        this.parent({ activate: false });

        this._slider = new Slider.Slider(value);
        this._slider.connect('value-changed', Lang.bind(this, function(actor, value) {
            this.emit('value-changed', value);
        }));
        this.addActor(this._slider.actor);
    },

    setValue: function(value) {
        this._slider.setValue(value);
    },

    get value() {
        return this._slider.value;
    },

    scroll: function (event) {
        this._slider.scroll(event);
    }
});

const Switch = new Lang.Class({
    Name: 'Switch',

    _init: function(state) {
        this.actor = new St.Bin({ style_class: 'toggle-switch',
                                  accessible_role: Atk.Role.CHECK_BOX,
                                  can_focus: true });
        // Translators: this MUST be either "toggle-switch-us"
        // (for toggle switches containing the English words
        // "ON" and "OFF") or "toggle-switch-intl" (for toggle
        // switches containing "◯" and "|"). Other values will
        // simply result in invisible toggle switches.
        this.actor.add_style_class_name(_("toggle-switch-us"));
        this.setToggleState(state);
    },

    setToggleState: function(state) {
        if (state)
            this.actor.add_style_pseudo_class('checked');
        else
            this.actor.remove_style_pseudo_class('checked');
        this.state = state;
    },

    toggle: function() {
        this.setToggleState(!this.state);
    }
});

const PopupSwitchMenuItem = new Lang.Class({
    Name: 'PopupSwitchMenuItem',
    Extends: PopupBaseMenuItem,

    _init: function(text, active, params) {
        this.parent(params);

        this.label = new St.Label({ text: text });
        this._switch = new Switch(active);

        this.actor.accessible_role = Atk.Role.CHECK_MENU_ITEM;
        this.checkAccessibleState();
        this.actor.label_actor = this.label;

        this.addActor(this.label);

        this._statusBin = new St.Bin({ x_align: St.Align.END });
        this.addActor(this._statusBin,
                      { expand: true, span: -1, align: St.Align.END });

        this._statusLabel = new St.Label({ text: '',
                                           style_class: 'popup-status-menu-item'
                                         });
        this._statusBin.child = this._switch.actor;
    },

    setStatus: function(text) {
        if (text != null) {
            this._statusLabel.text = text;
            this._statusBin.child = this._statusLabel;
            this.actor.reactive = false;
            this.actor.accessible_role = Atk.Role.MENU_ITEM;
        } else {
            this._statusBin.child = this._switch.actor;
            this.actor.reactive = true;
            this.actor.accessible_role = Atk.Role.CHECK_MENU_ITEM;
        }
        this.checkAccessibleState();
    },

    activate: function(event) {
        if (this._switch.actor.mapped) {
            this.toggle();
        }

        // we allow pressing space to toggle the switch
        // without closing the menu
        if (event.type() == Clutter.EventType.KEY_PRESS &&
            event.get_key_symbol() == Clutter.KEY_space)
            return;

        this.parent(event);
    },

    toggle: function() {
        this._switch.toggle();
        this.emit('toggled', this._switch.state);
        this.checkAccessibleState();
    },

    get state() {
        return this._switch.state;
    },

    setToggleState: function(state) {
        this._switch.setToggleState(state);
        this.checkAccessibleState();
    },

    checkAccessibleState: function() {
        switch (this.actor.accessible_role) {
        case Atk.Role.CHECK_MENU_ITEM:
            if (this._switch.state)
                this.actor.add_accessible_state (Atk.StateType.CHECKED);
            else
                this.actor.remove_accessible_state (Atk.StateType.CHECKED);
            break;
        default:
            this.actor.remove_accessible_state (Atk.StateType.CHECKED);
        }
    }
});

const PopupImageMenuItem = new Lang.Class({
    Name: 'PopupImageMenuItem',
    Extends: PopupBaseMenuItem,

    _init: function (text, iconName, params) {
        this.parent(params);

        this.label = new St.Label({ text: text });
        this.addActor(this.label);
        this._icon = new St.Icon({ style_class: 'popup-menu-icon' });
        this.addActor(this._icon, { align: St.Align.END });

        this.setIcon(iconName);
    },

    setIcon: function(name) {
        this._icon.icon_name = name;
    }
});

const PopupMenuBase = new Lang.Class({
    Name: 'PopupMenuBase',
    Abstract: true,

    _init: function(sourceActor, styleClass) {
        this.sourceActor = sourceActor;

        if (styleClass !== undefined) {
            this.box = new St.BoxLayout({ style_class: styleClass,
                                          vertical: true });
        } else {
            this.box = new St.BoxLayout({ vertical: true });
        }
        this.box.connect_after('queue-relayout', Lang.bind(this, this._menuQueueRelayout));
        this.length = 0;

        this.isOpen = false;

        // If set, we don't send events (including crossing events) to the source actor
        // for the menu which causes its prelight state to freeze
        this.blockSourceEvents = false;

        this._activeMenuItem = null;
        this._settingsActions = { };

        this._sessionUpdatedId = Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
    },

    _sessionUpdated: function() {
        this._setSettingsVisibility(Main.sessionMode.allowSettings);
    },

    addAction: function(title, callback) {
        let menuItem = new PopupMenuItem(title);
        this.addMenuItem(menuItem);
        menuItem.connect('activate', Lang.bind(this, function (menuItem, event) {
            callback(event);
        }));

        return menuItem;
    },

    addSettingsAction: function(title, desktopFile) {
        let menuItem = this.addAction(title, function() {
                           let app = Shell.AppSystem.get_default().lookup_app(desktopFile);

                           if (!app) {
                               log('Settings panel for desktop file ' + desktopFile + ' could not be loaded!');
                               return;
                           }

                           Main.overview.hide();
                           app.activate();
                       });

        menuItem.actor.visible = Main.sessionMode.allowSettings;
        this._settingsActions[desktopFile] = menuItem;

        return menuItem;
    },

    _setSettingsVisibility: function(visible) {
        for (let id in this._settingsActions) {
            let item = this._settingsActions[id];
            item.actor.visible = visible;
        }
    },

    isEmpty: function() {
        let hasVisibleChildren = this.box.get_children().some(function(child) {
            return child.visible;
        });

        return !hasVisibleChildren;
    },

    isChildMenu: function() {
        return false;
    },

    /**
     * _connectSubMenuSignals:
     * @object: a menu item, or a menu section
     * @menu: a sub menu, or a menu section
     *
     * Connects to signals on @menu that are necessary for
     * operating the submenu, and stores the ids on @object.
     */
    _connectSubMenuSignals: function(object, menu) {
        object._subMenuActivateId = menu.connect('activate', Lang.bind(this, function() {
            this.emit('activate');
            this.close(BoxPointer.PopupAnimation.FULL);
        }));
        object._subMenuActiveChangeId = menu.connect('active-changed', Lang.bind(this, function(submenu, submenuItem) {
            if (this._activeMenuItem && this._activeMenuItem != submenuItem)
                this._activeMenuItem.setActive(false);
            this._activeMenuItem = submenuItem;
            this.emit('active-changed', submenuItem);
        }));
    },

    _connectItemSignals: function(menuItem) {
        menuItem._activeChangeId = menuItem.connect('active-changed', Lang.bind(this, function (menuItem, active) {
            if (active && this._activeMenuItem != menuItem) {
                if (this._activeMenuItem)
                    this._activeMenuItem.setActive(false);
                this._activeMenuItem = menuItem;
                this.emit('active-changed', menuItem);
            } else if (!active && this._activeMenuItem == menuItem) {
                this._activeMenuItem = null;
                this.emit('active-changed', null);
            }
        }));
        menuItem._sensitiveChangeId = menuItem.connect('sensitive-changed', Lang.bind(this, function(menuItem, sensitive) {
            if (!sensitive && this._activeMenuItem == menuItem) {
                if (!this.actor.navigate_focus(menuItem.actor,
                                               Gtk.DirectionType.TAB_FORWARD,
                                               true))
                    this.actor.grab_key_focus();
            } else if (sensitive && this._activeMenuItem == null) {
                if (global.stage.get_key_focus() == this.actor)
                    menuItem.actor.grab_key_focus();
            }
        }));
        menuItem._activateId = menuItem.connect('activate', Lang.bind(this, function (menuItem, event) {
            this.emit('activate', menuItem);
            this.close(BoxPointer.PopupAnimation.FULL);
        }));
        // the weird name is to avoid a conflict with some random property
        // the menuItem may have, called destroyId
        // (FIXME: in the future it may make sense to have container objects
        // like PopupMenuManager does)
        menuItem._popupMenuDestroyId = menuItem.connect('destroy', Lang.bind(this, function(menuItem) {
            menuItem.disconnect(menuItem._popupMenuDestroyId);
            menuItem.disconnect(menuItem._activateId);
            menuItem.disconnect(menuItem._activeChangeId);
            menuItem.disconnect(menuItem._sensitiveChangeId);
            if (menuItem.menu) {
                menuItem.menu.disconnect(menuItem._subMenuActivateId);
                menuItem.menu.disconnect(menuItem._subMenuActiveChangeId);
                this.disconnect(menuItem._closingId);
            }
            if (menuItem == this._activeMenuItem)
                this._activeMenuItem = null;
        }));
    },

    _updateSeparatorVisibility: function(menuItem) {
        if (menuItem.label.text)
            return;

        let children = this.box.get_children();

        let index = children.indexOf(menuItem.actor);

        if (index < 0)
            return;

        let childBeforeIndex = index - 1;

        while (childBeforeIndex >= 0 && !children[childBeforeIndex].visible)
            childBeforeIndex--;

        if (childBeforeIndex < 0
            || children[childBeforeIndex]._delegate instanceof PopupSeparatorMenuItem) {
            menuItem.actor.hide();
            return;
        }

        let childAfterIndex = index + 1;

        while (childAfterIndex < children.length && !children[childAfterIndex].visible)
            childAfterIndex++;

        if (childAfterIndex >= children.length
            || children[childAfterIndex]._delegate instanceof PopupSeparatorMenuItem) {
            menuItem.actor.hide();
            return;
        }

        menuItem.actor.show();
    },

    addMenuItem: function(menuItem, position) {
        let before_item = null;
        if (position == undefined) {
            this.box.add(menuItem.actor);
        } else {
            let items = this._getMenuItems();
            if (position < items.length) {
                before_item = items[position].actor;
                this.box.insert_child_below(menuItem.actor, before_item);
            } else {
                this.box.add(menuItem.actor);
            }
        }

        if (menuItem instanceof PopupMenuSection) {
            this._connectSubMenuSignals(menuItem, menuItem);
            menuItem._parentOpenStateChangedId = this.connect('open-state-changed',
                function(self, open) {
                    if (open)
                        menuItem.open();
                    else
                        menuItem.close();
                });
            menuItem.connect('destroy', Lang.bind(this, function() {
                menuItem.disconnect(menuItem._subMenuActivateId);
                menuItem.disconnect(menuItem._subMenuActiveChangeId);
                this.disconnect(menuItem._parentOpenStateChangedId);

                this.length--;
            }));
        } else if (menuItem instanceof PopupSubMenuMenuItem) {
            if (before_item == null)
                this.box.add(menuItem.menu.actor);
            else
                this.box.insert_child_below(menuItem.menu.actor, before_item);
            this._connectSubMenuSignals(menuItem, menuItem.menu);
            this._connectItemSignals(menuItem);
            menuItem._closingId = this.connect('open-state-changed', function(self, open) {
                if (!open)
                    menuItem.menu.close(BoxPointer.PopupAnimation.FADE);
            });
        } else if (menuItem instanceof PopupSeparatorMenuItem) {
            this._connectItemSignals(menuItem);

            // updateSeparatorVisibility needs to get called any time the
            // separator's adjacent siblings change visibility or position.
            // open-state-changed isn't exactly that, but doing it in more
            // precise ways would require a lot more bookkeeping.
            this.connect('open-state-changed', Lang.bind(this, function() { this._updateSeparatorVisibility(menuItem); }));
        } else if (menuItem instanceof PopupBaseMenuItem)
            this._connectItemSignals(menuItem);
        else
            throw TypeError("Invalid argument to PopupMenuBase.addMenuItem()");

        this.length++;
    },

    getColumnWidths: function() {
        let columnWidths = [];
        let items = this.box.get_children();
        for (let i = 0; i < items.length; i++) {
            if (!items[i].visible &&
                !(items[i]._delegate instanceof PopupSubMenu && items[i-1].visible))
                continue;
            if (items[i]._delegate instanceof PopupBaseMenuItem || items[i]._delegate instanceof PopupMenuBase) {
                let itemColumnWidths = items[i]._delegate.getColumnWidths();
                for (let j = 0; j < itemColumnWidths.length; j++) {
                    if (j >= columnWidths.length || itemColumnWidths[j] > columnWidths[j])
                        columnWidths[j] = itemColumnWidths[j];
                }
            }
        }
        return columnWidths;
    },

    setColumnWidths: function(widths) {
        let items = this.box.get_children();
        for (let i = 0; i < items.length; i++) {
            if (items[i]._delegate instanceof PopupBaseMenuItem || items[i]._delegate instanceof PopupMenuBase)
                items[i]._delegate.setColumnWidths(widths);
        }
    },

    // Because of the above column-width funniness, we need to do a
    // queue-relayout on every item whenever the menu itself changes
    // size, to force clutter to drop its cached size requests. (The
    // menuitems will in turn call queue_relayout on their parent, the
    // menu, but that call will be a no-op since the menu already
    // has a relayout queued, so we won't get stuck in a loop.
    _menuQueueRelayout: function() {
        this.box.get_children().map(function (actor) { actor.queue_relayout(); });
    },

    addActor: function(actor) {
        this.box.add(actor);
    },

    _getMenuItems: function() {
        return this.box.get_children().map(function (actor) {
            return actor._delegate;
        }).filter(function(item) {
            return item instanceof PopupBaseMenuItem || item instanceof PopupMenuSection;
        });
    },

    get firstMenuItem() {
        let items = this._getMenuItems();
        if (items.length)
            return items[0];
        else
            return null;
    },

    get numMenuItems() {
        return this._getMenuItems().length;
    },

    removeAll: function() {
        let children = this._getMenuItems();
        for (let i = 0; i < children.length; i++) {
            let item = children[i];
            item.destroy();
        }
    },

    toggle: function() {
        if (this.isOpen)
            this.close(BoxPointer.PopupAnimation.FULL);
        else
            this.open(BoxPointer.PopupAnimation.FULL);
    },

    destroy: function() {
        this.close();
        this.removeAll();
        this.actor.destroy();

        this.emit('destroy');

        Main.sessionMode.disconnect(this._sessionUpdatedId);
        this._sessionUpdatedId = 0;
    }
});
Signals.addSignalMethods(PopupMenuBase.prototype);

const PopupMenu = new Lang.Class({
    Name: 'PopupMenu',
    Extends: PopupMenuBase,

    _init: function(sourceActor, arrowAlignment, arrowSide) {
        this.parent(sourceActor, 'popup-menu-content');

        this._arrowAlignment = arrowAlignment;
        this._arrowSide = arrowSide;

        this._boxPointer = new BoxPointer.BoxPointer(arrowSide,
                                                     { x_fill: true,
                                                       y_fill: true,
                                                       x_align: St.Align.START });
        this.actor = this._boxPointer.actor;
        this.actor._delegate = this;
        this.actor.style_class = 'popup-menu-boxpointer';

        this._boxWrapper = new Shell.GenericContainer();
        this._boxWrapper.connect('get-preferred-width', Lang.bind(this, this._boxGetPreferredWidth));
        this._boxWrapper.connect('get-preferred-height', Lang.bind(this, this._boxGetPreferredHeight));
        this._boxWrapper.connect('allocate', Lang.bind(this, this._boxAllocate));
        this._boxPointer.bin.set_child(this._boxWrapper);
        this._boxWrapper.add_actor(this.box);
        this.actor.add_style_class_name('popup-menu');

        global.focus_manager.add_group(this.actor);
        this.actor.reactive = true;

        this._childMenus = [];
    },

    _boxGetPreferredWidth: function (actor, forHeight, alloc) {
        let columnWidths = this.getColumnWidths();
        this.setColumnWidths(columnWidths);

        // Now they will request the right sizes
        [alloc.min_size, alloc.natural_size] = this.box.get_preferred_width(forHeight);
    },

    _boxGetPreferredHeight: function (actor, forWidth, alloc) {
        [alloc.min_size, alloc.natural_size] = this.box.get_preferred_height(forWidth);
    },

    _boxAllocate: function (actor, box, flags) {
        this.box.allocate(box, flags);
    },

    setArrowOrigin: function(origin) {
        this._boxPointer.setArrowOrigin(origin);
    },

    setSourceAlignment: function(alignment) {
        this._boxPointer.setSourceAlignment(alignment);
    },

    isChildMenu: function(menu) {
        return this._childMenus.indexOf(menu) != -1;
    },

    addChildMenu: function(menu) {
        if (this.isChildMenu(menu))
            return;

        this._childMenus.push(menu);
        this.emit('child-menu-added', menu);
    },

    removeChildMenu: function(menu) {
        let index = this._childMenus.indexOf(menu);

        if (index == -1)
            return;

        this._childMenus.splice(index, 1);
        this.emit('child-menu-removed', menu);
    },

    open: function(animate) {
        if (this.isOpen)
            return;

        if (this.isEmpty())
            return;

        this.isOpen = true;

        this._boxPointer.setPosition(this.sourceActor, this._arrowAlignment);
        this._boxPointer.show(animate);

        this.actor.raise_top();

        this.emit('open-state-changed', true);
    },

    close: function(animate) {
        if (this._activeMenuItem)
            this._activeMenuItem.setActive(false);

        this._childMenus.forEach(function(childMenu) {
            childMenu.close();
        });

        if (this._boxPointer.actor.visible)
            this._boxPointer.hide(animate);

        if (!this.isOpen)
            return;

        this.isOpen = false;
        this.emit('open-state-changed', false);
    }
});

const PopupDummyMenu = new Lang.Class({
    Name: 'PopupDummyMenu',

    _init: function(sourceActor) {
        this.sourceActor = sourceActor;
        this.actor = sourceActor;
        this.actor._delegate = this;
    },

    isChildMenu: function() {
        return false;
    },

    open: function() { this.emit('open-state-changed', true); },
    close: function() { this.emit('open-state-changed', false); },
    toggle: function() {},
    destroy: function() {
        this.emit('destroy');
    },
});
Signals.addSignalMethods(PopupDummyMenu.prototype);

const PopupSubMenu = new Lang.Class({
    Name: 'PopupSubMenu',
    Extends: PopupMenuBase,

    _init: function(sourceActor, sourceArrow) {
        this.parent(sourceActor);

        this._arrow = sourceArrow;
        this._arrow.rotation_center_z_gravity = Clutter.Gravity.CENTER;

        // Since a function of a submenu might be to provide a "More.." expander
        // with long content, we make it scrollable - the scrollbar will only take
        // effect if a CSS max-height is set on the top menu.
        this.actor = new St.ScrollView({ style_class: 'popup-sub-menu',
                                         hscrollbar_policy: Gtk.PolicyType.NEVER,
                                         vscrollbar_policy: Gtk.PolicyType.NEVER });

        this.actor.add_actor(this.box);
        this.actor._delegate = this;
        this.actor.clip_to_allocation = true;
        this.actor.connect('key-press-event', Lang.bind(this, this._onKeyPressEvent));
        this.actor.hide();
    },

    _getTopMenu: function() {
        let actor = this.actor.get_parent();
        while (actor) {
            if (actor._delegate && actor._delegate instanceof PopupMenu)
                return actor._delegate;

            actor = actor.get_parent();
        }

        return null;
    },

    _needsScrollbar: function() {
        let topMenu = this._getTopMenu();
        let [topMinHeight, topNaturalHeight] = topMenu.actor.get_preferred_height(-1);
        let topThemeNode = topMenu.actor.get_theme_node();

        let topMaxHeight = topThemeNode.get_max_height();
        return topMaxHeight >= 0 && topNaturalHeight >= topMaxHeight;
    },

    open: function(animate) {
        if (this.isOpen)
            return;

        if (this.isEmpty())
            return;

        this.isOpen = true;

        this.actor.show();

        let needsScrollbar = this._needsScrollbar();

        // St.ScrollView always requests space horizontally for a possible vertical
        // scrollbar if in AUTOMATIC mode. Doing better would require implementation
        // of width-for-height in St.BoxLayout and St.ScrollView. This looks bad
        // when we *don't* need it, so turn off the scrollbar when that's true.
        // Dynamic changes in whether we need it aren't handled properly.
        this.actor.vscrollbar_policy =
            needsScrollbar ? Gtk.PolicyType.AUTOMATIC : Gtk.PolicyType.NEVER;

        if (needsScrollbar)
            this.actor.add_style_pseudo_class('scrolled');
        else
            this.actor.remove_style_pseudo_class('scrolled');

        // It looks funny if we animate with a scrollbar (at what point is
        // the scrollbar added?) so just skip that case
        if (animate && needsScrollbar)
            animate = false;

        if (animate) {
            let [minHeight, naturalHeight] = this.actor.get_preferred_height(-1);
            this.actor.height = 0;
            this.actor._arrow_rotation = this._arrow.rotation_angle_z;
            Tweener.addTween(this.actor,
                             { _arrow_rotation: 90,
                               height: naturalHeight,
                               time: 0.25,
                               onUpdateScope: this,
                               onUpdate: function() {
                                   this._arrow.rotation_angle_z = this.actor._arrow_rotation;
                               },
                               onCompleteScope: this,
                               onComplete: function() {
                                   this.actor.set_height(-1);
                                   this.emit('open-state-changed', true);
                               }
                             });
        } else {
            this._arrow.rotation_angle_z = 90;
            this.emit('open-state-changed', true);
        }
    },

    close: function(animate) {
        if (!this.isOpen)
            return;

        this.isOpen = false;

        if (this._activeMenuItem)
            this._activeMenuItem.setActive(false);

        if (animate && this._needsScrollbar())
            animate = false;

        if (animate) {
            this.actor._arrow_rotation = this._arrow.rotation_angle_z;
            Tweener.addTween(this.actor,
                             { _arrow_rotation: 0,
                               height: 0,
                               time: 0.25,
                               onCompleteScope: this,
                               onComplete: function() {
                                   this.actor.hide();
                                   this.actor.set_height(-1);

                                   this.emit('open-state-changed', false);
                               },
                               onUpdateScope: this,
                               onUpdate: function() {
                                   this._arrow.rotation_angle_z = this.actor._arrow_rotation;
                               }
                             });
            } else {
                this._arrow.rotation_angle_z = 0;
                this.actor.hide();

                this.isOpen = false;
                this.emit('open-state-changed', false);
            }
    },

    _onKeyPressEvent: function(actor, event) {
        // Move focus back to parent menu if the user types Left.

        if (this.isOpen && event.get_key_symbol() == Clutter.KEY_Left) {
            this.close(BoxPointer.PopupAnimation.FULL);
            this.sourceActor._delegate.setActive(true);
            return true;
        }

        return false;
    }
});

/**
 * PopupMenuSection:
 *
 * A section of a PopupMenu which is handled like a submenu
 * (you can add and remove items, you can destroy it, you
 * can add it to another menu), but is completely transparent
 * to the user
 */
const PopupMenuSection = new Lang.Class({
    Name: 'PopupMenuSection',
    Extends: PopupMenuBase,

    _init: function() {
        this.parent();

        this.actor = this.box;
        this.actor._delegate = this;
        this.isOpen = true;
    },

    // deliberately ignore any attempt to open() or close(), but emit the
    // corresponding signal so children can still pick it up
    open: function() { this.emit('open-state-changed', true); },
    close: function() { this.emit('open-state-changed', false); },
});

const PopupSubMenuMenuItem = new Lang.Class({
    Name: 'PopupSubMenuMenuItem',
    Extends: PopupBaseMenuItem,

    _init: function(text) {
        this.parent();

        this.actor.add_style_class_name('popup-submenu-menu-item');

        this.label = new St.Label({ text: text });
        this.addActor(this.label);
        this.actor.label_actor = this.label;
        this._triangle = new St.Label({ text: '\u25B8' });
        this.addActor(this._triangle, { align: St.Align.END });

        this.menu = new PopupSubMenu(this.actor, this._triangle);
        this.menu.connect('open-state-changed', Lang.bind(this, this._subMenuOpenStateChanged));
    },

    _subMenuOpenStateChanged: function(menu, open) {
        if (open)
            this.actor.add_style_pseudo_class('open');
        else
            this.actor.remove_style_pseudo_class('open');
    },

    destroy: function() {
        this.menu.destroy();

        this.parent();
    },

    setSubmenuShown: function(open) {
        if (open)
            this.menu.open(BoxPointer.PopupAnimation.FULL);
        else
            this.menu.close(BoxPointer.PopupAnimation.FULL);
    },

    _setOpenState: function(open) {
        this.setSubmenuShown(open);
    },

    _getOpenState: function() {
        return this.menu.isOpen;
    },

    _onKeyPressEvent: function(actor, event) {
        let symbol = event.get_key_symbol();

        if (symbol == Clutter.KEY_Right) {
            this._setOpenState(true);
            this.menu.actor.navigate_focus(null, Gtk.DirectionType.DOWN, false);
            return true;
        } else if (symbol == Clutter.KEY_Left && this._getOpenState()) {
            this._setOpenState(false);
            return true;
        }

        return this.parent(actor, event);
    },

    activate: function(event) {
        this._setOpenState(true);
    },

    _onButtonReleaseEvent: function(actor) {
        this._setOpenState(!this._getOpenState());
    }
});

const PopupComboMenu = new Lang.Class({
    Name: 'PopupComboMenu',
    Extends: PopupMenuBase,

    _init: function(sourceActor) {
        this.parent(sourceActor, 'popup-combo-menu');

        this.actor = this.box;
        this.actor._delegate = this;
        this.actor.connect('key-focus-in', Lang.bind(this, this._onKeyFocusIn));
        sourceActor.connect('style-changed',
                            Lang.bind(this, this._onSourceActorStyleChanged));
        this._activeItemPos = -1;
        global.focus_manager.add_group(this.actor);
    },

    _onKeyFocusIn: function(actor) {
        let items = this._getMenuItems();
        let activeItem = items[this._activeItemPos];
        activeItem.actor.grab_key_focus();
    },

    _onSourceActorStyleChanged: function() {
        // PopupComboBoxMenuItem clones the active item's actors
        // to work with arbitrary items in the menu; this means
        // that we need to propagate some style information and
        // enforce style updates even when the menu is closed
        let activeItem = this._getMenuItems()[this._activeItemPos];
        if (this.sourceActor.has_style_pseudo_class('insensitive'))
            activeItem.actor.add_style_pseudo_class('insensitive');
        else
            activeItem.actor.remove_style_pseudo_class('insensitive');

        // To propagate the :active style, we need to make sure that the
        // internal state of the PopupComboMenu is updated as well, but
        // we must not move the keyboard grab
        activeItem.setActive(this.sourceActor.has_style_pseudo_class('active'),
                             { grabKeyboard: false });

        _ensureStyle(this.actor);
    },

    open: function() {
        if (this.isOpen)
            return;

        if (this.isEmpty())
            return;

        this.isOpen = true;

        let [sourceX, sourceY] = this.sourceActor.get_transformed_position();
        let items = this._getMenuItems();
        let activeItem = items[this._activeItemPos];

        this.actor.set_position(sourceX, sourceY - activeItem.actor.y);
        this.actor.width = Math.max(this.actor.width, this.sourceActor.width);
        this.actor.raise_top();

        this.actor.opacity = 0;
        this.actor.show();

        Tweener.addTween(this.actor,
                         { opacity: 255,
                           transition: 'linear',
                           time: BoxPointer.POPUP_ANIMATION_TIME });

        this.emit('open-state-changed', true);
    },

    close: function() {
        if (!this.isOpen)
            return;

        this.isOpen = false;
        Tweener.addTween(this.actor,
                         { opacity: 0,
                           transition: 'linear',
                           time: BoxPointer.POPUP_ANIMATION_TIME,
                           onComplete: Lang.bind(this,
                               function() {
                                   this.actor.hide();
                               })
                         });

        this.emit('open-state-changed', false);
    },

    setActiveItem: function(position) {
        this._activeItemPos = position;
    },

    getActiveItem: function() {
        return this._getMenuItems()[this._activeItemPos];
    },

    setItemVisible: function(position, visible) {
        if (!visible && position == this._activeItemPos) {
            log('Trying to hide the active menu item.');
            return;
        }

        this._getMenuItems()[position].actor.visible = visible;
    },

    getItemVisible: function(position) {
        return this._getMenuItems()[position].actor.visible;
    }
});

const PopupComboBoxMenuItem = new Lang.Class({
    Name: 'PopupComboBoxMenuItem',
    Extends: PopupBaseMenuItem,

    _init: function (params) {
        this.parent(params);

        this.actor.accessible_role = Atk.Role.COMBO_BOX;

        this._itemBox = new Shell.Stack();
        this.addActor(this._itemBox);

        let expander = new St.Label({ text: '\u2304' });
        this.addActor(expander, { align: St.Align.END,
                                  span: -1 });

        this._menu = new PopupComboMenu(this.actor);
        Main.uiGroup.add_actor(this._menu.actor);
        this._menu.actor.hide();

        if (params.style_class)
            this._menu.actor.add_style_class_name(params.style_class);

        this.actor.connect('scroll-event', Lang.bind(this, this._onScrollEvent));

        this._activeItemPos = -1;
        this._items = [];
    },

    _getTopMenu: function() {
        let actor = this.actor.get_parent();
        while (actor) {
            if (actor._delegate && actor._delegate instanceof PopupMenu)
                return actor._delegate;

            actor = actor.get_parent();
        }

        return null;
    },

    _onScrollEvent: function(actor, event) {
        if (this._activeItemPos == -1)
            return;

        let position = this._activeItemPos;
        let direction = event.get_scroll_direction();
        if (direction == Clutter.ScrollDirection.DOWN) {
            while (position < this._items.length - 1) {
                position++;
                if (this._menu.getItemVisible(position))
                    break;
            }
        } else if (direction == Clutter.ScrollDirection.UP) {
            while (position > 0) {
                position--;
                if (this._menu.getItemVisible(position))
                    break;
            }
        }

        if (position == this._activeItemPos)
            return;

        this.setActiveItem(position);
        this.emit('active-item-changed', position);
    },

    activate: function(event) {
        let topMenu = this._getTopMenu();
        if (!topMenu)
            return;

        topMenu.addChildMenu(this._menu);
        this._menu.toggle();
    },

    addMenuItem: function(menuItem, position) {
        if (position === undefined)
            position = this._menu.numMenuItems;

        this._menu.addMenuItem(menuItem, position);
        _ensureStyle(this._menu.actor);

        let item = new St.BoxLayout({ style_class: 'popup-combobox-item' });

        let children = menuItem.actor.get_children();
        for (let i = 0; i < children.length; i++) {
            let clone = new Clutter.Clone({ source: children[i] });
            item.add(clone, { y_fill: false });
        }

        let oldItem = this._items[position];
        if (oldItem)
            this._itemBox.remove_actor(oldItem);

        this._items[position] = item;
        this._itemBox.add_actor(item);

        menuItem.connect('activate',
                         Lang.bind(this, this._itemActivated, position));
    },

    checkAccessibleLabel: function() {
        let activeItem = this._menu.getActiveItem();
        this.actor.label_actor = activeItem.label;
    },

    setActiveItem: function(position) {
        let item = this._items[position];
        if (!item)
            return;
        if (this._activeItemPos == position)
            return;
        this._menu.setActiveItem(position);
        this._activeItemPos = position;
        for (let i = 0; i < this._items.length; i++)
            this._items[i].visible = (i == this._activeItemPos);

        this.checkAccessibleLabel();
    },

    setItemVisible: function(position, visible) {
        this._menu.setItemVisible(position, visible);
    },

    _itemActivated: function(menuItem, event, position) {
        this.setActiveItem(position);
        this.emit('active-item-changed', position);
    }
});

/* Basic implementation of a menu manager.
 * Call addMenu to add menus
 */
const PopupMenuManager = new Lang.Class({
    Name: 'PopupMenuManager',

    _init: function(owner, grabParams) {
        this._owner = owner;
        this._grabHelper = new GrabHelper.GrabHelper(owner.actor, grabParams);
        this._menus = [];
    },

    addMenu: function(menu, position) {
        if (this._findMenu(menu) > -1)
            return;

        let menudata = {
            menu:              menu,
            openStateChangeId: menu.connect('open-state-changed', Lang.bind(this, this._onMenuOpenState)),
            childMenuAddedId:  menu.connect('child-menu-added', Lang.bind(this, this._onChildMenuAdded)),
            childMenuRemovedId: menu.connect('child-menu-removed', Lang.bind(this, this._onChildMenuRemoved)),
            destroyId:         menu.connect('destroy', Lang.bind(this, this._onMenuDestroy)),
            enterId:           0,
            focusInId:         0
        };

        let source = menu.sourceActor;
        if (source) {
            if (!menu.blockSourceEvents)
                this._grabHelper.addActor(source);
            menudata.enterId = source.connect('enter-event', Lang.bind(this, function() { this._onMenuSourceEnter(menu); }));
            menudata.focusInId = source.connect('key-focus-in', Lang.bind(this, function() { this._onMenuSourceEnter(menu); }));
        }

        if (position == undefined)
            this._menus.push(menudata);
        else
            this._menus.splice(position, 0, menudata);
    },

    removeMenu: function(menu) {
        if (menu == this.activeMenu)
            this._closeMenu(menu);

        let position = this._findMenu(menu);
        if (position == -1) // not a menu we manage
            return;

        let menudata = this._menus[position];
        menu.disconnect(menudata.openStateChangeId);
        menu.disconnect(menudata.childMenuAddedId);
        menu.disconnect(menudata.childMenuRemovedId);
        menu.disconnect(menudata.destroyId);

        if (menudata.enterId)
            menu.sourceActor.disconnect(menudata.enterId);
        if (menudata.focusInId)
            menu.sourceActor.disconnect(menudata.focusInId);

        if (menu.sourceActor)
            this._grabHelper.removeActor(menu.sourceActor);
        this._menus.splice(position, 1);
    },

    get activeMenu() {
        let firstGrab = this._grabHelper.grabStack[0];
        if (firstGrab)
            return firstGrab.actor._delegate;
        else
            return null;
    },

    ignoreRelease: function() {
        return this._grabHelper.ignoreRelease();
    },

    _onMenuOpenState: function(menu, open) {
        if (open) {
            if (this.activeMenu && !this.activeMenu.isChildMenu(menu))
                this.activeMenu.close(BoxPointer.PopupAnimation.FADE);
            this._grabHelper.grab({ actor: menu.actor, modal: true, focus: menu.sourceActor,
                                    onUngrab: Lang.bind(this, this._closeMenu, menu) });
        } else {
            this._grabHelper.ungrab({ actor: menu.actor });
        }
    },

    _onChildMenuAdded: function(menu, childMenu) {
        this.addMenu(childMenu);
    },

    _onChildMenuRemoved: function(menu, childMenu) {
        this.removeMenu(childMenu);
    },

    _changeMenu: function(newMenu) {
        newMenu.open(this.activeMenu ? BoxPointer.PopupAnimation.FADE
                                     : BoxPointer.PopupAnimation.FULL);
    },

    _onMenuSourceEnter: function(menu) {
        if (!this._grabHelper.grabbed)
            return false;

        if (this._grabHelper.isActorGrabbed(menu.actor))
            return false;

        let isChildMenu = this._grabHelper.grabStack.some(function(grab) {
            let existingMenu = grab.actor._delegate;
            return existingMenu.isChildMenu(menu);
        });
        if (isChildMenu)
            return false;

        this._changeMenu(menu);
        return false;
    },

    _onMenuDestroy: function(menu) {
        this.removeMenu(menu);
    },

    _findMenu: function(item) {
        for (let i = 0; i < this._menus.length; i++) {
            let menudata = this._menus[i];
            if (item == menudata.menu)
                return i;
        }
        return -1;
    },

    _closeMenu: function(isUser, menu) {
        // If this isn't a user action, we called close()
        // on the BoxPointer ourselves, so we shouldn't
        // reanimate.
        if (isUser)
            menu.close(BoxPointer.PopupAnimation.FULL);
    }
});
