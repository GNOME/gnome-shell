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
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import St from 'gi://St';

const SCROLL_ANIMATION_TIME = 500;

const AuthListItem = GObject.registerClass({
    Signals: {'activate': {}},
}, class AuthListItem extends St.Button {
    _init(key, text) {
        this.key = key;
        const label = new St.Label({
            text,
            style_class: 'login-dialog-auth-list-label',
            y_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
        });

        super._init({
            style_class: 'login-dialog-auth-list-item',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            can_focus: true,
            child: label,
            reactive: true,
        });

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
});

export const AuthList = GObject.registerClass({
    Signals: {
        'activate': {param_types: [GObject.TYPE_STRING]},
        'item-added': {param_types: [AuthListItem.$gtype]},
    },
}, class AuthList extends St.BoxLayout {
    _init() {
        super._init({
            orientation: Clutter.Orientation.VERTICAL,
            style_class: 'login-dialog-auth-list-layout',
            x_align: Clutter.ActorAlign.START,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this.label = new St.Label({style_class: 'login-dialog-auth-list-title'});
        this.add_child(this.label);

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
                return false;
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

    addItem(key, text) {
        this.removeItem(key);

        const item = new AuthListItem(key, text);
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
        this.label.text = '';
        this._box.destroy_all_children();
        this._items.clear();
    }
});
