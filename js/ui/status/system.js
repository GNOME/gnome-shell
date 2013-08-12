// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const AccountsService = imports.gi.AccountsService;
const Gdm = imports.gi.Gdm;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
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

const Indicator = new Lang.Class({
    Name: 'SystemIndicator',
    Extends: PanelMenu.SystemStatusButton,

    _init: function() {
        this.parent('system-shutdown-symbolic', _("System"));

        this._screenSaverSettings = new Gio.Settings({ schema: SCREENSAVER_SCHEMA });
        this._lockdownSettings = new Gio.Settings({ schema: LOCKDOWN_SCHEMA });
        this._privacySettings = new Gio.Settings({ schema: PRIVACY_SCHEMA });

        this._session = new GnomeSession.SessionManager();
        this._haveShutdown = true;

        this._loginManager = LoginManager.getLoginManager();
        this._userManager = AccountsService.UserManager.get_default();
        this._user = this._userManager.get_user(GLib.get_user_name());

        this._createSubMenu();

        this._userManager.connect('notify::is-loaded',
                                  Lang.bind(this, this._updateMultiUser));
        this._userManager.connect('notify::has-multiple-users',
                                  Lang.bind(this, this._updateMultiUser));
        this._userManager.connect('user-added',
                                  Lang.bind(this, this._updateMultiUser));
        this._userManager.connect('user-removed',
                                  Lang.bind(this, this._updateMultiUser));
        this._lockdownSettings.connect('changed::' + DISABLE_USER_SWITCH_KEY,
                                       Lang.bind(this, this._updateMultiUser));
        this._lockdownSettings.connect('changed::' + DISABLE_LOG_OUT_KEY,
                                       Lang.bind(this, this._updateMultiUser));
        this._lockdownSettings.connect('changed::' + DISABLE_LOCK_SCREEN_KEY,
                                       Lang.bind(this, this._updateLockScreen));
        global.settings.connect('changed::' + ALWAYS_SHOW_LOG_OUT_KEY,
                                Lang.bind(this, this._updateMultiUser));
        this._updateSwitchUser();
        this._updateMultiUser();

        // Whether shutdown is available or not depends on both lockdown
        // settings (disable-log-out) and Polkit policy - the latter doesn't
        // notify, so we update the menu item each time the menu opens or
        // the lockdown setting changes, which should be close enough.
        this.menu.connect('open-state-changed', Lang.bind(this,
            function(menu, open) {
                if (!open)
                    return;

                this._updateHaveShutdown();
            }));
        this._lockdownSettings.connect('changed::' + DISABLE_LOG_OUT_KEY,
                                       Lang.bind(this, this._updateHaveShutdown));

        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
        this._sessionUpdated();
    },

    _sessionUpdated: function() {
        this._updateLockScreen();
        this._updatePowerOff();
        this._settingsAction.visible = Main.sessionMode.allowSettings;
    },

    _updateMultiUser: function() {
        let shouldShowInMode = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        let hasSwitchUser = this._updateSwitchUser();
        let hasLogout = this._updateLogout();

        this._switchUserSubMenu.actor.visible = shouldShowInMode && (hasSwitchUser || hasLogout);
    },

    _updateSwitchUser: function() {
        let allowSwitch = !this._lockdownSettings.get_boolean(DISABLE_USER_SWITCH_KEY);
        let multiUser = this._userManager.can_switch() && this._userManager.has_multiple_users;

        let visible = allowSwitch && multiUser;
        this._loginScreenItem.actor.visible = visible;
        return visible;
    },

    _updateLogout: function() {
        let allowLogout = !this._lockdownSettings.get_boolean(DISABLE_LOG_OUT_KEY);
        let alwaysShow = global.settings.get_boolean(ALWAYS_SHOW_LOG_OUT_KEY);
        let systemAccount = this._user.system_account;
        let localAccount = this._user.local_account;
        let multiUser = this._userManager.has_multiple_users;
        let multiSession = Gdm.get_session_ids().length > 1;

        let visible = allowLogout && (alwaysShow || multiUser || multiSession || systemAccount || !localAccount);
        this._logoutItem.actor.visible = visible;
        return visible;
    },

    _updateSwitchUserSubMenu: function() {
        this._switchUserSubMenu.label.text = this._user.get_real_name();

        let iconFile = this._user.get_icon_file();
        if (iconFile && !GLib.file_test(iconFile, GLib.FileTest.EXISTS))
            iconFile = null;

        if (iconFile) {
            let file = Gio.File.new_for_path(iconFile);
            let gicon = new Gio.FileIcon({ file: file });
            this._switchUserSubMenu.icon.gicon = gicon;
        } else {
            this._switchUserSubMenu.icon.icon_name = 'avatar-default-symbolic';
        }
    },

    _updateLockScreen: function() {
        let showLock = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        let allowLockScreen = !this._lockdownSettings.get_boolean(DISABLE_LOCK_SCREEN_KEY);
        this._lockScreenAction.visible = showLock && allowLockScreen && LoginManager.canLock();
    },

    _updateHaveShutdown: function() {
        this._session.CanShutdownRemote(Lang.bind(this,
            function(result, error) {
                if (!error) {
                    this._haveShutdown = result[0];
                    this._updatePowerOff();
                }
            }));
    },

    _updatePowerOff: function() {
        this._powerOffAction.visible = this._haveShutdown && !Main.sessionMode.isLocked;
    },

    _createActionButton: function(iconName, accessibleName) {
        let icon = new St.Button({ reactive: true,
                                   can_focus: true,
                                   track_hover: true,
                                   accessible_name: accessibleName,
                                   style_class: 'system-menu-action' });
        icon.child = new St.Icon({ icon_name: iconName });
        return icon;
    },

    _createSubMenu: function() {
        let item;

        this._switchUserSubMenu = new PopupMenu.PopupSubMenuMenuItem('', true);
        this._switchUserSubMenu.icon.style_class = 'system-switch-user-submenu-icon';

        item = new PopupMenu.PopupMenuItem(_("Switch User"));
        item.connect('activate', Lang.bind(this, this._onLoginScreenActivate));
        this._switchUserSubMenu.menu.addMenuItem(item);
        this._loginScreenItem = item;

        item = new PopupMenu.PopupMenuItem(_("Log Out"));
        item.connect('activate', Lang.bind(this, this._onQuitSessionActivate));
        this._switchUserSubMenu.menu.addMenuItem(item);
        this._logoutItem = item;

        this._user.connect('notify::is-loaded', Lang.bind(this, this._updateSwitchUserSubMenu));
        this._user.connect('changed', Lang.bind(this, this._updateSwitchUserSubMenu));

        this.menu.addMenuItem(this._switchUserSubMenu);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        let hbox = new St.BoxLayout({ style_class: 'system-menu-actions-box' });

        this._settingsAction = this._createActionButton('preferences-system-symbolic', _("Settings"));
        this._settingsAction.connect('clicked', Lang.bind(this, this._onSettingsClicked));
        hbox.add(this._settingsAction, { expand: true, x_fill: false });

        this._lockScreenAction = this._createActionButton('changes-prevent-symbolic', _("Lock"));
        this._lockScreenAction.connect('clicked', Lang.bind(this, this._onLockScreenClicked));
        hbox.add(this._lockScreenAction, { expand: true, x_fill: false });

        this._powerOffAction = this._createActionButton('system-shutdown-symbolic', _("Power Off"));
        this._powerOffAction.connect('clicked', Lang.bind(this, this._onPowerOffClicked));
        hbox.add(this._powerOffAction, { expand: true, x_fill: false });

        item = new PopupMenu.PopupBaseMenuItem({ reactive: false,
                                                 can_focus: false });
        item.addActor(hbox, { expand: true });

        this.menu.addMenuItem(item);
    },

    _onSettingsClicked: function() {
        this.menu.itemActivated();
        let app = Shell.AppSystem.get_default().lookup_app('gnome-control-center.desktop');
        Main.overview.hide();
        app.activate();
    },

    _onLockScreenClicked: function() {
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

    _onPowerOffClicked: function() {
        this.menu.itemActivated();
        Main.overview.hide();
        this._loginManager.listSessions(Lang.bind(this, function(result) {
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
    }
});
