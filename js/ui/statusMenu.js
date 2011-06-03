/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Gdm = imports.gi.Gdm;
const DBus = imports.dbus;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Tp = imports.gi.TelepathyGLib;
const UPowerGlib = imports.gi.UPowerGlib;

const GnomeSession = imports.misc.gnomeSession;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const Util = imports.misc.util;

const BUS_NAME = 'org.gnome.ScreenSaver';
const OBJECT_PATH = '/org/gnome/ScreenSaver';

const LOCKDOWN_SCHEMA = 'org.gnome.desktop.lockdown';
const DISABLE_USER_SWITCH_KEY = 'disable-user-switching';
const DISABLE_LOCK_SCREEN_KEY = 'disable-lock-screen';
const DISABLE_LOG_OUT_KEY = 'disable-log-out';

const ScreenSaverInterface = {
    name: BUS_NAME,
    methods: [ { name: 'Lock', inSignature: '' },
               { name: 'SetActive', inSignature: 'b' }]
};

let ScreenSaverProxy = DBus.makeProxyClass(ScreenSaverInterface);

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

        this._lockdownSettings = new Gio.Settings({ schema: LOCKDOWN_SCHEMA });

        this._gdm = Gdm.UserManager.ref_default();
        this._gdm.queue_load();

        this._user = this._gdm.get_user(GLib.get_user_name());
        this._presence = new GnomeSession.Presence();
        this._presenceItems = {};
        this._session = new GnomeSession.SessionManager();
        this._haveShutdown = true;

        this._account_mgr = Tp.AccountManager.dup()

        this._upClient = new UPowerGlib.Client();
        this._screenSaverProxy = new ScreenSaverProxy(DBus.session, BUS_NAME, OBJECT_PATH);
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
        this._lockdownSettings.connect('changed::' + DISABLE_USER_SWITCH_KEY,
                                       Lang.bind(this, this._updateSwitchUser));
        this._lockdownSettings.connect('changed::' + DISABLE_LOG_OUT_KEY,
                                       Lang.bind(this, this._updateLogout));

        this._lockdownSettings.connect('changed::' + DISABLE_LOCK_SCREEN_KEY,
                                       Lang.bind(this, this._updateLockScreen));
        this._updateSwitchUser();
        this._updateLogout();
        this._updateLockScreen();

        // Whether shutdown is available or not depends on both lockdown
        // settings (disable-log-out) and Polkit policy - the latter doesn't
        // notify, so we update the menu item each time the menu opens or
        // the lockdown setting changes, which should be close enough.
        this.menu.connect('open-state-changed', Lang.bind(this,
            function(menu, open) {
                if (open)
                    this._updateHaveShutdown();
            }));
        this._lockdownSettings.connect('changed::' + DISABLE_LOG_OUT_KEY,
                                       Lang.bind(this, this._updateHaveShutdown));

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

    _updateSessionSeparator: function() {
        let sessionItemsVisible = this._loginScreenItem.actor.visible ||
                                  this._logoutItem.actor.visible ||
                                  this._lockScreenItem.actor.visible;

        let showSessionSeparator = sessionItemsVisible &&
                                   this._suspendOrPowerOffItem.actor.visible;

        let showSettingsSeparator = sessionItemsVisible ||
                                    this._suspendOrPowerOffItem.actor.visible;

        if (showSessionSeparator)
            this._sessionSeparator.actor.show();
        else
            this._sessionSeparator.actor.hide();

        if (showSettingsSeparator)
            this._settingsSeparator.actor.show();
        else
            this._settingsSeparator.actor.hide();
    },

    _updateSwitchUser: function() {
        let allowSwitch = !this._lockdownSettings.get_boolean(DISABLE_USER_SWITCH_KEY);
        if (allowSwitch && this._gdm.can_switch ())
            this._loginScreenItem.actor.show();
        else
            this._loginScreenItem.actor.hide();
        this._updateSessionSeparator();
    },

    _updateLogout: function() {
        let allowLogout = !this._lockdownSettings.get_boolean(DISABLE_LOG_OUT_KEY);
        if (allowLogout)
            this._logoutItem.actor.show();
        else
            this._logoutItem.actor.hide();
        this._updateSessionSeparator();
    },

    _updateLockScreen: function() {
        let allowLockScreen = !this._lockdownSettings.get_boolean(DISABLE_LOCK_SCREEN_KEY);
        if (allowLockScreen)
            this._lockScreenItem.actor.show();
        else
            this._lockScreenItem.actor.hide();
        this._updateSessionSeparator();
    },

    _updateHaveShutdown: function() {
        this._session.CanShutdownRemote(Lang.bind(this,
            function(result, error) {
                if (!error) {
                    this._haveShutdown = result;
                    this._updateSuspendOrPowerOff();
                }
            }));
    },

    _updateSuspendOrPowerOff: function() {
        this._haveSuspend = this._upClient.get_can_suspend();

        if (!this._suspendOrPowerOffItem)
            return;

        if (!this._haveShutdown && !this._haveSuspend)
            this._suspendOrPowerOffItem.actor.hide();
        else
            this._suspendOrPowerOffItem.actor.show();
         this._updateSessionSeparator();

        // If we can't suspend show Power Off... instead
        // and disable the alt key
        if (!this._haveSuspend) {
            this._suspendOrPowerOffItem.updateText(_("Power Off..."), null);
        } else if (!this._haveShutdown) {
            this._suspendOrPowerOffItem.updateText(_("Suspend"), null);
        } else {
            this._suspendOrPowerOffItem.updateText(_("Suspend"), _("Power Off..."));
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
        this._settingsSeparator = item;

        item = new PopupMenu.PopupMenuItem(_("Lock Screen"));
        item.connect('activate', Lang.bind(this, this._onLockScreenActivate));
        this.menu.addMenuItem(item);
        this._lockScreenItem = item;

        item = new PopupMenu.PopupMenuItem(_("Switch User"));
        item.connect('activate', Lang.bind(this, this._onLoginScreenActivate));
        this.menu.addMenuItem(item);
        this._loginScreenItem = item;

        item = new PopupMenu.PopupMenuItem(_("Log Out..."));
        item.connect('activate', Lang.bind(this, this._onQuitSessionActivate));
        this.menu.addMenuItem(item);
        this._logoutItem = item;

        item = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(item);
        this._sessionSeparator = item;

        item = new PopupMenu.PopupAlternatingMenuItem(_("Suspend"),
                                                      _("Power Off..."));
        this.menu.addMenuItem(item);
        this._suspendOrPowerOffItem = item;
        item.connect('activate', Lang.bind(this, this._onSuspendOrPowerOffActivate));
        this._updateSuspendOrPowerOff();
    },

    _setPresenceStatus: function(item, event, status) {
        this._presence.setStatus(status);

        this._setIMStatus(status);
    },

    _onMyAccountActivate: function() {
        Main.overview.hide();
        let app = Shell.AppSystem.get_default().get_app('gnome-user-accounts-panel.desktop');
        app.activate(-1);
    },

    _onPreferencesActivate: function() {
        Main.overview.hide();
        let app = Shell.AppSystem.get_default().get_app('gnome-control-center.desktop');
        app.activate(-1);
    },

    _onLockScreenActivate: function() {
        Main.overview.hide();
        this._screenSaverProxy.LockRemote();
    },

    _onLoginScreenActivate: function() {
        Main.overview.hide();
        this._gdm.goto_login_session();
        this._onLockScreenActivate();
    },

    _onQuitSessionActivate: function() {
        Main.overview.hide();
        this._session.LogoutRemote(0);
    },

    _onSuspendOrPowerOffActivate: function() {
        Main.overview.hide();

        if (this._haveSuspend &&
            this._suspendOrPowerOffItem.state == PopupMenu.PopupAlternatingMenuItemState.DEFAULT) {
            this._screenSaverProxy.SetActiveRemote(true, Lang.bind(this, function() {
                this._upClient.suspend_sync(null);
            }));
        } else {
            this._session.ShutdownRemote();
        }
    },

    _setIMStatus: function(session_status) {
        let [presence_type, presence_status, msg] = this._account_mgr.get_most_available_presence();
        let type, status;

        // We change the IM presence only if there are connected accounts
        if (presence_type == Tp.ConnectionPresenceType.UNSET ||
            presence_type == Tp.ConnectionPresenceType.OFFLINE ||
            presence_type == Tp.ConnectionPresenceType.UNKNOWN ||
            presence_type == Tp.ConnectionPresenceType.ERROR)
          return;

        if (session_status == GnomeSession.PresenceStatus.AVAILABLE) {
            type = Tp.ConnectionPresenceType.AVAILABLE;
            status = "available";
        }
        else if (session_status == GnomeSession.PresenceStatus.BUSY) {
            type = Tp.ConnectionPresenceType.BUSY;
            status = "busy";
        }
        else {
          return;
        }

        this._account_mgr.set_all_requested_presences(type, status, msg);
    }
};
