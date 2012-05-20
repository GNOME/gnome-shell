// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const AccountsService = imports.gi.AccountsService;
const Clutter = imports.gi.Clutter;
const Gdm = imports.gi.Gdm;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Signals = imports.signals;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;
const ShellEntry = imports.ui.shellEntry;
const Tweener = imports.ui.tweener;
const UserMenu = imports.ui.userMenu;

const Batch = imports.gdm.batch;
const GdmUtil = imports.gdm.util;

// A widget showing the user avatar and name
const UserWidget = new Lang.Class({
    Name: 'UserWidget',

    _init: function(user) {
        this._user = user;

        this.actor = new St.BoxLayout({ style_class: 'unlock-dialog-user-name-container',
                                        vertical: false });

        this._avatar = new UserMenu.UserAvatarWidget(user);
        this.actor.add(this._avatar.actor,
                       { x_fill: true, y_fill: true });

        this._label = new St.Label({ style_class: 'unlock-dialog-user-name' });
        this.actor.add(this._label,
                       { expand: true,
                         x_fill: true,
                         y_fill: true });

        this._userLoadedId = this._user.connect('notify::is-loaded',
                                                Lang.bind(this, this._updateUser));
        this._userChangedId = this._user.connect('changed',
                                                 Lang.bind(this, this._updateUser));
        if (this._user.is_loaded)
            this._updateUser();
    },

    destroy: function() {
        if (this._userLoadedId != 0) {
            this._user.disconnect(this._userLoadedId);
            this._userLoadedId = 0;
        }

        if (this._userChangedId != 0) {
            this._user.disconnect(this._userChangedId);
            this._userChangedId = 0;
        }

        this.actor.destroy();
    },

    _updateUser: function() {
        if (this._user.is_loaded)
            this._label.text = this._user.get_real_name();
        else
            this._label.text = '';

        this._avatar.update();
    }
});

const UnlockDialog = new Lang.Class({
    Name: 'UnlockDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function() {
        this.parent({ shellReactive: true,
                      styleClass: 'login-dialog' });

        this._userManager = AccountsService.UserManager.get_default();
        this._userName = GLib.get_user_name();
        this._user = this._userManager.get_user(this._userName);

        this._greeterClient = new Gdm.Client();
        this._userVerifier = new GdmUtil.ShellUserVerifier(this._greeterClient);

        this._userVerifier.connect('reset', Lang.bind(this, this._reset));
        this._userVerifier.connect('ask-question', Lang.bind(this, this._onAskQuestion));
        this._userVerifier.connect('verification-complete', Lang.bind(this, this._onVerificationComplete));
        this._userVerifier.connect('verification-failed', Lang.bind(this, this._onVerificationFailed));

        this._userVerifier.connect('show-fingerprint-prompt', Lang.bind(this, this._showFingerprintPrompt));
        this._userVerifier.connect('hide-fingerprint-prompt', Lang.bind(this, this._hideFingerprintPrompt));

        this._userWidget = new UserWidget(this._user);
        this.contentLayout.add_actor(this._userWidget.actor);

        this._promptLayout = new St.BoxLayout({ style_class: 'login-dialog-prompt-layout',
                                                vertical: false });

        this._promptLabel = new St.Label({ style_class: 'login-dialog-prompt-label' });
        this._promptLayout.add(this._promptLabel,
                               { expand: false,
                                 x_fill: true,
                                 y_fill: true,
                                 x_align: St.Align.START });

        this._promptEntry = new St.Entry({ style_class: 'login-dialog-prompt-entry',
                                           can_focus: true });
        ShellEntry.addContextMenu(this._promptEntry);
        this.setInitialKeyFocus(this._promptEntry);
        this._promptEntry.clutter_text.connect('activate', Lang.bind(this, this._doUnlock));

        this._promptLayout.add(this._promptEntry,
                               { expand: true,
                                 x_fill: true,
                                 y_fill: false,
                                 x_align: St.Align.START });

        this.contentLayout.add_actor(this._promptLayout);

        // Translators: this message is shown below the password entry field
        // to indicate the user can swipe their finger instead
        this._promptFingerprintMessage = new St.Label({ text: _("(or swipe finger)"),
                                                        style_class: 'login-dialog-prompt-fingerprint-message' });
        this._promptFingerprintMessage.hide();
        this.contentLayout.add_actor(this._promptFingerprintMessage);

        let otherUserLabel = new St.Label({ text: _("Login as another user"),
                                            style_class: 'login-dialog-not-listed-label' });
        this._otherUserButton = new St.Button({ style_class: 'login-dialog-not-listed-button',
                                                can_focus: true,
                                                child: otherUserLabel,
                                                reactive: true,
                                                x_align: St.Align.START,
                                                x_fill: true });
        this._otherUserButton.connect('clicked', Lang.bind(this, this._otherUserClicked));
        this.contentLayout.add(this._otherUserButton,
                               { x_align: St.Align.START,
                                 x_fill: false });

        this._okButton = { label: _("Unlock"),
                           action: Lang.bind(this, this._doUnlock),
                           default: true };
        this.setButtons([this._okButton]);

        this._updateOkButton(false);
        this._reset();
    },

    _updateOkButton: function(sensitive) {
        this._okButton.button.reactive = sensitive;
    },

    _reset: function() {
        this._userVerifier.begin(this._userName, new Batch.Hold());
    },

    _onAskQuestion: function(verifier, serviceName, question, passwordChar) {
        this._promptLabel.text = question;

        this._promptEntry.text = '';
        this._promptEntry.clutter_text.set_password_char(passwordChar);
        this._promptEntry.menu.isPassword = passwordChar != '';

        this._currentQuery = serviceName;
        this._updateOkButton(true);
    },

    _showFingerprintPrompt: function() {
        GdmUtil.fadeInActor(this._promptFingerprintMessage);
    },

    _hideFingerprintPrompt: function() {
        GdmUtil.fadeOutActor(this._promptFingerprintMessage);
    },

    _doUnlock: function() {
        if (!this._currentQuery)
            return;

        let query = this._currentQuery;
        this._currentQuery = null;

        this._updateOkButton(false);

        this._userVerifier.answerQuery(query, this._promptEntry.text);
    },

    _onVerificationComplete: function() {
        this._userVerifier.clear();
        this.emit('unlocked');
    },

    _onVerificationFailed: function() {
        this._userVerifier.cancel();
        this.emit('failed');
    },

    _otherUserClicked: function(button, event) {
        this._userManager.goto_login_session();

        this._userVerifier.cancel();
        this.emit('failed');
    },

    destroy: function() {
        this._userVerifier.clear();
        this.parent();
    },

    cancel: function() {
        this._userVerifier.cancel(null);

        this.destroy();
    },
});
