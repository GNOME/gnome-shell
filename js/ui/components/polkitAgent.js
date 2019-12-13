// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Component */

const { AccountsService, Clutter, Gio, GLib,
        GObject, Pango, PolkitAgent, Polkit, Shell, St } = imports.gi;

const Animation = imports.ui.animation;
const Dialog = imports.ui.dialog;
const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;
const ShellEntry = imports.ui.shellEntry;
const UserWidget = imports.ui.userWidget;

const DialogMode = {
    AUTH: 0,
    CONFIRM: 1,
};

var DIALOG_ICON_SIZE = 48;

var WORK_SPINNER_ICON_SIZE = 16;

const DELAYED_RESET_TIMEOUT = 200;

var AuthenticationDialog = GObject.registerClass({
    Signals: { 'done': { param_types: [GObject.TYPE_BOOLEAN] } },
}, class AuthenticationDialog extends ModalDialog.ModalDialog {
    _init(actionId, body, cookie, userNames) {
        super._init({ styleClass: 'prompt-dialog' });

        this.actionId = actionId;
        this.message = body;
        this.userNames = userNames;

        this._sessionUpdatedId = Main.sessionMode.connect('updated', () => {
            this.visible = !Main.sessionMode.isLocked;
        });

        this.connect('closed', this._onDialogClosed.bind(this));

        let icon = new Gio.ThemedIcon({ name: 'dialog-password-symbolic' });
        let title = _("Authentication Required");

        let content = new Dialog.MessageDialogContent({ icon, title, body });
        this.contentLayout.add_actor(content);

        if (userNames.length > 1) {
            log(`polkitAuthenticationAgent: Received ${userNames.length} ` +
                'identities that can be used for authentication. Only ' +
                'considering one.');
        }

        let userName = GLib.get_user_name();
        if (!userNames.includes(userName))
            userName = 'root';
        if (!userNames.includes(userName))
            userName = userNames[0];

        this._user = AccountsService.UserManager.get_default().get_user(userName);

        let userBox = new St.BoxLayout({
            style_class: 'polkit-dialog-user-layout',
            vertical: false,
        });
        content.messageBox.add(userBox);

        this._userAvatar = new UserWidget.Avatar(this._user, {
            iconSize: DIALOG_ICON_SIZE,
            styleClass: 'polkit-dialog-user-icon',
        });
        userBox.add_child(this._userAvatar);

        this._userLabel = new St.Label({
            style_class: userName === 'root'
                ? 'polkit-dialog-user-root-label'
                : 'polkit-dialog-user-label',
            x_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });

        if (userName === 'root')
            this._userLabel.text = _('Administrator');

        userBox.add_child(this._userLabel);

        this._passwordBox = new St.BoxLayout({ vertical: false, style_class: 'prompt-dialog-password-box' });
        content.messageBox.add(this._passwordBox);
        this._passwordLabel = new St.Label({
            style_class: 'prompt-dialog-password-label',
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._passwordBox.add_child(this._passwordLabel);
        this._passwordEntry = new St.PasswordEntry({
            style_class: 'prompt-dialog-password-entry',
            text: "",
            can_focus: true,
            x_expand: true,
        });
        ShellEntry.addContextMenu(this._passwordEntry, { isPassword: true });
        this._passwordEntry.clutter_text.connect('activate', this._onEntryActivate.bind(this));
        this._passwordEntry.bind_property('reactive',
            this._passwordEntry.clutter_text, 'editable',
            GObject.BindingFlags.SYNC_CREATE);
        this._passwordBox.add_child(this._passwordEntry);

        this._workSpinner = new Animation.Spinner(WORK_SPINNER_ICON_SIZE, {
            animate: true,
        });
        this._passwordBox.add(this._workSpinner);

        this._passwordBox.hide();

        this._errorMessageLabel = new St.Label({ style_class: 'prompt-dialog-error-label' });
        this._errorMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._errorMessageLabel.clutter_text.line_wrap = true;
        content.messageBox.add_child(this._errorMessageLabel);
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
                                                text: 'abc' });
        this._nullMessageLabel.add_style_class_name('hidden');
        this._nullMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._nullMessageLabel.clutter_text.line_wrap = true;
        content.messageBox.add(this._nullMessageLabel);
        this._nullMessageLabel.show();

        this._cancelButton = this.addButton({ label: _("Cancel"),
                                              action: this.cancel.bind(this),
                                              key: Clutter.KEY_Escape });
        this._okButton = this.addButton({ label: _("Authenticate"),
                                          action: this._onAuthenticateButtonPressed.bind(this),
                                          reactive: false });
        this._okButton.bind_property('reactive',
            this._okButton, 'can-focus',
            GObject.BindingFlags.SYNC_CREATE);

        this._passwordEntry.clutter_text.connect('text-changed', text => {
            this._okButton.reactive = text.get_text().length > 0;
        });

        this._doneEmitted = false;

        this._mode = -1;

        this._identityToAuth = Polkit.UnixUser.new_for_name(userName);
        this._cookie = cookie;

        this._userLoadedId = this._user.connect('notify::is-loaded',
            this._onUserChanged.bind(this));
        this._userChangedId = this._user.connect('changed',
            this._onUserChanged.bind(this));
        this._onUserChanged();
    }

    _setWorking(working) {
        if (working)
            this._workSpinner.play();
        else
            this._workSpinner.stop();
    }

    _initiateSession() {
        this._destroySession(DELAYED_RESET_TIMEOUT);

        this._session = new PolkitAgent.Session({ identity: this._identityToAuth,
                                                  cookie: this._cookie });
        this._sessionCompletedId = this._session.connect('completed', this._onSessionCompleted.bind(this));
        this._sessionRequestId = this._session.connect('request', this._onSessionRequest.bind(this));
        this._sessionShowErrorId = this._session.connect('show-error', this._onSessionShowError.bind(this));
        this._sessionShowInfoId = this._session.connect('show-info', this._onSessionShowInfo.bind(this));
        this._session.initiate();
    }

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

            log('polkitAuthenticationAgent: Failed to show modal dialog. ' +
                `Dismissing authentication request for action-id ${this.actionId} ` +
                `cookie ${this._cookie}`);
            this._emitDone(true);
        }
    }

    _emitDone(dismissed) {
        if (!this._doneEmitted) {
            this._doneEmitted = true;
            this.emit('done', dismissed);
        }
    }

    _onEntryActivate() {
        let response = this._passwordEntry.get_text();
        if (response.length === 0)
            return;

        this._passwordEntry.reactive = false;
        this._okButton.reactive = false;
        this._setWorking(true);

        this._session.response(response);
        // When the user responds, dismiss already shown info and
        // error texts (if any)
        this._errorMessageLabel.hide();
        this._infoMessageLabel.hide();
        this._nullMessageLabel.show();
    }

    _onAuthenticateButtonPressed() {
        if (this._mode === DialogMode.CONFIRM)
            this._initiateSession();
        else
            this._onEntryActivate();
    }

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
            if (!this._errorMessageLabel.visible) {
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
            this._initiateSession();
        }
    }

    _onSessionRequest(session, request, echoOn) {
        if (this._sessionRequestTimeoutId) {
            GLib.source_remove(this._sessionRequestTimeoutId);
            this._sessionRequestTimeoutId = 0;
        }

        // Cheap localization trick
        if (request == 'Password:' || request == 'Password: ')
            this._passwordLabel.set_text(_("Password:"));
        else
            this._passwordLabel.set_text(request);

        this._passwordEntry.password_visible = echoOn;

        this._passwordBox.show();
        this._passwordEntry.set_text('');
        this._passwordEntry.reactive  = true;
        this._okButton.reactive = false;
        this._setWorking(false);

        this._ensureOpen();
        this._passwordEntry.grab_key_focus();
    }

    _onSessionShowError(session, text) {
        this._passwordEntry.set_text('');
        this._errorMessageLabel.set_text(text);
        this._errorMessageLabel.show();
        this._infoMessageLabel.hide();
        this._nullMessageLabel.hide();
        this._ensureOpen();
    }

    _onSessionShowInfo(session, text) {
        this._passwordEntry.set_text('');
        this._infoMessageLabel.set_text(text);
        this._infoMessageLabel.show();
        this._errorMessageLabel.hide();
        this._nullMessageLabel.hide();
        this._ensureOpen();
    }

    _destroySession(delay = 0) {
        if (this._session) {
            this._session.disconnect(this._sessionCompletedId);
            this._session.disconnect(this._sessionRequestId);
            this._session.disconnect(this._sessionShowErrorId);
            this._session.disconnect(this._sessionShowInfoId);

            if (!this._completed)
                this._session.cancel();

            this._completed = false;
            this._session = null;
        }

        if (this._sessionRequestTimeoutId) {
            GLib.source_remove(this._sessionRequestTimeoutId);
            this._sessionRequestTimeoutId = 0;
        }

        let resetDialog = () => {
            if (this.state != ModalDialog.State.OPENED)
                return;

            this._passwordBox.hide();
            this._cancelButton.grab_key_focus();
            this._okButton.reactive = false;
        };

        if (delay) {
            this._sessionRequestTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, delay, resetDialog);
            GLib.Source.set_name_by_id(this._sessionRequestTimeoutId, '[gnome-shell] this._sessionRequestTimeoutId');
        } else {
            resetDialog();
        }
    }

    _onUserChanged() {
        if (!this._user.is_loaded)
            return;

        let userName = this._user.get_user_name();
        let realName = this._user.get_real_name();

        if (userName !== 'root')
            this._userLabel.set_text(realName);

        this._userAvatar.update();

        if (this._user.get_password_mode() === AccountsService.UserPasswordMode.NONE) {
            if (this._mode === DialogMode.CONFIRM)
                return;

            this._mode = DialogMode.CONFIRM;
            this._destroySession();

            this._okButton.reactive = true;

            /* We normally open the dialog when we get a "request" signal, but
             * since in this case initiating a session would perform the
             * authentication, only open the dialog and initiate the session
             * when the user confirmed. */
            this._ensureOpen();
        } else {
            if (this._mode === DialogMode.AUTH)
                return;

            this._mode = DialogMode.AUTH;
            this._initiateSession();
        }
    }

    cancel() {
        this.close(global.get_current_time());
        this._emitDone(true);
    }

    _onDialogClosed() {
        if (this._sessionUpdatedId)
            Main.sessionMode.disconnect(this._sessionUpdatedId);

        if (this._sessionRequestTimeoutId)
            GLib.source_remove(this._sessionRequestTimeoutId);
        this._sessionRequestTimeoutId = 0;

        if (this._user) {
            this._user.disconnect(this._userLoadedId);
            this._user.disconnect(this._userChangedId);
            this._user = null;
        }

        this._destroySession();
    }
});

var AuthenticationAgent = GObject.registerClass(
class AuthenticationAgent extends Shell.PolkitAuthenticationAgent {
    _init() {
        super._init();

        this._currentDialog = null;
        this.connect('initiate', this._onInitiate.bind(this));
        this.connect('cancel', this._onCancel.bind(this));
        this._sessionUpdatedId = 0;
    }

    enable() {
        try {
            this.register();
        } catch (e) {
            log('Failed to register AuthenticationAgent');
        }
    }

    disable() {
        try {
            this.unregister();
        } catch (e) {
            log('Failed to unregister AuthenticationAgent');
        }
    }

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
        this._currentDialog.connect('done', this._onDialogDone.bind(this));
    }

    _onCancel(_nativeAgent) {
        this._completeRequest(false);
    }

    _onDialogDone(_dialog, dismissed) {
        this._completeRequest(dismissed);
    }

    _completeRequest(dismissed) {
        this._currentDialog.close();
        this._currentDialog = null;

        if (this._sessionUpdatedId)
            Main.sessionMode.disconnect(this._sessionUpdatedId);
        this._sessionUpdatedId = 0;

        this.complete(dismissed);
    }
});

var Component = AuthenticationAgent;
