// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported AuthPrompt */

const { AccountsService, Clutter, Gio,
    GLib, GObject, Pango, Polkit, Shell, St } = imports.gi;
const ByteArray = imports.byteArray;

const Animation = imports.ui.animation;
const Batch = imports.gdm.batch;
const GdmUtil = imports.gdm.util;
const Util = imports.misc.util;
const Main = imports.ui.main;
const Params = imports.misc.params;
const ShellEntry = imports.ui.shellEntry;
const UserWidget = imports.ui.userWidget;

var DEFAULT_BUTTON_WELL_ICON_SIZE = 16;
var DEFAULT_BUTTON_WELL_ANIMATION_DELAY = 1000;
var DEFAULT_BUTTON_WELL_ANIMATION_TIME = 300;

var MESSAGE_FADE_OUT_ANIMATION_TIME = 500;

const _RESET_CODE_LENGTH = 7;

var AuthPromptMode = {
    UNLOCK_ONLY: 0,
    UNLOCK_OR_LOG_IN: 1,
};

var AuthPromptStatus = {
    NOT_VERIFYING: 0,
    VERIFYING: 1,
    VERIFICATION_FAILED: 2,
    VERIFICATION_SUCCEEDED: 3,
};

var BeginRequestType = {
    PROVIDE_USERNAME: 0,
    DONT_PROVIDE_USERNAME: 1,
};

function _getMachineId() {
    let machineId;
    try {
        machineId = Shell.get_file_contents_utf8_sync('/etc/machine-id');
    } catch (e) {
        logError(e, "Failed to get contents for file '/etc/machine-id'");
        machineId = '00000000000000000000000000000000';
    }
    return machineId;
}

var AuthPrompt = GObject.registerClass({
    Signals: {
        'cancelled': {},
        'failed': {},
        'next': {},
        'prompted': {},
        'reset': { param_types: [GObject.TYPE_UINT] },
    },
}, class AuthPrompt extends St.BoxLayout {
    _init(gdmClient, mode) {
        super._init({
            style_class: 'login-dialog-prompt-layout',
            vertical: true,
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
        });

        this.verificationStatus = AuthPromptStatus.NOT_VERIFYING;

        this._gdmClient = gdmClient;
        this._mode = mode;
        this._defaultButtonWellActor = null;

        let reauthenticationOnly;
        if (this._mode == AuthPromptMode.UNLOCK_ONLY)
            reauthenticationOnly = true;
        else if (this._mode == AuthPromptMode.UNLOCK_OR_LOG_IN)
            reauthenticationOnly = false;

        this._userVerifier = new GdmUtil.ShellUserVerifier(this._gdmClient, { reauthenticationOnly });

        this._userVerifier.connect('ask-question', this._onAskQuestion.bind(this));
        this._userVerifier.connect('show-message', this._onShowMessage.bind(this));
        this._userVerifier.connect('verification-failed', this._onVerificationFailed.bind(this));
        this._userVerifier.connect('verification-complete', this._onVerificationComplete.bind(this));
        this._userVerifier.connect('reset', this._onReset.bind(this));
        this._userVerifier.connect('smartcard-status-changed', this._onSmartcardStatusChanged.bind(this));
        this._userVerifier.connect('ovirt-user-authenticated', this._onOVirtUserAuthenticated.bind(this));
        this.smartcardDetected = this._userVerifier.smartcardDetected;

        this.connect('destroy', this._onDestroy.bind(this));

        this._userWell = new St.Bin({
            x_expand: true,
            y_expand: true,
        });
        this.add_child(this._userWell);

        this._hasCancelButton = this._mode === AuthPromptMode.UNLOCK_OR_LOG_IN;

        this._initEntryRow();

        let capsLockPlaceholder = new St.Label();
        this.add_child(capsLockPlaceholder);

        this._capsLockWarningLabel = new ShellEntry.CapsLockWarning({
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(this._capsLockWarningLabel);

        this._capsLockWarningLabel.bind_property('visible',
            capsLockPlaceholder, 'visible',
            GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.INVERT_BOOLEAN);

        this._message = new St.Label({
            opacity: 0,
            styleClass: 'login-dialog-message',
            y_expand: true,
            x_expand: true,
            y_align: Clutter.ActorAlign.START,
            x_align: Clutter.ActorAlign.CENTER,
        });
        this._message.clutter_text.line_wrap = true;
        this._message.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this.add_child(this._message);

        let passwordHintLabel = new St.Label({
            text: _('Show password hint'),
            style_class: 'login-dialog-password-recovery-label',
        });
        this._passwordHintButton = new St.Button({
            style_class: 'login-dialog-password-recovery-button',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            can_focus: true,
            child: passwordHintLabel,
            reactive: true,
            x_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
            visible: false,
        });
        this.add_child(this._passwordHintButton);
        this._passwordHintButton.connect('clicked', this._showPasswordHint.bind(this));

        let passwordResetLabel = new St.Label({
            text: _('Forgot password?'),
            style_class: 'login-dialog-password-recovery-label',
        });
        this._passwordResetButton = new St.Button({
            style_class: 'login-dialog-password-recovery-button',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            can_focus: true,
            child: passwordResetLabel,
            reactive: true,
            x_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
            visible: false,
        });
        this.add_child(this._passwordResetButton);
        this._passwordResetButton.connect('clicked', this._showPasswordResetPrompt.bind(this));

        this._displayingPasswordHint = false;
        this._customerSupportKeyFile = null;
        this._passwordResetCode = null;
    }

    _onDestroy() {
        this._userVerifier.destroy();
        this._userVerifier = null;
    }

    vfunc_key_press_event(keyPressEvent) {
        if (keyPressEvent.keyval == Clutter.KEY_Escape)
            this.cancel();
        return Clutter.EVENT_PROPAGATE;
    }

    _initEntryRow() {
        this._mainBox = new St.BoxLayout({
            style_class: 'login-dialog-button-box',
            vertical: false,
        });
        this.add_child(this._mainBox);

        this.cancelButton = new St.Button({
            style_class: 'modal-dialog-button button cancel-button',
            accessible_name: _('Cancel'),
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: this._hasCancelButton,
            can_focus: this._hasCancelButton,
            x_align: Clutter.ActorAlign.START,
            y_align: Clutter.ActorAlign.CENTER,
            child: new St.Icon({ icon_name: 'go-previous-symbolic' }),
        });
        if (this._hasCancelButton)
            this.cancelButton.connect('clicked', () => this.cancel());
        else
            this.cancelButton.opacity = 0;
        this._mainBox.add_child(this.cancelButton);

        let entryParams = {
            style_class: 'login-dialog-prompt-entry',
            can_focus: true,
            x_expand: true,
        };

        this._entry = null;

        this._textEntry = new St.Entry(entryParams);
        ShellEntry.addContextMenu(this._textEntry, { actionMode: Shell.ActionMode.NONE });

        this._passwordEntry = new St.PasswordEntry(entryParams);
        ShellEntry.addContextMenu(this._passwordEntry, { actionMode: Shell.ActionMode.NONE });

        this._entry = this._passwordEntry;
        this._mainBox.add_child(this._entry);
        this._entry.grab_key_focus();

        [this._textEntry, this._passwordEntry].forEach(entry => {
            entry.clutter_text.connect('text-changed', () => {
                if (!this._passwordResetCode) {
                    if (!this._userVerifier.hasPendingMessages &&
                        !this._displayingPasswordHint)
                        this._fadeOutMessage();

                    this._canActivateNext =
                        this._entry.text.length > 0 ||
                        this.verificationStatus === AuthPromptStatus.VERIFYING;
                } else {
                    // Password unlock code must contain the right number of digits, and only digits.
                    this._canActivateNext =
                        this._entry.text.length === _RESET_CODE_LENGTH &&
                        this._entry.text.search(/\D/) === -1;
                }
            });

            entry.clutter_text.connect('activate', () => {
                let shouldSpin = entry === this._passwordEntry;
                if (entry.reactive)
                    this._activateNext(shouldSpin);
            });
        });

        this._defaultButtonWell = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            x_align: Clutter.ActorAlign.END,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._defaultButtonWell.add_constraint(new Clutter.BindConstraint({
            source: this.cancelButton,
            coordinate: Clutter.BindCoordinate.SIZE,
        }));
        this._mainBox.add_child(this._defaultButtonWell);

        this._spinner = new Animation.Spinner(DEFAULT_BUTTON_WELL_ICON_SIZE);
        this._defaultButtonWell.add_child(this._spinner);
    }

    _activateNext(shouldSpin) {
        if (!this._canActivateNext)
            return;

        if (this._passwordResetCode === null)
            this._respondToSessionWorker(shouldSpin);
        else if (this._entry.get_text() === this._computeUnlockCode(this._passwordResetCode))
            this._performPasswordReset();
        else
            this._handleIncorrectPasswordResetCode();

        this.emit('next');
    }

    _updateEntry(secret) {
        if (secret && this._entry !== this._passwordEntry) {
            this._mainBox.replace_child(this._entry, this._passwordEntry);
            this._entry = this._passwordEntry;
        } else if (!secret && this._entry !== this._textEntry) {
            this._mainBox.replace_child(this._entry, this._textEntry);
            this._entry = this._textEntry;
        }
        this._capsLockWarningLabel.visible = secret;
    }

    _onAskQuestion(verifier, serviceName, question, secret) {
        if (this._queryingService)
            this.clear();

        this._queryingService = serviceName;
        if (this._preemptiveAnswer) {
            this._userVerifier.answerQuery(this._queryingService, this._preemptiveAnswer);
            this._preemptiveAnswer = null;
            return;
        }

        this._updateEntry(secret);

        // Hack: The question string comes directly from PAM, if it's "Password:"
        // we replace it with our own to allow localization, if it's something
        // else we remove the last colon and any trailing or leading spaces.
        if (question === 'Password:' || question === 'Password: ')
            this.setQuestion(_('Password'));
        else
            this.setQuestion(question.replace(/: *$/, '').trim());

        this.updateSensitivity(true);
        this.emit('prompted');
    }

    _onOVirtUserAuthenticated() {
        if (this.verificationStatus != AuthPromptStatus.VERIFICATION_SUCCEEDED)
            this.reset();
    }

    _onSmartcardStatusChanged() {
        this.smartcardDetected = this._userVerifier.smartcardDetected;

        // Most of the time we want to reset if the user inserts or removes
        // a smartcard. Smartcard insertion "preempts" what the user was
        // doing, and smartcard removal aborts the preemption.
        // The exceptions are: 1) Don't reset on smartcard insertion if we're already verifying
        //                        with a smartcard
        //                     2) Don't reset if we've already succeeded at verification and
        //                        the user is getting logged in.
        if (this._userVerifier.serviceIsDefault(GdmUtil.SMARTCARD_SERVICE_NAME) &&
            this.verificationStatus == AuthPromptStatus.VERIFYING &&
            this.smartcardDetected)
            return;

        if (this.verificationStatus != AuthPromptStatus.VERIFICATION_SUCCEEDED)
            this.reset();
    }

    _onShowMessage(userVerifier, message, type) {
        this.setMessage(message, type);
        this.emit('prompted');
    }

    _onVerificationFailed(userVerifier, canRetry) {
        this._queryingService = null;
        this.clear();

        this.updateSensitivity(canRetry);
        this.setActorInDefaultButtonWell(null);
        this.verificationStatus = AuthPromptStatus.VERIFICATION_FAILED;

        Util.wiggle(this._entry);

        const userManager = AccountsService.UserManager.get_default();
        const user = userManager.get_user(this._username);
        if (user.get_password_hint().length > 0)
            this._passwordHintButton.show();
        else
            this._maybeShowPasswordResetButton();
    }

    _onVerificationComplete() {
        this.setActorInDefaultButtonWell(null);
        this.verificationStatus = AuthPromptStatus.VERIFICATION_SUCCEEDED;
        this.cancelButton.reactive = false;
        this.cancelButton.can_focus = false;
    }

    _onReset() {
        this.verificationStatus = AuthPromptStatus.NOT_VERIFYING;
        this.reset();
    }

    setActorInDefaultButtonWell(actor, animate) {
        if (!this._defaultButtonWellActor &&
            !actor)
            return;

        let oldActor = this._defaultButtonWellActor;

        if (oldActor)
            oldActor.remove_all_transitions();

        let wasSpinner;
        if (oldActor == this._spinner)
            wasSpinner = true;
        else
            wasSpinner = false;

        let isSpinner;
        if (actor == this._spinner)
            isSpinner = true;
        else
            isSpinner = false;

        if (this._defaultButtonWellActor != actor && oldActor) {
            if (!animate) {
                oldActor.opacity = 0;

                if (wasSpinner) {
                    if (this._spinner)
                        this._spinner.stop();
                }
            } else {
                oldActor.ease({
                    opacity: 0,
                    duration: DEFAULT_BUTTON_WELL_ANIMATION_TIME,
                    delay: DEFAULT_BUTTON_WELL_ANIMATION_DELAY,
                    mode: Clutter.AnimationMode.LINEAR,
                    onComplete: () => {
                        if (wasSpinner) {
                            if (this._spinner)
                                this._spinner.stop();
                        }
                    },
                });
            }
        }

        if (actor) {
            if (isSpinner)
                this._spinner.play();

            if (!animate) {
                actor.opacity = 255;
            } else {
                actor.ease({
                    opacity: 255,
                    duration: DEFAULT_BUTTON_WELL_ANIMATION_TIME,
                    delay: DEFAULT_BUTTON_WELL_ANIMATION_DELAY,
                    mode: Clutter.AnimationMode.LINEAR,
                });
            }
        }

        this._defaultButtonWellActor = actor;
    }

    startSpinning() {
        this.setActorInDefaultButtonWell(this._spinner, true);
    }

    stopSpinning() {
        this.setActorInDefaultButtonWell(null, false);
    }

    clear() {
        this._entry.text = '';
        this.stopSpinning();
    }

    setQuestion(question) {
        this._entry.hint_text = question;

        this._entry.show();
        this._entry.grab_key_focus();
    }

    getAnswer() {
        let text;

        if (this._preemptiveAnswer) {
            text = this._preemptiveAnswer;
            this._preemptiveAnswer = null;
        } else {
            text = this._entry.get_text();
        }

        return text;
    }

    _fadeOutMessage() {
        if (this._message.opacity == 0)
            return;
        this._message.remove_all_transitions();
        this._message.ease({
            opacity: 0,
            duration: MESSAGE_FADE_OUT_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    setMessage(message, type) {
        if (type == GdmUtil.MessageType.ERROR)
            this._message.add_style_class_name('login-dialog-message-warning');
        else
            this._message.remove_style_class_name('login-dialog-message-warning');

        if (type == GdmUtil.MessageType.HINT)
            this._message.add_style_class_name('login-dialog-message-hint');
        else
            this._message.remove_style_class_name('login-dialog-message-hint');

        if (message) {
            this._message.remove_all_transitions();
            this._message.text = message;
            this._message.opacity = 255;
        } else {
            this._message.opacity = 0;
        }

        this._displayingPasswordHint = false;
    }

    updateSensitivity(sensitive) {
        this._entry.reactive = sensitive;
        this._entry.clutter_text.editable = sensitive;
    }

    vfunc_hide() {
        this.setActorInDefaultButtonWell(null, true);
        super.vfunc_hide();
        this._message.opacity = 0;

        this.setUser(null);

        this.updateSensitivity(true);
        this._entry.set_text('');
    }

    setUser(user) {
        let oldChild = this._userWell.get_child();
        if (oldChild)
            oldChild.destroy();

        let userWidget = new UserWidget.UserWidget(user, Clutter.Orientation.VERTICAL);
        this._userWell.set_child(userWidget);

        if (!user)
            this._updateEntry(false);
    }

    reset() {
        let oldStatus = this.verificationStatus;
        this.verificationStatus = AuthPromptStatus.NOT_VERIFYING;
        this.cancelButton.reactive = this._hasCancelButton;
        this.cancelButton.can_focus = this._hasCancelButton;
        this._preemptiveAnswer = null;

        if (this._userVerifier)
            this._userVerifier.cancel();

        this._queryingService = null;
        this.clear();
        this._message.opacity = 0;
        this._message.text = '';
        this.setUser(null);
        this._updateEntry(true);
        this.stopSpinning();

        this._passwordHintButton.visible = false;
        this._passwordResetButton.visible = false;
        this._passwordResetCode = null;

        if (oldStatus == AuthPromptStatus.VERIFICATION_FAILED)
            this.emit('failed');

        let beginRequestType;

        if (this._mode == AuthPromptMode.UNLOCK_ONLY) {
            // The user is constant at the unlock screen, so it will immediately
            // respond to the request with the username
            beginRequestType = BeginRequestType.PROVIDE_USERNAME;
        } else if (this._userVerifier.serviceIsForeground(GdmUtil.OVIRT_SERVICE_NAME) ||
                   this._userVerifier.serviceIsForeground(GdmUtil.SMARTCARD_SERVICE_NAME)) {
            // We don't need to know the username if the user preempted the login screen
            // with a smartcard or with preauthenticated oVirt credentials
            beginRequestType = BeginRequestType.DONT_PROVIDE_USERNAME;
        } else {
            // In all other cases, we should get the username up front.
            beginRequestType = BeginRequestType.PROVIDE_USERNAME;
        }

        this.emit('reset', beginRequestType);
    }

    addCharacter(unichar) {
        if (!this._entry.visible)
            return;

        this._entry.grab_key_focus();
        this._entry.clutter_text.insert_unichar(unichar);
    }

    begin(params) {
        params = Params.parse(params, { userName: null,
                                        hold: null });

        this._username = params.userName;
        this.updateSensitivity(false);

        let hold = params.hold;
        if (!hold)
            hold = new Batch.Hold();

        this._userVerifier.begin(params.userName, hold);
        this.verificationStatus = AuthPromptStatus.VERIFYING;
    }

    finish(onComplete) {
        if (!this._userVerifier.hasPendingMessages) {
            this._userVerifier.clear();
            this._username = null;
            onComplete();
            return;
        }

        let signalId = this._userVerifier.connect('no-more-messages', () => {
            this._userVerifier.disconnect(signalId);
            this._userVerifier.clear();
            this._username = null;
            onComplete();
        });
    }

    cancel() {
        if (this.verificationStatus == AuthPromptStatus.VERIFICATION_SUCCEEDED)
            return;

        this.reset();
        this.emit('cancelled');
    }

    _getUserLastLoginTime() {
        let userManager = AccountsService.UserManager.get_default();
        let user = userManager.get_user(this._username);
        return user.get_login_time();
    }

    _generateResetCode() {
        // Note: These are not secure random numbers. Doesn't matter. The
        // mechanism to convert a reset code to unlock code is well-known, so
        // who cares how random the reset code is?

        // The fist digit is fixed to "1" as version of the hash code (the zeroth
        // version had one less digit in the code).
        let resetCode = Main.customerSupport.passwordResetSalt ? '1' : '';

        let machineId = _getMachineId();
        let lastLoginTime = this._getUserLastLoginTime();
        let input = machineId + this._username + lastLoginTime;
        let checksum = GLib.compute_checksum_for_data(GLib.ChecksumType.SHA256, input);
        checksum = checksum.replace(/\D/g, '');

        let hashCode = `${parseInt(checksum) % (10 ** _RESET_CODE_LENGTH)}`;
        resetCode = `${resetCode}${hashCode.padStart(_RESET_CODE_LENGTH, '0')}`;

        return resetCode;
    }

    _computeUnlockCode(resetCode) {
        let checksum = new GLib.Checksum(GLib.ChecksumType.MD5);
        checksum.update(ByteArray.fromString(resetCode));

        if (Main.customerSupport.passwordResetSalt) {
            checksum.update(ByteArray.fromString(Main.customerSupport.passwordResetSalt));
            checksum.update([0]);
        }

        let unlockCode = checksum.get_string();
        // Remove everything except digits.
        unlockCode = unlockCode.replace(/\D/g, '');
        unlockCode = unlockCode.slice(0, _RESET_CODE_LENGTH);

        while (unlockCode.length < _RESET_CODE_LENGTH)
            unlockCode += '0';

        return unlockCode;
    }

    _showPasswordResetPrompt() {
        if (!Main.customerSupport.customerSupportEmail)
            return;

        // Stop the normal gdm conversation so it doesn't interfere.
        this._userVerifier.cancel();

        this._passwordResetButton.hide();
        this._entry.text = null;
        this._entry.clutter_text.set_password_char('');
        this._passwordResetCode = this._generateResetCode();

        // Translators: During a password reset, prompt for the "secret code" provided by customer support.
        this.setQuestion(_('Enter unlock code'));
        this.setMessage(
            // Translators: Password reset. The first %s is a verification code and the second is an email.
            _('Please inform customer support of your verification code %s by emailing %s. Customer support will use the verification code to provide you with an unlock code, which you can enter here.').format(
                this._passwordResetCode,
                Main.customerSupport.customerSupportEmail));
    }

    _maybeShowPasswordResetButton() {
        // Do not allow password reset if we are not performing password auth.
        if (!this._userVerifier.serviceIsDefault(GdmUtil.PASSWORD_SERVICE_NAME))
            return;

        // Do not allow password reset on the unlock screen.
        if (this._userVerifier.reauthenticating)
            return;

        // Do not allow password reset if we are already in the middle of
        // performing a password reset. Or if there is no password.
        let userManager = AccountsService.UserManager.get_default();
        let user = userManager.get_user(this._username);
        if (user.get_password_mode() !== AccountsService.UserPasswordMode.REGULAR)
            return;

        // Do not allow password reset if it's disabled in GSettings.
        let policy = global.settings.get_enum('password-reset-allowed');
        if (policy === 0)
            return;

        // There's got to be a better way to get our pid in gjs?
        let credentials = new Gio.Credentials();
        let pid = credentials.get_unix_pid();

        // accountsservice provides no async API, and unconditionally informs
        // polkit that interactive authorization is permissible. If interactive
        // authorization is attempted on the login screen during the call to
        // set_password_mode, it will hang forever. Ensure the password reset
        // button is hidden in this case. Besides, it's stupid to prompt for a
        // password in order to perform password reset.
        Polkit.Permission.new(
            'org.freedesktop.accounts.user-administration',
            Polkit.UnixProcess.new_for_owner(pid, 0, -1),
            null,
            (obj, result) => {
                try {
                    const permission = Polkit.Permission.new_finish(result);
                    if (permission.get_allowed() && Main.customerSupport.customerSupportEmail)
                        this._passwordResetButton.show();
                } catch (e) {
                    logError(e, 'Failed to determine if password reset is allowed');
                }
            });
    }

    _respondToSessionWorker(shouldSpin) {
        this.updateSensitivity(false);

        if (shouldSpin)
            this.startSpinning();

        if (this._queryingService)
            this._userVerifier.answerQuery(this._queryingService, this._entry.text);
        else
            this._preemptiveAnswer = this._entry.text;
    }

    _performPasswordReset() {
        this._entry.text = null;
        this._passwordResetCode = null;
        this.updateSensitivity(false);

        let userManager = AccountsService.UserManager.get_default();
        let user = userManager.get_user(this._username);
        user.set_password_mode(AccountsService.UserPasswordMode.SET_AT_LOGIN);

        this._userVerifier.begin(this._username, new Batch.Hold());
        this.verificationStatus = AuthPromptStatus.VERIFYING;
    }

    _handleIncorrectPasswordResetCode() {
        this._entry.text = null;
        this.updateSensitivity(true);
        this._message.text = _('Your unlock code was incorrect. Please try again.');
    }

    _showPasswordHint() {
        const userManager = AccountsService.UserManager.get_default();
        const user = userManager.get_user(this._username);

        this.setMessage(user.get_password_hint());
        this._displayingPasswordHint = true;
        this._passwordHintButton.hide();
        this._maybeShowPasswordResetButton();
    }
});
