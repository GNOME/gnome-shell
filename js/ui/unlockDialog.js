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
const Signals = imports.signals;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Layout = imports.ui.layout;
const Main = imports.ui.main;
const Panel = imports.ui.panel;
const Tweener = imports.ui.tweener;
const UserWidget = imports.ui.userWidget;

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
                                     visible: false });

        this.actor.add_constraint(new Layout.MonitorConstraint({ primary: true }));
        parentActor.add_child(this.actor);

        this._userManager = AccountsService.UserManager.get_default();
        this._userName = GLib.get_user_name();
        this._user = this._userManager.get_user(this._userName);

        this._failCounter = 0;

        this._greeterClient = new Gdm.Client();
        this._userVerifier = new GdmUtil.ShellUserVerifier(this._greeterClient, { reauthenticationOnly: true });
        this._userVerified = false;

        this._userVerifier.connect('ask-question', Lang.bind(this, this._onAskQuestion));
        this._userVerifier.connect('show-message', Lang.bind(this, this._showMessage));
        this._userVerifier.connect('verification-complete', Lang.bind(this, this._onVerificationComplete));
        this._userVerifier.connect('verification-failed', Lang.bind(this, this._onVerificationFailed));
        this._userVerifier.connect('reset', Lang.bind(this, this._onReset));

        this._userVerifier.connect('show-login-hint', Lang.bind(this, this._showLoginHint));
        this._userVerifier.connect('hide-login-hint', Lang.bind(this, this._hideLoginHint));

        this._promptBox = new St.BoxLayout({ vertical: true });
        this.actor.add_child(this._promptBox);
        this._promptBox.add_constraint(new Clutter.AlignConstraint({ source: this.actor,
                                                                     align_axis: Clutter.AlignAxis.BOTH,
                                                                     factor: 0.5 }));

        this._authPrompt = new GdmUtil.AuthPrompt({ style_class: 'login-dialog-prompt-layout',
                                                    vertical: true });
        this._authPrompt.setUser(this._user);
        this._authPrompt.setPasswordChar('\u25cf');
        this._authPrompt.resetButtons(_("Cancel"), _("Unlock"));
        this._authPrompt.connect('cancel', Lang.bind(this, this._escape));
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
                                                    x_fill: true });
            this._otherUserButton.connect('clicked', Lang.bind(this, this._otherUserClicked));
            this._promptBox.add_child(this._otherUserButton);
        } else {
            this._otherUserButton = null;
        }

        this._updateSensitivity(true);

        let batch = new Batch.Hold();
        this._userVerifier.begin(this._userName, batch);

        Main.ctrlAltTabManager.addGroup(this.actor, _("Unlock Window"), 'dialog-password-symbolic');

        this._idleMonitor = new GnomeDesktop.IdleMonitor();
        this._idleWatchId = this._idleMonitor.add_idle_watch(IDLE_TIMEOUT * 1000, Lang.bind(this, this._escape));
    },

    _updateSensitivity: function(sensitive) {
        this._authPrompt.updateSensitivity(sensitive);

        if (this._otherUserButton) {
            this._otherUserButton.reactive = sensitive;
            this._otherUserButton.can_focus = sensitive;
        }
    },

    _showMessage: function(userVerifier, message, styleClass) {
        this._authPrompt.setMessage(message, styleClass);
    },

    _onAskQuestion: function(verifier, serviceName, question, passwordChar) {
        this._currentQuery = serviceName;

        this._authPrompt.setPasswordChar(passwordChar);
        this._authPrompt.setQuestion(question);

        let signalId = this._authPrompt.connect('next', Lang.bind(this, function() {
                                                    this._authPrompt.disconnect(signalId);
                                                    this._doUnlock();
                                                }));

        this._updateSensitivity(true);
        this._authPrompt.stopSpinning();
    },

    _showLoginHint: function(verifier, message) {
        this._authPrompt.setHint(message);
    },

    _hideLoginHint: function() {
        this._authPrompt.setHint(null);
    },

    _doUnlock: function() {
        if (!this._currentQuery)
            return;

        let query = this._currentQuery;
        this._currentQuery = null;

        this._updateSensitivity(false);
        this._authPrompt.startSpinning();

        this._userVerifier.answerQuery(query, this._authPrompt.getAnswer());
    },

    _finishUnlock: function() {
        this._userVerifier.clear();
        this._authPrompt.clear();
        this._authPrompt.stopSpinning();
        this._updateSensitivity(true);
        this.emit('unlocked');
    },

    _onVerificationComplete: function() {
        this._userVerified = true;
        if (!this._userVerifier.hasPendingMessages) {
            this._finishUnlock();
        } else {
            let signalId = this._userVerifier.connect('no-more-messages',
                                                      Lang.bind(this, function() {
                                                          this._userVerifier.disconnect(signalId);
                                                          this._finishUnlock();
                                                      }));
        }
    },

    _onReset: function() {
        if (!this._userVerified) {
            this._userVerifier.clear();
            this.emit('failed');
        }
    },

    _onVerificationFailed: function() {
        this._currentQuery = null;
        this._firstQuestion = true;
        this._userVerified = false;

        this._authPrompt.clear();

        this._updateSensitivity(false);
        this._authPrompt.stopSpinning();
    },

    _escape: function() {
        if (this.allowCancel) {
            this._userVerifier.cancel();
            this.emit('failed');
        }
    },

    _otherUserClicked: function(button, event) {
        Gdm.goto_login_session_sync(null);

        this._userVerifier.cancel();
        this.emit('failed');
    },

    destroy: function() {
        this._userVerifier.clear();

        if (this._idleWatchId) {
            this._idleMonitor.remove_watch(this._idleWatchId);
            this._idleWatchId = 0;
        }
    },

    cancel: function() {
        this._userVerifier.cancel(null);

        this.destroy();
    },

    addCharacter: function(unichar) {
        this._authPrompt.addCharacter(unichar);
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
