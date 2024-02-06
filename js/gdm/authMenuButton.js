// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/*
 * Copyright 2024 Red Hat, Inc
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

import Atk from 'gi://Atk';
import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as BoxPointer from '../ui/boxpointer.js';
import * as Main from '../ui/main.js';
import * as Params from '../misc/params.js';
import * as PopupMenu from '../ui/popupMenu.js';

export const AuthMenuButton = GObject.registerClass({
    Signals: {'active-item-changed': {}},
}, class AuthMenuButton extends St.Bin {
    _init(params) {
        params = Params.parse(params, {
            title: '',
            iconName: '',
        });

        let button = new St.Button({
            style_class: 'login-dialog-button login-dialog-auth-menu-button',
            icon_name: params.iconName,
            reactive: true,
            track_hover: true,
            can_focus: true,
            accessible_name: params.title,
            accessible_role: Atk.Role.MENU,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });

        super._init({child: button});
        this._button = button;

        this._menu = new PopupMenu.PopupMenu(this._button, 0, St.Side.BOTTOM);
        this._menu.box.add_style_class_name('login-dialog-auth-menu-button-popup-menu-box');
        Main.uiGroup.add_child(this._menu.actor);
        this._menu.actor.hide();

        this._header = new St.Label({
            text: params.title,
            style_class: 'login-dialog-auth-menu-button-title',
            y_align: Clutter.ActorAlign.START,
            y_expand: true,
        });
        this._menu.box.add_child(this._header);

        this._menu.connect('open-state-changed', (menu, isOpen) => {
            if (isOpen)
                this._button.add_style_pseudo_class('active');
            else
                this._button.remove_style_pseudo_class('active');
        });

        this._manager = new PopupMenu.PopupMenuManager(this._button,
            {actionMode: Shell.ActionMode.NONE});
        this._manager.addMenu(this._menu);

        this._button.connect('clicked', () => this._menu.toggle());

        this._items = new Map();
        this._activeItem = null;
        this.updateSensitivity(true);
    }

    _getMenuItem(item) {
        if (!item)
            return null;

        return this._items.get(JSON.stringify(item));
    }

    updateSensitivity(sensitive) {
        this._sensitive = sensitive;

        if (this._items.size <= 1)
            sensitive = false;

        this._button.reactive = sensitive;
        this._button.can_focus = sensitive;
        this.opacity = sensitive ? 255 : 0;
        this._menu.close(BoxPointer.PopupAnimation.NONE);
    }

    _updateOrnament() {
        for (const [itemKey, menuItem] of this._items)
            menuItem.setOrnament(PopupMenu.Ornament.NO_DOT);

        const activeMenuItem = this._getMenuItem(this._activeItem);

        if (activeMenuItem)
            activeMenuItem.setOrnament(PopupMenu.Ornament.DOT);
    }

    _findItems(searchCriteria) {
        let items = [];
        for (const [itemKey, menuItem] of this._items) {
            const item = JSON.parse(itemKey);

            let criteriaMismatch = false;
            for (const key of Object.keys(searchCriteria)) {
                if (!searchCriteria[key])
                    continue;

                if (item[key] === searchCriteria[key])
                    continue;

                criteriaMismatch = true;
                break;
            }

            if (criteriaMismatch)
                continue;

            items.push(itemKey);
        }

        return items;
    }

    clearItems(searchCriteria) {
        if (!searchCriteria)
            searchCriteria = {};

        const activeMenuItem = this._getMenuItem(this._activeItem);
        const itemKeys = this._findItems(searchCriteria);
        for (const itemKey of itemKeys) {
            const menuItem = this._items.get(itemKey);

            if (activeMenuItem === menuItem)
                this._activeItem = null;

            menuItem.destroy();
            this._items.delete(itemKey);
        }
    }

    addItem(item) {
        let menuItem = this._getMenuItem(item);

        if (menuItem)
            throw new Error(`Duplicate item ${JSON.stringify(item)}`);

        if (!item.name)
            throw new Error(`item ${JSON.stringify(item)} lacks name`);

        menuItem = new PopupMenu.PopupMenuItem(item.name, {
	    style_class: 'login-dialog-auth-menu-button-item',
	});
        menuItem.setOrnament(PopupMenu.Ornament.NO_DOT);
        menuItem.connect('activate', () => {
            this.setActiveItem(item);
        });

        this._menu.addMenuItem(menuItem);
        this._items.set(JSON.stringify(item), menuItem);
        this.updateSensitivity(this._sensitive);
    }

    _resolveItem(searchCriteria) {
        const itemKeys = this._findItems(searchCriteria);

        if (itemKeys.length == 0)
            throw new Error(`Unknown item ${JSON.stringify(searchCriteria)}`);

        if (itemKeys.length > 1)
            throw new Error(`Matched multiple items with criteria ${JSON.stringify(searchCriteria)}`);

        const item = JSON.parse(itemKeys[0]);
        const menuItem = this._items.get(itemKeys[0]);
        return { item, menuItem };
    }

    setActiveItem(searchCriteria) {
        const { item, menuItem } = this._resolveItem(searchCriteria);
        const activeMenuItem = this._getMenuItem(this._activeItem);

        if (menuItem === activeMenuItem)
            return;

        this._activeItem = item;
        this._updateOrnament();
        this.emit('active-item-changed');
    }

    getActiveItem() {
        return this._activeItem;
    }

    close() {
        this._menu.close();
    }
});
