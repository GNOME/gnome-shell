/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Gdm = imports.gi.Gdm;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const GnomeSession = imports.misc.gnomeSession;
const Main = imports.ui.main;
const Panel = imports.ui.panel;
const PopupMenu = imports.ui.popupMenu;

// Adapted from gdm/gui/user-switch-applet/applet.c
//
// Copyright (C) 2004-2005 James M. Cape <jcape@ignore-your.tv>.
// Copyright (C) 2008,2009 Red Hat, Inc.

function StatusMenuButton() {
    this._init();
}

StatusMenuButton.prototype = {
    __proto__: Panel.PanelMenuButton.prototype,

    _init: function() {
        Panel.PanelMenuButton.prototype._init.call(this, St.Align.START);
        let box = new St.BoxLayout({ name: 'panelStatusMenu' });
        this.actor.set_child(box);

        this._gdm = Gdm.UserManager.ref_default();
        this._user = this._gdm.get_user(GLib.get_user_name());
        this._presence = new GnomeSession.Presence();

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._iconBox = new St.Bin();
        box.add(this._iconBox, { y_align: St.Align.MIDDLE, y_fill: false });

        let textureCache = St.TextureCache.get_default();
        // FIXME: these icons are all wrong (likewise in createSubMenu)
        this._availableIcon = textureCache.load_icon_name('gtk-yes', 16);
        this._busyIcon = textureCache.load_icon_name('gtk-no', 16);
        this._invisibleIcon = textureCache.load_icon_name('gtk-close', 16);
        this._idleIcon = textureCache.load_icon_name('gtk-media-pause', 16);

        this._presence.connect('StatusChanged', Lang.bind(this, this._updatePresenceIcon));
        this._presence.getStatus(Lang.bind(this, this._updatePresenceIcon));

        this._name = new St.Label({ text: this._user.get_real_name() });
        box.add(this._name, { y_align: St.Align.MIDDLE, y_fill: false });
        this._userNameChangedId = this._user.connect('notify::display-name', Lang.bind(this, this._updateUserName));

        this._createSubMenu();
        this._gdm.connect('users-loaded', Lang.bind(this, this._updateSwitchUser));
        this._gdm.connect('user-added', Lang.bind(this, this._updateSwitchUser));
        this._gdm.connect('user-removed', Lang.bind(this, this._updateSwitchUser));
    },

    _onDestroy: function() {
        this._user.disconnect(this._userNameChangedId);
    },

    _updateUserName: function() {
        this._name.set_text(this._user.get_real_name());
    },

    _updateSwitchUser: function() {
        let users = this._gdm.list_users();
        if (users.length > 1)
            this._loginScreenItem.actor.show();
        else
            this._loginScreenItem.actor.hide();
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
    },

    _createSubMenu: function() {
        let item;

        item = new PopupMenu.PopupImageMenuItem(_("Available"), 'gtk-yes', true);
        item.connect('activate', Lang.bind(this, this._setPresenceStatus, GnomeSession.PresenceStatus.AVAILABLE));
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupImageMenuItem(_("Busy"), 'gtk-no', true);
        item.connect('activate', Lang.bind(this, this._setPresenceStatus, GnomeSession.PresenceStatus.BUSY));
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupImageMenuItem(_("Invisible"), 'gtk-close', true);
        item.connect('activate', Lang.bind(this, this._setPresenceStatus, GnomeSession.PresenceStatus.INVISIBLE));
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupImageMenuItem(_("Account Information..."), 'user-info');
        item.connect('activate', Lang.bind(this, this._onAccountInformationActivate));
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupImageMenuItem(_("System Preferences..."), 'preferences-desktop');
        item.connect('activate', Lang.bind(this, this._onPreferencesActivate));
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupImageMenuItem(_("Lock Screen"), 'system-lock-screen');
        item.connect('activate', Lang.bind(this, this._onLockScreenActivate));
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupImageMenuItem(_("Switch User"), 'system-users');
        item.connect('activate', Lang.bind(this, this._onLoginScreenActivate));
        this.menu.addMenuItem(item);
        this._loginScreenItem = item;

        item = new PopupMenu.PopupImageMenuItem(_("Log Out..."), 'system-log-out');
        item.connect('activate', Lang.bind(this, this._onQuitSessionActivate));
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupImageMenuItem(_("Shut Down..."), 'system-shutdown');
        item.connect('activate', Lang.bind(this, this._onShutDownActivate));
        this.menu.addMenuItem(item);
    },

    _setPresenceStatus: function(item, event, status) {
        this._presence.setStatus(status);
    },

    _onAccountInformationActivate: function() {
        Main.overview.hide();
        this._spawn(['gnome-about-me']);
    },

    _onPreferencesActivate: function() {
        Main.overview.hide();
        this._spawn(['gnome-control-center']);
    },

    _onLockScreenActivate: function() {
        Main.overview.hide();
        this._spawn(['gnome-screensaver-command', '--lock']);
    },

    _onLoginScreenActivate: function() {
        Main.overview.hide();
        this._gdm.goto_login_session();
        this._onLockScreenActivate();
    },

    _onQuitSessionActivate: function() {
        Main.overview.hide();
        this._spawn(['gnome-session-save', '--logout-dialog']);
    },

    _onShutDownActivate: function() {
        Main.overview.hide();
        this._spawn(['gnome-session-save', '--shutdown-dialog']);
    },

    _spawn: function(args) {
        // FIXME: once Shell.Process gets support for signalling
        // errors we should pop up an error dialog or something here
        // on failure
        let p = new Shell.Process({'args' : args});
        p.run();
    }
};
