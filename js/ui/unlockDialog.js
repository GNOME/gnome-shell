// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const AccountsService = imports.gi.AccountsService;
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

const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;
const Panel = imports.ui.panel;
const ShellEntry = imports.ui.shellEntry;
const Tweener = imports.ui.tweener;
const UserMenu = imports.ui.userMenu;

const Batch = imports.gdm.batch;
const GdmUtil = imports.gdm.util;
const LoginDialog = imports.gdm.loginDialog;

// The timeout before going back automatically to the lock screen (in seconds)
const IDLE_TIMEOUT = 2 * 60;

function versionCompare(required, reference) {
    required = required.split('.');
    reference = reference.split('.');

    for (let i = 0; i < required.length; i++) {
        if (required[i] != reference[i])
            return required[i] < reference[i];
    }

    return true;
}

function isSupported() {
    try {
        let params = GLib.Variant.new('(ss)', ['org.gnome.DisplayManager.Manager', 'Version']);
        let result = Gio.DBus.system.call_sync('org.gnome.DisplayManager',
                                               '/org/gnome/DisplayManager/Manager',
                                               'org.freedesktop.DBus.Properties',
                                               'Get', params, null,
                                               Gio.DBusCallFlags.NONE,
                                               -1, null);

        let version = result.deep_unpack()[0].deep_unpack();
        return versionCompare('3.5.91', version);
    } catch(e) {
        return false;
    }
}

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

        this._label = new St.Label({ style_class: 'login-dialog-username' });
        this.actor.add(this._label,
                       { expand: true,
                         x_fill: true,
                         y_fill: false,
                         y_align: St.Align.MIDDLE });

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

    _init: function(parentActor) {
        this.parent({ shellReactive: true,
                      styleClass: 'login-dialog',
                      keybindingMode: Shell.KeyBindingMode.UNLOCK_SCREEN,
                      parentActor: parentActor
                    });

        this._userManager = AccountsService.UserManager.get_default();
        this._userName = GLib.get_user_name();
        this._user = this._userManager.get_user(this._userName);

        this._failCounter = 0;
        this._firstQuestion = true;

        this._greeterClient = new Gdm.Client();
        this._userVerifier = new GdmUtil.ShellUserVerifier(this._greeterClient, { reauthenticationOnly: true });

        this._userVerifier.connect('ask-question', Lang.bind(this, this._onAskQuestion));
        this._userVerifier.connect('show-message', Lang.bind(this, this._showMessage));
        this._userVerifier.connect('verification-complete', Lang.bind(this, this._onVerificationComplete));
        this._userVerifier.connect('verification-failed', Lang.bind(this, this._onVerificationFailed));
        this._userVerifier.connect('reset', Lang.bind(this, this._onReset));

        this._userVerifier.connect('show-login-hint', Lang.bind(this, this._showLoginHint));
        this._userVerifier.connect('hide-login-hint', Lang.bind(this, this._hideLoginHint));

        this._userWidget = new UserWidget(this._user);
        this.contentLayout.add_actor(this._userWidget.actor);

        this._promptLayout = new St.BoxLayout({ style_class: 'login-dialog-prompt-layout',
                                                vertical: true });

        this._promptLabel = new St.Label({ style_class: 'login-dialog-prompt-label' });
        this._promptLayout.add(this._promptLabel,
                               { x_align: St.Align.START });

        this._promptEntry = new St.Entry({ style_class: 'login-dialog-prompt-entry',
                                           can_focus: true });
        this._promptEntry.clutter_text.set_password_char('\u25cf');
        ShellEntry.addContextMenu(this._promptEntry, { isPassword: true });
        this.setInitialKeyFocus(this._promptEntry);
        this._promptEntry.clutter_text.connect('text-changed', Lang.bind(this, function() {
            this._updateOkButtonSensitivity(this._promptEntry.text.length > 0);
        }));

        this._promptLayout.add(this._promptEntry,
                               { expand: true,
                                 x_fill: true });

        this.contentLayout.add_actor(this._promptLayout);

        this._promptMessage = new St.Label({ visible: false });
        this.contentLayout.add(this._promptMessage, { x_fill: true });

        this._promptLoginHint = new St.Label({ style_class: 'login-dialog-prompt-login-hint' });
        this._promptLoginHint.hide();
        this.contentLayout.add_actor(this._promptLoginHint);

        this._workSpinner = new Panel.AnimatedIcon('process-working.svg', LoginDialog.WORK_SPINNER_ICON_SIZE);
        this._workSpinner.actor.opacity = 0;

        this.allowCancel = false;
        this.buttonLayout.visible = true;
        this.addButton({ label: _("Cancel"),
                         action: Lang.bind(this, this._escape),
                         key: Clutter.KEY_Escape },
                       { expand: true,
                         x_fill: false,
                         y_fill: false,
                         x_align: St.Align.START,
                         y_align: St.Align.MIDDLE });
        this.buttonLayout.add(this._workSpinner.actor,
                              { expand: false,
                                x_fill: false,
                                y_fill: false,
                                x_align: St.Align.END,
                                y_align: St.Align.MIDDLE });
        this._okButton = this.addButton({ label: _("Unlock"),
                                          action: Lang.bind(this, this._doUnlock),
                                          default: true },
                                        { expand: false,
                                          x_fill: false,
                                          y_fill: false,
                                          x_align: St.Align.END,
                                          y_align: St.Align.MIDDLE });

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
            this.dialogLayout.add(this._otherUserButton,
                                  { x_align: St.Align.START,
                                    x_fill: false });
        } else {
            this._otherUserButton = null;
        }

        this._updateSensitivity(true);

        let batch = new Batch.Hold();
        this._userVerifier.begin(this._userName, batch);

        GLib.idle_add(GLib.PRIORITY_DEFAULT, Lang.bind(this, function() {
            this.emit('loaded');
            return false;
        }));

        Main.ctrlAltTabManager.addGroup(this.dialogLayout, _("Unlock Window"), 'dialog-password-symbolic');

        this._idleMonitor = new GnomeDesktop.IdleMonitor();
        this._idleWatchId = this._idleMonitor.add_idle_watch(IDLE_TIMEOUT * 1000, Lang.bind(this, this._escape));
    },

    _updateSensitivity: function(sensitive) {
        this._promptEntry.reactive = sensitive;
        this._promptEntry.clutter_text.editable = sensitive;
        this._updateOkButtonSensitivity(sensitive && this._promptEntry.text.length > 0);
        if (this._otherUserButton) {
            this._otherUserButton.reactive = sensitive;
            this._otherUserButton.can_focus = sensitive;
        }
    },

    _updateOkButtonSensitivity: function(sensitive) {
        this._okButton.reactive = sensitive;
        this._okButton.can_focus = sensitive;
    },

    _setWorking: function(working) {
        if (working) {
            this._workSpinner.play();
            Tweener.addTween(this._workSpinner.actor,
                             { opacity: 255,
                               delay: LoginDialog.WORK_SPINNER_ANIMATION_DELAY,
                               time: LoginDialog.WORK_SPINNER_ANIMATION_TIME,
                               transition: 'linear'
                             });
        } else {
            Tweener.addTween(this._workSpinner.actor,
                             { opacity: 0,
                               time: LoginDialog.WORK_SPINNER_ANIMATION_TIME,
                               transition: 'linear',
                               onCompleteScope: this,
                               onComplete: function() {
                                   this._workSpinner.stop();
                               }
                             });
        }
    },

    _showMessage: function(userVerifier, message, styleClass) {
        if (message) {
            this._promptMessage.text = message;
            this._promptMessage.styleClass = styleClass;
            GdmUtil.fadeInActor(this._promptMessage);
        } else {
            GdmUtil.fadeOutActor(this._promptMessage);
        }
    },

    _onAskQuestion: function(verifier, serviceName, question, passwordChar) {
        if (this._firstQuestion && this._firstQuestionAnswer) {
            this._userVerifier.answerQuery(serviceName, this._firstQuestionAnswer);
            this._firstQuestionAnswer = null;
            this._firstQuestion = false;
            return;
        }

        this._promptLabel.text = question;

        if (!this._firstQuestion)
            this._promptEntry.text = '';
        else
            this._firstQuestion = false;

        this._promptEntry.clutter_text.set_password_char(passwordChar);
        this._promptEntry.menu.isPassword = passwordChar != '';

        this._currentQuery = serviceName;
        this._updateSensitivity(true);
        this._setWorking(false);
    },

    _showLoginHint: function(verifier, message) {
        this._promptLoginHint.set_text(message)
        GdmUtil.fadeInActor(this._promptLoginHint);
    },

    _hideLoginHint: function() {
        GdmUtil.fadeOutActor(this._promptLoginHint);
    },

    _doUnlock: function() {
        if (this._firstQuestion) {
            // we haven't received a query yet, so stash the answer
            // and make ourself non-reactive
            // the actual reply to GDM will be sent as soon as asked
            this._firstQuestionAnswer = this._promptEntry.text;
            this._updateSensitivity(false);
            this._setWorking(true);
            return;
        }

        if (!this._currentQuery)
            return;

        let query = this._currentQuery;
        this._currentQuery = null;

        this._updateSensitivity(false);
        this._setWorking(true);

        this._userVerifier.answerQuery(query, this._promptEntry.text);
    },

    _onVerificationComplete: function() {
        this._userVerifier.clear();
        this.emit('unlocked');
    },

    _onReset: function() {
        this.emit('failed');
    },

    _onVerificationFailed: function() {
        this._currentQuery = null;
        this._firstQuestion = true;

        this._promptEntry.text = '';
        this._promptEntry.clutter_text.set_password_char('\u25cf');
        this._promptEntry.menu.isPassword = true;

        this._updateSensitivity(false);
        this._setWorking(false);
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

        this.parent();
    },

    cancel: function() {
        this._userVerifier.cancel(null);

        this.destroy();
    },
});
