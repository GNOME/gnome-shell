// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const AccountsService = imports.gi.AccountsService;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Tp = imports.gi.TelepathyGLib;
const UPowerGlib = imports.gi.UPowerGlib;
const Atk = imports.gi.Atk;

const GnomeSession = imports.misc.gnomeSession;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const ScreenSaver = imports.misc.screenSaver;
const Util = imports.misc.util;

const LOCKDOWN_SCHEMA = 'org.gnome.desktop.lockdown';
const DISABLE_USER_SWITCH_KEY = 'disable-user-switching';
const DISABLE_LOCK_SCREEN_KEY = 'disable-lock-screen';
const DISABLE_LOG_OUT_KEY = 'disable-log-out';

const DIALOG_ICON_SIZE = 64;

const IMStatus = {
    AVAILABLE: 0,
    BUSY: 1,
    HIDDEN: 2,
    AWAY: 3,
    IDLE: 4,
    OFFLINE: 5,
    LAST: 6
};

// Adapted from gdm/gui/user-switch-applet/applet.c
//
// Copyright (C) 2004-2005 James M. Cape <jcape@ignore-your.tv>.
// Copyright (C) 2008,2009 Red Hat, Inc.


const IMStatusItem = new Lang.Class({
    Name: 'IMStatusItem',
    Extends: PopupMenu.PopupBaseMenuItem,

    _init: function(label, iconName) {
        this.parent();

        this.actor.add_style_class_name('status-chooser-status-item');

        this._icon = new St.Icon({ style_class: 'popup-menu-icon' });
        this.addActor(this._icon);

        if (iconName)
            this._icon.icon_name = iconName;

        this.label = new St.Label({ text: label });
        this.actor.label_actor = this.label;
        this.addActor(this.label);
    }
});

const IMUserNameItem = new Lang.Class({
    Name: 'IMUserNameItem',
    Extends: PopupMenu.PopupBaseMenuItem,

    _init: function() {
        this.parent({ reactive: false,
                      style_class: 'status-chooser-user-name' });

        this._wrapper = new Shell.GenericContainer();
        this._wrapper.connect('get-preferred-width',
                              Lang.bind(this, this._wrapperGetPreferredWidth));
        this._wrapper.connect('get-preferred-height',
                              Lang.bind(this, this._wrapperGetPreferredHeight));
        this._wrapper.connect('allocate',
                              Lang.bind(this, this._wrapperAllocate));
        this.addActor(this._wrapper, { expand: true, span: -1 });

        this.label = new St.Label();
        this.label.clutter_text.set_line_wrap(true);
        this.label.clutter_text.set_ellipsize(Pango.EllipsizeMode.NONE);
        this._wrapper.add_actor(this.label);
    },

    _wrapperGetPreferredWidth: function(actor, forHeight, alloc) {
        alloc.min_size = 1;
        alloc.natural_size = 1;
    },

    _wrapperGetPreferredHeight: function(actor, forWidth, alloc) {
        [alloc.min_size, alloc.natural_size] = this.label.get_preferred_height(forWidth);
    },

    _wrapperAllocate: function(actor, box, flags) {
        this.label.allocate(box, flags);
    }
});

const IMStatusChooserItem = new Lang.Class({
    Name: 'IMStatusChooserItem',
    Extends: PopupMenu.PopupBaseMenuItem,

    _init: function() {
        this.parent({ reactive: false,
                      style_class: 'status-chooser' });

        this._iconBin = new St.Button({ style_class: 'status-chooser-user-icon' });
        this.addActor(this._iconBin);

        this._iconBin.connect('clicked', Lang.bind(this,
            function() {
                this.activate();
            }));

        this._section = new PopupMenu.PopupMenuSection();
        this.addActor(this._section.actor);

        this._name = new IMUserNameItem();
        this._section.addMenuItem(this._name);

        this._combo = new PopupMenu.PopupComboBoxMenuItem({ style_class: 'status-chooser-combo' });
        this._section.addMenuItem(this._combo);

        let item;

        item = new IMStatusItem(_("Available"), 'user-available');
        this._combo.addMenuItem(item, IMStatus.AVAILABLE);

        item = new IMStatusItem(_("Busy"), 'user-busy');
        this._combo.addMenuItem(item, IMStatus.BUSY);

        item = new IMStatusItem(_("Hidden"), 'user-invisible');
        this._combo.addMenuItem(item, IMStatus.HIDDEN);

        item = new IMStatusItem(_("Away"), 'user-away');
        this._combo.addMenuItem(item, IMStatus.AWAY);

        item = new IMStatusItem(_("Idle"), 'user-idle');
        this._combo.addMenuItem(item, IMStatus.IDLE);

        item = new IMStatusItem(_("Unavailable"), 'user-offline');
        this._combo.addMenuItem(item, IMStatus.OFFLINE);

        this._combo.connect('active-item-changed',
                            Lang.bind(this, this._changeIMStatus));

        this._presence = new GnomeSession.Presence();
        this._presence.connectSignal('StatusChanged', Lang.bind(this, function(proxy, senderName, [status]) {
            this._sessionStatusChanged(status);
        }));

        this._sessionPresenceRestored = false;
        this._imPresenceRestored = false;
        this._currentPresence = undefined;

        this._accountMgr = Tp.AccountManager.dup();
        this._accountMgr.connect('most-available-presence-changed',
                                 Lang.bind(this, this._IMStatusChanged));
        this._accountMgr.connect('account-enabled',
                                 Lang.bind(this, this._IMAccountsChanged));
        this._accountMgr.connect('account-disabled',
                                 Lang.bind(this, this._IMAccountsChanged));
        this._accountMgr.connect('account-removed',
                                 Lang.bind(this, this._IMAccountsChanged));
        this._accountMgr.connect('account-validity-changed',
                                 Lang.bind(this, this._IMAccountsChanged));
        this._accountMgr.prepare_async(null, Lang.bind(this,
            function(mgr) {
                let [presence, status, msg] = mgr.get_most_available_presence();

                let savedPresence = global.settings.get_int('saved-im-presence');

                this._IMAccountsChanged(mgr);

                if (savedPresence == presence) {
                    this._IMStatusChanged(mgr, presence, status, msg);
                } else {
                    this._setComboboxPresence(savedPresence);
                    status = this._statusForPresence(savedPresence);
                    msg = msg ? msg : '';
                    mgr.set_all_requested_presences(savedPresence, status, msg);
                }
            }));

        this._userManager = AccountsService.UserManager.get_default();

        this._user = this._userManager.get_user(GLib.get_user_name());

        this._userLoadedId = this._user.connect('notify::is-loaded',
                                                Lang.bind(this,
                                                          this._updateUser));
        this._userChangedId = this._user.connect('changed',
                                                 Lang.bind(this,
                                                           this._updateUser));
        this.actor.connect('notify::mapped', Lang.bind(this, function() {
            if (this.actor.mapped)
                this._updateUser();
        }));
    },

    destroy: function() {
        // clean up signal handlers
        if (this._userLoadedId != 0) {
            this._user.disconnect(this._userLoadedId);
            this._userLoadedId = 0;
        }

        if (this._userChangedId != 0) {
            this._user.disconnect(this._userChangedId);
            this._userChangedId = 0;
        }

        this.parent();
    },

    // Override getColumnWidths()/setColumnWidths() to make the item
    // independent from the overall column layout of the menu
    getColumnWidths: function() {
        return [];
    },

    setColumnWidths: function(widths) {
    },

    _updateUser: function() {
        let iconFile = null;
        if (this._user.is_loaded) {
            this._name.label.set_text(this._user.get_real_name());
            iconFile = this._user.get_icon_file();
            if (!GLib.file_test(iconFile, GLib.FileTest.EXISTS))
                iconFile = null;
        } else {
            this._name.label.set_text("");
        }

        if (iconFile)
            this._setIconFromFile(iconFile);
        else
            this._setIconFromName('avatar-default');
    },

    _setIconFromFile: function(iconFile) {
        this._iconBin.set_style('background-image: url("' + iconFile + '");' +
                                'background-size: contain;');
        this._iconBin.child = null;
    },

    _setIconFromName: function(iconName) {
        this._iconBin.set_style(null);

        if (iconName != null) {
            let textureCache = St.TextureCache.get_default();
            let icon = textureCache.load_icon_name(this._iconBin.get_theme_node(),
                                                   iconName,
                                                   St.IconType.SYMBOLIC,
                                                   DIALOG_ICON_SIZE);

            this._iconBin.child = icon;
            this._iconBin.show();
        } else {
            this._iconBin.child = null;
            this._iconBin.hide();
        }
    },

    _statusForPresence: function(presence) {
        switch(presence) {
            case Tp.ConnectionPresenceType.AVAILABLE:
                return 'available';
            case Tp.ConnectionPresenceType.BUSY:
                return 'busy';
            case Tp.ConnectionPresenceType.OFFLINE:
                return 'offline';
            case Tp.ConnectionPresenceType.HIDDEN:
                return 'hidden';
            case Tp.ConnectionPresenceType.AWAY:
                return 'away';
            case Tp.ConnectionPresenceType.EXTENDED_AWAY:
                return 'xa';
            default:
                return 'unknown';
        }
    },

    _IMAccountsChanged: function(mgr) {
        let accounts = mgr.get_valid_accounts().filter(function(account) {
            return account.enabled;
        });
        this._combo.setSensitive(accounts.length > 0);
    },

    _IMStatusChanged: function(accountMgr, presence, status, message) {
        if (!this._imPresenceRestored)
            this._imPresenceRestored = true;

        if (presence == this._currentPresence)
            return;

        this._currentPresence = presence;
        this._setComboboxPresence(presence);

        if (!this._sessionPresenceRestored) {
            this._sessionStatusChanged(this._presence.status);
            return;
        }

        if (presence == Tp.ConnectionPresenceType.AVAILABLE)
            this._presence.status = GnomeSession.PresenceStatus.AVAILABLE;

        // We ignore the actual value of _expectedPresence and never safe
        // the first presence change after an "automatic" change, assuming
        // that it is the response to our request; this is to account for
        // mission control falling back to "similar" presences if an account
        // type does not implement the requested presence.
        if (!this._expectedPresence)
            global.settings.set_int('saved-im-presence', presence);
        else
            this._expectedPresence = undefined;
    },

    _setComboboxPresence: function(presence) {
        let activatedItem;

        if (presence == Tp.ConnectionPresenceType.AVAILABLE)
            activatedItem = IMStatus.AVAILABLE;
        else if (presence == Tp.ConnectionPresenceType.BUSY)
            activatedItem = IMStatus.BUSY;
        else if (presence == Tp.ConnectionPresenceType.HIDDEN)
            activatedItem = IMStatus.HIDDEN;
        else if (presence == Tp.ConnectionPresenceType.AWAY)
            activatedItem = IMStatus.AWAY;
        else if (presence == Tp.ConnectionPresenceType.EXTENDED_AWAY)
            activatedItem = IMStatus.IDLE;
        else
            activatedItem = IMStatus.OFFLINE;

        this._combo.setActiveItem(activatedItem);
        for (let i = 0; i < IMStatus.LAST; i++) {
            if (i == IMStatus.AVAILABLE || i == IMStatus.OFFLINE)
                continue;   // always visible

            this._combo.setItemVisible(i, i == activatedItem);
        }
    },

    _changeIMStatus: function(menuItem, id) {
        let [presence, s, msg] = this._accountMgr.get_most_available_presence();
        let newPresence, status;

        if (id == IMStatus.AVAILABLE) {
            newPresence = Tp.ConnectionPresenceType.AVAILABLE;
        } else if (id == IMStatus.OFFLINE) {
            newPresence = Tp.ConnectionPresenceType.OFFLINE;
        } else
            return;

        status = this._statusForPresence(newPresence);
        msg = msg ? msg : '';
        this._accountMgr.set_all_requested_presences(newPresence, status, msg);
    },

    getIMPresenceForSessionStatus: function(sessionStatus) {
        // Restore the last user-set presence when coming back from
        // BUSY/IDLE (otherwise the last user-set presence matches
        // the current one)
        if (sessionStatus == GnomeSession.PresenceStatus.AVAILABLE)
            return global.settings.get_int('saved-im-presence');

        if (sessionStatus == GnomeSession.PresenceStatus.BUSY) {
            // Only change presence if the current one is "more present" than
            // busy, or if coming back from idle
            if (this._currentPresence == Tp.ConnectionPresenceType.AVAILABLE ||
                this._currentPresence == Tp.ConnectionPresenceType.EXTENDED_AWAY)
                return Tp.ConnectionPresenceType.BUSY;
        }

        if (sessionStatus == GnomeSession.PresenceStatus.IDLE) {
            // Only change presence if the current one is "more present" than
            // idle
            if (this._currentPresence != Tp.ConnectionPresenceType.OFFLINE &&
                this._currentPresence != Tp.ConnectionPresenceType.HIDDEN)
                return Tp.ConnectionPresenceType.EXTENDED_AWAY;
        }

        return this._currentPresence;
    },

    _sessionStatusChanged: function(sessionStatus) {
        if (!this._imPresenceRestored)
            return;

        let savedStatus = global.settings.get_int('saved-session-presence');
        if (!this._sessionPresenceRestored) {

            // We should never save/restore a status other than AVAILABLE
            // or BUSY
            if (savedStatus != GnomeSession.PresenceStatus.AVAILABLE &&
                savedStatus != GnomeSession.PresenceStatus.BUSY)
                savedStatus = GnomeSession.PresenceStatus.AVAILABLE;

            if (sessionStatus != savedStatus) {
                this._presence.status = savedStatus;
                return;
            }
            this._sessionPresenceRestored = true;
        }

        if ((sessionStatus == GnomeSession.PresenceStatus.AVAILABLE ||
             sessionStatus == GnomeSession.PresenceStatus.BUSY) &&
            savedStatus != sessionStatus)
            global.settings.set_int('saved-session-presence', sessionStatus);

        let [presence, s, msg] = this._accountMgr.get_most_available_presence();
        let newPresence, status;

        let newPresence = this.getIMPresenceForSessionStatus(sessionStatus);

        if (!newPresence || newPresence == presence)
            return;

        status = this._statusForPresence(newPresence);
        msg = msg ? msg : '';

        this._expectedPresence = newPresence;
        this._accountMgr.set_all_requested_presences(newPresence, status, msg);
    }
});


const UserMenuButton = new Lang.Class({
    Name: 'UserMenuButton',
    Extends: PanelMenu.Button,

    _init: function() {
        this.parent(0.0);

        this.actor.accessible_role = Atk.Role.MENU;

        let box = new St.BoxLayout({ name: 'panelUserMenu' });
        this.actor.add_actor(box);

        this._lockdownSettings = new Gio.Settings({ schema: LOCKDOWN_SCHEMA });

        this._userManager = AccountsService.UserManager.get_default();

        this._user = this._userManager.get_user(GLib.get_user_name());
        this._presence = new GnomeSession.Presence();
        this._session = new GnomeSession.SessionManager();
        this._haveShutdown = true;

        this._accountMgr = Tp.AccountManager.dup();

        this._upClient = new UPowerGlib.Client();
        this._screenSaverProxy = new ScreenSaver.ScreenSaverProxy();
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._iconBox = new St.Bin();
        box.add(this._iconBox, { y_align: St.Align.MIDDLE, y_fill: false });

        let textureCache = St.TextureCache.get_default();
        this._offlineIcon = new St.Icon({ icon_name: 'user-offline',
                                          style_class: 'popup-menu-icon' });
        this._availableIcon = new St.Icon({ icon_name: 'user-available',
                                            style_class: 'popup-menu-icon' });
        this._busyIcon = new St.Icon({ icon_name: 'user-busy',
                                       style_class: 'popup-menu-icon' });
        this._invisibleIcon = new St.Icon({ icon_name: 'user-invisible',
                                            style_class: 'popup-menu-icon' });
        this._awayIcon = new St.Icon({ icon_name: 'user-away',
                                       style_class: 'popup-menu-icon' });
        this._idleIcon = new St.Icon({ icon_name: 'user-idle',
                                       style_class: 'popup-menu-icon' });

        this._accountMgr.connect('most-available-presence-changed',
                                  Lang.bind(this, this._updatePresenceIcon));
        this._accountMgr.prepare_async(null, Lang.bind(this,
            function(mgr) {
                let [presence, s, msg] = mgr.get_most_available_presence();
                this._updatePresenceIcon(mgr, presence, s, msg);
            }));

        this._name = new St.Label();
        this.actor.label_actor = this._name;
        box.add(this._name, { y_align: St.Align.MIDDLE, y_fill: false });
        this._userLoadedId = this._user.connect('notify::is-loaded', Lang.bind(this, this._updateUserName));
        this._userChangedId = this._user.connect('changed', Lang.bind(this, this._updateUserName));
        this._updateUserName();

        this._createSubMenu();

        this._updateSwitch(this._presence.status);
        this._presence.connectSignal('StatusChanged', Lang.bind(this, function (proxy, senderName, [status]) {
            this._updateSwitch(status);
        }));

        this._userManager.connect('notify::is-loaded',
                                  Lang.bind(this, this._updateSwitchUser));
        this._userManager.connect('notify::has-multiple-users',
                                  Lang.bind(this, this._updateSwitchUser));
        this._userManager.connect('user-added',
                                  Lang.bind(this, this._updateSwitchUser));
        this._userManager.connect('user-removed',
                                  Lang.bind(this, this._updateSwitchUser));
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

    _updateSwitchUser: function() {
        let allowSwitch = !this._lockdownSettings.get_boolean(DISABLE_USER_SWITCH_KEY);
        if (allowSwitch &&
            this._userManager.can_switch() &&
            this._userManager.has_multiple_users)
            this._loginScreenItem.actor.show();
        else
            this._loginScreenItem.actor.hide();
    },

    _updateLogout: function() {
        let allowLogout = !this._lockdownSettings.get_boolean(DISABLE_LOG_OUT_KEY);
        if (allowLogout)
            this._logoutItem.actor.show();
        else
            this._logoutItem.actor.hide();
    },

    _updateLockScreen: function() {
        let allowLockScreen = !this._lockdownSettings.get_boolean(DISABLE_LOCK_SCREEN_KEY);
        if (allowLockScreen)
            this._lockScreenItem.actor.show();
        else
            this._lockScreenItem.actor.hide();
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

    _updateSwitch: function(status) {
        let active = status == GnomeSession.PresenceStatus.AVAILABLE;
        this._notificationsSwitch.setToggleState(active);
    },

    _updatePresenceIcon: function(accountMgr, presence, status, message) {
        if (presence == Tp.ConnectionPresenceType.AVAILABLE)
            this._iconBox.child = this._availableIcon;
        else if (presence == Tp.ConnectionPresenceType.BUSY)
            this._iconBox.child = this._busyIcon;
        else if (presence == Tp.ConnectionPresenceType.HIDDEN)
            this._iconBox.child = this._invisibleIcon;
        else if (presence == Tp.ConnectionPresenceType.AWAY)
            this._iconBox.child = this._awayIcon;
        else if (presence == Tp.ConnectionPresenceType.EXTENDED_AWAY)
            this._iconBox.child = this._idleIcon;
        else
            this._iconBox.child = this._offlineIcon;
    },

    _createSubMenu: function() {
        let item;

        item = new IMStatusChooserItem();
        item.connect('activate', Lang.bind(this, this._onMyAccountActivate));
        this.menu.addMenuItem(item);
        this._statusChooser = item;

        item = new PopupMenu.PopupSwitchMenuItem(_("Notifications"));
        item.connect('toggled', Lang.bind(this, this._updatePresenceStatus));
        this.menu.addMenuItem(item);
        this._notificationsSwitch = item;

        item = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupMenuItem(_("Online Accounts"));
        item.connect('activate', Lang.bind(this, this._onOnlineAccountsActivate));
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupMenuItem(_("System Settings"));
        item.connect('activate', Lang.bind(this, this._onPreferencesActivate));
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(item);

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

        item = new PopupMenu.PopupAlternatingMenuItem(_("Suspend"),
                                                      _("Power Off..."));
        this.menu.addMenuItem(item);
        this._suspendOrPowerOffItem = item;
        item.connect('activate', Lang.bind(this, this._onSuspendOrPowerOffActivate));
        this._updateSuspendOrPowerOff();
    },

    _updatePresenceStatus: function(item, event) {
        let status;

        if (item.state) {
            status = GnomeSession.PresenceStatus.AVAILABLE;
        } else {
            status = GnomeSession.PresenceStatus.BUSY;

            let [presence, s, msg] = this._accountMgr.get_most_available_presence();
            let newPresence = this._statusChooser.getIMPresenceForSessionStatus(status);
            if (newPresence != presence &&
                newPresence == Tp.ConnectionPresenceType.BUSY)
                Main.notify(_("Your chat status will be set to busy"),
                            _("Notifications are now disabled, including chat messages. Your online status has been adjusted to let others know that you might not see their messages."));
        }

        this._presence.status = status;
    },

    _onMyAccountActivate: function() {
        Main.overview.hide();
        let app = Shell.AppSystem.get_default().lookup_setting('gnome-user-accounts-panel.desktop');
        app.activate();
    },

    _onOnlineAccountsActivate: function() {
        Main.overview.hide();
        let app = Shell.AppSystem.get_default().lookup_setting('gnome-online-accounts-panel.desktop');
        app.activate(-1);
    },

    _onPreferencesActivate: function() {
        Main.overview.hide();
        let app = Shell.AppSystem.get_default().lookup_app('gnome-control-center.desktop');
        app.activate();
    },

    _onLockScreenActivate: function() {
        Main.overview.hide();
        this._screenSaverProxy.LockRemote();
    },

    _onLoginScreenActivate: function() {
        Main.overview.hide();
        // Ensure we only move to GDM after the screensaver has activated; in some
        // OS configurations, the X server may block event processing on VT switch
        this._screenSaverProxy.SetActiveRemote(true, Lang.bind(this, function() {
            this._userManager.goto_login_session();
        }));
    },

    _onQuitSessionActivate: function() {
        Main.overview.hide();
        this._session.LogoutRemote(0);
    },

    _onSuspendOrPowerOffActivate: function() {
        Main.overview.hide();

        if (this._haveSuspend &&
            this._suspendOrPowerOffItem.state == PopupMenu.PopupAlternatingMenuItemState.DEFAULT) {
            // Ensure we only suspend after locking the screen
            this._screenSaverProxy.LockRemote(Lang.bind(this, function() {
                this._upClient.suspend_sync(null);
            }));
        } else {
            this._session.ShutdownRemote();
        }
    }
});
