// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/*
 * Copyright 2011 Red Hat, Inc
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

const Gio = imports.gi.Gio;
const Lang = imports.lang;

const LoginManager = imports.misc.loginManager;

const GdmUtil = imports.gdm.util;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const PowerMenuButton = new Lang.Class({
    Name: 'PowerMenuButton',
    Extends: PanelMenu.SystemIndicator,

    _init: function() {
        /* Translators: accessible name of the power menu in the login screen */
        this.parent('system-shutdown-symbolic', _("Power"));

        this._loginManager = LoginManager.getLoginManager();

        this._settings = new Gio.Settings({ schema: GdmUtil.LOGIN_SCREEN_SCHEMA });
        this._settings.connect('changed::disable-restart-buttons',
                               Lang.bind(this, this._updateVisibility));

        this._createSubMenu();

        // ConsoleKit doesn't send notifications when shutdown/reboot
        // are disabled, so we update the menu item each time the menu opens
        this.menu.connect('open-state-changed', Lang.bind(this,
            function(menu, open) {
                if (open) {
                    this._updateHaveShutdown();
                    this._updateHaveRestart();
                    this._updateHaveSuspend();
                }
            }));
        this._updateHaveShutdown();
        this._updateHaveRestart();
        this._updateHaveSuspend();
    },

    _updateVisibility: function() {
        let shouldBeVisible = (this._haveSuspend || this._haveShutdown || this._haveRestart);
        this.actor.visible = shouldBeVisible && !this._settings.get_boolean('disable-restart-buttons');
    },

    _updateHaveShutdown: function() {
        this._loginManager.canPowerOff(Lang.bind(this, function(result) {
            this._haveShutdown = result;
            this._powerOffItem.actor.visible = this._haveShutdown;
            this._updateVisibility();
        }));
    },

    _updateHaveRestart: function() {
        this._loginManager.canReboot(Lang.bind(this, function(result) {
            this._haveRestart = result;
            this._restartItem.actor.visible = this._haveRestart;
            this._updateVisibility();
        }));
    },

    _updateHaveSuspend: function() {
        this._loginManager.canSuspend(Lang.bind(this, function(result) {
            this._haveSuspend = result;
            this._suspendItem.actor.visible = this._haveSuspend;
            this._updateVisibility();
        }));
    },

    _createSubMenu: function() {
        let item;

        item = new PopupMenu.PopupMenuItem(_("Suspend"));
        item.connect('activate', Lang.bind(this, this._onActivateSuspend));
        this.menu.addMenuItem(item);
        this._suspendItem = item;

        item = new PopupMenu.PopupMenuItem(_("Restart"));
        item.connect('activate', Lang.bind(this, this._onActivateRestart));
        this.menu.addMenuItem(item);
        this._restartItem = item;

        item = new PopupMenu.PopupMenuItem(_("Power Off"));
        item.connect('activate', Lang.bind(this, this._onActivatePowerOff));
        this.menu.addMenuItem(item);
        this._powerOffItem = item;
    },

    _onActivateSuspend: function() {
        if (!this._haveSuspend)
            return;

        this._loginManager.suspend();
    },

    _onActivateRestart: function() {
        if (!this._haveRestart)
            return;

        this._loginManager.reboot();
    },

    _onActivatePowerOff: function() {
        if (!this._haveShutdown)
            return;

        this._loginManager.powerOff();
    }
});
