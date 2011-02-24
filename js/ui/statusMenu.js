/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Gdm = imports.gi.Gdm;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const UPowerGlib = imports.gi.UPowerGlib;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const GnomeSession = imports.misc.gnomeSession;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const Util = imports.misc.util;

// Adapted from gdm/gui/user-switch-applet/applet.c
//
// Copyright (C) 2004-2005 James M. Cape <jcape@ignore-your.tv>.
// Copyright (C) 2008,2009 Red Hat, Inc.

function StatusMenuButton() {
    this._init();
}

StatusMenuButton.prototype = {
    __proto__: PanelMenu.Button.prototype,

    _init: function() {
        PanelMenu.Button.prototype._init.call(this, 0.0);
        let box = new St.BoxLayout({ name: 'panelStatusMenu' });
        this.actor.set_child(box);

        this._gdm = Gdm.UserManager.ref_default();
        this._gdm.queue_load();

        this._user = this._gdm.get_user(GLib.get_user_name());
        this._presence = new GnomeSession.Presence();
        this._presenceItems = {};

        this._upClient = new UPowerGlib.Client();

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._iconBox = new St.Bin();
        box.add(this._iconBox, { y_align: St.Align.MIDDLE, y_fill: false });

        let textureCache = St.TextureCache.get_default();
        this._availableIcon = new St.Icon({ icon_name: 'user-available', style_class: 'popup-menu-icon' });
        this._busyIcon = new St.Icon({ icon_name: 'user-busy', style_class: 'popup-menu-icon' });
        this._invisibleIcon = new St.Icon({ icon_name: 'user-invisible', style_class: 'popup-menu-icon' });
        this._idleIcon = new St.Icon({ icon_name: 'user-idle', style_class: 'popup-menu-icon' });

        this._presence.connect('StatusChanged', Lang.bind(this, this._updatePresenceIcon));
        this._presence.getStatus(Lang.bind(this, this._updatePresenceIcon));

        this._name = new St.Label();
        box.add(this._name, { y_align: St.Align.MIDDLE, y_fill: false });
        this._userLoadedId = this._user.connect('notify::is-loaded', Lang.bind(this, this._updateUserName));
        this._userChangedId = this._user.connect('changed', Lang.bind(this, this._updateUserName));

        this._createSubMenu();
        this._gdm.connect('notify::is-loaded', Lang.bind(this, this._updateSwitchUser));
        this._gdm.connect('user-added', Lang.bind(this, this._updateSwitchUser));
        this._gdm.connect('user-removed', Lang.bind(this, this._updateSwitchUser));

        this._upClient.connect('notify::can-suspend', Lang.bind(this, this._updateSuspendOrPowerOff));
    },

    _onDestroy: function() {
        this._user.disconnect(this._userLoadedId);
        this._user.disconnect(this._userChangedId);
    },

    _updateUserName: function() {
        if (this._user.is_loaded)
          this._name.set_text(this._user.get_real_name());
        else
          this._name.set_text("");
    },

    _updateSwitchUser: function() {
        if (this._gdm.can_switch ())
            this._loginScreenItem.actor.show();
        else
            this._loginScreenItem.actor.hide();
    },

    _updateSuspendOrPowerOff: function() {
        this._haveSuspend = this._upClient.get_can_suspend();

        if (!this._suspendOrPowerOffItem)
            return;

        // If we can't suspend show Power Off... instead
        // and disable the alt key
        if (!this._haveSuspend) {
            this._suspendOrPowerOffItem.updateText(_("Power Off..."), null);
        } else {
            this._suspendOrPowerOffItem.updateText(_("Suspend"), ("Power Off..."));
        }
    },

    _updatePresenceIcon: function(presence, status) {
        if (status == GnomeSession.PresenceStatus.AVAILABLE)
            this._iconBox.child = this._availableIcon;
        else if (status == GnomeSession.PresenceStatus.BUSY)
            this._iconBox.child = this._busyIcon;
        else if (status == GnomeSession.PresenceStatus.INVISIBLE)
            this._iconBox.child = this._invisibleIcon;
        else
            this._iconBox.child = this._idleIcon;

        for (let itemStatus in this._presenceItems)
            this._presenceItems[itemStatus].setShowDot(itemStatus == status);
    },

    _createSubMenu: function() {
        let item;

        item = new PopupMenu.PopupImageMenuItem(_("Available"), 'user-available');
        item.connect('activate', Lang.bind(this, this._setPresenceStatus, GnomeSession.PresenceStatus.AVAILABLE));
        this.menu.addMenuItem(item);
        this._presenceItems[GnomeSession.PresenceStatus.AVAILABLE] = item;

        item = new PopupMenu.PopupImageMenuItem(_("Busy"), 'user-busy');
        item.connect('activate', Lang.bind(this, this._setPresenceStatus, GnomeSession.PresenceStatus.BUSY));
        this.menu.addMenuItem(item);
        this._presenceItems[GnomeSession.PresenceStatus.BUSY] = item;

        item = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupMenuItem(_("My Account"));
        item.connect('activate', Lang.bind(this, this._onMyAccountActivate));
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupMenuItem(_("System Settings"));
        item.connect('activate', Lang.bind(this, this._onPreferencesActivate));
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupMenuItem(_("Lock Screen"));
        item.connect('activate', Lang.bind(this, this._onLockScreenActivate));
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupMenuItem(_("Switch User"));
        item.connect('activate', Lang.bind(this, this._onLoginScreenActivate));
        this.menu.addMenuItem(item);
        this._loginScreenItem = item;

        item = new PopupMenu.PopupMenuItem(_("Log Out..."));
        item.connect('activate', Lang.bind(this, this._onQuitSessionActivate));
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupAlternatingMenuItem(_("Suspend"),
                                                      _("Power Off..."));
        this.menu.addMenuItem(item);
        this._suspendOrPowerOffItem = item;
        item.connect('activate', Lang.bind(this, this._onSuspendOrPowerOffActivate));
        this._updateSuspendOrPowerOff();
    },

    _setPresenceStatus: function(item, event, status) {
        this._presence.setStatus(status);
    },

    _onMyAccountActivate: function() {
        Main.overview.hide();
        Util.spawnDesktop('gnome-user-accounts-panel');
    },

    _onPreferencesActivate: function() {
        Main.overview.hide();
        Util.spawnDesktop('gnome-control-center');
    },

    _onLockScreenActivate: function() {
        Main.overview.hide();
        Util.spawn(['gnome-screensaver-command', '--lock']);
    },

    _onLoginScreenActivate: function() {
        Main.overview.hide();
        this._gdm.goto_login_session();
        this._onLockScreenActivate();
    },

    _onQuitSessionActivate: function() {
        Main.overview.hide();
        Util.spawn(['gnome-session-quit', '--logout']);
    },

    _onSuspendOrPowerOffActivate: function() {
        Main.overview.hide();

        if (this._haveSuspend &&
            this._suspendOrPowerOffItem.state == PopupMenu.PopupAlternatingMenuItemState.DEFAULT) {
            this._upClient.suspend_sync(null);
        } else {
            Util.spawn(['gnome-session-quit', '--power-off']);
        }
    }
};
