// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Gio = imports.gi.Gio;
const GObject = imports.gi.GObject;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const Atk = imports.gi.Atk;

const BoxPointer = imports.ui.boxpointer;
const GrabHelper = imports.ui.grabHelper;
const Main = imports.ui.main;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;

var Ornament = {
    NONE: 0,
    DOT: 1,
    CHECK: 2,
};

function isPopupMenuItemVisible(child) {
    if (child._delegate instanceof PopupMenuSection)
        if (child._delegate.isEmpty())
            return false;
    return child.visible;
}

/**
 * @side Side to which the arrow points.
 */
function arrowIcon(side) {
    let iconName;
    switch (side) {
        case St.Side.TOP:
            iconName = 'pan-up-symbolic';
            break;
        case St.Side.RIGHT:
            iconName = 'pan-end-symbolic';
            break;
        case St.Side.BOTTOM:
            iconName = 'pan-down-symbolic';
            break;
        case St.Side.LEFT:
            iconName = 'pan-start-symbolic';
            break;
    }

    let arrow = new St.Icon({ style_class: 'popup-menu-arrow',
                              icon_name: iconName,
                              accessible_role: Atk.Role.ARROW,
                              y_expand: true,
                              y_align: Clutter.ActorAlign.CENTER });

    return arrow;
}

var PopupBaseMenuItem = new Lang.Class({
    Name: 'PopupBaseMenuItem',

    _init(params) {
        params = Params.parse (params, { reactive: true,
                                         activate: true,
                                         hover: true,
                                         style_class: null,
                                         can_focus: true
                                       });

        this.actor = new St.BoxLayout({ style_class: 'popup-menu-item',
                                        reactive: params.reactive,
                                        track_hover: params.reactive,
                                        can_focus: params.can_focus,
                                        accessible_role: Atk.Role.MENU_ITEM });
        this.actor._delegate = this;

        this._ornament = Ornament.NONE;
        this._ornamentLabel = new St.Label({ style_class: 'popup-menu-ornament' });
        this.actor.add(this._ornamentLabel);

        this._parent = null;
        this.active = false;
        this._activatable = params.reactive && params.activate;
        this._sensitive = true;

        if (!this._activatable)
            this.actor.add_style_class_name('popup-inactive-menu-item');

        if (params.style_class)
            this.actor.add_style_class_name(params.style_class);

        if (this._activatable) {
            this.actor.connect('button-press-event', this._onButtonPressEvent.bind(this));
            this.actor.connect('button-release-event', this._onButtonReleaseEvent.bind(this));
            this.actor.connect('touch-event', this._onTouchEvent.bind(this));
            this.actor.connect('key-press-event', this._onKeyPressEvent.bind(this));
        }
        if (params.reactive && params.hover)
            this.actor.connect('notify::hover', this._onHoverChanged.bind(this));

        this.actor.connect('key-focus-in', this._onKeyFocusIn.bind(this));
        this.actor.connect('key-focus-out', this._onKeyFocusOut.bind(this));
        this.actor.connect('destroy', this._onDestroy.bind(this));
    },

    _getTopMenu() {
        if (this._parent)
            return this._parent._getTopMenu();
        else
            return this;
    },

    _setParent(parent) {
        this._parent = parent;
    },

    _onButtonPressEvent(actor, event) {
        // This is the CSS active state
        this.actor.add_style_pseudo_class ('active');
        return Clutter.EVENT_PROPAGATE;
    },

    _onButtonReleaseEvent(actor, event) {
        this.actor.remove_style_pseudo_class ('active');
        this.activate(event);
        return Clutter.EVENT_STOP;
    },

    _onTouchEvent(actor, event) {
        if (event.type() == Clutter.EventType.TOUCH_END) {
            this.actor.remove_style_pseudo_class ('active');
            this.activate(event);
            return Clutter.EVENT_STOP;
        } else if (event.type() == Clutter.EventType.TOUCH_BEGIN) {
            // This is the CSS active state
            this.actor.add_style_pseudo_class ('active');
        }
        return Clutter.EVENT_PROPAGATE;
    },

    _onKeyPressEvent(actor, event) {
        let state = event.get_state();

        // if user has a modifier down (except capslock and numlock)
        // then don't handle the key press here
        state &= ~Clutter.ModifierType.LOCK_MASK;
        state &= ~Clutter.ModifierType.MOD2_MASK;
        state &= Clutter.ModifierType.MODIFIER_MASK;

        if (state)
            return Clutter.EVENT_PROPAGATE;

        let symbol = event.get_key_symbol();
        if (symbol == Clutter.KEY_space || symbol == Clutter.KEY_Return) {
            this.activate(event);
            return Clutter.EVENT_STOP;
        }
        return Clutter.EVENT_PROPAGATE;
    },

    _onKeyFocusIn(actor) {
        this.setActive(true);
    },

    _onKeyFocusOut(actor) {
        this.setActive(false);
    },

    _onHoverChanged(actor) {
        this.setActive(actor.hover);
    },

    activate(event) {
        this.emit('activate', event);
    },

    setActive(active) {
        let activeChanged = active != this.active;
        if (activeChanged) {
            this.active = active;
            if (active) {
                this.actor.add_style_class_name('selected');
                this.actor.grab_key_focus();
            } else {
                this.actor.remove_style_class_name('selected');
                // Remove the CSS active state if the user press the button and
                // while holding moves to another menu item, so we don't paint all items.
                // The correct behaviour would be to set the new item with the CSS
                // active state as well, but button-press-event is not trigered,
                // so we should track it in our own, which would involve some work
                // in the container
                this.actor.remove_style_pseudo_class ('active');
            }
            this.emit('active-changed', active);
        }
    },

    syncSensitive() {
        let sensitive = this.getSensitive();
        this.actor.reactive = sensitive;
        this.actor.can_focus = sensitive;
        this.emit('sensitive-changed');
        return sensitive;
    },

    getSensitive() {
        let parentSensitive = this._parent ? this._parent.getSensitive() : true;
        return this._activatable && this._sensitive && parentSensitive;
    },

    setSensitive(sensitive) {
        if (this._sensitive == sensitive)
            return;

        this._sensitive = sensitive;
        this.syncSensitive();
    },

    destroy() {
        this.actor.destroy();
    },

    _onDestroy() {
        this.emit('destroy');
    },

    setOrnament(ornament) {
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
    }
});
Signals.addSignalMethods(PopupBaseMenuItem.prototype);

var PopupMenuItem = new Lang.Class({
    Name: 'PopupMenuItem',
    Extends: PopupBaseMenuItem,

    _init(text, params) {
        this.parent(params);

        this.label = new St.Label({ text: text });
        this.actor.add_child(this.label);
        this.actor.label_actor = this.label
    }
});

var PopupSeparatorMenuItem = new Lang.Class({
    Name: 'PopupSeparatorMenuItem',
    Extends: PopupBaseMenuItem,

    _init(text) {
        this.parent({ reactive: false,
                      can_focus: false});

        this.label = new St.Label({ text: text || '' });
        this.actor.add(this.label);
        this.actor.label_actor = this.label;

        this.label.connect('notify::text',
                           this._syncVisibility.bind(this));
        this._syncVisibility();

        this._separator = new St.Widget({ style_class: 'popup-separator-menu-item',
                                          y_expand: true,
                                          y_align: Clutter.ActorAlign.CENTER });
        this.actor.add(this._separator, { expand: true });
    },

    _syncVisibility() {
        this.label.visible = this.label.text != '';
    }
});

var Switch = new Lang.Class({
    Name: 'Switch',

    _init(state) {
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

    setToggleState(state) {
        if (state)
            this.actor.add_style_pseudo_class('checked');
        else
            this.actor.remove_style_pseudo_class('checked');
        this.state = state;
    },

    toggle() {
        this.setToggleState(!this.state);
    }
});

var PopupSwitchMenuItem = new Lang.Class({
    Name: 'PopupSwitchMenuItem',
    Extends: PopupBaseMenuItem,

    _init(text, active, params) {
        this.parent(params);

        this.label = new St.Label({ text: text });
        this._switch = new Switch(active);

        this.actor.accessible_role = Atk.Role.CHECK_MENU_ITEM;
        this.checkAccessibleState();
        this.actor.label_actor = this.label;

        this.actor.add_child(this.label);

        this._statusBin = new St.Bin({ x_align: St.Align.END });
        this.actor.add(this._statusBin, { expand: true, x_align: St.Align.END });

        this._statusLabel = new St.Label({ text: '',
                                           style_class: 'popup-status-menu-item'
                                         });
        this._statusBin.child = this._switch.actor;
    },

    setStatus(text) {
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

    activate(event) {
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

    toggle() {
        this._switch.toggle();
        this.emit('toggled', this._switch.state);
        this.checkAccessibleState();
    },

    get state() {
        return this._switch.state;
    },

    setToggleState(state) {
        this._switch.setToggleState(state);
        this.checkAccessibleState();
    },

    checkAccessibleState() {
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

var PopupImageMenuItem = new Lang.Class({
    Name: 'PopupImageMenuItem',
    Extends: PopupBaseMenuItem,

    _init(text, icon, params) {
        this.parent(params);

        this._icon = new St.Icon({ style_class: 'popup-menu-icon',
                                   x_align: Clutter.ActorAlign.END });
        this.actor.add_child(this._icon);
        this.label = new St.Label({ text: text });
        this.actor.add_child(this.label);
        this.actor.label_actor = this.label;

        this.setIcon(icon);
    },

    setIcon(icon) {
        // The 'icon' parameter can be either a Gio.Icon or a string.
        if (icon instanceof GObject.Object && GObject.type_is_a(icon, Gio.Icon))
            this._icon.gicon = icon;
        else
            this._icon.icon_name = icon;
    }
});

var PopupMenuBase = new Lang.Class({
    Name: 'PopupMenuBase',
    Abstract: true,

    _init(sourceActor, styleClass) {
        this.sourceActor = sourceActor;
        this._parent = null;

        if (styleClass !== undefined) {
            this.box = new St.BoxLayout({ style_class: styleClass,
                                          vertical: true });
        } else {
            this.box = new St.BoxLayout({ vertical: true });
        }
        this.length = 0;

        this.isOpen = false;

        // If set, we don't send events (including crossing events) to the source actor
        // for the menu which causes its prelight state to freeze
        this.blockSourceEvents = false;

        this._activeMenuItem = null;
        this._settingsActions = { };

        this._sensitive = true;

        this._sessionUpdatedId = Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
    },

    _getTopMenu() {
        if (this._parent)
            return this._parent._getTopMenu();
        else
            return this;
    },

    _setParent(parent) {
        this._parent = parent;
    },

    getSensitive() {
        let parentSensitive = this._parent ? this._parent.getSensitive() : true;
        return this._sensitive && parentSensitive;
    },

    setSensitive(sensitive) {
        this._sensitive = sensitive;
        this.emit('sensitive-changed');
    },

    _sessionUpdated() {
        this._setSettingsVisibility(Main.sessionMode.allowSettings);
        this.close();
    },

    addAction(title, callback, icon) {
        let menuItem;
        if (icon != undefined)
            menuItem = new PopupImageMenuItem(title, icon);
        else
            menuItem = new PopupMenuItem(title);

        this.addMenuItem(menuItem);
        menuItem.connect('activate', (menuItem, event) => {
            callback(event);
        });

        return menuItem;
    },

    addSettingsAction(title, desktopFile) {
        let menuItem = this.addAction(title, () => {
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

    _setSettingsVisibility(visible) {
        for (let id in this._settingsActions) {
            let item = this._settingsActions[id];
            item.actor.visible = visible;
        }
    },

    isEmpty() {
        let hasVisibleChildren = this.box.get_children().some(child => {
            if (child._delegate instanceof PopupSeparatorMenuItem)
                return false;
            return isPopupMenuItemVisible(child);
        });

        return !hasVisibleChildren;
    },

    itemActivated(animate) {
        if (animate == undefined)
            animate = BoxPointer.PopupAnimation.FULL;

        this._getTopMenu().close(animate);
    },

    _subMenuActiveChanged(submenu, submenuItem) {
        if (this._activeMenuItem && this._activeMenuItem != submenuItem)
            this._activeMenuItem.setActive(false);
        this._activeMenuItem = submenuItem;
        this.emit('active-changed', submenuItem);
    },

    _connectItemSignals(menuItem) {
        menuItem._activeChangeId = menuItem.connect('active-changed', (menuItem, active) => {
            if (active && this._activeMenuItem != menuItem) {
                if (this._activeMenuItem)
                    this._activeMenuItem.setActive(false);
                this._activeMenuItem = menuItem;
                this.emit('active-changed', menuItem);
            } else if (!active && this._activeMenuItem == menuItem) {
                this._activeMenuItem = null;
                this.emit('active-changed', null);
            }
        });
        menuItem._sensitiveChangeId = menuItem.connect('sensitive-changed', () => {
            let sensitive = menuItem.getSensitive();
            if (!sensitive && this._activeMenuItem == menuItem) {
                if (!this.actor.navigate_focus(menuItem.actor,
                                               Gtk.DirectionType.TAB_FORWARD,
                                               true))
                    this.actor.grab_key_focus();
            } else if (sensitive && this._activeMenuItem == null) {
                if (global.stage.get_key_focus() == this.actor)
                    menuItem.actor.grab_key_focus();
            }
        });
        menuItem._activateId = menuItem.connect('activate', (menuItem, event) => {
            this.emit('activate', menuItem);
            this.itemActivated(BoxPointer.PopupAnimation.FULL);
        });

        menuItem._parentSensitiveChangeId = this.connect('sensitive-changed', () => {
            menuItem.syncSensitive();
        });

        // the weird name is to avoid a conflict with some random property
        // the menuItem may have, called destroyId
        // (FIXME: in the future it may make sense to have container objects
        // like PopupMenuManager does)
        menuItem._popupMenuDestroyId = menuItem.connect('destroy', menuItem => {
            menuItem.disconnect(menuItem._popupMenuDestroyId);
            menuItem.disconnect(menuItem._activateId);
            menuItem.disconnect(menuItem._activeChangeId);
            menuItem.disconnect(menuItem._sensitiveChangeId);
            this.disconnect(menuItem._parentSensitiveChangeId);
            if (menuItem == this._activeMenuItem)
                this._activeMenuItem = null;
        });
    },

    _updateSeparatorVisibility(menuItem) {
        if (menuItem.label.text)
            return;

        let children = this.box.get_children();

        let index = children.indexOf(menuItem.actor);

        if (index < 0)
            return;

        let childBeforeIndex = index - 1;

        while (childBeforeIndex >= 0 && !isPopupMenuItemVisible(children[childBeforeIndex]))
            childBeforeIndex--;

        if (childBeforeIndex < 0
            || children[childBeforeIndex]._delegate instanceof PopupSeparatorMenuItem) {
            menuItem.actor.hide();
            return;
        }

        let childAfterIndex = index + 1;

        while (childAfterIndex < children.length && !isPopupMenuItemVisible(children[childAfterIndex]))
            childAfterIndex++;

        if (childAfterIndex >= children.length
            || children[childAfterIndex]._delegate instanceof PopupSeparatorMenuItem) {
            menuItem.actor.hide();
            return;
        }

        menuItem.actor.show();
    },

    moveMenuItem(menuItem, position) {
        let items = this._getMenuItems();
        let i = 0;

        while (i < items.length && position > 0) {
                if (items[i] != menuItem)
                        position--;
                i++;
        }

        if (i < items.length) {
                if (items[i] != menuItem)
                        this.box.set_child_below_sibling(menuItem.actor, items[i].actor);
        } else {
                this.box.set_child_above_sibling(menuItem.actor, null);
        }
    },

    addMenuItem(menuItem, position) {
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
            let activeChangeId = menuItem.connect('active-changed', this._subMenuActiveChanged.bind(this));

            let parentOpenStateChangedId = this.connect('open-state-changed', (self, open) => {
                if (open)
                    menuItem.open();
                else
                    menuItem.close();
            });
            let parentClosingId = this.connect('menu-closed', () => {
                menuItem.emit('menu-closed');
            });
            let subMenuSensitiveChangedId = this.connect('sensitive-changed', () => {
                menuItem.emit('sensitive-changed');
            });

            menuItem.connect('destroy', () => {
                menuItem.disconnect(activeChangeId);
                this.disconnect(subMenuSensitiveChangedId);
                this.disconnect(parentOpenStateChangedId);
                this.disconnect(parentClosingId);
                this.length--;
            });
        } else if (menuItem instanceof PopupSubMenuMenuItem) {
            if (before_item == null)
                this.box.add(menuItem.menu.actor);
            else
                this.box.insert_child_below(menuItem.menu.actor, before_item);

            this._connectItemSignals(menuItem);
            let subMenuActiveChangeId = menuItem.menu.connect('active-changed', this._subMenuActiveChanged.bind(this));
            let closingId = this.connect('menu-closed', () => {
                menuItem.menu.close(BoxPointer.PopupAnimation.NONE);
            });

            menuItem.connect('destroy', () => {
                menuItem.menu.disconnect(subMenuActiveChangeId);
                this.disconnect(closingId);
            });
        } else if (menuItem instanceof PopupSeparatorMenuItem) {
            this._connectItemSignals(menuItem);

            // updateSeparatorVisibility needs to get called any time the
            // separator's adjacent siblings change visibility or position.
            // open-state-changed isn't exactly that, but doing it in more
            // precise ways would require a lot more bookkeeping.
            let openStateChangeId = this.connect('open-state-changed', () => {
                this._updateSeparatorVisibility(menuItem);
            });
            let destroyId = menuItem.connect('destroy', () => {
                this.disconnect(openStateChangeId);
                menuItem.disconnect(destroyId);
            });
        } else if (menuItem instanceof PopupBaseMenuItem)
            this._connectItemSignals(menuItem);
        else
            throw TypeError("Invalid argument to PopupMenuBase.addMenuItem()");

        menuItem._setParent(this);

        this.length++;
    },

    _getMenuItems() {
        return this.box.get_children().map(a => a._delegate).filter(item => {
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

    removeAll() {
        let children = this._getMenuItems();
        for (let i = 0; i < children.length; i++) {
            let item = children[i];
            item.destroy();
        }
    },

    toggle() {
        if (this.isOpen)
            this.close(BoxPointer.PopupAnimation.FULL);
        else
            this.open(BoxPointer.PopupAnimation.FULL);
    },

    destroy() {
        this.close();
        this.removeAll();
        this.actor.destroy();

        this.emit('destroy');

        Main.sessionMode.disconnect(this._sessionUpdatedId);
        this._sessionUpdatedId = 0;
    }
});
Signals.addSignalMethods(PopupMenuBase.prototype);

var PopupMenu = new Lang.Class({
    Name: 'PopupMenu',
    Extends: PopupMenuBase,

    _init(sourceActor, arrowAlignment, arrowSide) {
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

        this._boxPointer.bin.set_child(this.box);
        this.actor.add_style_class_name('popup-menu');

        global.focus_manager.add_group(this.actor);
        this.actor.reactive = true;

        if (this.sourceActor)
            this._keyPressId = this.sourceActor.connect('key-press-event',
                                                        this._onKeyPress.bind(this));

        this._openedSubMenu = null;
    },

    _setOpenedSubMenu(submenu) {
        if (this._openedSubMenu)
            this._openedSubMenu.close(true);

        this._openedSubMenu = submenu;
    },

    _onKeyPress(actor, event) {
        // Disable toggling the menu by keyboard
        // when it cannot be toggled by pointer
        if (!actor.reactive)
            return Clutter.EVENT_PROPAGATE;

        let navKey;
        switch (this._boxPointer.arrowSide) {
            case St.Side.TOP:
                navKey = Clutter.KEY_Down;
                break;
            case St.Side.BOTTOM:
                navKey = Clutter.KEY_Up;
                break;
            case St.Side.LEFT:
                navKey = Clutter.KEY_Right;
                break;
            case St.Side.RIGHT:
                navKey = Clutter.KEY_Left;
                break;
        }

        let state = event.get_state();

        // if user has a modifier down (except capslock)
        // then don't handle the key press here
        state &= ~Clutter.ModifierType.LOCK_MASK;
        state &= Clutter.ModifierType.MODIFIER_MASK;

        if (state)
            return Clutter.EVENT_PROPAGATE;

        let symbol = event.get_key_symbol();
        if (symbol == Clutter.KEY_space || symbol == Clutter.KEY_Return) {
            this.toggle();
            return Clutter.EVENT_STOP;
        } else if (symbol == Clutter.KEY_Escape && this.isOpen) {
            this.close();
            return Clutter.EVENT_STOP;
        } else if (symbol == navKey) {
            if (!this.isOpen)
                this.toggle();
            this.actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
            return Clutter.EVENT_STOP;
        } else
            return Clutter.EVENT_PROPAGATE;
    },


    setArrowOrigin(origin) {
        this._boxPointer.setArrowOrigin(origin);
    },

    setSourceAlignment(alignment) {
        this._boxPointer.setSourceAlignment(alignment);
    },

    open(animate) {
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

    close(animate) {
        if (this._activeMenuItem)
            this._activeMenuItem.setActive(false);

        if (this._boxPointer.actor.visible) {
            this._boxPointer.hide(animate, () => {
                this.emit('menu-closed');
            });
        }

        if (!this.isOpen)
            return;

        this.isOpen = false;
        this.emit('open-state-changed', false);
    },

    destroy() {
        if (this._keyPressId)
            this.sourceActor.disconnect(this._keyPressId);
        this.parent();
    }
});

var PopupDummyMenu = new Lang.Class({
    Name: 'PopupDummyMenu',

    _init(sourceActor) {
        this.sourceActor = sourceActor;
        this.actor = sourceActor;
        this.actor._delegate = this;
    },

    getSensitive() {
        return true;
    },

    open() { this.emit('open-state-changed', true); },
    close() { this.emit('open-state-changed', false); },
    toggle() {},
    destroy() {
        this.emit('destroy');
    },
});
Signals.addSignalMethods(PopupDummyMenu.prototype);

var PopupSubMenu = new Lang.Class({
    Name: 'PopupSubMenu',
    Extends: PopupMenuBase,

    _init(sourceActor, sourceArrow) {
        this.parent(sourceActor);

        this._arrow = sourceArrow;

        // Since a function of a submenu might be to provide a "More.." expander
        // with long content, we make it scrollable - the scrollbar will only take
        // effect if a CSS max-height is set on the top menu.
        this.actor = new St.ScrollView({ style_class: 'popup-sub-menu',
                                         hscrollbar_policy: Gtk.PolicyType.NEVER,
                                         vscrollbar_policy: Gtk.PolicyType.NEVER });

        this.actor.add_actor(this.box);
        this.actor._delegate = this;
        this.actor.clip_to_allocation = true;
        this.actor.connect('key-press-event', this._onKeyPressEvent.bind(this));
        this.actor.hide();
    },

    _needsScrollbar() {
        let topMenu = this._getTopMenu();
        let [topMinHeight, topNaturalHeight] = topMenu.actor.get_preferred_height(-1);
        let topThemeNode = topMenu.actor.get_theme_node();

        let topMaxHeight = topThemeNode.get_max_height();
        return topMaxHeight >= 0 && topNaturalHeight >= topMaxHeight;
    },

    getSensitive() {
        return this._sensitive && this.sourceActor._delegate.getSensitive();
    },

    open(animate) {
        if (this.isOpen)
            return;

        if (this.isEmpty())
            return;

        this.isOpen = true;
        this.emit('open-state-changed', true);

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

        let targetAngle = this.actor.text_direction == Clutter.TextDirection.RTL ? -90 : 90;

        if (animate) {
            let [minHeight, naturalHeight] = this.actor.get_preferred_height(-1);
            this.actor.height = 0;
            this.actor._arrowRotation = this._arrow.rotation_angle_z;
            Tweener.addTween(this.actor,
                             { _arrowRotation: targetAngle,
                               height: naturalHeight,
                               time: 0.25,
                               onUpdateScope: this,
                               onUpdate() {
                                   this._arrow.rotation_angle_z = this.actor._arrowRotation;
                               },
                               onCompleteScope: this,
                               onComplete() {
                                   this.actor.set_height(-1);
                               }
                             });
        } else {
            this._arrow.rotation_angle_z = targetAngle;
        }
    },

    close(animate) {
        if (!this.isOpen)
            return;

        this.isOpen = false;
        this.emit('open-state-changed', false);

        if (this._activeMenuItem)
            this._activeMenuItem.setActive(false);

        if (animate && this._needsScrollbar())
            animate = false;

        if (animate) {
            this.actor._arrowRotation = this._arrow.rotation_angle_z;
            Tweener.addTween(this.actor,
                             { _arrowRotation: 0,
                               height: 0,
                               time: 0.25,
                               onUpdateScope: this,
                               onUpdate() {
                                   this._arrow.rotation_angle_z = this.actor._arrowRotation;
                               },
                               onCompleteScope: this,
                               onComplete() {
                                   this.actor.hide();
                                   this.actor.set_height(-1);
                               },
                             });
        } else {
            this._arrow.rotation_angle_z = 0;
            this.actor.hide();
        }
    },

    _onKeyPressEvent(actor, event) {
        // Move focus back to parent menu if the user types Left.

        if (this.isOpen && event.get_key_symbol() == Clutter.KEY_Left) {
            this.close(BoxPointer.PopupAnimation.FULL);
            this.sourceActor._delegate.setActive(true);
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
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
var PopupMenuSection = new Lang.Class({
    Name: 'PopupMenuSection',
    Extends: PopupMenuBase,

    _init() {
        this.parent();

        this.actor = this.box;
        this.actor._delegate = this;
        this.isOpen = true;
    },

    // deliberately ignore any attempt to open() or close(), but emit the
    // corresponding signal so children can still pick it up
    open() { this.emit('open-state-changed', true); },
    close() { this.emit('open-state-changed', false); },
});

var PopupSubMenuMenuItem = new Lang.Class({
    Name: 'PopupSubMenuMenuItem',
    Extends: PopupBaseMenuItem,

    _init(text, wantIcon) {
        this.parent();

        this.actor.add_style_class_name('popup-submenu-menu-item');

        if (wantIcon) {
            this.icon = new St.Icon({ style_class: 'popup-menu-icon' });
            this.actor.add_child(this.icon);
        }

        this.label = new St.Label({ text: text,
                                    y_expand: true,
                                    y_align: Clutter.ActorAlign.CENTER });
        this.actor.add_child(this.label);
        this.actor.label_actor = this.label;

        let expander = new St.Bin({ style_class: 'popup-menu-item-expander' });
        this.actor.add(expander, { expand: true });

        this._triangle = arrowIcon(St.Side.RIGHT);
        this._triangle.pivot_point = new Clutter.Point({ x: 0.5, y: 0.6 });

        this._triangleBin = new St.Widget({ y_expand: true,
                                            y_align: Clutter.ActorAlign.CENTER });
        this._triangleBin.add_child(this._triangle);

        this.actor.add_child(this._triangleBin);
        this.actor.add_accessible_state (Atk.StateType.EXPANDABLE);

        this.menu = new PopupSubMenu(this.actor, this._triangle);
        this.menu.connect('open-state-changed', this._subMenuOpenStateChanged.bind(this));
    },

    _setParent(parent) {
        this.parent(parent);
        this.menu._setParent(parent);
    },

    syncSensitive() {
        let sensitive = this.parent();
        this._triangle.visible = sensitive;
        if (!sensitive)
            this.menu.close(false);
    },

    _subMenuOpenStateChanged(menu, open) {
        if (open) {
            this.actor.add_style_pseudo_class('open');
            this._getTopMenu()._setOpenedSubMenu(this.menu);
            this.actor.add_accessible_state (Atk.StateType.EXPANDED);
            this.actor.add_style_pseudo_class('checked');
        } else {
            this.actor.remove_style_pseudo_class('open');
            this._getTopMenu()._setOpenedSubMenu(null);
            this.actor.remove_accessible_state (Atk.StateType.EXPANDED);
            this.actor.remove_style_pseudo_class('checked');
        }
    },

    destroy() {
        this.menu.destroy();

        this.parent();
    },

    setSubmenuShown(open) {
        if (open)
            this.menu.open(BoxPointer.PopupAnimation.FULL);
        else
            this.menu.close(BoxPointer.PopupAnimation.FULL);
    },

    _setOpenState(open) {
        this.setSubmenuShown(open);
    },

    _getOpenState() {
        return this.menu.isOpen;
    },

    _onKeyPressEvent(actor, event) {
        let symbol = event.get_key_symbol();

        if (symbol == Clutter.KEY_Right) {
            this._setOpenState(true);
            this.menu.actor.navigate_focus(null, Gtk.DirectionType.DOWN, false);
            return Clutter.EVENT_STOP;
        } else if (symbol == Clutter.KEY_Left && this._getOpenState()) {
            this._setOpenState(false);
            return Clutter.EVENT_STOP;
        }

        return this.parent(actor, event);
    },

    activate(event) {
        this._setOpenState(true);
    },

    _onButtonReleaseEvent(actor) {
        // Since we override the parent, we need to manage what the parent does
        // with the active style class
        this.actor.remove_style_pseudo_class ('active');
        this._setOpenState(!this._getOpenState());
        return Clutter.EVENT_PROPAGATE;
    },

    _onTouchEvent(actor, event) {
        if (event.type() == Clutter.EventType.TOUCH_END) {
            // Since we override the parent, we need to manage what the parent does
            // with the active style class
            this.actor.remove_style_pseudo_class ('active');
            this._setOpenState(!this._getOpenState());
        }
        return Clutter.EVENT_PROPAGATE;
    }
});

/* Basic implementation of a menu manager.
 * Call addMenu to add menus
 */
var PopupMenuManager = new Lang.Class({
    Name: 'PopupMenuManager',

    _init(owner, grabParams) {
        grabParams = Params.parse(grabParams,
                                  { actionMode: Shell.ActionMode.POPUP });
        this._owner = owner;
        this._grabHelper = new GrabHelper.GrabHelper(owner.actor, grabParams);
        this._menus = [];
    },

    addMenu(menu, position) {
        if (this._findMenu(menu) > -1)
            return;

        let menudata = {
            menu:              menu,
            openStateChangeId: menu.connect('open-state-changed', this._onMenuOpenState.bind(this)),
            destroyId:         menu.connect('destroy', this._onMenuDestroy.bind(this)),
            enterId:           0,
            focusInId:         0
        };

        let source = menu.sourceActor;
        if (source) {
            if (!menu.blockSourceEvents)
                this._grabHelper.addActor(source);
            menudata.enterId = source.connect('enter-event',
                () => this._onMenuSourceEnter(menu));
            menudata.focusInId = source.connect('key-focus-in', () => {
                this._onMenuSourceEnter(menu);
            });
        }

        if (position == undefined)
            this._menus.push(menudata);
        else
            this._menus.splice(position, 0, menudata);
    },

    removeMenu(menu) {
        if (menu == this.activeMenu)
            this._closeMenu(false, menu);

        let position = this._findMenu(menu);
        if (position == -1) // not a menu we manage
            return;

        let menudata = this._menus[position];
        menu.disconnect(menudata.openStateChangeId);
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

    ignoreRelease() {
        return this._grabHelper.ignoreRelease();
    },

    _onMenuOpenState(menu, open) {
        if (open) {
            if (this.activeMenu)
                this.activeMenu.close(BoxPointer.PopupAnimation.FADE);
            this._grabHelper.grab({ actor: menu.actor, focus: menu.sourceActor,
                                    onUngrab: isUser => {
                                        this._closeMenu(isUser, menu);
                                    } });
        } else {
            this._grabHelper.ungrab({ actor: menu.actor });
        }
    },

    _changeMenu(newMenu) {
        newMenu.open(this.activeMenu ? BoxPointer.PopupAnimation.FADE
                                     : BoxPointer.PopupAnimation.FULL);
    },

    _onMenuSourceEnter(menu) {
        if (!this._grabHelper.grabbed)
            return Clutter.EVENT_PROPAGATE;

        if (this._grabHelper.isActorGrabbed(menu.actor))
            return Clutter.EVENT_PROPAGATE;

        this._changeMenu(menu);
        return Clutter.EVENT_PROPAGATE;
    },

    _onMenuDestroy(menu) {
        this.removeMenu(menu);
    },

    _findMenu(item) {
        for (let i = 0; i < this._menus.length; i++) {
            let menudata = this._menus[i];
            if (item == menudata.menu)
                return i;
        }
        return -1;
    },

    _closeMenu(isUser, menu) {
        // If this isn't a user action, we called close()
        // on the BoxPointer ourselves, so we shouldn't
        // reanimate.
        if (isUser)
            menu.close(BoxPointer.PopupAnimation.FULL);
    }
});
