// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const AccountsService = imports.gi.AccountsService;
const Atk = imports.gi.Atk;
const Clutter = imports.gi.Clutter;
const Gdm  = imports.gi.Gdm;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Signals = imports.signals;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Layout = imports.ui.layout;
const Main = imports.ui.main;
const Panel = imports.ui.panel;
const Tweener = imports.ui.tweener;
const UserWidget = imports.ui.userWidget;

const AuthPrompt = imports.gdm.authPrompt;
const Batch = imports.gdm.batch;
const GdmUtil = imports.gdm.util;
const LoginDialog = imports.gdm.loginDialog;

// The timeout before going back automatically to the lock screen (in seconds)
const IDLE_TIMEOUT = 2 * 60;

const UnlockDialog = new Lang.Class({
    Name: 'UnlockDialog',

    _init: function(parentActor) {
        this.actor = new St.Widget({ accessible_role: Atk.Role.WINDOW,
                                     style_class: 'login-dialog',
                                     layout_manager: new Clutter.BoxLayout(),
                                     visible: false });

        this.actor.add_constraint(new Layout.MonitorConstraint({ primary: true }));
        parentActor.add_child(this.actor);

        this._userManager = AccountsService.UserManager.get_default();
        this._userName = GLib.get_user_name();
        this._user = this._userManager.get_user(this._userName);

        this._promptBox = new St.BoxLayout({ vertical: true,
                                             x_align: Clutter.ActorAlign.CENTER,
                                             y_align: Clutter.ActorAlign.CENTER,
                                             x_expand: true,
                                             y_expand: true });
        this.actor.add_child(this._promptBox);

        this._authPrompt = new AuthPrompt.AuthPrompt(new Gdm.Client(), AuthPrompt.AuthPromptMode.UNLOCK_ONLY);
        this._authPrompt.connect('failed', Lang.bind(this, this._fail));
        this._authPrompt.connect('cancelled', Lang.bind(this, this._fail));
        this._authPrompt.connect('reset', Lang.bind(this, this._onReset));
        this._authPrompt.setPasswordChar('\u25cf');
        this._authPrompt.nextButton.label = _("Unlock");

        this._promptBox.add_child(this._authPrompt.actor);

        this.allowCancel = false;

        let screenSaverSettings = new Gio.Settings({ schema: 'org.gnome.desktop.screensaver' });
        if (screenSaverSettings.get_boolean('user-switch-enabled')) {
            let otherUserLabel = new St.Label({ text: _("Log in as another user"),
                                                style_class: 'login-dialog-not-listed-label' });
            this._otherUserButton = new St.Button({ style_class: 'login-dialog-not-listed-button',
                                                    can_focus: true,
                                                    child: otherUserLabel,
                                                    reactive: true,
                                                    x_align: St.Align.START,
                                                    x_fill: false });
            this._otherUserButton.connect('clicked', Lang.bind(this, this._otherUserClicked));
            this._promptBox.add_child(this._otherUserButton);
        } else {
            this._otherUserButton = null;
        }

        this._authPrompt.reset();
        this._updateSensitivity(true);

        Main.ctrlAltTabManager.addGroup(this.actor, _("Unlock Window"), 'dialog-password-symbolic');

        this._idleMonitor = Meta.IdleMonitor.get_core();
        this._idleWatchId = this._idleMonitor.add_idle_watch(IDLE_TIMEOUT * 1000, Lang.bind(this, this._escape));
    },

    _updateSensitivity: function(sensitive) {
        this._authPrompt.updateSensitivity(sensitive);

        if (this._otherUserButton) {
            this._otherUserButton.reactive = sensitive;
            this._otherUserButton.can_focus = sensitive;
        }
    },

    _fail: function() {
        this.emit('failed');
    },

    _onReset: function(authPrompt, beginRequest) {
        let userName;
        if (beginRequest == AuthPrompt.BeginRequestType.PROVIDE_USERNAME) {
            this._authPrompt.setUser(this._user);
            userName = this._userName;
        } else {
            userName = null;
        }

        this._authPrompt.begin({ userName: userName });
    },

    _escape: function() {
        if (this.allowCancel)
            this._authPrompt.cancel();
    },

    _otherUserClicked: function(button, event) {
        Gdm.goto_login_session_sync(null);

        this._authPrompt.cancel();
    },

    destroy: function() {
        this.popModal();
        this.actor.destroy();

        if (this._idleWatchId) {
            this._idleMonitor.remove_watch(this._idleWatchId);
            this._idleWatchId = 0;
        }
    },

    cancel: function() {
        this._authPrompt.cancel();

        this.destroy();
    },

    addCharacter: function(unichar) {
        this._authPrompt.addCharacter(unichar);
    },

    finish: function(onComplete) {
        this._authPrompt.finish(onComplete);
    },

    open: function(timestamp) {
        this.actor.show();

        if (this._isModal)
            return true;

        if (!Main.pushModal(this.actor, { timestamp: timestamp,
                                          keybindingMode: Shell.KeyBindingMode.UNLOCK_SCREEN }))
            return false;

        this._isModal = true;

        return true;
    },

    popModal: function(timestamp) {
        if (this._isModal) {
            Main.popModal(this.actor, timestamp);
            this._isModal = false;
        }
    }
});
Signals.addSignalMethods(UnlockDialog.prototype);
