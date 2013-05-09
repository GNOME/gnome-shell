// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const PopupMenu = imports.ui.popupMenu;

/**
 * RemoteMenu:
 *
 * A PopupMenu that tracks a GMenuModel and shows its actions
 * (exposed by GApplication/GActionGroup)
 */
const RemoteMenu = new Lang.Class({
    Name: 'RemoteMenu',
    Extends: PopupMenu.PopupMenu,

    _init: function(sourceActor, model, actionGroup) {
        this.parent(sourceActor, 0.0, St.Side.TOP);

        this.model = model;
        this.actionGroup = actionGroup;

        this._actions = {};
        this._trackMenu(model, this);

        this._actionStateChangeId = this.actionGroup.connect('action-state-changed', Lang.bind(this, this._actionStateChanged));
        this._actionEnableChangeId = this.actionGroup.connect('action-enabled-changed', Lang.bind(this, this._actionEnabledChanged));
    },

    destroy: function() {
        if (this._actionStateChangeId) {
            this.actionGroup.disconnect(this._actionStateChangeId);
            this._actionStateChangeId = 0;
        }

        if (this._actionEnableChangeId) {
            this.actionGroup.disconnect(this._actionEnableChangeId);
            this._actionEnableChangeId = 0;
        }

        this.parent();
    },

    _actionAdded: function(model, item, index) {
        let action_id = item.action_id;

        if (!this._actions[action_id])
            this._actions[action_id] = { enabled: this.actionGroup.get_action_enabled(action_id),
                                         state: this.actionGroup.get_action_state(action_id),
                                         items: [ ],
                                       };
        let action = this._actions[action_id];
        let target, destroyId, specificSignalId;

        if (action.state) {
            // Docs have get_state_hint(), except that the DBus protocol
            // has no provision for it (so ShellApp does not implement it,
            // and neither GApplication), and g_action_get_state_hint()
            // always returns null
            // Funny :)

            switch (String.fromCharCode(action.state.classify())) {
            case 'b':
                action.items.push(item);
                item.setOrnament(action.state.get_boolean() ?
                                 PopupMenu.Ornament.CHECK :
                                 PopupMenu.Ornament.NONE);
                specificSignalId = item.connect('activate', Lang.bind(this, function(item) {
                    this.actionGroup.activate_action(action_id, null);
                }));
                break;
            case 's':
                action.items.push(item);
                item._remoteTarget = model.get_item_attribute_value(index, Gio.MENU_ATTRIBUTE_TARGET, null).deep_unpack();
                item.setOrnament(action.state.deep_unpack() == item._remoteTarget ?
                                 PopupMenu.Ornament.DOT :
                                 PopupMenu.Ornament.NONE);
                specificSignalId = item.connect('activate', Lang.bind(this, function(item) {
                    this.actionGroup.activate_action(action_id, GLib.Variant.new_string(item._remoteTarget));
                }));
                break;
            default:
                log('Action "%s" has state of type %s, which is not supported'.format(action_id, action.state.get_type_string()));
                return;
            }
        } else {
            target = model.get_item_attribute_value(index, Gio.MENU_ATTRIBUTE_TARGET, null);
            action.items.push(item);
            specificSignalId = item.connect('activate', Lang.bind(this, function() {
                this.actionGroup.activate_action(action_id, target);
            }));
        }

        item.actor.reactive = item.actor.can_focus = action.enabled;

        destroyId = item.connect('destroy', Lang.bind(this, function() {
            item.disconnect(destroyId);
            item.disconnect(specificSignalId);

            let pos = action.items.indexOf(item);
            if (pos != -1)
                action.items.splice(pos, 1);
        }));
    },

    _trackMenu: function(model, item) {
        item._tracker = Shell.MenuTracker.new(model,
                                              null, /* action namespace */
                                              Lang.bind(this, this._insertItem, item),
                                              Lang.bind(this, this._removeItem, item));

        item.connect('destroy', function() {
            item._tracker.destroy();
            item._tracker = null;
        });
    },

    _createMenuItem: function(model, index) {
        let labelValue = model.get_item_attribute_value(index, Gio.MENU_ATTRIBUTE_LABEL, null);
        let label = labelValue ? labelValue.deep_unpack() : '';
        // remove all underscores that are not followed by another underscore
        label = label.replace(/_([^_])/, '$1');

        let submenuModel = model.get_item_link(index, Gio.MENU_LINK_SUBMENU);
        if (submenuModel) {
            let item = new PopupMenu.PopupSubMenuMenuItem(label);
            this._trackMenu(submenuModel, item.menu);
            return item;
        }

        let item = new PopupMenu.PopupMenuItem(label);
        let action_id = model.get_item_attribute_value(index, Gio.MENU_ATTRIBUTE_ACTION, null).deep_unpack();
        item.actor.can_focus = item.actor.reactive = false;

        item.action_id = action_id;

        if (this.actionGroup.has_action(action_id)) {
            this._actionAdded(model, item, index);
            return item;
        }

        let signalId = this.actionGroup.connect('action-added', Lang.bind(this, function(actionGroup, actionName) {
            actionGroup.disconnect(signalId);
            if (this._actions[actionName]) return;

            this._actionAdded(model, item, index);
        }));

        return item;
    },

    _actionStateChanged: function(actionGroup, action_id) {
        let action = this._actions[action_id];
        if (!action)
            return;

        action.state = actionGroup.get_action_state(action_id);
        if (action.items.length) {
            switch (String.fromCharCode(action.state.classify())) {
            case 'b':
                for (let i = 0; i < action.items.length; i++)
                    action.items[i].setOrnament(action.state.get_boolean() ?
                                                Ornament.CHECK : Ornament.NONE);
                break;
            case 'd':
                for (let i = 0; i < action.items.length; i++)
                    action.items[i].setValue(action.state.get_double());
                break;
            case 's':
                for (let i = 0; i < action.items.length; i++)
                    action.items[i].setOrnament(action.items[i]._remoteTarget == action.state.deep_unpack() ?
                                                Ornament.DOT : Ornament.NONE);
            }
        }
    },

    _actionEnabledChanged: function(actionGroup, action_id) {
        let action = this._actions[action_id];
        if (!action)
            return;

        action.enabled = actionGroup.get_action_enabled(action_id);
        if (action.items.length) {
            for (let i = 0; i < action.items.length; i++) {
                let item = action.items[i];
                item.actor.reactive = item.actor.can_focus = action.enabled;
            }
        }
    },

    _insertItem: function(position, model, item_index, action_namespace, is_separator, target) {
        let item;

        if (is_separator)
            item = new PopupMenu.PopupSeparatorMenuItem();
        else
            item = this._createMenuItem(model, item_index);

        target.addMenuItem(item, position);
    },

    _removeItem: function(position, target) {
        let items = target._getMenuItems();
        items[position].destroy();
    },
});
