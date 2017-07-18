// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
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

const Clutter = imports.gi.Clutter;
const GObject = imports.gi.GObject;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Signals = imports.signals;
const St = imports.gi.St;

const Tweener = imports.ui.tweener;

const _SCROLL_ANIMATION_TIME = 0.5;

const AuthListItem = new Lang.Class({
    Name: 'AuthListItem',

    _init: function(key, text) {
        this.key = key;
        let label = new St.Label({ style_class: 'auth-list-item-label',
                                   y_align: Clutter.ActorAlign.CENTER });
        label.text = text;

        this.actor = new St.Button({ style_class: 'login-dialog-user-list-item',
                                     button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                     can_focus: true,
                                     child: label,
                                     reactive: true,
                                     x_align: St.Align.START,
                                     x_fill: true });

        this.actor.connect('key-focus-in', () => {
            this._setSelected(true);
        });
        this.actor.connect('key-focus-out', () => {
            this._setSelected(false);
        });
        this.actor.connect('notify::hover', () => {
            this._setSelected(this.actor.hover);
        });

        this.actor.connect('clicked', Lang.bind(this, this._onClicked));
    },

    _onClicked: function() {
        this.emit('activate');
    },

    _setSelected: function(selected) {
        if (selected) {
            this.actor.add_style_pseudo_class('selected');
            this.actor.grab_key_focus();
        } else {
            this.actor.remove_style_pseudo_class('selected');
        }
    }
});
Signals.addSignalMethods(AuthListItem.prototype);

const AuthList = new Lang.Class({
    Name: 'AuthList',

    _init: function() {
        this.actor = new St.ScrollView({ style_class: 'login-dialog-user-list-view'});
        this.actor.set_policy(Gtk.PolicyType.NEVER,
                              Gtk.PolicyType.AUTOMATIC);

        this._box = new St.BoxLayout({ vertical: true,
                                       style_class: 'login-dialog-user-list',
                                       pseudo_class: 'expanded' });

        this.actor.add_actor(this._box);
        this._items = {};

        this.actor.connect('key-focus-in', Lang.bind(this, this._moveFocusToItems));
    },

    _moveFocusToItems: function() {
        let hasItems = Object.keys(this._items).length > 0;

        if (!hasItems)
            return;

        if (global.stage.get_key_focus() != this.actor)
            return;

        let focusSet = this.actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
        if (!focusSet) {
            Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this, function() {
                this._moveFocusToItems();
                return false;
            }));
        }
    },

    _onItemActivated: function(activatedItem) {
        this.emit('activate', activatedItem.key);
    },

    scrollToItem: function(item) {
        let box = item.actor.get_allocation_box();

        let adjustment = this.actor.get_vscroll_bar().get_adjustment();

        let value = (box.y1 + adjustment.step_increment / 2.0) - (adjustment.page_size / 2.0);
        Tweener.removeTweens(adjustment);
        Tweener.addTween (adjustment,
                          { value: value,
                            time: _SCROLL_ANIMATION_TIME,
                            transition: 'easeOutQuad' });
    },

    jumpToItem: function(item) {
        let box = item.actor.get_allocation_box();

        let adjustment = this.actor.get_vscroll_bar().get_adjustment();

        let value = (box.y1 + adjustment.step_increment / 2.0) - (adjustment.page_size / 2.0);

        adjustment.set_value(value);
    },

    getItem: function(key) {
        let item = this._items[key];

        if (!item)
            return null;

        return item;
    },

    addItem: function(key, text) {
        this.removeItem(key);

        let item = new AuthListItem(key, text);
        this._box.add(item.actor, { x_fill: true });

        this._items[key] = item;

        item.connect('activate',
                     Lang.bind(this, this._onItemActivated));

        // Try to keep the focused item front-and-center
        item.actor.connect('key-focus-in',
                           Lang.bind(this,
                                     function() {
                                         this.scrollToItem(item);
                                     }));

        this._moveFocusToItems();

        this.emit('item-added', item);
    },

    removeItem: function(key) {
        let item = this._items[key];

        if (!item)
            return;

        item.actor.destroy();
        delete this._items[key];
    },

    numItems: function() {
        return Object.keys(this._items).length;
    },

    clear: function() {
        this._box.destroy_all_children();
        this._items = {};
    }
});
Signals.addSignalMethods(AuthList.prototype);
