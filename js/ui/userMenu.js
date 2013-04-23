// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const AccountsService = imports.gi.AccountsService;
const Gdm = imports.gi.Gdm;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const St = imports.gi.St;
const Clutter = imports.gi.Clutter;

const BoxPointer = imports.ui.boxpointer;
const GnomeSession = imports.misc.gnomeSession;
const LoginManager = imports.misc.loginManager;
const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const Util = imports.misc.util;
const UserWidget = imports.ui.userWidget;

const LOCKDOWN_SCHEMA = 'org.gnome.desktop.lockdown';
const SCREENSAVER_SCHEMA = 'org.gnome.desktop.screensaver';
const PRIVACY_SCHEMA = 'org.gnome.desktop.privacy'
const DISABLE_USER_SWITCH_KEY = 'disable-user-switching';
const DISABLE_LOCK_SCREEN_KEY = 'disable-lock-screen';
const DISABLE_LOG_OUT_KEY = 'disable-log-out';
const ALWAYS_SHOW_LOG_OUT_KEY = 'always-show-log-out';

const MAX_USERS_IN_SESSION_DIALOG = 5;

const SystemdLoginSessionIface = <interface name='org.freedesktop.login1.Session'>
    <property name="Id" type="s" access="read"/>
    <property name="Remote" type="b" access="read"/>
    <property name="Class" type="s" access="read"/>
    <property name="Type" type="s" access="read"/>
    <property name="State" type="s" access="read"/>
</interface>;

const SystemdLoginSession = Gio.DBusProxy.makeProxyWrapper(SystemdLoginSessionIface);

const UserMenuButton = new Lang.Class({
    Name: 'UserMenuButton',
    Extends: PanelMenu.SystemStatusButton,

    _init: function() {
        this.parent('user-available-symbolic', _("User Menu"));

        this._screenSaverSettings = new Gio.Settings({ schema: SCREENSAVER_SCHEMA });
        this._lockdownSettings = new Gio.Settings({ schema: LOCKDOWN_SCHEMA });
        this._privacySettings = new Gio.Settings({ schema: PRIVACY_SCHEMA });

        this._session = new GnomeSession.SessionManager();
        this._haveShutdown = true;
        this._haveSuspend = true;

        this._createSubMenu();

        this._loginManager = LoginManager.getLoginManager();
        this._userManager = AccountsService.UserManager.get_default();
        this._user = this._userManager.get_user(GLib.get_user_name());
        this._userManager.connect('notify::is-loaded',
                                  Lang.bind(this, this._updateMultiUser));
        this._userManager.connect('notify::has-multiple-users',
                                  Lang.bind(this, this._updateMultiUser));
        this._userManager.connect('user-added',
                                  Lang.bind(this, this._updateMultiUser));
        this._userManager.connect('user-removed',
                                  Lang.bind(this, this._updateMultiUser));
        this._lockdownSettings.connect('changed::' + DISABLE_USER_SWITCH_KEY,
                                       Lang.bind(this, this._updateSwitchUser));
        this._lockdownSettings.connect('changed::' + DISABLE_LOG_OUT_KEY,
                                       Lang.bind(this, this._updateLogout));
        this._lockdownSettings.connect('changed::' + DISABLE_LOCK_SCREEN_KEY,
                                       Lang.bind(this, this._updateLockScreen));
        global.settings.connect('changed::' + ALWAYS_SHOW_LOG_OUT_KEY,
                                Lang.bind(this, this._updateLogout));
        this._updateSwitchUser();
        this._updateLogout();
        this._updateLockScreen();

        this._updatesFile = Gio.File.new_for_path('/var/lib/PackageKit/prepared-update');
        this._updatesMonitor = this._updatesFile.monitor(Gio.FileMonitorFlags.NONE, null);
        this._updatesMonitor.connect('changed', Lang.bind(this, this._updateInstallUpdates));

        // Whether shutdown is available or not depends on both lockdown
        // settings (disable-log-out) and Polkit policy - the latter doesn't
        // notify, so we update the menu item each time the menu opens or
        // the lockdown setting changes, which should be close enough.
        this.menu.connect('open-state-changed', Lang.bind(this,
            function(menu, open) {
                if (!open)
                    return;

                this._updateHaveShutdown();
                this._updateHaveSuspend();
            }));
        this._lockdownSettings.connect('changed::' + DISABLE_LOG_OUT_KEY,
                                       Lang.bind(this, this._updateHaveShutdown));

        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
        this._sessionUpdated();
    },

    _sessionUpdated: function() {
        this.actor.visible = !Main.sessionMode.isGreeter;
        this.setSensitive(!Main.sessionMode.isLocked);
    },

    _updateMultiUser: function() {
        this._updateSwitchUser();
        this._updateLogout();
    },

    _updateSwitchUser: function() {
        let allowSwitch = !this._lockdownSettings.get_boolean(DISABLE_USER_SWITCH_KEY);
        let multiUser = this._userManager.can_switch() && this._userManager.has_multiple_users;

        this._loginScreenItem.actor.visible = allowSwitch && multiUser;
    },

    _updateLogout: function() {
        let allowLogout = !this._lockdownSettings.get_boolean(DISABLE_LOG_OUT_KEY);
        let alwaysShow = global.settings.get_boolean(ALWAYS_SHOW_LOG_OUT_KEY);
        let systemAccount = this._user.system_account;
        let localAccount = this._user.local_account;
        let multiUser = this._userManager.has_multiple_users;
        let multiSession = Gdm.get_session_ids().length > 1;

        this._logoutItem.actor.visible = allowLogout && (alwaysShow || multiUser || multiSession || systemAccount || !localAccount);
    },

    _updateLockScreen: function() {
        let allowLockScreen = !this._lockdownSettings.get_boolean(DISABLE_LOCK_SCREEN_KEY);
        this._lockScreenItem.actor.visible = allowLockScreen && LoginManager.canLock();
    },

    _updateInstallUpdates: function() {
        let haveUpdates = this._updatesFile.query_exists(null);
        this._installUpdatesItem.actor.visible = haveUpdates && this._haveShutdown;
    },

    _updateHaveShutdown: function() {
        this._session.CanShutdownRemote(Lang.bind(this,
            function(result, error) {
                if (!error) {
                    this._haveShutdown = result[0];
                    this._updateInstallUpdates();
                    this._updateSuspendOrPowerOff();
                }
            }));
    },

    _updateHaveSuspend: function() {
        this._loginManager.canSuspend(Lang.bind(this,
            function(result) {
                this._haveSuspend = result;
                this._updateSuspendOrPowerOff();
        }));
    },

    _updateSuspendOrPowerOff: function() {
        if (!this._suspendOrPowerOffItem)
            return;

        this._suspendOrPowerOffItem.actor.visible = this._haveShutdown || this._haveSuspend;

        // If we can't power off show Suspend instead
        // and disable the alt key
        if (!this._haveShutdown) {
            this._suspendOrPowerOffItem.updateText(_("Suspend"), null);
        } else if (!this._haveSuspend) {
            this._suspendOrPowerOffItem.updateText(_("Power Off"), null);
        } else {
            this._suspendOrPowerOffItem.updateText(_("Power Off"), _("Suspend"));
        }
    },

    _createSubMenu: function() {
        let item;

        this.menu.addSettingsAction(_("Settings"), 'gnome-control-center.desktop');

        item = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupMenuItem(_("Switch User"));
        item.connect('activate', Lang.bind(this, this._onLoginScreenActivate));
        this.menu.addMenuItem(item);
        this._loginScreenItem = item;

        item = new PopupMenu.PopupMenuItem(_("Log Out"));
        item.connect('activate', Lang.bind(this, this._onQuitSessionActivate));
        this.menu.addMenuItem(item);
        this._logoutItem = item;

        item = new PopupMenu.PopupMenuItem(_("Lock"));
        item.connect('activate', Lang.bind(this, this._onLockScreenActivate));
        this.menu.addMenuItem(item);
        this._lockScreenItem = item;

        item = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(item);

        item = new PopupMenu.PopupAlternatingMenuItem(_("Power Off"),
                                                      _("Suspend"));
        this.menu.addMenuItem(item);
        item.connect('activate', Lang.bind(this, this._onSuspendOrPowerOffActivate));
        this._suspendOrPowerOffItem = item;
        this._updateSuspendOrPowerOff();

        item = new PopupMenu.PopupMenuItem(_("Install Updates & Restart"));
        item.connect('activate', Lang.bind(this, this._onInstallUpdatesActivate));
        this.menu.addMenuItem(item);
        this._installUpdatesItem = item;
    },

    _onLockScreenActivate: function() {
        this.menu.itemActivated(BoxPointer.PopupAnimation.NONE);
        Main.overview.hide();
        Main.screenShield.lock(true);
    },

    _onLoginScreenActivate: function() {
        this.menu.itemActivated(BoxPointer.PopupAnimation.NONE);
        Main.overview.hide();
        if (Main.screenShield)
            Main.screenShield.lock(false);
        Gdm.goto_login_session_sync(null);
    },

    _onQuitSessionActivate: function() {
        Main.overview.hide();
        this._session.LogoutRemote(0);
    },

    _onInstallUpdatesActivate: function() {
        Main.overview.hide();
        Util.spawn(['pkexec', '/usr/libexec/pk-trigger-offline-update']);

        this._session.RebootRemote();
    },

    _openSessionWarnDialog: function(sessions) {
        let dialog = new ModalDialog.ModalDialog();
        let subjectLabel = new St.Label({ style_class: 'end-session-dialog-subject',
                                          text: _("Other users are logged in.") });
        dialog.contentLayout.add(subjectLabel, { y_fill: true,
                                                 y_align: St.Align.START });

        let descriptionLabel = new St.Label({ style_class: 'end-session-dialog-description'});
        descriptionLabel.set_text(_("Shutting down might cause them to lose unsaved work."));
        descriptionLabel.clutter_text.line_wrap = true;
        dialog.contentLayout.add(descriptionLabel, { x_fill: true,
                                                     y_fill: true,
                                                     y_align: St.Align.START });

        let scrollView = new St.ScrollView({ style_class: 'end-session-dialog-app-list' });
        scrollView.add_style_class_name('vfade');
        scrollView.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC);
        dialog.contentLayout.add(scrollView, { x_fill: true, y_fill: true });

        let userList = new St.BoxLayout({ vertical: true });
        scrollView.add_actor(userList);

        for (let i = 0; i < sessions.length; i++) {
            let session = sessions[i];
            let userEntry = new St.BoxLayout({ style_class: 'login-dialog-user-list-item',
                                               vertical: false });
            let avatar = new UserWidget.Avatar(session.user);
            avatar.update();
            userEntry.add(avatar.actor);

            let userLabelText = "";;
            let userName = session.user.get_real_name() ?
                           session.user.get_real_name() : session.username;

            if (session.info.remote)
                /* Translators: Remote here refers to a remote session, like a ssh login */
                userLabelText = _("%s (remote)").format(userName);
            else if (session.info.type == "tty")
                /* Translators: Console here refers to a tty like a VT console */
                userLabelText = _("%s (console)").format(userName);
            else
                userLabelText = userName;

            let textLayout = new St.BoxLayout({ style_class: 'login-dialog-user-list-item-text-box',
                                                vertical: true });
            textLayout.add(new St.Label({ text: userLabelText }),
                           { y_fill: false,
                             y_align: St.Align.MIDDLE,
                             expand: true });
            userEntry.add(textLayout, { expand: true });
            userList.add(userEntry, { x_fill: true });
        }

        let cancelButton = { label: _("Cancel"),
                             action: function() { dialog.close(); },
                             key: Clutter.Escape };

        let powerOffButton = { label: _("Power Off"),  action: Lang.bind(this, function() {
            dialog.close();
            this._session.ShutdownRemote();
        }), default: true };

        dialog.setButtons([cancelButton, powerOffButton]);

        dialog.open();
    },

    _onSuspendOrPowerOffActivate: function() {
        Main.overview.hide();

        if (this._haveShutdown &&
            this._suspendOrPowerOffItem.state == PopupMenu.PopupAlternatingMenuItemState.DEFAULT) {
            this._loginManager.listSessions(Lang.bind(this,
                function(result) {
                    let sessions = [];
                    let n = 0;
                    for (let i = 0; i < result.length; i++) {
                        let[id, uid, userName, seat, sessionPath] = result[i];
                        let proxy = new SystemdLoginSession(Gio.DBus.system,
                                                            'org.freedesktop.login1',
                                                            sessionPath);

                        if (proxy.Class != 'user')
                            continue;

                        if (proxy.State == 'closing')
                            continue;

                        if (proxy.Id == GLib.getenv('XDG_SESSION_ID'))
                            continue;

                        sessions.push({ user: this._userManager.get_user(userName),
                                        username: userName,
                                        info: { type: proxy.Type,
                                                remote: proxy.Remote }
                        });

                        // limit the number of entries
                        n++;
                        if (n == MAX_USERS_IN_SESSION_DIALOG)
                            break;
                    }

                    if (n != 0)
                        this._openSessionWarnDialog(sessions);
                    else
                        this._session.ShutdownRemote();
            }));
        } else {
            this.menu.itemActivated(BoxPointer.PopupAnimation.NONE);
            this._loginManager.suspend();
        }
    }
});
