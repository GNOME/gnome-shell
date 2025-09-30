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

const SCROLL_ANIMATION_TIME = 500;

const PopupLabel = class extends PopupMenu.PopupMenu {
    constructor(sourceActor, label) {
        super(sourceActor, 0.5, St.Side.TOP);

        const menuItem = new PopupMenu.PopupBaseMenuItem({
            reactive: false,
            can_focus: false,
        });
        menuItem.add_child(label);
        this.addMenuItem(menuItem);

        Main.uiGroup.add_child(this.actor);

        // Overwrite min-width to allow smaller width than the default
        this.actor.set_style('min-width: 0;');
    }
};

const AuthListItem = GObject.registerClass({
    Signals: {'activate': {}},
}, class AuthListItem extends St.Button {
    _init(key, content) {
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

        const {commonName, description, organization} = content;
        this._appendLine(commonName);
        this._appendLine(description);
        this._appendIcon(organization);

        super._init({
            style_class: 'login-dialog-auth-list-item',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            can_focus: true,
            child: this._container,
            reactive: true,
            accessible_name: [commonName, organization, description]
                .filter(p => p)
                .join(' '),
        });

        this.connect('key-focus-in',
            () => this._setSelected(true));
        this.connect('key-focus-out',
            () => this._setSelected(false));
        this.connect('notify::hover',
            () => this._setSelected(this.hover));

        this.connect('clicked', this._onClicked.bind(this));
    }

    _appendLine(text) {
        if (!text)
            return;

        if (!this._firstLine) {
            const label = new St.Label({
                text,
                style_class: 'login-dialog-auth-list-item-first-line',
                y_align: Clutter.ActorAlign.CENTER,
                x_expand: true,
            });
            this._labelBox.add_child(label);
            this._firstLine = label;
        } else if (!this._secondLine) {
            const label = new St.Label({
                text,
                style_class: 'login-dialog-auth-list-item-second-line',
                y_align: Clutter.ActorAlign.CENTER,
                x_expand: true,
            });
            this._labelBox.add_child(label);
            this._secondLine = label;
        }
    }

    _appendIcon(text) {
        if (!text || this._icon)
            return;

        const icon = new St.Button({
            style_class: 'login-dialog-auth-list-item-icon',
            child: new St.Icon({icon_name: 'vcard-symbolic'}),
        });
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

        const textLines = [_('Organization'), text];
        this.popupLabel = this._createPopupLabel(icon, textLines);

        this._container.add_child(icon);
        this._icon = icon;
    }

    _createPopupLabel(sourceActor, textLines) {
        const labelsContainer = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
            style_class: 'login-dialog-auth-list-item-popup-labels',
        });
        textLines.forEach(text => {
            labelsContainer.add_child(new St.Label({text}));
        });

        const popup = new PopupLabel(sourceActor, labelsContainer);
        popup.box.add_style_class_name('login-dialog-auth-list-item-popup-box');
        popup.actor.hide();

        if (!this._menuManager) {
            this._menuManager = new PopupMenu.PopupMenuManager(sourceActor, {
                actionMode: Shell.ActionMode.NONE,
            });
        }
        this._menuManager.addMenu(popup);

        sourceActor.connect('clicked', () => popup.toggle());

        return popup;
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
});
