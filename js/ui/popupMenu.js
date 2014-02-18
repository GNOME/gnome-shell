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
    let rotation;
    switch (side) {
        case St.Side.TOP:
            rotation = 180;
            break;
        case St.Side.RIGHT:
            rotation = - 90;
            break;
        case St.Side.BOTTOM:
            rotation = 0;
            break;
        case St.Side.LEFT:
            rotation = 90;
            break;
    }

    let gicon = new Gio.FileIcon({ file: Gio.File.new_for_path(global.datadir +
                                             '/theme/menu-arrow-symbolic.svg') });

    let arrow = new St.Icon({ style_class: 'popup-menu-arrow',
                              gicon: gicon,
                              accessible_role: Atk.Role.ARROW,
                              y_expand: true,
                              y_align: Clutter.ActorAlign.CENTER });

    arrow.rotation_angle_z = rotation;

    return arrow;
}

const PopupBaseMenuItem = new Lang.Class({
    Name: 'PopupBaseMenuItem',

    _init: function (params) {
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
            this.actor.connect('button-release-event', Lang.bind(this, this._onButtonReleaseEvent));
            this.actor.connect('key-press-event', Lang.bind(this, this._onKeyPressEvent));
        }
        if (params.reactive && params.hover)
            this.actor.connect('notify::hover', Lang.bind(this, this._onHoverChanged));

        this.actor.connect('key-focus-in', Lang.bind(this, this._onKeyFocusIn));
        this.actor.connect('key-focus-out', Lang.bind(this, this._onKeyFocusOut));
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));
    },

    _getTopMenu: function() {
        if (this._parent)
            return this._parent._getTopMenu();
        else
            return this;
    },

    _setParent: function(parent) {
        this._parent = parent;
    },

    _onButtonReleaseEvent: function (actor, event) {
        this.activate(event);
        return Clutter.EVENT_STOP;
    },

    _onKeyPressEvent: function (actor, event) {
        let symbol = event.get_key_symbol();

        if (symbol == Clutter.KEY_space || symbol == Clutter.KEY_Return) {
            this.activate(event);
            return Clutter.EVENT_STOP;
        }
        return Clutter.EVENT_PROPAGATE;
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

    setActive: function (active) {
        let activeChanged = active != this.active;
        if (activeChanged) {
            this.active = active;
            if (active) {
                this.actor.add_style_pseudo_class('active');
                this.actor.grab_key_focus();
            } else {
                this.actor.remove_style_pseudo_class('active');
            }
            this.emit('active-changed', active);
        }
    },

    syncSensitive: function() {
        let sensitive = this.getSensitive();
        this.actor.reactive = sensitive;
        this.actor.can_focus = sensitive;
        this.emit('sensitive-changed');
        return sensitive;
    },

    getSensitive: function() {
        let parentSensitive = this._parent ? this._parent.getSensitive() : true;
        return this._activatable && this._sensitive && parentSensitive;
    },

    setSensitive: function(sensitive) {
        if (this._sensitive == sensitive)
            return;

        this._sensitive = sensitive;
        this.syncSensitive();
    },

    destroy: function() {
        this.actor.destroy();
    },

    _onDestroy: function() {
        this.emit('destroy');
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
    }
});
Signals.addSignalMethods(PopupBaseMenuItem.prototype);

const PopupMenuItem = new Lang.Class({
    Name: 'PopupMenuItem',
    Extends: PopupBaseMenuItem,

    _init: function (text, params) {
        this.parent(params);

        this.label = new St.Label({ text: text });
        this.actor.add_child(this.label);
        this.actor.label_actor = this.label
    }
});

const PopupSeparatorMenuItem = new Lang.Class({
    Name: 'PopupSeparatorMenuItem',
    Extends: PopupBaseMenuItem,

    _init: function (text) {
        this.parent({ reactive: false,
                      can_focus: false});

        this.label = new St.Label({ text: text || '' });
        this.actor.add(this.label);
        this.actor.label_actor = this.label;

        this._separator = new Separator.HorizontalSeparator({ style_class: 'popup-separator-menu-item' });
        this.actor.add(this._separator.actor, { expand: true });
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

        this.actor.add_child(this.label);

        this._statusBin = new St.Bin({ x_align: St.Align.END });
        this.actor.add(this._statusBin, { expand: true, x_align: St.Align.END });

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

        this._sessionUpdatedId = Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
    },

    _getTopMenu: function() {
        if (this._parent)
            return this._parent._getTopMenu();
        else
            return this;
    },

    _setParent: function(parent) {
        this._parent = parent;
    },

    getSensitive: function() {
        let parentSensitive = this._parent ? this._parent.getSensitive() : true;
        return this._sensitive && parentSensitive;
    },

    setSensitive: function(sensitive) {
        this._sensitive = sensitive;
        this.emit('sensitive-changed');
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
            if (child._delegate instanceof PopupSeparatorMenuItem)
                return false;
            return isPopupMenuItemVisible(child);
        });

        return !hasVisibleChildren;
    },

    itemActivated: function(animate) {
        if (animate == undefined)
            animate = BoxPointer.PopupAnimation.FULL;

        this._getTopMenu().close(animate);
    },

    _subMenuActiveChanged: function(submenu, submenuItem) {
        if (this._activeMenuItem && this._activeMenuItem != submenuItem)
            this._activeMenuItem.setActive(false);
        this._activeMenuItem = submenuItem;
        this.emit('active-changed', submenuItem);
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
        menuItem._sensitiveChangeId = menuItem.connect('sensitive-changed', Lang.bind(this, function() {
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
        }));
        menuItem._activateId = menuItem.connect('activate', Lang.bind(this, function (menuItem, event) {
            this.emit('activate', menuItem);
            this.itemActivated(BoxPointer.PopupAnimation.FULL);
        }));

        menuItem._parentSensitiveChangeId = this.connect('sensitive-changed', Lang.bind(this, function() {
            menuItem.syncSensitive();
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
            this.disconnect(menuItem._parentSensitiveChangeId);
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
            let activeChangeId = menuItem.connect('active-changed', Lang.bind(this, this._subMenuActiveChanged));

            let parentOpenStateChangedId = this.connect('open-state-changed', function(self, open) {
                if (open)
                    menuItem.open();
                else
                    menuItem.close();
            });
            let parentClosingId = this.connect('menu-closed', function() {
                menuItem.emit('menu-closed');
            });
            let subMenuSensitiveChangedId = this.connect('sensitive-changed', Lang.bind(this, function() {
                menuItem.emit('sensitive-changed');
            }));

            menuItem.connect('destroy', Lang.bind(this, function() {
                menuItem.disconnect(activeChangeId);
                this.disconnect(subMenuSensitiveChangedId);
                this.disconnect(parentOpenStateChangedId);
                this.disconnect(parentClosingId);
                this.length--;
            }));
        } else if (menuItem instanceof PopupSubMenuMenuItem) {
            if (before_item == null)
                this.box.add(menuItem.menu.actor);
            else
                this.box.insert_child_below(menuItem.menu.actor, before_item);

            this._connectItemSignals(menuItem);
            let subMenuActiveChangeId = menuItem.menu.connect('active-changed', Lang.bind(this, this._subMenuActiveChanged));
            let closingId = this.connect('menu-closed', function() {
                menuItem.menu.close(BoxPointer.PopupAnimation.NONE);
            });

            menuItem.connect('destroy', Lang.bind(this, function() {
                menuItem.menu.disconnect(subMenuActiveChangeId);
                this.disconnect(closingId);
            }));
        } else if (menuItem instanceof PopupSeparatorMenuItem) {
            this._connectItemSignals(menuItem);

            // updateSeparatorVisibility needs to get called any time the
            // separator's adjacent siblings change visibility or position.
            // open-state-changed isn't exactly that, but doing it in more
            // precise ways would require a lot more bookkeeping.
            let openStateChangeId = this.connect('open-state-changed', Lang.bind(this, function() { this._updateSeparatorVisibility(menuItem); }));
            let destroyId = menuItem.connect('destroy', Lang.bind(this, function() {
                this.disconnect(openStateChangeId);
                menuItem.disconnect(destroyId);
            }));
        } else if (menuItem instanceof PopupBaseMenuItem)
            this._connectItemSignals(menuItem);
        else
            throw TypeError("Invalid argument to PopupMenuBase.addMenuItem()");

        menuItem._setParent(this);

        this.length++;
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

        this._boxPointer.bin.set_child(this.box);
        this.actor.add_style_class_name('popup-menu');

        global.focus_manager.add_group(this.actor);
        this.actor.reactive = true;

        this._openedSubMenu = null;
    },

    _setOpenedSubMenu: function(submenu) {
        if (this._openedSubMenu)
            this._openedSubMenu.close(true);

        this._openedSubMenu = submenu;
    },

    setArrowOrigin: function(origin) {
        this._boxPointer.setArrowOrigin(origin);
    },

    setSourceAlignment: function(alignment) {
        this._boxPointer.setSourceAlignment(alignment);
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

        if (this._boxPointer.actor.visible) {
            this._boxPointer.hide(animate, Lang.bind(this, function() {
                this.emit('menu-closed');
            }));
        }

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

    getSensitive: function() {
        return true;
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

    _needsScrollbar: function() {
        let topMenu = this._getTopMenu();
        let [topMinHeight, topNaturalHeight] = topMenu.actor.get_preferred_height(-1);
        let topThemeNode = topMenu.actor.get_theme_node();

        let topMaxHeight = topThemeNode.get_max_height();
        return topMaxHeight >= 0 && topNaturalHeight >= topMaxHeight;
    },

    getSensitive: function() {
        return this._sensitive && this.sourceActor._delegate.getSensitive();
    },

    open: function(animate) {
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

        if (animate) {
            let [minHeight, naturalHeight] = this.actor.get_preferred_height(-1);
            this.actor.height = 0;
            this.actor._arrowRotation = this._arrow.rotation_angle_z;
            Tweener.addTween(this.actor,
                             { _arrowRotation: this.actor._arrowRotation + 90,
                               height: naturalHeight,
                               time: 0.25,
                               onUpdateScope: this,
                               onUpdate: function() {
                                   this._arrow.rotation_angle_z = this.actor._arrowRotation;
                               },
                               onCompleteScope: this,
                               onComplete: function() {
                                   this.actor.set_height(-1);
                               }
                             });
        } else {
            this._arrow.rotation_angle_z = this.actor._arrowRotation + 90;
        }
    },

    close: function(animate) {
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
                             { _arrowRotation: this.actor._arrowRotation - 90,
                               height: 0,
                               time: 0.25,
                               onUpdateScope: this,
                               onUpdate: function() {
                                   this._arrow.rotation_angle_z = this.actor._arrowRotation;
                               },
                               onCompleteScope: this,
                               onComplete: function() {
                                   this.actor.hide();
                                   this.actor.set_height(-1);
                               },
                             });
        } else {
            this._arrow.rotation_angle_z = this.actor._arrowRotation - 90;
            this.actor.hide();
        }
    },

    _onKeyPressEvent: function(actor, event) {
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

    _init: function(text, wantIcon) {
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

        this.status = new St.Label({ style_class: 'popup-status-menu-item',
                                     y_expand: true,
                                     y_align: Clutter.ActorAlign.CENTER });
        this.actor.add_child(this.status);

        this._triangle = arrowIcon(St.Side.RIGHT);
        this._triangle.pivot_point = new Clutter.Point({ x: 0.5, y: 0.6 });

        this._triangleBin = new St.Widget({ y_expand: true,
                                            y_align: Clutter.ActorAlign.CENTER });
        this._triangleBin.add_child(this._triangle);
        if (this._triangleBin.get_text_direction() == Clutter.TextDirection.RTL)
            this._triangleBin.set_scale(-1.0, 1.0);

        this.actor.add_child(this._triangleBin);
        this.actor.add_accessible_state (Atk.StateType.EXPANDABLE);

        this.menu = new PopupSubMenu(this.actor, this._triangle);
        this.menu.connect('open-state-changed', Lang.bind(this, this._subMenuOpenStateChanged));
    },

    _setParent: function(parent) {
        this.parent(parent);
        this.menu._setParent(parent);
    },

    syncSensitive: function() {
        let sensitive = this.parent();
        this._triangle.visible = sensitive;
        if (!sensitive)
            this.menu.close(false);
    },

    _subMenuOpenStateChanged: function(menu, open) {
        if (open) {
            this.actor.add_style_pseudo_class('open');
            this._getTopMenu()._setOpenedSubMenu(this.menu);
            this.actor.add_accessible_state (Atk.StateType.EXPANDED);
        } else {
            this.actor.remove_style_pseudo_class('open');
            this._getTopMenu()._setOpenedSubMenu(null);
            this.actor.remove_accessible_state (Atk.StateType.EXPANDED);
        }
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
            return Clutter.EVENT_STOP;
        } else if (symbol == Clutter.KEY_Left && this._getOpenState()) {
            this._setOpenState(false);
            return Clutter.EVENT_STOP;
        }

        return this.parent(actor, event);
    },

    activate: function(event) {
        this._setOpenState(true);
    },

    _onButtonReleaseEvent: function(actor) {
        this._setOpenState(!this._getOpenState());
        return Clutter.EVENT_PROPAGATE;
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
            destroyId:         menu.connect('destroy', Lang.bind(this, this._onMenuDestroy)),
            enterId:           0,
            focusInId:         0
        };

        let source = menu.sourceActor;
        if (source) {
            if (!menu.blockSourceEvents)
                this._grabHelper.addActor(source);
            menudata.enterId = source.connect('enter-event', Lang.bind(this, function() { return this._onMenuSourceEnter(menu); }));
            menudata.focusInId = source.connect('key-focus-in', Lang.bind(this, function() { this._onMenuSourceEnter(menu); }));
        }

        if (position == undefined)
            this._menus.push(menudata);
        else
            this._menus.splice(position, 0, menudata);
    },

    removeMenu: function(menu) {
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

    ignoreRelease: function() {
        return this._grabHelper.ignoreRelease();
    },

    _onMenuOpenState: function(menu, open) {
        if (open) {
            if (this.activeMenu)
                this.activeMenu.close(BoxPointer.PopupAnimation.FADE);
            this._grabHelper.grab({ actor: menu.actor, focus: menu.sourceActor,
                                    onUngrab: Lang.bind(this, this._closeMenu, menu) });
        } else {
            this._grabHelper.ungrab({ actor: menu.actor });
        }
    },

    _changeMenu: function(newMenu) {
        newMenu.open(this.activeMenu ? BoxPointer.PopupAnimation.FADE
                                     : BoxPointer.PopupAnimation.FULL);
    },

    _onMenuSourceEnter: function(menu) {
        if (!this._grabHelper.grabbed)
            return Clutter.EVENT_PROPAGATE;

        if (this._grabHelper.isActorGrabbed(menu.actor))
            return Clutter.EVENT_PROPAGATE;

        this._changeMenu(menu);
        return Clutter.EVENT_PROPAGATE;
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
