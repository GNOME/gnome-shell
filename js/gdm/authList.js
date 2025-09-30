/*
 * Copyright 2017 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Graphene from 'gi://Graphene';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Main from '../ui/main.js';
import * as PopupMenu from '../ui/popupMenu.js';

export const SCROLL_ANIMATION_TIME = 500;

const ItemIconPopup = class extends PopupMenu.PopupMenu {
    constructor(sourceActor, title, subtitle) {
        super(sourceActor, 0.5, St.Side.TOP);

        this.box.add_style_class_name('login-dialog-item-icon-popup-box');
        this.actor.add_style_class_name('login-dialog-item-icon-popup');

        const labels = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
            style_class: 'login-dialog-item-icon-popup-labels',
        });
        labels.add_child(new St.Label({text: title}));
        labels.add_child(new St.Label({text: subtitle}));

        const item = new PopupMenu.PopupBaseMenuItem({
            reactive: false,
            can_focus: false,
        });
        item.add_child(labels);
        this.addMenuItem(item);

        sourceActor.connect('clicked', () => this.toggle());
        sourceActor.connect('destroy', () => this.destroy());

        this.actor.hide();

        this._menuManager = new PopupMenu.PopupMenuManager(sourceActor, {
            actionMode: Shell.ActionMode.NONE,
        });
        this._menuManager.addMenu(this);
    }
};

class ItemIcon extends St.Button {
    static [GObject.GTypeName] = 'ItemIcon';

    static {
        GObject.registerClass(this);
    }

    constructor(iconName, iconTitle, iconSubtitle) {
        super({
            style_class: 'login-dialog-item-icon',
            child: new St.Icon({icon_name: iconName}),
        });

        this._popup = new ItemIconPopup(this, iconTitle, iconSubtitle);
        Main.uiGroup.add_child(this._popup.actor);
    }
}

class AuthListItem extends St.Button {
    static [GObject.GTypeName] = 'AuthListItem';

    static [GObject.signals] = {
        'activate': {},
    };

    static {
        GObject.registerClass(this);
    }

    constructor(key, content) {
        const {title, subtitle, iconName, iconTitle, iconSubtitle} = content;

        super({
            style_class: 'login-dialog-auth-list-item',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            can_focus: true,
            reactive: true,
            accessible_name: [title, subtitle, iconTitle, iconSubtitle]
                .filter(p => p)
                .join(', '),
        });

        this.key = key;

        this._container = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
        });
        this._labelBox = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
            y_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
        });
        this._container.add_child(this._labelBox);

        if (title) {
            const label = new St.Label({
                text: title,
                style_class: 'login-dialog-auth-list-item-title',
                y_align: Clutter.ActorAlign.CENTER,
                x_expand: true,
            });
            this._labelBox.add_child(label);
        }

        if (subtitle) {
            const label = new St.Label({
                text: subtitle,
                style_class: 'login-dialog-auth-list-item-subtitle',
                y_align: Clutter.ActorAlign.CENTER,
                x_expand: true,
            });
            this._labelBox.add_child(label);
        }

        if (iconName && iconTitle && iconSubtitle) {
            const icon = new ItemIcon(iconName, iconTitle, iconSubtitle);
            icon.add_constraint(new Clutter.AlignConstraint({
                source: this._container,
                align_axis: Clutter.AlignAxis.X_AXIS,
                factor: 0.5,
            }));
            icon.add_constraint(new Clutter.AlignConstraint({
                source: this._container,
                align_axis: Clutter.AlignAxis.Y_AXIS,
                factor: 0.0,
                pivot_point: new Graphene.Point({x: 0.0, y: 0.35}),
            }));
            this._container.add_child(icon);
        }

        this.set_child(this._container);

        this.connect('key-focus-in',
            () => this._setSelected(true));
        this.connect('key-focus-out',
            () => this._setSelected(false));
        this.connect('notify::hover',
            () => this._setSelected(this.hover));

        this.connect('clicked', this._onClicked.bind(this));
    }

    _onClicked() {
        this.emit('activate');
    }

    _setSelected(selected) {
        if (selected) {
            this.add_style_pseudo_class('selected');
            this.grab_key_focus();
        } else {
            this.remove_style_pseudo_class('selected');
        }
    }
}

export class AuthList extends St.BoxLayout {
    static [GObject.GTypeName] = 'AuthList';

    static [GObject.signals] = {
        'activate': {param_types: [GObject.TYPE_STRING]},
        'item-added': {param_types: [AuthListItem.$gtype]},
    };

    static {
        GObject.registerClass(this);
    }

    constructor() {
        super({
            orientation: Clutter.Orientation.VERTICAL,
            style_class: 'login-dialog-auth-list-layout',
            x_align: Clutter.ActorAlign.FILL,
            y_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
        });

        this._box = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
            style_class: 'login-dialog-auth-list',
            pseudo_class: 'expanded',
        });

        this._scrollView = new St.ScrollView({
            style_class: 'login-dialog-auth-list-view',
            child: this._box,
        });
        this.add_child(this._scrollView);

        this._items = new Map();

        this.connect('key-focus-in', this._moveFocusToItems.bind(this));
    }

    _moveFocusToItems() {
        const hasItems = this.numItems > 0;

        if (!hasItems)
            return;

        if (global.stage.get_key_focus() !== this)
            return;

        const focusSet = this.navigate_focus(null, St.DirectionType.TAB_FORWARD, false);
        if (!focusSet) {
            const laters = global.compositor.get_laters();
            laters.add(Meta.LaterType.BEFORE_REDRAW, () => {
                this._moveFocusToItems();
                return GLib.SOURCE_REMOVE;
            });
        }
    }

    _onItemActivated(activatedItem) {
        this.emit('activate', activatedItem.key);
    }

    scrollToItem(item) {
        const box = item.get_allocation_box();

        const adjustment = this._scrollView.vadjustment;

        const value = (box.y1 + adjustment.step_increment / 2.0) - (adjustment.page_size / 2.0);
        adjustment.ease(value, {
            duration: SCROLL_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    addItem(key, content) {
        this.removeItem(key);

        const item = new AuthListItem(key, content);
        this._box.add_child(item);

        this._items.set(key, item);

        item.connect('activate', this._onItemActivated.bind(this));

        // Try to keep the focused item front-and-center
        item.connect('key-focus-in', () => this.scrollToItem(item));

        this._moveFocusToItems();

        this.emit('item-added', item);
    }

    removeItem(key) {
        if (!this._items.has(key))
            return;

        const item = this._items.get(key);

        item.destroy();

        this._items.delete(key);
    }

    get numItems() {
        return this._items.size;
    }

    clear() {
        this._box.destroy_all_children();
        this._items.clear();
    }
}
