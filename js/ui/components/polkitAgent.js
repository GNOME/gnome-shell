// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Signals = imports.signals;
const Shell = imports.gi.Shell;
const AccountsService = imports.gi.AccountsService;
const Clutter = imports.gi.Clutter;
const St = imports.gi.St;
const Pango = imports.gi.Pango;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const Mainloop = imports.mainloop;
const Polkit = imports.gi.Polkit;
const PolkitAgent = imports.gi.PolkitAgent;

const Animation = imports.ui.animation;
const Components = imports.ui.components;
const Dialog = imports.ui.dialog;
const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;
const ShellEntry = imports.ui.shellEntry;
const UserWidget = imports.ui.userWidget;
const Tweener = imports.ui.tweener;

var DIALOG_ICON_SIZE = 48;

var WORK_SPINNER_ICON_SIZE = 16;
var WORK_SPINNER_ANIMATION_DELAY = 1.0;
var WORK_SPINNER_ANIMATION_TIME = 0.3;

var AuthenticationDialog = new Lang.Class({
    Name: 'AuthenticationDialog',
    Extends: ModalDialog.ModalDialog,

    _init(actionId, body, cookie, userNames) {
        this.parent({ styleClass: 'prompt-dialog' });

        this.actionId = actionId;
        this.message = body;
        this.userNames = userNames;
        this._wasDismissed = false;

        this._sessionUpdatedId = Main.sessionMode.connect('updated', () => {
            this._group.visible = !Main.sessionMode.isLocked;
        });

        let icon = new Gio.ThemedIcon({ name: 'dialog-password-symbolic' });
        let title = _("Authentication Required");

        let content = new Dialog.MessageDialogContent({ icon, title, body });
        this.contentLayout.add_actor(content);

        if (userNames.length > 1) {
            log('polkitAuthenticationAgent: Received ' + userNames.length +
                ' identities that can be used for authentication. Only ' +
                'considering one.');
        }

        let userName = GLib.get_user_name();
        if (userNames.indexOf(userName) < 0)
            userName = 'root';
        if (userNames.indexOf(userName) < 0)
            userName = userNames[0];

        this._user = AccountsService.UserManager.get_default().get_user(userName);
        let userRealName = this._user.get_real_name()
        this._userLoadedId = this._user.connect('notify::is_loaded',
                                                this._onUserChanged.bind(this));
        this._userChangedId = this._user.connect('changed',
                                                 this._onUserChanged.bind(this));

        // Special case 'root'
        let userIsRoot = false;
        if (userName == 'root') {
            userIsRoot = true;
            userRealName = _("Administrator");
        }

        if (userIsRoot) {
            let userLabel = new St.Label(({ style_class: 'polkit-dialog-user-root-label',
                                            text: userRealName }));
            content.messageBox.add(userLabel, { x_fill: false,
                                                x_align: St.Align.START });
        } else {
            let userBox = new St.BoxLayout({ style_class: 'polkit-dialog-user-layout',
                                             vertical: false });
            content.messageBox.add(userBox);
            this._userAvatar = new UserWidget.Avatar(this._user,
                                                     { iconSize: DIALOG_ICON_SIZE,
                                                       styleClass: 'polkit-dialog-user-icon' });
            this._userAvatar.actor.hide();
            userBox.add(this._userAvatar.actor,
                        { x_fill:  true,
                          y_fill:  false,
                          x_align: St.Align.END,
                          y_align: St.Align.START });
            let userLabel = new St.Label(({ style_class: 'polkit-dialog-user-label',
                                            text: userRealName }));
            userBox.add(userLabel,
                        { x_fill:  true,
                          y_fill:  false,
                          x_align: St.Align.END,
                          y_align: St.Align.MIDDLE });
        }

        this._onUserChanged();

        this._passwordBox = new St.BoxLayout({ vertical: false, style_class: 'prompt-dialog-password-box' });
        content.messageBox.add(this._passwordBox);
        this._passwordLabel = new St.Label(({ style_class: 'prompt-dialog-password-label' }));
        this._passwordBox.add(this._passwordLabel, { y_fill: false, y_align: St.Align.MIDDLE });
        this._passwordEntry = new St.Entry({ style_class: 'prompt-dialog-password-entry',
                                             text: "",
                                             can_focus: true});
        ShellEntry.addContextMenu(this._passwordEntry, { isPassword: true });
        this._passwordEntry.clutter_text.connect('activate', this._onEntryActivate.bind(this));
        this._passwordBox.add(this._passwordEntry,
                              { expand: true });

        let spinnerIcon = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/process-working.svg');
        this._workSpinner = new Animation.AnimatedIcon(spinnerIcon, WORK_SPINNER_ICON_SIZE);
        this._workSpinner.actor.opacity = 0;

        this._passwordBox.add(this._workSpinner.actor);

        this.setInitialKeyFocus(this._passwordEntry);
        this._passwordBox.hide();

        this._errorMessageLabel = new St.Label({ style_class: 'prompt-dialog-error-label' });
        this._errorMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._errorMessageLabel.clutter_text.line_wrap = true;
        content.messageBox.add(this._errorMessageLabel, { x_fill: false, x_align: St.Align.START });
        this._errorMessageLabel.hide();

        this._infoMessageLabel = new St.Label({ style_class: 'prompt-dialog-info-label' });
        this._infoMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._infoMessageLabel.clutter_text.line_wrap = true;
        content.messageBox.add(this._infoMessageLabel);
        this._infoMessageLabel.hide();

        /* text is intentionally non-blank otherwise the height is not the same as for
         * infoMessage and errorMessageLabel - but it is still invisible because
         * gnome-shell.css sets the color to be transparent
         */
        this._nullMessageLabel = new St.Label({ style_class: 'prompt-dialog-null-label',
                                                text: 'abc'});
        this._nullMessageLabel.add_style_class_name('hidden');
        this._nullMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._nullMessageLabel.clutter_text.line_wrap = true;
        content.messageBox.add(this._nullMessageLabel);
        this._nullMessageLabel.show();

        this._cancelButton = this.addButton({ label: _("Cancel"),
                                              action: this.cancel.bind(this),
                                              key: Clutter.Escape });
        this._okButton = this.addButton({ label:  _("Authenticate"),
                                          action: this._onAuthenticateButtonPressed.bind(this),
                                          default: true });

        this._doneEmitted = false;

        this._identityToAuth = Polkit.UnixUser.new_for_name(userName);
        this._cookie = cookie;
    },

    _setWorking(working) {
        Tweener.removeTweens(this._workSpinner.actor);
        if (working) {
            this._workSpinner.play();
            Tweener.addTween(this._workSpinner.actor,
                             { opacity: 255,
                               delay: WORK_SPINNER_ANIMATION_DELAY,
                               time: WORK_SPINNER_ANIMATION_TIME,
                               transition: 'linear'
                             });
        } else {
            Tweener.addTween(this._workSpinner.actor,
                             { opacity: 0,
                               time: WORK_SPINNER_ANIMATION_TIME,
                               transition: 'linear',
                               onCompleteScope: this,
                               onComplete() {
                                   if (this._workSpinner)
                                       this._workSpinner.stop();
                               }
                             });
        }
    },

    performAuthentication() {
        this.destroySession();
        this._session = new PolkitAgent.Session({ identity: this._identityToAuth,
                                                  cookie: this._cookie });
        this._session.connect('completed', this._onSessionCompleted.bind(this));
        this._session.connect('request', this._onSessionRequest.bind(this));
        this._session.connect('show-error', this._onSessionShowError.bind(this));
        this._session.connect('show-info', this._onSessionShowInfo.bind(this));
        this._session.initiate();
    },

    close(timestamp) {
        this.parent(timestamp);

        if (this._sessionUpdatedId)
            Main.sessionMode.disconnect(this._sessionUpdatedId);
        this._sessionUpdatedId = 0;
    },

    _ensureOpen() {
        // NOTE: ModalDialog.open() is safe to call if the dialog is
        // already open - it just returns true without side-effects
        if (!this.open(global.get_current_time())) {
            // This can fail if e.g. unable to get input grab
            //
            // In an ideal world this wouldn't happen (because the
            // Shell is in complete control of the session) but that's
            // just not how things work right now.
            //
            // One way to make this happen is by running 'sleep 3;
            // pkexec bash' and then opening a popup menu.
            //
            // We could add retrying if this turns out to be a problem

            log('polkitAuthenticationAgent: Failed to show modal dialog.' +
                ' Dismissing authentication request for action-id ' + this.actionId +
                ' cookie ' + this._cookie);
            this._emitDone(true);
        }
    },

    _emitDone(dismissed) {
        if (!this._doneEmitted) {
            this._doneEmitted = true;
            this.emit('done', dismissed);
        }
    },

    _updateSensitivity(sensitive) {
        this._passwordEntry.reactive = sensitive;
        this._passwordEntry.clutter_text.editable = sensitive;

        this._okButton.can_focus = sensitive;
        this._okButton.reactive = sensitive;
        this._setWorking(!sensitive);
    },

    _onEntryActivate() {
        let response = this._passwordEntry.get_text();
        this._updateSensitivity(false);
        this._session.response(response);
        // When the user responds, dismiss already shown info and
        // error texts (if any)
        this._errorMessageLabel.hide();
        this._infoMessageLabel.hide();
        this._nullMessageLabel.show();
    },

    _onAuthenticateButtonPressed() {
        this._onEntryActivate();
    },

    _onSessionCompleted(session, gainedAuthorization) {
        if (this._completed || this._doneEmitted)
            return;

        this._completed = true;

        /* Yay, all done */
        if (gainedAuthorization) {
            this._emitDone(false);

        } else {
            /* Unless we are showing an existing error message from the PAM
             * module (the PAM module could be reporting the authentication
             * error providing authentication-method specific information),
             * show "Sorry, that didn't work. Please try again."
             */
            if (!this._errorMessageLabel.visible && !this._wasDismissed) {
                /* Translators: "that didn't work" refers to the fact that the
                 * requested authentication was not gained; this can happen
                 * because of an authentication error (like invalid password),
                 * for instance. */
                this._errorMessageLabel.set_text(_("Sorry, that didn’t work. Please try again."));
                this._errorMessageLabel.show();
                this._infoMessageLabel.hide();
                this._nullMessageLabel.hide();
            }

            /* Try and authenticate again */
            this.performAuthentication();
        }
    },

    _onSessionRequest(session, request, echo_on) {
        // Cheap localization trick
        if (request == 'Password:' || request == 'Password: ')
            this._passwordLabel.set_text(_("Password:"));
        else
            this._passwordLabel.set_text(request);

        if (echo_on)
            this._passwordEntry.clutter_text.set_password_char('');
        else
            this._passwordEntry.clutter_text.set_password_char('\u25cf'); // ● U+25CF BLACK CIRCLE

        this._passwordBox.show();
        this._passwordEntry.set_text('');
        this._passwordEntry.grab_key_focus();
        this._updateSensitivity(true);
        this._ensureOpen();
    },

    _onSessionShowError(session, text) {
        this._passwordEntry.set_text('');
        this._errorMessageLabel.set_text(text);
        this._errorMessageLabel.show();
        this._infoMessageLabel.hide();
        this._nullMessageLabel.hide();
        this._ensureOpen();
    },

    _onSessionShowInfo(session, text) {
        this._passwordEntry.set_text('');
        this._infoMessageLabel.set_text(text);
        this._infoMessageLabel.show();
        this._errorMessageLabel.hide();
        this._nullMessageLabel.hide();
        this._ensureOpen();
    },

    destroySession() {
        if (this._session) {
            if (!this._completed)
                this._session.cancel();
            this._completed = false;
            this._session = null;
        }
    },

    _onUserChanged() {
        if (this._user.is_loaded && this._userAvatar) {
            this._userAvatar.update();
            this._userAvatar.actor.show();
        }
    },

    cancel() {
        this._wasDismissed = true;
        this.close(global.get_current_time());
        this._emitDone(true);
    },
});
Signals.addSignalMethods(AuthenticationDialog.prototype);

var AuthenticationAgent = new Lang.Class({
    Name: 'AuthenticationAgent',

    _init() {
        this._currentDialog = null;
        this._handle = null;
        this._native = new Shell.PolkitAuthenticationAgent();
        this._native.connect('initiate', this._onInitiate.bind(this));
        this._native.connect('cancel', this._onCancel.bind(this));
        this._sessionUpdatedId = 0;
    },

    enable() {
        try {
            this._native.register();
        } catch(e) {
            log('Failed to register AuthenticationAgent');
        }
    },

    disable() {
        try {
            this._native.unregister();
        } catch(e) {
            log('Failed to unregister AuthenticationAgent');
        }
    },

    _onInitiate(nativeAgent, actionId, message, iconName, cookie, userNames) {
        // Don't pop up a dialog while locked
        if (Main.sessionMode.isLocked) {
            this._sessionUpdatedId = Main.sessionMode.connect('updated', () => {
                Main.sessionMode.disconnect(this._sessionUpdatedId);
                this._sessionUpdatedId = 0;

                this._onInitiate(nativeAgent, actionId, message, iconName, cookie, userNames);
            });
            return;
        }

        this._currentDialog = new AuthenticationDialog(actionId, message, cookie, userNames);

        // We actually don't want to open the dialog until we know for
        // sure that we're going to interact with the user. For
        // example, if the password for the identity to auth is blank
        // (which it will be on a live CD) then there will be no
        // conversation at all... of course, we don't *know* that
        // until we actually try it.
        //
        // See https://bugzilla.gnome.org/show_bug.cgi?id=643062 for more
        // discussion.

        this._currentDialog.connect('done', this._onDialogDone.bind(this));
        this._currentDialog.performAuthentication();
    },

    _onCancel(nativeAgent) {
        this._completeRequest(false);
    },

    _onDialogDone(dialog, dismissed) {
        this._completeRequest(dismissed);
    },

    _completeRequest(dismissed) {
        this._currentDialog.close();
        this._currentDialog.destroySession();
        this._currentDialog = null;

        if (this._sessionUpdatedId)
            Main.sessionMode.disconnect(this._sessionUpdatedId);
        this._sessionUpdatedId = 0;

        this._native.complete(dismissed);
    },
});

var Component = AuthenticationAgent;
