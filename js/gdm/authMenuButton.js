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

/*
 * AuthMenuButton implements a menu button that is used to manage login options
 * (authentication methods, session)
 *
 * Item objects are mapped to menu items internally using the item objects
 * themselves as their own key.
 * Each item can have arbitrary properties, but certain properties have special
 * meaning:
 *   - name: (required) Display text for the item
 *   - iconName: Icon to show for the item and optionally in the button
 *   - description: Additional descriptive text shown below the name
 *   - sectionName: Groups items into labeled sections
 *
 * The button supports searching for items using partial criteria objects. Any
 * item matching all non-null properties in the criteria will be returned.
 * For example:
 *   {sectionName: 'foo'} matches all items in section 'foo'
 *   {name: 'bar', sectionName: 'foo'} matches the item named 'bar' in
 *     section 'foo'
 *
 * Items within sections can be ordered using the sectionOrder parameter during
 * initialization. Sections not in sectionOrder appear at the end.
 *
 * Only one item per section can be active at a time. Setting a new active item
 * deactivates any other active item in the same section.
 */

import Atk from 'gi://Atk';
import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as BoxPointer from '../ui/boxpointer.js';
import * as Main from '../ui/main.js';
import * as PopupMenu from '../ui/popupMenu.js';

const VISIBILITY_ANIMATION_TIME = 200;

class AuthMenuItem extends PopupMenu.PopupImageMenuItem {
    static [GObject.GTypeName] = 'AuthMenuItem';

    static {
        GObject.registerClass(this);
    }

    constructor(item, params) {
        super(item.name, item.iconName || '', params);

        // Move ornament to the left
        this.set_child_at_index(this._ornamentIcon, 0);
    }

    updateLabelActor(labelActor) {
        this.insert_child_below(labelActor, this.label_actor);
        this.remove_child(this.label_actor);

        this.label_actor.destroy();
        this.label_actor = labelActor;
    }
}

class AuthMenuItemIndicator extends AuthMenuItem {
    static [GObject.GTypeName] = 'AuthMenuItemIndicator';

    static {
        GObject.registerClass(this);
    }

    constructor(item, params) {
        super(item, params);

        if (item.description) {
            const box = new St.BoxLayout({
                vertical: true,
                style_class: 'login-dialog-auth-menu-item-indicator',
            });

            const nameLabel = new St.Label({
                text: item.name,
                style_class: 'login-dialog-auth-menu-item-indicator-name',
            });

            const descriptionLabel = new St.Label({
                text: item.description,
                style_class: 'login-dialog-auth-menu-item-indicator-description',
            });

            box.add_child(nameLabel);
            box.add_child(descriptionLabel);

            this.updateLabelActor(box);
        }
    }
}

export class AuthMenuButton extends St.Bin {
    static [GObject.GTypeName] = 'AuthMenuButton';

    static [GObject.properties] = {
        'title': GObject.ParamSpec.string(
            'title', null, null,
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT_ONLY,
            ''),
        'icon-name': GObject.ParamSpec.string(
            'icon-name', null, null,
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT_ONLY,
            ''),
        'read-only': GObject.ParamSpec.boolean(
            'read-only', null, null,
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT_ONLY,
            false),
        'section-order': GObject.ParamSpec.jsobject(
            'section-order', null, null,
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT_ONLY),
        'animate-visibility': GObject.ParamSpec.boolean(
            'animate-visibility', null, null,
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT_ONLY,
            false),
    };

    static [GObject.signals] = {
        'active-item-changed': {param_types: [GObject.TYPE_STRING]},
    };

    static {
        GObject.registerClass(this);
    }

    constructor(params) {
        params.sectionOrder ??= [];
        super(params);

        const button = new St.Button({
            style_class: 'login-dialog-button login-dialog-auth-menu-button',
            child: new St.Icon({icon_name: this.iconName}),
            reactive: true,
            track_hover: true,
            can_focus: true,
            accessible_name: this.title,
            accessible_role: Atk.Role.MENU,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this.child = button;

        this._button = button;

        this._menu = new PopupMenu.PopupMenu(this, 0, St.Side.BOTTOM);
        this._menu.box.add_style_class_name('login-dialog-auth-menu-button-popup');
        Main.uiGroup.add_child(this._menu.actor);
        this._menu.actor.hide();

        this._menu.connect('open-state-changed', (_menu, isOpen) => {
            if (this.readOnly) {
                if (isOpen)
                    this._addMenuShield();
                else
                    this._removeMenuShield();
            }
        });

        this._manager = new PopupMenu.PopupMenuManager(this._button,
            {actionMode: Shell.ActionMode.NONE});
        this._manager.addMenu(this._menu);

        this._button.connect('clicked', () => {
            if (this.readOnly && this._getVisibleItemsCount() === 1)
                return;
            this._menu.toggle();
        });

        this._items = new Map();
        this._activeItems = new Set();
        this._headers = new Map();
        this.updateSensitivity(true);
    }

    _addMenuShield() {
        if (this._menuShield)
            return;

        this._menuShield = new St.Widget({
            reactive: true,
            opacity: 0,
        });

        Main.uiGroup.add_child(this._menuShield);

        this._menuShield.add_constraint(new Clutter.BindConstraint({
            source: this._menu.actor,
            coordinate: Clutter.BindCoordinate.ALL,
        }));
    }

    _removeMenuShield() {
        if (this._menuShield) {
            this._menuShield.destroy();
            this._menuShield = null;
        }
    }

    _getMenuItem(item) {
        if (!item)
            return null;

        return this._items.get(JSON.stringify(item));
    }

    _getVisibleItemsCount() {
        return Array.from(this._items.values()).filter(item => item.visible).length;
    }

    updateReactive(reactive) {
        this._button.reactive = reactive;
        this._button.can_focus = reactive;
    }

    updateSensitivity(sensitive) {
        this._sensitive = sensitive;

        const visibleItems = this._getVisibleItemsCount();
        if (visibleItems === 0 || (visibleItems <= 1 && !this.readOnly))
            sensitive = false;

        this._button.reactive = sensitive;
        this._button.can_focus = sensitive;
        this._menu.close(BoxPointer.PopupAnimation.NONE);

        if (this.animateVisibility) {
            if (sensitive) {
                this.opacity = 0;
                this.visible = true;
                this.ease({
                    opacity: 255,
                    duration: VISIBILITY_ANIMATION_TIME,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                });
            } else {
                this.ease({
                    opacity: 0,
                    duration: VISIBILITY_ANIMATION_TIME,
                    mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                    onComplete: () => {
                        this.visible = false;
                    },
                });
            }
        } else {
            this.opacity = sensitive ? 255 : 0;
            this.visible = sensitive;
        }
    }

    _updateOrnament() {
        for (const menuItem of this._items.values())
            menuItem.setOrnament(PopupMenu.Ornament.NO_DOT);

        for (const itemKey of this._activeItems) {
            const menuItem = this._getMenuItem(JSON.parse(itemKey));
            if (menuItem)
                menuItem.setOrnament(PopupMenu.Ornament.DOT);
        }
    }

    getSections() {
        const sectionsSet = new Set();
        for (const itemKey of this._items.keys()) {
            const item = JSON.parse(itemKey);
            if (item.sectionName)
                sectionsSet.add(item.sectionName);
        }

        const sections = Array.from(sectionsSet);
        sections.sort((a, b) => {
            const indexA = this.sectionOrder.indexOf(a);
            const indexB = this.sectionOrder.indexOf(b);

            if (indexA !== -1 && indexB !== -1)
                return indexA - indexB;
            if (indexA !== -1)
                return -1;
            if (indexB !== -1)
                return 1;
            return 0;
        });

        return sections;
    }

    _deepEquals(a, b) {
        if (a === b)
            return true;

        if (a == null || b == null)
            return false;

        const keysA = Object.keys(a);
        const keysB = Object.keys(b);

        if (keysA.length !== keysB.length)
            return false;

        for (const key of keysA) {
            if (!keysB.includes(key))
                return false;

            if (!this._deepEquals(a[key], b[key]))
                return false;
        }

        return true;
    }

    _findItems(searchCriteria) {
        const items = [];
        for (const itemKey of this._items.keys()) {
            const item = JSON.parse(itemKey);

            let criteriaMismatch = false;
            for (const key of Object.keys(searchCriteria)) {
                if (!searchCriteria[key])
                    continue;

                if (this._deepEquals(item[key], searchCriteria[key]))
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

    getItems(searchCriteria = {}) {
        return this._findItems(searchCriteria).map(itemKey => JSON.parse(itemKey));
    }

    clearItems(searchCriteria = {}) {
        const sections = this.getSections();

        this._findItems(searchCriteria).forEach(itemKey => {
            const menuItem = this._items.get(itemKey);
            this._activeItems.delete(itemKey);
            menuItem.destroy();
            this._items.delete(itemKey);
        });

        sections.forEach(sectionName => {
            const itemsInSection = this._findItems({sectionName});
            if (itemsInSection.length === 0) {
                const header = this._headers.get(sectionName);
                if (header) {
                    header.destroy();
                    this._headers.delete(sectionName);
                }
            }
        });

        this._updateVisibility();
        this.updateSensitivity(this._sensitive);
    }

    addItem(item) {
        const itemKey = JSON.stringify(item);
        if (this._items.has(itemKey))
            throw new Error(`Duplicate item ${itemKey}`);

        if (!item.name)
            throw new Error(`item ${itemKey} lacks name`);

        const sectionName = item.sectionName ?? null;
        if (sectionName && !this._headers.has(sectionName)) {
            const header = new St.Label({
                text: sectionName,
                style_class: 'login-dialog-auth-menu-header',
                y_align: Clutter.ActorAlign.START,
                y_expand: true,
            });

            let insertIndex = 0;
            const orderIndex = this.sectionOrder.indexOf(sectionName);

            if (orderIndex === -1) {
                insertIndex = -1;
            } else {
                for (const existingSectionName of this._headers.keys()) {
                    const existingIndex = this.sectionOrder.indexOf(existingSectionName);
                    if (existingIndex === -1 || existingIndex > orderIndex)
                        break;
                    insertIndex++;
                }
            }

            this._menu.box.insert_child_at_index(header, insertIndex);
            this._headers.set(sectionName, header);
        }

        const menuItem = this._createMenuItem(item);
        menuItem.setOrnament(PopupMenu.Ornament.HIDDEN);

        menuItem.connect('activate', () => {
            this.setActiveItem(item);
        });

        if (sectionName) {
            const children = this._menu.box.get_children();
            const header = this._headers.get(sectionName);
            const headerIndex = children.indexOf(header);

            let insertIndex = headerIndex + 1;
            while (insertIndex < children.length && children[insertIndex] instanceof AuthMenuItem)
                insertIndex++;

            this._menu.box.insert_child_at_index(menuItem, insertIndex);
        } else {
            this._menu.box.add_child(menuItem);
        }

        this._items.set(itemKey, menuItem);
        this._updateVisibility();
        this.updateSensitivity(this._sensitive);
    }

    _createMenuItem(item) {
        return new AuthMenuItem(item);
    }

    _updateVisibility() {
        const visibleSections = this._getVisibleSections();

        for (const [sectionName, header] of this._headers) {
            const showThisSection = visibleSections.includes(sectionName);

            header.visible = showThisSection;

            const sectionItems = this._findItems({sectionName});
            for (const itemKey of sectionItems) {
                const menuItem = this._items.get(itemKey);
                menuItem.visible = showThisSection;
            }
        }

        this._button.visible = visibleSections.length > 0;
    }

    _getVisibleSections() {
        return Array.from(this._headers.keys()).filter(sectionName =>
            this._getSectionItemCount(sectionName) > 1
        );
    }

    _getSectionItemCount(sectionName) {
        return this._findItems({sectionName}).length;
    }

    _resolveItem(searchCriteria) {
        const itemKeys = this._findItems(searchCriteria);

        if (!itemKeys.length)
            throw new Error(`Unknown item ${JSON.stringify(searchCriteria)}`);

        if (itemKeys.length > 1)
            throw new Error(`Matched multiple items with criteria ${JSON.stringify(searchCriteria)}`);

        const item = JSON.parse(itemKeys[0]);
        const menuItem = this._items.get(itemKeys[0]);
        return {item, menuItem};
    }

    setActiveItem(searchCriteria) {
        const {item} = this._resolveItem(searchCriteria);
        const itemKey = JSON.stringify(item);

        if (this._activeItems.has(itemKey))
            return;

        const sectionName = item.sectionName ?? null;
        const activeInSection = this._findItems({sectionName})
            .filter(key => this._activeItems.has(key));

        for (const key of activeInSection)
            this._activeItems.delete(key);

        this._activeItems.add(itemKey);
        this._updateOrnament();
        this.emit('active-item-changed', sectionName);
    }

    getActiveItem(searchCriteria = {}) {
        const activeKeys = this._findItems(searchCriteria)
            .filter(key => this._activeItems.has(key));

        if (activeKeys.length === 0)
            return null;

        if (activeKeys.length > 1)
            throw new Error(`Multiple active items found with criteria ${JSON.stringify(searchCriteria)}`);

        return JSON.parse(activeKeys[0]);
    }

    close() {
        this._menu.close();
    }
}

export class AuthMenuButtonIndicator extends AuthMenuButton {
    static [GObject.GTypeName] = 'AuthMenuButtonIndicator';

    static {
        GObject.registerClass(this);
    }

    constructor(params) {
        params.readOnly = true;
        super(params);

        this._button.add_style_class_name('login-dialog-auth-menu-button-indicator');

        const container = new St.BoxLayout({
            x_align: Clutter.ActorAlign.START,
        });

        this._iconsBox = new St.BoxLayout({
            style_class: 'login-dialog-auth-menu-button-indicator-icons',
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
        });
        this._button.child = this._iconsBox;

        this.remove_child(this._button);
        container.add_child(this._button);

        this._descriptionLabel = new St.Label({
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._descriptionLabel.bind_property_full('text',
            this._descriptionLabel, 'visible',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, source) => [true, !!source],
            null);
        container.add_child(this._descriptionLabel);

        this.child = container;

        this._menu.setSourceActor(this._button);
    }

    addItem(item) {
        if (item.iconName) {
            const icon = new St.Icon({
                icon_name: item.iconName,
                style_class: 'login-dialog-auth-menu-button-indicator-icon',
            });
            this._iconsBox.add_child(icon);
        }

        super.addItem(item);
    }

    _createMenuItem(item) {
        return new AuthMenuItemIndicator(item);
    }

    clearItems(searchCriteria = {}) {
        this._findItems(searchCriteria).forEach(itemKey => {
            const item = JSON.parse(itemKey);
            if (item.iconName) {
                this._iconsBox.get_children()
                    .find(icon => icon.icon_name === item.iconName)
                    ?.destroy();
            }
        });

        super.clearItems(searchCriteria);
        this.updateDescriptionLabel();
    }

    updateDescriptionLabel() {
        const [item] = this._items.size === 1 ? this.getItems() : [];
        this._descriptionLabel.text = item?.description ?? '';
    }

    // Override to force visibility even when there's only one item
    _updateVisibility() {
    }
}
