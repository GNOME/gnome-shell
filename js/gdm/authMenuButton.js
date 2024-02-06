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

import * as Main from '../ui/main.js';
import * as PopupMenu from '../ui/popupMenu.js';

const VISIBILITY_ANIMATION_TIME = 200;

class AuthMenuItemSection extends PopupMenu.PopupMenuSection {
    constructor(sectionName) {
        super();

        this.actor.add_style_class_name('login-dialog-auth-menu-item-section');

        if (sectionName) {
            const itemSection = new PopupMenu.PopupMenuItem(sectionName, {
                reactive: false,
                can_focus: false,
            });
            itemSection.label.style_class = 'login-dialog-auth-menu-item-section-label';
            itemSection.label.x_expand = true;
            itemSection.label.x_align = Clutter.ActorAlign.CENTER;
            this.addMenuItem(itemSection);
        }
    }
}

class AuthMenuItem extends PopupMenu.PopupBaseMenuItem {
    static {
        GObject.registerClass(this);
    }

    constructor(item, params = {}) {
        super(params);

        if (item.description && item.iconName) {
            const icon = new St.Icon({
                style_class: 'login-dialog-auth-menu-item-icon',
                icon_name: item.iconName,
                x_align: Clutter.ActorAlign.CENTER,
            });
            this.add_child(icon);

            const box = new St.BoxLayout({
                vertical: true,
                style_class: 'login-dialog-auth-menu-item-box',
            });
            const nameLabel = new St.Label({
                text: item.name,
                style_class: 'login-dialog-auth-menu-item-box-name',
            });
            const descriptionLabel = new St.Label({
                text: item.description,
                style_class: 'login-dialog-auth-menu-item-box-description',
            });
            box.add_child(nameLabel);
            box.add_child(descriptionLabel);
            this.add_child(box);

            this.label_actor = nameLabel;
        } else {
            // When no extra info is provided, use the default style
            const nameLabel = new St.Label({
                text: item.name,
                y_expand: true,
                y_align: Clutter.ActorAlign.CENTER,
            });
            this.add_child(nameLabel);
            this.label_actor = nameLabel;
        }
    }
}

export class AuthMenuButton extends St.Button {
    static [GObject.properties] = {
        'read-only': GObject.ParamSpec.boolean(
            'read-only', null, null,
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT_ONLY,
            false),
        'section-order': GObject.ParamSpec.jsobject(
            'section-order', null, null,
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT_ONLY),
        'animate-visibility': GObject.ParamSpec.boolean(
            'animate-visibility', null, null,
            GObject.ParamFlags.READWRITE,
            false),
    };

    static [GObject.signals] = {
        'active-item-changed': {param_types: [GObject.TYPE_STRING]},
    };

    static {
        GObject.registerClass(this);
    }

    constructor(params = {}) {
        super({
            icon_name: 'cog-wheel-symbolic',
            sectionOrder: [],
            ...params,
            style_class: 'login-dialog-button login-dialog-auth-menu-button',
            can_focus: true,
            accessible_role: Atk.Role.MENU,
            button_mask: St.ButtonMask.PRIMARY | St.ButtonMask.SECONDARY,
        });
        this.bind_property('reactive',
            this, 'can-focus',
            GObject.BindingFlags.SYNC_CREATE);

        this._menu = new PopupMenu.PopupMenu(this, 0, St.Side.BOTTOM);
        this._menu.box.add_style_class_name('login-dialog-auth-menu-button-popup');
        Main.uiGroup.add_child(this._menu.actor);
        this._menu.actor.hide();

        this._manager = new PopupMenu.PopupMenuManager(this,
            {actionMode: Shell.ActionMode.NONE});
        this._manager.addMenu(this._menu);

        this.connect('clicked', () => this._onClicked());

        this._items = new Map();
        this._sections = new Map();
        this._activeItems = new Set();
    }

    _onClicked() {
        this._menu.toggle();
    }

    _getVisibleItemsCount() {
        return Array.from(this._items.values()).filter(item => item.visible).length;
    }

    _updateOrnament() {
        for (const menuItem of this._items.values())
            menuItem.setOrnament(PopupMenu.Ornament.NO_DOT);

        for (const itemKey of this._activeItems) {
            const menuItem = this._items.get(itemKey);
            if (menuItem)
                menuItem.setOrnament(PopupMenu.Ornament.DOT);
        }
    }

    _deepEquals(a, b) {
        if (a === b)
            return true;

        if (a == null || b == null)
            return false;

        if (typeof a !== 'object' || typeof b !== 'object')
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
        this._findItems(searchCriteria).forEach(itemKey => {
            const menuItem = this._items.get(itemKey);
            this._activeItems.delete(itemKey);
            menuItem.destroy();
            this._items.delete(itemKey);
        });

        this._sections.keys().forEach(sectionName => {
            const itemsInSection = this._findItems({sectionName});
            if (itemsInSection.length === 0) {
                const section = this._sections.get(sectionName);
                if (section) {
                    section.destroy();
                    this._sections.delete(sectionName);
                }
            }
        });

        this.updateVisibility();
    }

    addItem(item) {
        const itemKey = JSON.stringify(item);
        if (this._items.has(itemKey))
            throw new Error(`Duplicate item ${itemKey}`);

        if (!item.name)
            throw new Error(`item ${itemKey} lacks name`);

        const menuItem = new AuthMenuItem(item, {reactive: !this.readOnly});
        menuItem.connect('activate', () => {
            this.setActiveItem(item);
        });

        const section = this._getSection(item.sectionName);
        section.addMenuItem(menuItem);

        this._items.set(itemKey, menuItem);
        this.updateVisibility();
    }

    _getSection(sectionName) {
        const key = sectionName ?? null;
        let section = this._sections.get(key);
        if (section)
            return section;

        section = new AuthMenuItemSection(sectionName);

        if (key === null) {
            // Empty section is always last
            this._menu.addMenuItem(section);
        } else {
            const orderIndex = this.sectionOrder.indexOf(sectionName);
            if (orderIndex !== -1) {
                // Section is in sectionOrder, add at its position
                this._menu.addMenuItem(section, orderIndex);
            } else {
                // Section not in sectionOrder, add before empty section if it exists
                const emptySection = this._sections.get(null);
                if (emptySection) {
                    const emptyIndex = this._menu._getMenuItems().indexOf(emptySection);
                    this._menu.addMenuItem(section, emptyIndex);
                } else {
                    this._menu.addMenuItem(section);
                }
            }
        }

        this._sections.set(key, section);

        return section;
    }

    _canBeVisible() {
        const visibleSections = this._getVisibleSections();

        for (const [sectionName, section] of this._sections)
            section.actor.visible = visibleSections.includes(sectionName);

        return visibleSections.length > 0;
    }

    updateVisibility({visible = true} = {}) {
        visible &&= this._canBeVisible();

        if (this._targetVisibility === visible)
            return;

        this._targetVisibility = visible;

        if (this.animateVisibility) {
            if (visible) {
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
            this.opacity = visible ? 255 : 0;
            this.visible = visible;
        }
    }

    _getVisibleSections() {
        return Array.from(this._sections.keys()).filter(sectionName =>
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

    closeMenu() {
        this._menu.close();
    }
}

export class AuthMenuButtonIndicator extends AuthMenuButton {
    static {
        GObject.registerClass(this);
    }

    constructor(params = {}) {
        super({
            readOnly: true,
            ...params,
        });

        this._createChild();

        this.add_style_class_name('login-dialog-auth-menu-button-indicator');
    }

    _createChild() {
        const container = new St.BoxLayout({
            x_align: Clutter.ActorAlign.START,
        });

        this._iconsBox = new St.BoxLayout({
            style_class: 'login-dialog-auth-menu-button-indicator-icons',
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
        });
        container.add_child(this._iconsBox);

        this._descriptionLabel = new St.Label({
            style_class: 'login-dialog-auth-menu-button-indicator-description',
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._descriptionLabel.bind_property_full('text',
            this._descriptionLabel, 'visible',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, source) => [true, !!source],
            null);
        container.add_child(this._descriptionLabel);

        this.child = container;
    }

    _onClicked() {
        // When there's only one item, all the info (icon, description) is
        // already visible in the button, so no need to show the menu
        if (this._getVisibleItemsCount() > 1)
            this._menu.toggle();
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
    _canBeVisible() {
        return this._items.size > 0;
    }
}
