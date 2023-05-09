// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Component */

const {
    AccountsService, Clutter, GLib, GObject,
    Pango, PolkitAgent, Polkit, Shell, St,
} = imports.gi;

const Dialog = imports.ui.dialog;
const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;
const ShellEntry = imports.ui.shellEntry;
const UserWidget = imports.ui.userWidget;
const Util = imports.misc.util;

const DialogMode = {
    AUTH: 0,
    CONFIRM: 1,
};

const DIALOG_ICON_SIZE = 64;

const DELAYED_RESET_TIMEOUT = 200;

var AuthenticationDialog = GObject.registerClass({
    Signals: { 'done': { param_types: [GObject.TYPE_BOOLEAN] } },
}, class AuthenticationDialog extends ModalDialog.ModalDialog {
    _init(actionId, description, cookie, userNames) {
        super._init({ styleClass: 'prompt-dialog' });

        this.actionId = actionId;
        this.message = description;
        this.userNames = userNames;

        Main.sessionMode.connectObject('updated', () => {
            this.visible = !Main.sessionMode.isLocked;
        }, this);

        this.connect('closed', this._onDialogClosed.bind(this));

        let title = _("Authentication Required");

        let headerContent = new Dialog.MessageDialogContent({ title, description });
        this.contentLayout.add_child(headerContent);

        let bodyContent = new Dialog.MessageDialogContent();

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
            vertical: true,
        });
        bodyContent.add_child(userBox);

        this._userAvatar = new UserWidget.Avatar(this._user, {
            iconSize: DIALOG_ICON_SIZE,
        });
        this._userAvatar.x_align = Clutter.ActorAlign.CENTER;
        userBox.add_child(this._userAvatar);

        this._userLabel = new St.Label({
            style_class: userName === 'root'
                ? 'polkit-dialog-user-root-label'
                : 'polkit-dialog-user-label',
        });

        if (userName === 'root')
            this._userLabel.text = _('Administrator');

        userBox.add_child(this._userLabel);

        let passwordBox = new St.BoxLayout({
            style_class: 'prompt-dialog-password-layout',
            vertical: true,
        });

        this._passwordEntry = new St.PasswordEntry({
            style_class: 'prompt-dialog-password-entry',
            text: "",
            can_focus: true,
            visible: false,
            x_align: Clutter.ActorAlign.CENTER,
        });
        ShellEntry.addContextMenu(this._passwordEntry);
        this._passwordEntry.clutter_text.connect('activate', this._onEntryActivate.bind(this));
        this._passwordEntry.bind_property('reactive',
            this._passwordEntry.clutter_text, 'editable',
            GObject.BindingFlags.SYNC_CREATE);
        passwordBox.add_child(this._passwordEntry);

        let warningBox = new St.BoxLayout({ vertical: true });

        let capsLockWarning = new ShellEntry.CapsLockWarning();
        this._passwordEntry.bind_property('visible',
            capsLockWarning, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        warningBox.add_child(capsLockWarning);

        this._errorMessageLabel = new St.Label({
            style_class: 'prompt-dialog-error-label',
            visible: false,
        });
        this._errorMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._errorMessageLabel.clutter_text.line_wrap = true;
        warningBox.add_child(this._errorMessageLabel);

        this._infoMessageLabel = new St.Label({
            style_class: 'prompt-dialog-info-label',
            visible: false,
        });
        this._infoMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._infoMessageLabel.clutter_text.line_wrap = true;
        warningBox.add_child(this._infoMessageLabel);

        /* text is intentionally non-blank otherwise the height is not the same as for
         * infoMessage and errorMessageLabel - but it is still invisible because
         * gnome-shell.css sets the color to be transparent
         */
        this._nullMessageLabel = new St.Label({ style_class: 'prompt-dialog-null-label' });
        this._nullMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._nullMessageLabel.clutter_text.line_wrap = true;
        warningBox.add_child(this._nullMessageLabel);

        passwordBox.add_child(warningBox);
        bodyContent.add_child(passwordBox);

        this._cancelButton = this.addButton({
            label: _('Cancel'),
            action: this.cancel.bind(this),
            key: Clutter.KEY_Escape,
        });
        this._okButton = this.addButton({
            label: _('Authenticate'),
            action: this._onAuthenticateButtonPressed.bind(this),
            reactive: false,
        });
        this._okButton.bind_property('reactive',
            this._okButton, 'can-focus',
            GObject.BindingFlags.SYNC_CREATE);

        this._passwordEntry.clutter_text.connect('text-changed', text => {
            this._okButton.reactive = text.get_text().length > 0;
        });

        this.contentLayout.add_child(bodyContent);

        this._doneEmitted = false;

        this._mode = -1;

        this._identityToAuth = Polkit.UnixUser.new_for_name(userName);
        this._cookie = cookie;

        this._user.connectObject(
            'notify::is-loaded', this._onUserChanged.bind(this),
            'changed', this._onUserChanged.bind(this), this);
        this._onUserChanged();
    }

    _initiateSession() {
        this._destroySession(DELAYED_RESET_TIMEOUT);

        this._session = new PolkitAgent.Session({
            identity: this._identityToAuth,
            cookie: this._cookie,
        });
        this._session.connectObject(
            'completed', this._onSessionCompleted.bind(this),
            'request', this._onSessionRequest.bind(this),
            'show-error', this._onSessionShowError.bind(this),
            'show-info', this._onSessionShowInfo.bind(this), this);
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

                Util.wiggle(this._passwordEntry);
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

        // Hack: The request string comes directly from PAM, if it's "Password:"
        // we replace it with our own to allow localization, if it's something
        // else we remove the last colon and any trailing or leading spaces.
        if (request === 'Password:' || request === 'Password: ')
            this._passwordEntry.hint_text = _('Password');
        else
            this._passwordEntry.hint_text = request.replace(/: *$/, '').trim();

        this._passwordEntry.password_visible = echoOn;

        this._passwordEntry.show();
        this._passwordEntry.set_text('');
        this._passwordEntry.reactive  = true;
        this._okButton.reactive = false;

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
        this._session?.disconnectObject(this);

        if (!this._completed)
            this._session?.cancel();

        this._completed = false;
        this._session = null;

        if (this._sessionRequestTimeoutId) {
            GLib.source_remove(this._sessionRequestTimeoutId);
            this._sessionRequestTimeoutId = 0;
        }

        let resetDialog = () => {
            this._sessionRequestTimeoutId = 0;

            if (this.state != ModalDialog.State.OPENED)
                return GLib.SOURCE_REMOVE;

            this._passwordEntry.hide();
            this._cancelButton.grab_key_focus();
            this._okButton.reactive = false;

            return GLib.SOURCE_REMOVE;
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

    close(timestamp) {
        // Ensure cleanup if the dialog was never shown
        if (this.state === ModalDialog.State.CLOSED)
            this._onDialogClosed();
        super.close(timestamp);
    }

    cancel() {
        this._emitDone(true);
    }

    _onDialogClosed() {
        Main.sessionMode.disconnectObject(this);

        if (this._sessionRequestTimeoutId)
            GLib.source_remove(this._sessionRequestTimeoutId);
        this._sessionRequestTimeoutId = 0;

        this._user?.disconnectObject(this);
        this._user = null;

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
            Main.sessionMode.connectObject('updated', () => {
                Main.sessionMode.disconnectObject(this);

                this._onInitiate(nativeAgent, actionId, message, iconName, cookie, userNames);
            }, this);
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

        Main.sessionMode.disconnectObject(this);

        this.complete(dismissed);
    }
});

var Component = AuthenticationAgent;
