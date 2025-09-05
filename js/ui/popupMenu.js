import Atk from 'gi://Atk';
import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import Graphene from 'gi://Graphene';
import Shell from 'gi://Shell';
import St from 'gi://St';
import * as Signals from '../misc/signals.js';

import * as BoxPointer from './boxpointer.js';
import * as Main from './main.js';
import * as Params from '../misc/params.js';

/** @enum {number} */
export const Ornament = {
    NONE: 0,
    DOT: 1,
    CHECK: 2,
    HIDDEN: 3,
    NO_DOT: 4,
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
 *
 * @param {St.Side} side - Side to which the arrow points.
 * @returns {St.Icon} a new arrow icon
 */
export function arrowIcon(side) {
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

    const arrow = new St.Icon({
        style_class: 'popup-menu-arrow',
        icon_name: iconName,
        accessible_role: Atk.Role.ARROW,
        y_expand: true,
        y_align: Clutter.ActorAlign.CENTER,
    });

    return arrow;
}

export const PopupBaseMenuItem = GObject.registerClass({
    Properties: {
        'active': GObject.ParamSpec.boolean(
            'active', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'sensitive': GObject.ParamSpec.boolean(
            'sensitive', null, null,
            GObject.ParamFlags.READWRITE,
            true),
    },
    Signals: {
        'activate': {param_types: [Clutter.Event.$gtype]},
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
        super._init({
            style_class: 'popup-menu-item',
            reactive: params.reactive,
            track_hover: params.reactive,
            can_focus: params.can_focus,
            accessible_role: Atk.Role.MENU_ITEM,
        });
        this._delegate = this;

        this._ornamentIcon = new St.Icon({style_class: 'popup-menu-ornament'});
        this.add_child(this._ornamentIcon);
        this.setOrnament(Ornament.HIDDEN);

        this._parent = null;
        this._active = false;
        this._activatable = params.reactive && params.activate;
        this._sensitive = true;

        this._clickGesture = new Clutter.ClickGesture({
            enabled: this._activatable,
        });
        this._clickGesture.connect('recognize',
            () => this.activate(Clutter.get_current_event()));
        this._clickGesture.connect('notify::pressed', () => {
            if (this._clickGesture.pressed)
                this.add_style_pseudo_class('active');
            else
                this.remove_style_pseudo_class('active');
        });
        this.add_action(this._clickGesture);

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

    vfunc_key_press_event(event) {
        if (global.focus_manager.navigate_from_event(event))
            return Clutter.EVENT_STOP;

        if (!this._activatable)
            return super.vfunc_key_press_event(event);

        let state = event.get_state();

        // if user has a modifier down (except capslock and numlock)
        // then don't handle the key press here
        state &= ~Clutter.ModifierType.LOCK_MASK;
        state &= ~Clutter.ModifierType.MOD2_MASK;
        state &= Clutter.ModifierType.MODIFIER_MASK;

        if (state)
            return Clutter.EVENT_PROPAGATE;

        let symbol = event.get_key_symbol();
        if (symbol === Clutter.KEY_space || symbol === Clutter.KEY_Return) {
            this.activate(event);
            return Clutter.EVENT_STOP;
        }

        // Support wrapping navigation in the menu
        if (symbol === Clutter.KEY_Up || symbol === Clutter.KEY_Down) {
            const group = global.focus_manager.get_group(this);
            const direction = symbol === Clutter.KEY_Up
                ? St.DirectionType.UP
                : St.DirectionType.DOWN;
            if (group?.navigate_focus(this, direction, true))
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
        let activeChanged = active !== this.active;
        if (activeChanged) {
            this._active = active;
            if (active) {
                this.add_style_pseudo_class('selected');
                if (this.can_focus)
                    this.grab_key_focus();
            } else {
                this.remove_style_pseudo_class('selected');
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
        if (this._sensitive === sensitive)
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
        if (ornament === this._ornament)
            return;

        this._ornament = ornament;

        if (ornament === Ornament.DOT) {
            this._ornamentIcon.icon_name = 'ornament-dot-checked-symbolic';
            this.add_accessible_state(Atk.StateType.CHECKED);
        } else if (ornament === Ornament.NO_DOT) {
            this._ornamentIcon.icon_name = 'ornament-dot-unchecked-symbolic';
            this.remove_accessible_state(Atk.StateType.CHECKED);
        } else if (ornament === Ornament.CHECK) {
            this._ornamentIcon.icon_name = 'ornament-check-symbolic';
            this.add_accessible_state(Atk.StateType.CHECKED);
        } else if (ornament === Ornament.NONE || ornament === Ornament.HIDDEN) {
            this._ornamentIcon.icon_name = '';
            this.remove_accessible_state(Atk.StateType.CHECKED);
        }

        this._ornamentIcon.visible = ornament !== Ornament.HIDDEN;
        this._updateOrnamentStyle();
    }

    _updateOrnamentStyle() {
        if (this._ornament === Ornament.CHECK || this._ornament === Ornament.NONE)
            this.add_style_class_name('popup-ornamented-menu-item');
        else
            this.remove_style_class_name('popup-ornamented-menu-item');
    }
});

export const PopupMenuItem = GObject.registerClass(
class PopupMenuItem extends PopupBaseMenuItem {
    _init(text, params) {
        super._init(params);

        this.label = new St.Label({
            text,
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(this.label);
        this.label_actor = this.label;
    }
});


export const PopupSeparatorMenuItem = GObject.registerClass(
class PopupSeparatorMenuItem extends PopupBaseMenuItem {
    _init(text) {
        super._init({
            style_class: 'popup-separator-menu-item',
            reactive: false,
            can_focus: false,
        });

        this.label = new St.Label({text: text || ''});
        this.add_child(this.label);
        this.label_actor = this.label;
        this.accessible_role = Atk.Role.SEPARATOR;

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
        this.label.visible = this.label.text !== '';
    }
});

export const Switch = GObject.registerClass({
    Properties: {
        'state': GObject.ParamSpec.boolean(
            'state', null, null,
            GObject.ParamFlags.READWRITE,
            false),
    },
}, class Switch extends St.Widget {
    _init(state) {
        this._state = false;
        this._dragging = false;

        super._init({
            style_class: 'toggle-switch',
            accessible_role: Atk.Role.CHECK_BOX,
            y_align: Clutter.ActorAlign.CENTER,
            track_hover: true,
            reactive: true,
        });

        const box = new St.BoxLayout({
            x_expand: true,
            y_expand: true,
            constraints: new Clutter.BindConstraint({
                source: this,
                coordinate: Clutter.BindCoordinate.SIZE,
            }),
        });
        this.add_child(box);

        this._onIcon = new St.Icon({
            icon_name: 'switch-on-symbolic',
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });
        box.add_child(this._onIcon);

        this._offIcon = new St.Icon({
            icon_name: 'switch-off-symbolic',
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });
        box.add_child(this._offIcon);

        this._handle = new St.Widget({
            style_class: 'handle',
            y_align: Clutter.ActorAlign.CENTER,
            constraints: new Clutter.BindConstraint({
                source: this,
                coordinate: Clutter.BindCoordinate.HEIGHT,
            }),
        });
        this.bind_property('reactive', this._handle, 'reactive', GObject.BindingFlags.SYNC_CREATE);

        this._handleAlignConstraint = new Clutter.AlignConstraint({
            name: 'align',
            align_axis: Clutter.AlignAxis.X_AXIS,
            source: this,
        });
        this._handle.add_constraint(this._handleAlignConstraint);
        this._handle.connect('button-press-event', (actor, event) => this._startDragging(event));
        this._handle.connect('touch-event', this._touchDragging.bind(this));
        this.add_child(this._handle);

        this.state = state;

        this._a11ySettings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.a11y.interface',
        });

        this._a11ySettings.connectObject('changed::show-status-shapes',
            () => this._updateIconOpacity(),
            this);
        this._updateIconOpacity();
    }

    _updateIconOpacity() {
        const activeOpacity = this._a11ySettings.get_boolean('show-status-shapes')
            ? 255. : 0.;

        this._onIcon.opacity = activeOpacity;
        this._offIcon.opacity = activeOpacity;
    }

    get state() {
        return this._state;
    }

    set state(state) {
        let handleAlignFactor;
        // Calling get_theme_node() while unmapped is an error, avoid that
        const duration = this._handle.mapped
            ? this._handle.get_theme_node().get_transition_duration()
            : 0;

        if (state) {
            this.add_style_pseudo_class('checked');
            handleAlignFactor = 1.0;
        } else {
            this.remove_style_pseudo_class('checked');
            handleAlignFactor = 0.0;
        }

        this._handle.ease_property('@constraints.align.factor', handleAlignFactor, {
            duration,
        });

        if (this._state !== state) {
            this._state = state;
            this.notify('state');
        }
    }

    toggle() {
        this.state = !this.state;
    }

    _startDragging(event) {
        if (this._dragging)
            return Clutter.EVENT_PROPAGATE;

        this._dragging = true;
        [this._initialGrabX] = event.get_coords();

        this._grab = global.stage.grab(this);

        const backend = global.stage.get_context().get_backend();
        const sprite = backend.get_sprite(global.stage, event);
        this._sprite = sprite;

        return Clutter.EVENT_STOP;
    }

    vfunc_motion_event(event) {
        const backend = this.get_context().get_backend();
        const sprite = backend.get_sprite(global.stage, event);

        if (this._dragging && this._sprite === sprite)
            return this._motionEvent(this, Clutter.get_current_event());

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_button_release_event(event) {
        const backend = this.get_context().get_backend();
        const sprite = backend.get_sprite(global.stage, event);

        if (this._dragging && this._sprite === sprite)
            return this._endDragging();

        return Clutter.EVENT_PROPAGATE;
    }

    _touchDragging(actor, event) {
        const backend = actor.get_context().get_backend();
        const sprite = backend.get_sprite(global.stage, event);

        if (!this._dragging &&
            event.type() === Clutter.EventType.TOUCH_BEGIN) {
            this.startDragging(event);
            return Clutter.EVENT_STOP;
        } else if (this._sprite === sprite) {
            if (event.type() === Clutter.EventType.TOUCH_UPDATE)
                return this._motionEvent(this, event);
            else if (event.type() === Clutter.EventType.TOUCH_END)
                return this._endDragging();
        }

        return Clutter.EVENT_PROPAGATE;
    }

    _endDragging() {
        if (!this._dragging)
            return Clutter.EVENT_PROPAGATE;

        if (this._grab) {
            this._grab.dismiss();
            this._grab = null;
        }

        if (this._dragged)
            this.state = this._handleAlignConstraint.get_factor() > 0.5;
        else
            this.toggle();

        this._dragged = false;
        this._sprite = null;
        this._dragging = false;

        return Clutter.EVENT_STOP;
    }

    _motionEvent(actor, event) {
        this._dragged = true;

        let [absX] = event.get_coords();
        let factorDiff = (absX - this._initialGrabX) / (this.get_width() - this._handle.get_width());
        let factor = factorDiff + (this.state ? 1.0 : 0.0);

        this._handleAlignConstraint.set_factor(Math.clamp(factor, 0, 1));

        return Clutter.EVENT_STOP;
    }
});

export const PopupSwitchMenuItem = GObject.registerClass({
    Properties: {
        'state': GObject.ParamSpec.boolean(
            'state', null, null,
            GObject.ParamFlags.READWRITE,
            false),
    },
    Signals: {'toggled': {param_types: [GObject.TYPE_BOOLEAN]}},
}, class PopupSwitchMenuItem extends PopupBaseMenuItem {
    _init(text, active, params) {
        super._init(params);

        this.label = new St.Label({
            text,
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this._switch = new Switch(active);
        this._switch.connect('notify::state', this._onToggled.bind(this));
        this.bind_property('reactive', this._switch, 'reactive', GObject.BindingFlags.SYNC_CREATE);

        this.accessible_role = Atk.Role.CHECK_MENU_ITEM;
        this._checkAccessibleState();
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
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
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
        this._checkAccessibleState();
    }

    activate(event) {
        if (this._switch.mapped)
            this.toggle();

        // we allow pressing space to toggle the switch
        // without closing the menu
        if (event.type() === Clutter.EventType.KEY_PRESS &&
            event.get_key_symbol() === Clutter.KEY_space)
            return;

        super.activate(event);
    }

    toggle() {
        this._switch.toggle();
    }

    get state() {
        return this._switch.state;
    }

    set state(state) {
        if (this._switch.state === state)
            return;

        this._switch.set({state});
    }

    setToggleState(state) {
        this.set({state});
    }

    _onToggled() {
        this.emit('toggled', this._switch.state);
        this.notify('state');
        this._checkAccessibleState();
    }

    _checkAccessibleState() {
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

export const PopupImageMenuItem = GObject.registerClass(
class PopupImageMenuItem extends PopupBaseMenuItem {
    _init(text, icon, params) {
        super._init(params);

        this._icon = new St.Icon({
            style_class: 'popup-menu-icon',
            x_align: Clutter.ActorAlign.END,
        });
        this.add_child(this._icon);
        this.label = new St.Label({
            text,
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(this.label);
        this.label_actor = this.label;

        this.set_child_above_sibling(this._ornamentIcon, this.label);

        this.setIcon(icon);
    }

    setIcon(icon) {
        // The 'icon' parameter can be either a Gio.Icon or a string.
        if (icon instanceof GObject.Object && GObject.type_is_a(icon, Gio.Icon))
            this._icon.gicon = icon;
        else
            this._icon.icon_name = icon;
    }

    _updateOrnamentStyle() {
        // we move the ornament after the label, so we don't need
        // additional padding regardless of ornament visibility
    }
});

export class PopupMenuBase extends Signals.EventEmitter {
    constructor(sourceActor, styleClass) {
        super();

        if (this.constructor === PopupMenuBase)
            throw new TypeError(`Cannot instantiate abstract class ${this.constructor.name}`);

        this.sourceActor = sourceActor;
        this.focusActor = sourceActor;
        this._parent = null;

        this.box = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
            y_expand: true,
        });

        if (styleClass !== undefined)
            this.box.style_class = styleClass;
        this.length = 0;

        this.isOpen = false;

        this._activeMenuItem = null;
        this._settingsActions = { };

        this._sensitive = true;

        Main.sessionMode.connectObject('updated', () => this._sessionUpdated(), this);
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
        if (icon !== undefined)
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
                log(`Settings panel for desktop file ${desktopFile} could not be loaded!`);
                return;
            }

            Main.overview.hide();
            Main.panel.closeQuickSettings();
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
        if (animate === undefined)
            animate = BoxPointer.PopupAnimation.FULL;

        this._getTopMenu().close(animate);
    }

    _subMenuActiveChanged(submenu, submenuItem) {
        if (this._activeMenuItem && this._activeMenuItem !== submenuItem)
            this._activeMenuItem.active = false;
        this._activeMenuItem = submenuItem;
        this.emit('active-changed', submenuItem);
    }

    _connectItemSignals(menuItem) {
        menuItem.connectObject(
            'notify::active', () => {
                const {active} = menuItem;
                if (active && this._activeMenuItem !== menuItem) {
                    if (this._activeMenuItem)
                        this._activeMenuItem.active = false;
                    this._activeMenuItem = menuItem;
                    this.emit('active-changed', menuItem);
                } else if (!active && this._activeMenuItem === menuItem) {
                    this._activeMenuItem = null;
                    this.emit('active-changed', null);
                }
            },
            'notify::sensitive', () => {
                const {sensitive} = menuItem;
                if (!sensitive && this._activeMenuItem === menuItem) {
                    if (!this.actor.navigate_focus(menuItem.actor,
                        St.DirectionType.TAB_FORWARD, true))
                        this.actor.grab_key_focus();
                } else if (sensitive && this._activeMenuItem === null) {
                    if (global.stage.get_key_focus() === this.actor)
                        menuItem.actor.grab_key_focus();
                }
            },
            'activate', () => {
                this.emit('activate', menuItem);
                this.itemActivated(BoxPointer.PopupAnimation.FULL);
            }, GObject.ConnectFlags.AFTER,
            'destroy', () => {
                if (menuItem === this._activeMenuItem)
                    this._activeMenuItem = null;
            }, this);

        this.connectObject('notify::sensitive',
            () => menuItem.syncSensitive(), menuItem);
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
            if (items[i] !== menuItem)
                position--;
            i++;
        }

        if (i < items.length) {
            if (items[i] !== menuItem)
                this.box.set_child_below_sibling(menuItem.actor, items[i].actor);
        } else {
            this.box.set_child_above_sibling(menuItem.actor, null);
        }
    }

    addMenuItem(menuItem, position) {
        let beforeItem = null;
        if (position === undefined) {
            this.box.add_child(menuItem.actor);
        } else {
            let items = this._getMenuItems();
            if (position < items.length) {
                beforeItem = items[position].actor;
                this.box.insert_child_below(menuItem.actor, beforeItem);
            } else {
                this.box.add_child(menuItem.actor);
            }
        }

        if (menuItem instanceof PopupMenuSection) {
            menuItem.connectObject(
                'active-changed', this._subMenuActiveChanged.bind(this),
                'destroy', () => this.length--, this);

            this.connectObject(
                'open-state-changed', (self, open) => {
                    if (open)
                        menuItem.open();
                    else
                        menuItem.close();
                },
                'menu-closed', () => menuItem.emit('menu-closed'),
                'notify::sensitive', () => menuItem.emit('notify::sensitive'),
                menuItem);
        } else if (menuItem instanceof PopupSubMenuMenuItem) {
            if (beforeItem == null)
                this.box.add_child(menuItem.menu.actor);
            else
                this.box.insert_child_below(menuItem.menu.actor, beforeItem);

            this._connectItemSignals(menuItem);
            menuItem.menu.connectObject('active-changed',
                this._subMenuActiveChanged.bind(this), this);
            this.connectObject('menu-closed', () => {
                menuItem.menu.close(BoxPointer.PopupAnimation.NONE);
            }, menuItem);
        } else if (menuItem instanceof PopupSeparatorMenuItem) {
            this._connectItemSignals(menuItem);

            // updateSeparatorVisibility needs to get called any time the
            // separator's adjacent siblings change visibility or position.
            // open-state-changed isn't exactly that, but doing it in more
            // precise ways would require a lot more bookkeeping.
            this.connectObject('open-state-changed', () => {
                this._updateSeparatorVisibility(menuItem);
            }, menuItem);
        } else if (menuItem instanceof PopupBaseMenuItem) {
            this._connectItemSignals(menuItem);
        } else {
            throw TypeError('Invalid argument to PopupMenuBase.addMenuItem()');
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

        Main.sessionMode.disconnectObject(this);
    }
}

export class PopupMenu extends PopupMenuBase {
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
            this.sourceActor.connectObject(
                'key-press-event', this._onKeyPress.bind(this),
                'notify::mapped', () => {
                    if (!this.sourceActor.mapped)
                        this.close();
                }, this);
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

        if (symbol === Clutter.KEY_space || symbol === Clutter.KEY_Return) {
            this.toggle();
            return Clutter.EVENT_STOP;
        } else if (symbol === navKey) {
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

        /* The position calculation expects the source actor allocation to be
         * up to date, so queue relayout on parent to ensure it gets allocated
         * first.
         */
        if (!this.sourceActor?.has_allocation())
            this.sourceActor?.get_parent().queue_relayout();

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
        this.sourceActor?.disconnectObject(this);

        if (this._systemModalOpenedId)
            Main.layoutManager.disconnect(this._systemModalOpenedId);
        this._systemModalOpenedId = 0;

        super.destroy();
    }
}

export class PopupDummyMenu extends Signals.EventEmitter {
    constructor(sourceActor) {
        super();

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
        if (this.isOpen)
            return;
        this.isOpen = true;
        this.emit('open-state-changed', true);
    }

    close() {
        if (!this.isOpen)
            return;
        this.isOpen = false;
        this.emit('open-state-changed', false);
    }

    toggle() {}

    destroy() {
        this.emit('destroy');
    }
}

export class PopupSubMenu extends PopupMenuBase {
    constructor(sourceActor, sourceArrow) {
        super(sourceActor);

        this._arrow = sourceArrow;

        // Since a function of a submenu might be to provide a "More.." expander
        // with long content, we make it scrollable - the scrollbar will only take
        // effect if a CSS max-height is set on the top menu.
        this.actor = new St.ScrollView({
            style_class: 'popup-sub-menu',
            vscrollbar_policy: St.PolicyType.NEVER,
            child: this.box,
        });

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

        let targetAngle = this.actor.text_direction === Clutter.TextDirection.RTL ? -90 : 90;

        const duration = animate ? 250 : 0;
        let [, naturalHeight] = this.actor.get_preferred_height(-1);
        this.actor.height = 0;
        this.actor.ease({
            height: naturalHeight,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_EXPO,
            onComplete: () => this.actor.set_height(-1),
        });
        this._arrow.ease({
            rotation_angle_z: targetAngle,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_EXPO,
        });
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

        const duration = animate ? 250 : 0;
        this.actor.ease({
            height: 0,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_EXPO,
            onComplete: () => {
                this.actor.hide();
                this.actor.set_height(-1);
            },
        });
        this._arrow.ease({
            rotation_angle_z: 0,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_EXPO,
        });
    }

    _onKeyPressEvent(actor, event) {
        // Move focus back to parent menu if the user types Left.

        if (this.isOpen && event.get_key_symbol() === Clutter.KEY_Left) {
            this.close(BoxPointer.PopupAnimation.FULL);
            this.sourceActor._delegate.active = true;
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    }
}

/**
 * PopupMenuSection:
 *
 * A section of a PopupMenu which is handled like a submenu
 * (you can add and remove items, you can destroy it, you
 * can add it to another menu), but is completely transparent
 * to the user
 */
export class PopupMenuSection extends PopupMenuBase {
    constructor() {
        super();

        this.actor = this.box;
        this.actor._delegate = this;
        this.isOpen = true;

        this.actor.add_style_class_name('popup-menu-section');
    }

    // deliberately ignore any attempt to open() or close(), but emit the
    // corresponding signal so children can still pick it up
    open() {
        this.emit('open-state-changed', true);
    }

    close() {
        this.emit('open-state-changed', false);
    }
}

export const PopupSubMenuMenuItem = GObject.registerClass(
class PopupSubMenuMenuItem extends PopupBaseMenuItem {
    _init(text, wantIcon) {
        super._init();

        this.add_style_class_name('popup-submenu-menu-item');

        if (wantIcon) {
            this.icon = new St.Icon({style_class: 'popup-menu-icon'});
            this.add_child(this.icon);
        }

        this.label = new St.Label({
            text,
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(this.label);
        this.label_actor = this.label;

        let expander = new St.Bin({
            style_class: 'popup-menu-item-expander',
            x_expand: true,
        });
        this.add_child(expander);

        this._triangle = arrowIcon(St.Side.RIGHT);
        this._triangle.pivot_point = new Graphene.Point({x: 0.5, y: 0.6});

        this._triangleBin = new St.Widget({
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
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

    vfunc_key_press_event(event) {
        let symbol = event.get_key_symbol();

        if (symbol === Clutter.KEY_Right) {
            this._setOpenState(true);
            this.menu.actor.navigate_focus(null, St.DirectionType.DOWN, false);
            return Clutter.EVENT_STOP;
        } else if (symbol === Clutter.KEY_Left && this._getOpenState()) {
            this._setOpenState(false);
            return Clutter.EVENT_STOP;
        }

        return super.vfunc_key_press_event(event);
    }

    activate(_event) {
        this._setOpenState(!this._getOpenState());
    }
});

/* Basic implementation of a menu manager.
 * Call addMenu to add menus
 */
export class PopupMenuManager {
    constructor(owner, grabParams) {
        this._grabParams = Params.parse(grabParams,
            {actionMode: Shell.ActionMode.POPUP});
        this._menus = [];
    }

    addMenu(menu, position) {
        if (this._menus.includes(menu))
            return;

        menu.connectObject(
            'open-state-changed', this._onMenuOpenState.bind(this),
            'destroy', () => this.removeMenu(menu), this);
        menu.actor.connectObject('captured-event',
            this._onCapturedEvent.bind(this), this);

        if (position === undefined)
            this._menus.push(menu);
        else
            this._menus.splice(position, 0, menu);
    }

    removeMenu(menu) {
        if (menu === this.activeMenu) {
            Main.popModal(this._grab);
            this._grab = null;

            if (this._keyFocusId) {
                global.stage.disconnect(this._keyFocusId);
                delete this._keyFocusId;
            }
        }

        const position = this._menus.indexOf(menu);
        if (position === -1) // not a menu we manage
            return;

        menu.disconnectObject(this);
        menu.actor.disconnectObject(this);

        this._menus.splice(position, 1);
    }

    ignoreRelease() {
    }

    _onMenuOpenState(menu, open) {
        if (open && this.activeMenu === menu)
            return;

        if (open) {
            const oldMenu = this.activeMenu;
            const oldGrab = this._grab;
            this._grab = Main.pushModal(menu.actor, this._grabParams);
            this.activeMenu = menu;
            oldMenu?.close(BoxPointer.PopupAnimation.FADE);
            if (oldGrab)
                Main.popModal(oldGrab);

            if (!this._keyFocusId) {
                this._keyFocusId =
                    global.stage.connect('notify::key-focus', () => {
                        if (!this.activeMenu)
                            return;

                        let actor = global.stage.get_key_focus();
                        let newMenu = this._findMenuForSource(actor);

                        if (newMenu)
                            this._changeMenu(newMenu);
                    });
            }
        } else if (this.activeMenu === menu) {
            this.activeMenu = null;
            Main.popModal(this._grab);
            this._grab = null;

            if (this._keyFocusId) {
                global.stage.disconnect(this._keyFocusId);
                delete this._keyFocusId;
            }
        }
    }

    _changeMenu(newMenu) {
        newMenu.open(this.activeMenu
            ? BoxPointer.PopupAnimation.FADE
            : BoxPointer.PopupAnimation.FULL);
    }

    _onCapturedEvent(actor, event) {
        let menu = actor._delegate;
        const targetActor = global.stage.get_event_actor(event);

        if (event.type() === Clutter.EventType.KEY_PRESS) {
            let symbol = event.get_key_symbol();
            if (symbol === Clutter.KEY_Down &&
                global.stage.get_key_focus() === menu.actor) {
                actor.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
                return Clutter.EVENT_STOP;
            } else if (symbol === Clutter.KEY_Escape && menu.isOpen) {
                menu.close(BoxPointer.PopupAnimation.FULL);
                return Clutter.EVENT_STOP;
            }
        } else if (event.type() === Clutter.EventType.ENTER &&
                   (event.get_flags() & Clutter.EventFlags.FLAG_GRAB_NOTIFY) === 0) {
            let hoveredMenu = this._findMenuForSource(targetActor);

            if (hoveredMenu && hoveredMenu !== menu)
                this._changeMenu(hoveredMenu);
        } else if ((event.type() === Clutter.EventType.BUTTON_PRESS ||
                    event.type() === Clutter.EventType.TOUCH_BEGIN) &&
                   !actor.contains(targetActor)) {
            menu.close(BoxPointer.PopupAnimation.FULL);
        }

        return Clutter.EVENT_PROPAGATE;
    }

    _findMenuForSource(source) {
        while (source) {
            let actor = source;
            const menu = this._menus.find(m => m.sourceActor === actor);
            if (menu)
                return menu;
            source = source.get_parent();
        }

        return null;
    }

    _closeMenu(isUser, menu) {
        // If this isn't a user action, we called close()
        // on the BoxPointer ourselves, so we shouldn't
        // reanimate.
        if (isUser)
            menu.close(BoxPointer.PopupAnimation.FULL);
    }
}
