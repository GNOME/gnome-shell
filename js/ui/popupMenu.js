// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported PopupMenuItem, PopupSeparatorMenuItem, Switch, PopupSwitchMenuItem,
            PopupImageMenuItem, PopupMenu, PopupDummyMenu, PopupSubMenu,
            PopupMenuSection, PopupSubMenuMenuItem, PopupMenuManager */

const { Atk, Clutter, Gio, GObject, Graphene, Shell, St } = imports.gi;
const Signals = imports.signals;

const BoxPointer = imports.ui.boxpointer;
const GrabHelper = imports.ui.grabHelper;
const Main = imports.ui.main;
const Params = imports.misc.params;

var Ornament = {
    NONE: 0,
    DOT: 1,
    CHECK: 2,
    HIDDEN: 3,
};

function isPopupMenuItemVisible(child) {
    if (child._delegate instanceof PopupMenuSection) {
        if (child._delegate.isEmpty())
            return false;
    }
    return child.visible;
}

/**
 * arrowIcon
 * @param {St.Side} side - Side to which the arrow points.
 * @returns {St.Icon} a new arrow icon
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

var PopupBaseMenuItem = GObject.registerClass({
    Properties: {
        'active': GObject.ParamSpec.boolean('active', 'active', 'active',
                                            GObject.ParamFlags.READWRITE,
                                            false),
        'sensitive': GObject.ParamSpec.boolean('sensitive', 'sensitive', 'sensitive',
                                               GObject.ParamFlags.READWRITE,
                                               true),
    },
    Signals: {
        'activate': { param_types: [Clutter.Event.$gtype] },
    },
}, class PopupBaseMenuItem extends St.BoxLayout {
    _init(params) {
        params = Params.parse(params, {
            reactive: true,
            activate: true,
            hover: true,
            style_class: null,
            can_focus: true,
        });
        super._init({ style_class: 'popup-menu-item',
                      reactive: params.reactive,
                      track_hover: params.reactive,
                      can_focus: params.can_focus,
                      accessible_role: Atk.Role.MENU_ITEM });
        this._delegate = this;

        this._ornament = Ornament.NONE;
        this._ornamentLabel = new St.Label({ style_class: 'popup-menu-ornament' });
        this.add(this._ornamentLabel);

        this._parent = null;
        this._active = false;
        this._activatable = params.reactive && params.activate;
        this._sensitive = true;

        if (!this._activatable)
            this.add_style_class_name('popup-inactive-menu-item');

        if (params.style_class)
            this.add_style_class_name(params.style_class);

        if (params.reactive && params.hover)
            this.bind_property('hover', this, 'active', GObject.BindingFlags.SYNC_CREATE);
    }

    get actor() {
        /* This is kept for compatibility with current implementation, and we
           don't want to warn here yet since PopupMenu depends on this */
        return this;
    }

    _getTopMenu() {
        if (this._parent)
            return this._parent._getTopMenu();
        else
            return this;
    }

    _setParent(parent) {
        this._parent = parent;
    }

    vfunc_button_press_event() {
        if (!this._activatable)
            return Clutter.EVENT_PROPAGATE;

        // This is the CSS active state
        this.add_style_pseudo_class('active');
        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_button_release_event() {
        if (!this._activatable)
            return Clutter.EVENT_PROPAGATE;

        this.remove_style_pseudo_class('active');
        this.activate(Clutter.get_current_event());
        return Clutter.EVENT_STOP;
    }

    vfunc_touch_event(touchEvent) {
        if (!this._activatable)
            return Clutter.EVENT_PROPAGATE;

        if (touchEvent.type == Clutter.EventType.TOUCH_END) {
            this.remove_style_pseudo_class('active');
            this.activate(Clutter.get_current_event());
            return Clutter.EVENT_STOP;
        } else if (touchEvent.type == Clutter.EventType.TOUCH_BEGIN) {
            // This is the CSS active state
            this.add_style_pseudo_class('active');
        }
        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_key_press_event(keyEvent) {
        if (!this._activatable)
            return super.vfunc_key_press_event(keyEvent);

        let state = keyEvent.modifier_state;

        // if user has a modifier down (except capslock and numlock)
        // then don't handle the key press here
        state &= ~Clutter.ModifierType.LOCK_MASK;
        state &= ~Clutter.ModifierType.MOD2_MASK;
        state &= Clutter.ModifierType.MODIFIER_MASK;

        if (state)
            return Clutter.EVENT_PROPAGATE;

        let symbol = keyEvent.keyval;
        if (symbol == Clutter.KEY_space || symbol == Clutter.KEY_Return) {
            this.activate(Clutter.get_current_event());
            return Clutter.EVENT_STOP;
        }
        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_key_focus_in() {
        super.vfunc_key_focus_in();
        this.active = true;
    }

    vfunc_key_focus_out() {
        super.vfunc_key_focus_out();
        this.active = false;
    }

    activate(event) {
        this.emit('activate', event);
    }

    get active() {
        return this._active;
    }

    set active(active) {
        let activeChanged = active != this.active;
        if (activeChanged) {
            this._active = active;
            if (active) {
                this.add_style_class_name('selected');
                if (this.can_focus)
                    this.grab_key_focus();
            } else {
                this.remove_style_class_name('selected');
                // Remove the CSS active state if the user press the button and
                // while holding moves to another menu item, so we don't paint all items.
                // The correct behaviour would be to set the new item with the CSS
                // active state as well, but button-press-event is not triggered,
                // so we should track it in our own, which would involve some work
                // in the container
                this.remove_style_pseudo_class('active');
            }
            this.notify('active');
        }
    }

    syncSensitive() {
        let sensitive = this.sensitive;
        this.reactive = sensitive;
        this.can_focus = sensitive;
        this.notify('sensitive');
        return sensitive;
    }

    getSensitive() {
        const parentSensitive = this._parent?.sensitive ?? true;
        return this._activatable && this._sensitive && parentSensitive;
    }

    setSensitive(sensitive) {
        if (this._sensitive == sensitive)
            return;

        this._sensitive = sensitive;
        this.syncSensitive();
    }

    get sensitive() {
        return this.getSensitive();
    }

    set sensitive(sensitive) {
        this.setSensitive(sensitive);
    }

    setOrnament(ornament) {
        if (ornament == this._ornament)
            return;

        this._ornament = ornament;

        if (ornament == Ornament.DOT) {
            this._ornamentLabel.text = '\u2022';
            this.add_accessible_state(Atk.StateType.CHECKED);
        } else if (ornament == Ornament.CHECK) {
            this._ornamentLabel.text = '\u2713';
            this.add_accessible_state(Atk.StateType.CHECKED);
        } else if (ornament == Ornament.NONE || ornament == Ornament.HIDDEN) {
            this._ornamentLabel.text = '';
            this.remove_accessible_state(Atk.StateType.CHECKED);
        }

        this._ornamentLabel.visible = ornament != Ornament.HIDDEN;
    }
});

var PopupMenuItem = GObject.registerClass(
class PopupMenuItem extends PopupBaseMenuItem {
    _init(text, params) {
        super._init(params);

        this.label = new St.Label({ text });
        this.add_child(this.label);
        this.label_actor = this.label;
    }
});


var PopupSeparatorMenuItem = GObject.registerClass(
class PopupSeparatorMenuItem extends PopupBaseMenuItem {
    _init(text) {
        super._init({
            style_class: 'popup-separator-menu-item',
            reactive: false,
            can_focus: false,
        });

        this.label = new St.Label({ text: text || '' });
        this.add(this.label);
        this.label_actor = this.label;

        this.label.connect('notify::text',
                           this._syncVisibility.bind(this));
        this._syncVisibility();

        this._separator = new St.Widget({
            style_class: 'popup-separator-menu-item-separator',
            x_expand: true,
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(this._separator);
    }

    _syncVisibility() {
        this.label.visible = this.label.text != '';
    }
});

var Switch = GObject.registerClass({
    Properties: {
        'state': GObject.ParamSpec.boolean(
            'state', 'state', 'state',
            GObject.ParamFlags.READWRITE,
            false),
    },
}, class Switch extends St.Bin {
    _init(state) {
        this._state = false;

        super._init({
            style_class: 'toggle-switch',
            accessible_role: Atk.Role.CHECK_BOX,
            state,
        });
    }

    get state() {
        return this._state;
    }

    set state(state) {
        if (this._state === state)
            return;

        if (state)
            this.add_style_pseudo_class('checked');
        else
            this.remove_style_pseudo_class('checked');

        this._state = state;
        this.notify('state');
    }

    toggle() {
        this.state = !this.state;
    }
});

var PopupSwitchMenuItem = GObject.registerClass({
    Signals: { 'toggled': { param_types: [GObject.TYPE_BOOLEAN] } },
}, class PopupSwitchMenuItem extends PopupBaseMenuItem {
    _init(text, active, params) {
        super._init(params);

        this.label = new St.Label({ text });
        this._switch = new Switch(active);

        this.accessible_role = Atk.Role.CHECK_MENU_ITEM;
        this.checkAccessibleState();
        this.label_actor = this.label;

        this.add_child(this.label);

        this._statusBin = new St.Bin({
            x_align: Clutter.ActorAlign.END,
            x_expand: true,
        });
        this.add_child(this._statusBin);

        this._statusLabel = new St.Label({
            text: '',
            style_class: 'popup-status-menu-item',
        });
        this._statusBin.child = this._switch;
    }

    setStatus(text) {
        if (text != null) {
            this._statusLabel.text = text;
            this._statusBin.child = this._statusLabel;
            this.reactive = false;
            this.accessible_role = Atk.Role.MENU_ITEM;
        } else {
            this._statusBin.child = this._switch;
            this.reactive = true;
            this.accessible_role = Atk.Role.CHECK_MENU_ITEM;
        }
        this.checkAccessibleState();
    }

    activate(event) {
        if (this._switch.mapped)
            this.toggle();

        // we allow pressing space to toggle the switch
        // without closing the menu
        if (event.type() == Clutter.EventType.KEY_PRESS &&
            event.get_key_symbol() == Clutter.KEY_space)
            return;

        super.activate(event);
    }

    toggle() {
        this._switch.toggle();
        this.emit('toggled', this._switch.state);
        this.checkAccessibleState();
    }

    get state() {
        return this._switch.state;
    }

    setToggleState(state) {
        this._switch.state = state;
        this.checkAccessibleState();
    }

    checkAccessibleState() {
        switch (this.accessible_role) {
        case Atk.Role.CHECK_MENU_ITEM:
            if (this._switch.state)
                this.add_accessible_state(Atk.StateType.CHECKED);
            else
                this.remove_accessible_state(Atk.StateType.CHECKED);
            break;
        default:
            this.remove_accessible_state(Atk.StateType.CHECKED);
        }
    }
});

var PopupImageMenuItem = GObject.registerClass(
class PopupImageMenuItem extends PopupBaseMenuItem {
    _init(text, icon, params) {
        super._init(params);

        this._icon = new St.Icon({ style_class: 'popup-menu-icon',
                                   x_align: Clutter.ActorAlign.END });
        this.add_child(this._icon);
        this.label = new St.Label({ text });
        this.add_child(this.label);
        this.label_actor = this.label;

        this.setIcon(icon);
    }

    setIcon(icon) {
        // The 'icon' parameter can be either a Gio.Icon or a string.
        if (icon instanceof GObject.Object && GObject.type_is_a(icon, Gio.Icon))
            this._icon.gicon = icon;
        else
            this._icon.icon_name = icon;
    }
});

var PopupMenuBase = class {
    constructor(sourceActor, styleClass) {
        if (this.constructor === PopupMenuBase)
            throw new TypeError('Cannot instantiate abstract class %s'.format(this.constructor.name));

        this.sourceActor = sourceActor;
        this.focusActor = sourceActor;
        this._parent = null;

        this.box = new St.BoxLayout({
            vertical: true,
            x_expand: true,
            y_expand: true,
        });

        if (styleClass !== undefined)
            this.box.style_class = styleClass;
        this.length = 0;

        this.isOpen = false;

        // If set, we don't send events (including crossing events) to the source actor
        // for the menu which causes its prelight state to freeze
        this.blockSourceEvents = false;

        this._activeMenuItem = null;
        this._settingsActions = { };

        this._sensitive = true;

        this._sessionUpdatedId = Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
    }

    _getTopMenu() {
        if (this._parent)
            return this._parent._getTopMenu();
        else
            return this;
    }

    _setParent(parent) {
        this._parent = parent;
    }

    getSensitive() {
        const parentSensitive = this._parent?.sensitive ?? true;
        return this._sensitive && parentSensitive;
    }

    setSensitive(sensitive) {
        this._sensitive = sensitive;
        this.emit('notify::sensitive');
    }

    get sensitive() {
        return this.getSensitive();
    }

    set sensitive(sensitive) {
        this.setSensitive(sensitive);
    }

    _sessionUpdated() {
        this._setSettingsVisibility(Main.sessionMode.allowSettings);
        this.close();
    }

    addAction(title, callback, icon) {
        let menuItem;
        if (icon != undefined)
            menuItem = new PopupImageMenuItem(title, icon);
        else
            menuItem = new PopupMenuItem(title);

        this.addMenuItem(menuItem);
        menuItem.connect('activate', (o, event) => {
            callback(event);
        });

        return menuItem;
    }

    addSettingsAction(title, desktopFile) {
        let menuItem = this.addAction(title, () => {
            let app = Shell.AppSystem.get_default().lookup_app(desktopFile);

            if (!app) {
                log('Settings panel for desktop file %s could not be loaded!'.format(desktopFile));
                return;
            }

            Main.overview.hide();
            app.activate();
        });

        menuItem.visible = Main.sessionMode.allowSettings;
        this._settingsActions[desktopFile] = menuItem;

        return menuItem;
    }

    _setSettingsVisibility(visible) {
        for (let id in this._settingsActions) {
            let item = this._settingsActions[id];
            item.visible = visible;
        }
    }

    isEmpty() {
        let hasVisibleChildren = this.box.get_children().some(child => {
            if (child._delegate instanceof PopupSeparatorMenuItem)
                return false;
            return isPopupMenuItemVisible(child);
        });

        return !hasVisibleChildren;
    }

    itemActivated(animate) {
        if (animate == undefined)
            animate = BoxPointer.PopupAnimation.FULL;

        this._getTopMenu().close(animate);
    }

    _subMenuActiveChanged(submenu, submenuItem) {
        if (this._activeMenuItem && this._activeMenuItem != submenuItem)
            this._activeMenuItem.active = false;
        this._activeMenuItem = submenuItem;
        this.emit('active-changed', submenuItem);
    }

    _connectItemSignals(menuItem) {
        menuItem._activeChangeId = menuItem.connect('notify::active', () => {
            let active = menuItem.active;
            if (active && this._activeMenuItem != menuItem) {
                if (this._activeMenuItem)
                    this._activeMenuItem.active = false;
                this._activeMenuItem = menuItem;
                this.emit('active-changed', menuItem);
            } else if (!active && this._activeMenuItem == menuItem) {
                this._activeMenuItem = null;
                this.emit('active-changed', null);
            }
        });
        menuItem._sensitiveChangeId = menuItem.connect('notify::sensitive', () => {
            let sensitive = menuItem.sensitive;
            if (!sensitive && this._activeMenuItem == menuItem) {
                if (!this.actor.navigate_focus(menuItem.actor,
                                               St.DirectionType.TAB_FORWARD,
                                               true))
                    this.actor.grab_key_focus();
            } else if (sensitive && this._activeMenuItem == null) {
                if (global.stage.get_key_focus() == this.actor)
                    menuItem.actor.grab_key_focus();
            }
        });
        menuItem._activateId = menuItem.connect_after('activate', () => {
            this.emit('activate', menuItem);
            this.itemActivated(BoxPointer.PopupAnimation.FULL);
        });

        menuItem._parentSensitiveChangeId = this.connect('notify::sensitive', () => {
            menuItem.syncSensitive();
        });

        // the weird name is to avoid a conflict with some random property
        // the menuItem may have, called destroyId
        // (FIXME: in the future it may make sense to have container objects
        // like PopupMenuManager does)
        menuItem._popupMenuDestroyId = menuItem.connect('destroy', () => {
            menuItem.disconnect(menuItem._popupMenuDestroyId);
            menuItem.disconnect(menuItem._activateId);
            menuItem.disconnect(menuItem._activeChangeId);
            menuItem.disconnect(menuItem._sensitiveChangeId);
            this.disconnect(menuItem._parentSensitiveChangeId);
            if (menuItem == this._activeMenuItem)
                this._activeMenuItem = null;
        });
    }

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

        if (childBeforeIndex < 0 ||
            children[childBeforeIndex]._delegate instanceof PopupSeparatorMenuItem) {
            menuItem.actor.hide();
            return;
        }

        let childAfterIndex = index + 1;

        while (childAfterIndex < children.length && !isPopupMenuItemVisible(children[childAfterIndex]))
            childAfterIndex++;

        if (childAfterIndex >= children.length ||
            children[childAfterIndex]._delegate instanceof PopupSeparatorMenuItem) {
            menuItem.actor.hide();
            return;
        }

        menuItem.show();
    }

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
    }

    addMenuItem(menuItem, position) {
        let beforeItem = null;
        if (position == undefined) {
            this.box.add(menuItem.actor);
        } else {
            let items = this._getMenuItems();
            if (position < items.length) {
                beforeItem = items[position].actor;
                this.box.insert_child_below(menuItem.actor, beforeItem);
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
            let subMenuSensitiveChangedId = this.connect('notify::sensitive', () => {
                menuItem.emit('notify::sensitive');
            });

            menuItem.connect('destroy', () => {
                menuItem.disconnect(activeChangeId);
                this.disconnect(subMenuSensitiveChangedId);
                this.disconnect(parentOpenStateChangedId);
                this.disconnect(parentClosingId);
                this.length--;
            });
        } else if (menuItem instanceof PopupSubMenuMenuItem) {
            if (beforeItem == null)
                this.box.add(menuItem.menu.actor);
            else
                this.box.insert_child_below(menuItem.menu.actor, beforeItem);

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
        } else if (menuItem instanceof PopupBaseMenuItem) {
            this._connectItemSignals(menuItem);
        } else {
            throw TypeError("Invalid argument to PopupMenuBase.addMenuItem()");
        }

        menuItem._setParent(this);

        this.length++;
    }

    _getMenuItems() {
        return this.box.get_children().map(a => a._delegate).filter(item => {
            return item instanceof PopupBaseMenuItem || item instanceof PopupMenuSection;
        });
    }

    get firstMenuItem() {
        let items = this._getMenuItems();
        if (items.length)
            return items[0];
        else
            return null;
    }

    get numMenuItems() {
        return this._getMenuItems().length;
    }

    removeAll() {
        let children = this._getMenuItems();
        for (let i = 0; i < children.length; i++) {
            let item = children[i];
            item.destroy();
        }
    }

    toggle() {
        if (this.isOpen)
            this.close(BoxPointer.PopupAnimation.FULL);
        else
            this.open(BoxPointer.PopupAnimation.FULL);
    }

    destroy() {
        this.close();
        this.removeAll();
        this.actor.destroy();

        this.emit('destroy');

        Main.sessionMode.disconnect(this._sessionUpdatedId);
        this._sessionUpdatedId = 0;
    }
};
Signals.addSignalMethods(PopupMenuBase.prototype);

var PopupMenu = class extends PopupMenuBase {
    constructor(sourceActor, arrowAlignment, arrowSide) {
        super(sourceActor, 'popup-menu-content');

        this._arrowAlignment = arrowAlignment;
        this._arrowSide = arrowSide;

        this._boxPointer = new BoxPointer.BoxPointer(arrowSide);
        this.actor = this._boxPointer;
        this.actor._delegate = this;
        this.actor.style_class = 'popup-menu-boxpointer';

        this._boxPointer.bin.set_child(this.box);
        this.actor.add_style_class_name('popup-menu');

        global.focus_manager.add_group(this.actor);
        this.actor.reactive = true;

        if (this.sourceActor) {
            this._keyPressId = this.sourceActor.connect('key-press-event',
                this._onKeyPress.bind(this));
            this._notifyMappedId = this.sourceActor.connect('notify::mapped',
                () => {
                    if (!this.sourceActor.mapped)
                        this.close();
                });
        }

        this._systemModalOpenedId = 0;
        this._openedSubMenu = null;
    }

    _setOpenedSubMenu(submenu) {
        if (this._openedSubMenu)
            this._openedSubMenu.close(true);

        this._openedSubMenu = submenu;
    }

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

        // if user has a modifier down (except capslock and numlock)
        // then don't handle the key press here
        state &= ~Clutter.ModifierType.LOCK_MASK;
        state &= ~Clutter.ModifierType.MOD2_MASK;
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
            this.actor.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
            return Clutter.EVENT_STOP;
        } else {
            return Clutter.EVENT_PROPAGATE;
        }
    }

    setArrowOrigin(origin) {
        this._boxPointer.setArrowOrigin(origin);
    }

    setSourceAlignment(alignment) {
        this._boxPointer.setSourceAlignment(alignment);
    }

    open(animate) {
        if (this.isOpen)
            return;

        if (this.isEmpty())
            return;

        if (!this._systemModalOpenedId) {
            this._systemModalOpenedId =
                Main.layoutManager.connect('system-modal-opened', () => this.close());
        }

        this.isOpen = true;

        this._boxPointer.setPosition(this.sourceActor, this._arrowAlignment);
        this._boxPointer.open(animate);

        this.actor.get_parent().set_child_above_sibling(this.actor, null);

        this.emit('open-state-changed', true);
    }

    close(animate) {
        if (this._activeMenuItem)
            this._activeMenuItem.active = false;

        if (this._boxPointer.visible) {
            this._boxPointer.close(animate, () => {
                this.emit('menu-closed');
            });
        }

        if (!this.isOpen)
            return;

        this.isOpen = false;
        this.emit('open-state-changed', false);
    }

    destroy() {
        if (this._keyPressId)
            this.sourceActor.disconnect(this._keyPressId);

        if (this._notifyMappedId)
            this.sourceActor.disconnect(this._notifyMappedId);

        if (this._systemModalOpenedId)
            Main.layoutManager.disconnect(this._systemModalOpenedId);
        this._systemModalOpenedId = 0;

        super.destroy();
    }
};

var PopupDummyMenu = class {
    constructor(sourceActor) {
        this.sourceActor = sourceActor;
        this.actor = sourceActor;
        this.actor._delegate = this;
    }

    getSensitive() {
        return true;
    }

    get sensitive() {
        return this.getSensitive();
    }

    open() {
        this.emit('open-state-changed', true);
    }

    close() {
        this.emit('open-state-changed', false);
    }

    toggle() {}

    destroy() {
        this.emit('destroy');
    }
};
Signals.addSignalMethods(PopupDummyMenu.prototype);

var PopupSubMenu = class extends PopupMenuBase {
    constructor(sourceActor, sourceArrow) {
        super(sourceActor);

        this._arrow = sourceArrow;

        // Since a function of a submenu might be to provide a "More.." expander
        // with long content, we make it scrollable - the scrollbar will only take
        // effect if a CSS max-height is set on the top menu.
        this.actor = new St.ScrollView({ style_class: 'popup-sub-menu',
                                         hscrollbar_policy: St.PolicyType.NEVER,
                                         vscrollbar_policy: St.PolicyType.NEVER });

        this.actor.add_actor(this.box);
        this.actor._delegate = this;
        this.actor.clip_to_allocation = true;
        this.actor.connect('key-press-event', this._onKeyPressEvent.bind(this));
        this.actor.hide();
    }

    _needsScrollbar() {
        let topMenu = this._getTopMenu();
        let [, topNaturalHeight] = topMenu.actor.get_preferred_height(-1);
        let topThemeNode = topMenu.actor.get_theme_node();

        let topMaxHeight = topThemeNode.get_max_height();
        return topMaxHeight >= 0 && topNaturalHeight >= topMaxHeight;
    }

    getSensitive() {
        return this._sensitive && this.sourceActor.sensitive;
    }

    get sensitive() {
        return this.getSensitive();
    }

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
            needsScrollbar ? St.PolicyType.AUTOMATIC : St.PolicyType.NEVER;

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
            let [, naturalHeight] = this.actor.get_preferred_height(-1);
            this.actor.height = 0;
            this.actor.ease({
                height: naturalHeight,
                duration: 250,
                mode: Clutter.AnimationMode.EASE_OUT_EXPO,
                onComplete: () => this.actor.set_height(-1),
            });
            this._arrow.ease({
                rotation_angle_z: targetAngle,
                duration: 250,
                mode: Clutter.AnimationMode.EASE_OUT_EXPO,
            });
        } else {
            this._arrow.rotation_angle_z = targetAngle;
        }
    }

    close(animate) {
        if (!this.isOpen)
            return;

        this.isOpen = false;
        this.emit('open-state-changed', false);

        if (this._activeMenuItem)
            this._activeMenuItem.active = false;

        if (animate && this._needsScrollbar())
            animate = false;

        if (animate) {
            this.actor.ease({
                height: 0,
                duration: 250,
                mode: Clutter.AnimationMode.EASE_OUT_EXPO,
                onComplete: () => {
                    this.actor.hide();
                    this.actor.set_height(-1);
                },
            });
            this._arrow.ease({
                rotation_angle_z: 0,
                duration: 250,
                mode: Clutter.AnimationMode.EASE_OUT_EXPO,
            });
        } else {
            this._arrow.rotation_angle_z = 0;
            this.actor.hide();
        }
    }

    _onKeyPressEvent(actor, event) {
        // Move focus back to parent menu if the user types Left.

        if (this.isOpen && event.get_key_symbol() == Clutter.KEY_Left) {
            this.close(BoxPointer.PopupAnimation.FULL);
            this.sourceActor._delegate.active = true;
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    }
};

/**
 * PopupMenuSection:
 *
 * A section of a PopupMenu which is handled like a submenu
 * (you can add and remove items, you can destroy it, you
 * can add it to another menu), but is completely transparent
 * to the user
 */
var PopupMenuSection = class extends PopupMenuBase {
    constructor() {
        super();

        this.actor = this.box;
        this.actor._delegate = this;
        this.isOpen = true;
    }

    // deliberately ignore any attempt to open() or close(), but emit the
    // corresponding signal so children can still pick it up
    open() {
        this.emit('open-state-changed', true);
    }

    close() {
        this.emit('open-state-changed', false);
    }
};

var PopupSubMenuMenuItem = GObject.registerClass(
class PopupSubMenuMenuItem extends PopupBaseMenuItem {
    _init(text, wantIcon) {
        super._init();

        this.add_style_class_name('popup-submenu-menu-item');

        if (wantIcon) {
            this.icon = new St.Icon({ style_class: 'popup-menu-icon' });
            this.add_child(this.icon);
        }

        this.label = new St.Label({ text,
                                    y_expand: true,
                                    y_align: Clutter.ActorAlign.CENTER });
        this.add_child(this.label);
        this.label_actor = this.label;

        let expander = new St.Bin({
            style_class: 'popup-menu-item-expander',
            x_expand: true,
        });
        this.add_child(expander);

        this._triangle = arrowIcon(St.Side.RIGHT);
        this._triangle.pivot_point = new Graphene.Point({ x: 0.5, y: 0.6 });

        this._triangleBin = new St.Widget({ y_expand: true,
                                            y_align: Clutter.ActorAlign.CENTER });
        this._triangleBin.add_child(this._triangle);

        this.add_child(this._triangleBin);
        this.add_accessible_state(Atk.StateType.EXPANDABLE);

        this.menu = new PopupSubMenu(this, this._triangle);
        this.menu.connect('open-state-changed', this._subMenuOpenStateChanged.bind(this));
        this.connect('destroy', () => this.menu.destroy());
    }

    _setParent(parent) {
        super._setParent(parent);
        this.menu._setParent(parent);
    }

    syncSensitive() {
        let sensitive = super.syncSensitive();
        this._triangle.visible = sensitive;
        if (!sensitive)
            this.menu.close(false);
    }

    _subMenuOpenStateChanged(menu, open) {
        if (open) {
            this.add_style_pseudo_class('open');
            this._getTopMenu()._setOpenedSubMenu(this.menu);
            this.add_accessible_state(Atk.StateType.EXPANDED);
            this.add_style_pseudo_class('checked');
        } else {
            this.remove_style_pseudo_class('open');
            this._getTopMenu()._setOpenedSubMenu(null);
            this.remove_accessible_state(Atk.StateType.EXPANDED);
            this.remove_style_pseudo_class('checked');
        }
    }

    setSubmenuShown(open) {
        if (open)
            this.menu.open(BoxPointer.PopupAnimation.FULL);
        else
            this.menu.close(BoxPointer.PopupAnimation.FULL);
    }

    _setOpenState(open) {
        this.setSubmenuShown(open);
    }

    _getOpenState() {
        return this.menu.isOpen;
    }

    vfunc_key_press_event(keyPressEvent) {
        let symbol = keyPressEvent.keyval;

        if (symbol == Clutter.KEY_Right) {
            this._setOpenState(true);
            this.menu.actor.navigate_focus(null, St.DirectionType.DOWN, false);
            return Clutter.EVENT_STOP;
        } else if (symbol == Clutter.KEY_Left && this._getOpenState()) {
            this._setOpenState(false);
            return Clutter.EVENT_STOP;
        }

        return super.vfunc_key_press_event(keyPressEvent);
    }

    activate(_event) {
        this._setOpenState(true);
    }

    vfunc_button_release_event() {
        // Since we override the parent, we need to manage what the parent does
        // with the active style class
        this.remove_style_pseudo_class('active');
        this._setOpenState(!this._getOpenState());
        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_touch_event(touchEvent) {
        if (touchEvent.type == Clutter.EventType.TOUCH_END) {
            // Since we override the parent, we need to manage what the parent does
            // with the active style class
            this.remove_style_pseudo_class('active');
            this._setOpenState(!this._getOpenState());
        }
        return Clutter.EVENT_PROPAGATE;
    }
});

/* Basic implementation of a menu manager.
 * Call addMenu to add menus
 */
var PopupMenuManager = class {
    constructor(owner, grabParams) {
        grabParams = Params.parse(grabParams,
                                  { actionMode: Shell.ActionMode.POPUP });
        this._grabHelper = new GrabHelper.GrabHelper(owner, grabParams);
        this._menus = [];
    }

    addMenu(menu, position) {
        if (this._findMenu(menu) > -1)
            return;

        let menudata = {
            menu,
            openStateChangeId: menu.connect('open-state-changed', this._onMenuOpenState.bind(this)),
            destroyId:         menu.connect('destroy', this._onMenuDestroy.bind(this)),
            enterId:           0,
            focusInId:         0,
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
    }

    removeMenu(menu) {
        if (menu == this.activeMenu)
            this._grabHelper.ungrab({ actor: menu.actor });

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
    }

    get activeMenu() {
        let firstGrab = this._grabHelper.grabStack[0];
        if (firstGrab)
            return firstGrab.actor._delegate;
        else
            return null;
    }

    ignoreRelease() {
        return this._grabHelper.ignoreRelease();
    }

    _onMenuOpenState(menu, open) {
        if (open) {
            if (this.activeMenu)
                this.activeMenu.close(BoxPointer.PopupAnimation.FADE);
            this._grabHelper.grab({
                actor: menu.actor,
                focus: menu.focusActor,
                onUngrab: isUser => this._closeMenu(isUser, menu),
            });
        } else {
            this._grabHelper.ungrab({ actor: menu.actor });
        }
    }

    _changeMenu(newMenu) {
        newMenu.open(this.activeMenu
            ? BoxPointer.PopupAnimation.FADE
            : BoxPointer.PopupAnimation.FULL);
    }

    _onMenuSourceEnter(menu) {
        if (!this._grabHelper.grabbed)
            return Clutter.EVENT_PROPAGATE;

        if (this._grabHelper.isActorGrabbed(menu.actor))
            return Clutter.EVENT_PROPAGATE;

        this._changeMenu(menu);
        return Clutter.EVENT_PROPAGATE;
    }

    _onMenuDestroy(menu) {
        this.removeMenu(menu);
    }

    _findMenu(item) {
        for (let i = 0; i < this._menus.length; i++) {
            let menudata = this._menus[i];
            if (item == menudata.menu)
                return i;
        }
        return -1;
    }

    _closeMenu(isUser, menu) {
        // If this isn't a user action, we called close()
        // on the BoxPointer ourselves, so we shouldn't
        // reanimate.
        if (isUser)
            menu.close(BoxPointer.PopupAnimation.FULL);
    }
};
