// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { AccountsService, Clutter, GLib, Gio,
        Pango, Polkit, Shell, St } = imports.gi;
const ByteArray = imports.byteArray;
const Signals = imports.signals;

const Animation = imports.ui.animation;
const Batch = imports.gdm.batch;
const Config = imports.misc.config;
const GdmUtil = imports.gdm.util;
const Params = imports.misc.params;
const ShellEntry = imports.ui.shellEntry;
const UserWidget = imports.ui.userWidget;

var DEFAULT_BUTTON_WELL_ICON_SIZE = 16;
var DEFAULT_BUTTON_WELL_ANIMATION_DELAY = 1000;
var DEFAULT_BUTTON_WELL_ANIMATION_TIME = 300;

var MESSAGE_FADE_OUT_ANIMATION_TIME = 500;

const _RESET_CODE_LENGTH = 7;

const CUSTOMER_SUPPORT_FILENAME = 'vendor-customer-support.ini';
const CUSTOMER_SUPPORT_LOCATIONS = [
    Config.LOCALSTATEDIR + '/lib/eos-image-defaults/' + CUSTOMER_SUPPORT_FILENAME,
    Config.PKGDATADIR + '/' + CUSTOMER_SUPPORT_FILENAME
];

const CUSTOMER_SUPPORT_GROUP_NAME = 'Customer Support';
const CUSTOMER_SUPPORT_KEY_EMAIL = 'Email';
const PASSWORD_RESET_GROUP_NAME = 'Password Reset';
const PASSWORD_RESET_KEY_SALT = 'Salt';

var AuthPromptMode = {
    UNLOCK_ONLY: 0,
    UNLOCK_OR_LOG_IN: 1
};

var AuthPromptStatus = {
    NOT_VERIFYING: 0,
    VERIFYING: 1,
    VERIFICATION_FAILED: 2,
    VERIFICATION_SUCCEEDED: 3
};

var BeginRequestType = {
    PROVIDE_USERNAME: 0,
    DONT_PROVIDE_USERNAME: 1
};

var AuthPrompt = class {
    constructor(gdmClient, mode) {
        this.verificationStatus = AuthPromptStatus.NOT_VERIFYING;

        this._gdmClient = gdmClient;
        this._mode = mode;

        let reauthenticationOnly;
        if (this._mode == AuthPromptMode.UNLOCK_ONLY)
            reauthenticationOnly = true;
        else if (this._mode == AuthPromptMode.UNLOCK_OR_LOG_IN)
            reauthenticationOnly = false;

        this._userVerifier = new GdmUtil.ShellUserVerifier(this._gdmClient, { reauthenticationOnly: reauthenticationOnly });

        this._userVerifier.connect('ask-question', this._onAskQuestion.bind(this));
        this._userVerifier.connect('show-message', this._onShowMessage.bind(this));
        this._userVerifier.connect('verification-failed', this._onVerificationFailed.bind(this));
        this._userVerifier.connect('verification-complete', this._onVerificationComplete.bind(this));
        this._userVerifier.connect('reset', this._onReset.bind(this));
        this._userVerifier.connect('smartcard-status-changed', this._onSmartcardStatusChanged.bind(this));
        this._userVerifier.connect('ovirt-user-authenticated', this._onOVirtUserAuthenticated.bind(this));
        this.smartcardDetected = this._userVerifier.smartcardDetected;

        this.connect('next', () => {
            if (this._passwordResetCode == null)
                this._respondToSessionWorker();
             else if (this._entry.get_text() == this._computeUnlockCode(this._passwordResetCode))
                this._performPasswordReset();
             else
                this._handleIncorrectPasswordResetCode();
        });

        this.actor = new St.BoxLayout({ style_class: 'login-dialog-prompt-layout',
                                        vertical: true });
        this.actor.connect('destroy', this._onDestroy.bind(this));
        this.actor.connect('key-press-event', (actor, event) => {
            if (event.get_key_symbol() == Clutter.KEY_Escape)
                this.cancel();
            return Clutter.EVENT_PROPAGATE;
        });

        this._userWell = new St.Bin({ x_fill: true,
                                      x_align: St.Align.START });
        this.actor.add(this._userWell,
                       { x_align: St.Align.START,
                         x_fill: true,
                         y_fill: true,
                         expand: true });
        this._label = new St.Label({ style_class: 'login-dialog-prompt-label' });

        this.actor.add(this._label,
                       { expand: true,
                         x_fill: false,
                         y_fill: true,
                         x_align: St.Align.START });
        this._entry = new St.Entry({ style_class: 'login-dialog-prompt-entry',
                                     can_focus: true });
        ShellEntry.addContextMenu(this._entry, { isPassword: true, actionMode: Shell.ActionMode.NONE });

        this.actor.add(this._entry,
                       { expand: true,
                         x_fill: true,
                         y_fill: false,
                         x_align: St.Align.START });

        this._entry.grab_key_focus();

        this._message = new St.Label({ opacity: 0,
                                       styleClass: 'login-dialog-message' });
        this._message.clutter_text.line_wrap = true;
        this._message.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this.actor.add(this._message, { x_fill: false,
                                        x_align: St.Align.START,
                                        y_fill: true,
                                        y_align: St.Align.START });

        let passwordHintLabel = new St.Label({
            text: _("Show password hint"),
            style_class: 'login-dialog-password-recovery-link',
        });
        this._passwordHintButton = new St.Button({
            style_class: 'login-dialog-password-recovery-button',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            can_focus: true,
            child: passwordHintLabel,
            reactive: true,
            x_align: St.Align.START,
            x_fill: true,
            visible: false,
        });
        this.actor.add_child(this._passwordHintButton);
        this._passwordHintButton.connect('clicked', this._showPasswordHint.bind(this));

        let passwordResetLabel = new St.Label({ text: _("Forgot password?"),
                                                style_class: 'login-dialog-password-recovery-link' });
        this._passwordResetButton = new St.Button({ style_class: 'login-dialog-password-recovery-button',
                                                    button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                                    can_focus: true,
                                                    child: passwordResetLabel,
                                                    reactive: true,
                                                    x_align: St.Align.START,
                                                    x_fill: true,
                                                    visible: false });
        this.actor.add(this._passwordResetButton,
                       { x_fill: false,
                         x_align: St.Align.START });
        this._passwordResetButton.connect('clicked', this._showPasswordResetPrompt.bind(this));

        this._buttonBox = new St.BoxLayout({ style_class: 'login-dialog-button-box',
                                             vertical: false });
        this.actor.add(this._buttonBox,
                       { expand: true,
                         x_align: St.Align.MIDDLE,
                         y_align: St.Align.END });

        this._defaultButtonWell = new St.Widget({ layout_manager: new Clutter.BinLayout() });
        this._defaultButtonWellActor = null;

        this._initButtons();

        this._spinner = new Animation.Spinner(DEFAULT_BUTTON_WELL_ICON_SIZE);
        this._spinner.actor.opacity = 0;
        this._spinner.actor.show();
        this._defaultButtonWell.add_child(this._spinner.actor);

        this._customerSupportEmail = null;

        this._displayingPasswordHint = false;
        this._passwordResetCode = null;

        this._customerSupportKeyFile = null;
    }

    _onDestroy() {
        this._userVerifier.destroy();
        this._userVerifier = null;
    }

    _initButtons() {
        this.cancelButton = new St.Button({ style_class: 'modal-dialog-button button',
                                            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                            reactive: true,
                                            can_focus: true,
                                            label: _("Cancel") });
        this.cancelButton.connect('clicked', () => this.cancel());
        this._buttonBox.add(this.cancelButton,
                            { expand: false,
                              x_fill: false,
                              y_fill: false,
                              x_align: St.Align.START,
                              y_align: St.Align.END });

        this._buttonBox.add(this._defaultButtonWell,
                            { expand: true,
                              x_fill: false,
                              y_fill: false,
                              x_align: St.Align.END,
                              y_align: St.Align.MIDDLE });
        this.nextButton = new St.Button({ style_class: 'modal-dialog-button button',
                                          button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                          reactive: true,
                                          can_focus: true,
                                          label: _("Next") });
        this.nextButton.connect('clicked', () => this.emit('next'));
        this.nextButton.add_style_pseudo_class('default');
        this._buttonBox.add(this.nextButton,
                            { expand: false,
                              x_fill: false,
                              y_fill: false,
                              x_align: St.Align.END,
                              y_align: St.Align.END });

        this._updateNextButtonSensitivity(this._entry.text.length > 0);

        this._entry.clutter_text.connect('text-changed', () => {
            if (this._passwordResetCode == null) {
                if (!this._userVerifier.hasPendingMessages &&
                    !this._displayingPasswordHint) {
                    this._fadeOutMessage();
                }
                this._updateNextButtonSensitivity(this._entry.text.length > 0 || this.verificationStatus == AuthPromptStatus.VERIFYING);
            } else {
                // Password unlock code must contain the right number of digits, and only digits.
                this._updateNextButtonSensitivity(
                this._entry.text.length == _RESET_CODE_LENGTH &&
                this._entry.text.search(/\D/) == -1);
            }
        });
        this._entry.clutter_text.connect('activate', () => {
            if (this.nextButton.reactive)
                this.emit('next');
        });
    }

    _onAskQuestion(verifier, serviceName, question, passwordChar) {
        if (this._queryingService)
            this.clear();

        this._queryingService = serviceName;
        if (this._preemptiveAnswer) {
            this._userVerifier.answerQuery(this._queryingService, this._preemptiveAnswer);
            this._preemptiveAnswer = null;
            return;
        }
        this.setPasswordChar(passwordChar);
        this.setQuestion(question);

        if (passwordChar) {
            if (this._userVerifier.reauthenticating)
                this.nextButton.label = _("Unlock");
            else
                this.nextButton.label = C_("button", "Sign In");
        } else {
            this.nextButton.label = _("Next");
        }

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

        let userManager = AccountsService.UserManager.get_default();
        let user = userManager.get_user(this._username);
        if (user.get_password_hint().length > 0)
            this._passwordHintButton.show();
        else
            this._maybeShowPasswordResetButton();
    }

    _onVerificationComplete() {
        this.setActorInDefaultButtonWell(null);
        this.verificationStatus = AuthPromptStatus.VERIFICATION_SUCCEEDED;
        this.cancelButton.reactive = false;
    }

    _onReset() {
        this.verificationStatus = AuthPromptStatus.NOT_VERIFYING;
        this.reset();
    }

    addActorToDefaultButtonWell(actor) {
        this._defaultButtonWell.add_child(actor);
    }

    setActorInDefaultButtonWell(actor, animate) {
        if (!this._defaultButtonWellActor &&
            !actor)
            return;

        let oldActor = this._defaultButtonWellActor;

        if (oldActor)
            oldActor.remove_all_transitions();

        let wasSpinner;
        if (oldActor == this._spinner.actor)
            wasSpinner = true;
        else
            wasSpinner = false;

        let isSpinner;
        if (actor == this._spinner.actor)
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
                    }
                });
            }
        }

        if (actor) {
            if (isSpinner)
                this._spinner.play();

            if (!animate)
                actor.opacity = 255;
            else
                actor.ease({
                    opacity: 255,
                    duration: DEFAULT_BUTTON_WELL_ANIMATION_TIME,
                    delay: DEFAULT_BUTTON_WELL_ANIMATION_DELAY,
                    mode: Clutter.AnimationMode.LINEAR
                });
        }

        this._defaultButtonWellActor = actor;
    }

    startSpinning() {
        this.setActorInDefaultButtonWell(this._spinner.actor, true);
    }

    stopSpinning() {
        this.setActorInDefaultButtonWell(null, false);
    }

    clear() {
        this._entry.text = '';
        this.stopSpinning();
    }

    setPasswordChar(passwordChar) {
        this._entry.clutter_text.set_password_char(passwordChar);
        this._entry.menu.isPassword = passwordChar != '';
    }

    setQuestion(question) {
        this._label.set_text(question);

        this._label.show();
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
            mode: Clutter.AnimationMode.EASE_OUT_QUAD
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

    _updateNextButtonSensitivity(sensitive) {
        this.nextButton.reactive = sensitive;
        this.nextButton.can_focus = sensitive;
    }

    updateSensitivity(sensitive) {
        this._updateNextButtonSensitivity(sensitive && (this._entry.text.length > 0 || this.verificationStatus == AuthPromptStatus.VERIFYING));
        this._entry.reactive = sensitive;
        this._entry.clutter_text.editable = sensitive;
    }

    hide() {
        this.setActorInDefaultButtonWell(null, true);
        this.actor.hide();
        this._message.opacity = 0;

        this.setUser(null);

        this.updateSensitivity(true);
        this._entry.set_text('');
    }

    setUser(user) {
        let oldChild = this._userWell.get_child();
        if (oldChild)
            oldChild.destroy();

        if (user) {
            let userWidget = new UserWidget.UserWidget(user);
            this._userWell.set_child(userWidget.actor);
        }
    }

    reset() {
        let oldStatus = this.verificationStatus;
        this.verificationStatus = AuthPromptStatus.NOT_VERIFYING;
        this.cancelButton.reactive = true;
        this.nextButton.label = _("Next");
        this._preemptiveAnswer = null;

        if (this._userVerifier)
            this._userVerifier.cancel();

        this._queryingService = null;
        this.clear();
        this._message.opacity = 0;
        this._message.text = '';
        this.setUser(null);
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
        if (this.verificationStatus == AuthPromptStatus.VERIFICATION_SUCCEEDED) {
            return;
        }
        this.reset();
        this.emit('cancelled');
    }

    _ensureCustomerSupportFile() {
        if (this._customerSupportKeyFile)
            return this._customerSupportKeyFile;

        this._customerSupportKeyFile = new GLib.KeyFile();

        for (let path of CUSTOMER_SUPPORT_LOCATIONS) {
            try {
                this._customerSupportKeyFile.load_from_file(path, GLib.KeyFileFlags.NONE);
                break;
            } catch (e) {
                logError(e, 'Failed to read customer support data from ' + path);
            }
        }

        return this._customerSupportKeyFile;
    }

    _generateResetCode() {
        // Note: These are not secure random numbers. Doesn't matter. The
        // mechanism to convert a reset code to unlock code is well-known, so
        // who cares how random the reset code is?

        // The fist digit is fixed to "1" as version of the hash code (the zeroth
        // version had one less digit in the code).
        let resetCode = this._getResetCodeSalt() ? '1' : '';

        for (let n = 0; n < _RESET_CODE_LENGTH; n++)
            resetCode = '%s%d'.format(resetCode, GLib.random_int_range(0, 10));
        return resetCode;
    }

    _computeUnlockCode(resetCode) {
        let salt = this._getResetCodeSalt();
        let checksum = new GLib.Checksum(GLib.ChecksumType.MD5);
        checksum.update(ByteArray.fromString(resetCode));

        if (salt) {
            checksum.update(ByteArray.fromString(salt));
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

    _getCustomerSupportEmail() {
        let keyFile = this._ensureCustomerSupportFile();

        try {
            return keyFile.get_locale_string(CUSTOMER_SUPPORT_GROUP_NAME,
                                             CUSTOMER_SUPPORT_KEY_EMAIL,
                                             null);
        } catch (e) {
            logError(e, 'Failed to read customer support email');
            return null;
        }
    }

    _getResetCodeSalt() {
        let keyFile = this._ensureCustomerSupportFile();

        try {
            return keyFile.get_locale_string(PASSWORD_RESET_GROUP_NAME,
                                             PASSWORD_RESET_KEY_SALT,
                                             null);
        } catch (e) {
            logError(e, 'Failed to read password reset salt value');
            return null;
        }
    }

    _showPasswordResetPrompt() {
        let customerSupportEmail = this._getCustomerSupportEmail();
        if (!customerSupportEmail)
            return;

        // Stop the normal gdm conversation so it doesn't interfere.
        this._userVerifier.cancel();

        this._passwordResetButton.hide();
        this._entry.text = null;
        this._entry.clutter_text.set_password_char('');
        this._passwordResetCode = this._generateResetCode();

        // FIXME: This string is too long. It is ellipsized, even in English.
        // It must be shortened after the Endless 3.2 release, once there is
        // time to translate the shortened version.
        // Translators: During a password reset, prompt for the "secret code" provided by customer support.
        this.setQuestion(_("Enter unlock code provided by customer support:"));
        this.setMessage(
            // Translators: Password reset. The first %s is a verification code and the second is an email.
            _("Please inform customer support of your verification code %s by emailing %s. The code will remain valid until you click Cancel or turn off your computer.").format(
                this._passwordResetCode,
                customerSupportEmail));

        // Translators: Button on login dialog, after clicking Forgot Password?
        this.nextButton.set_label(_("Reset Password"));
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
        if (user.get_password_mode() != AccountsService.UserPasswordMode.REGULAR)
            return;

        // Do not allow password reset if it's disabled in GSettings.
        let policy = global.settings.get_enum('password-reset-allowed');
        if (policy == 0)
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
        Polkit.Permission.new('org.freedesktop.accounts.user-administration',
                              Polkit.UnixProcess.new_for_owner(pid, 0, -1),
                              null, (obj, result) => {
                                  try {
                                      let permission = Polkit.Permission.new_finish(result);
                                      if (permission.get_allowed() && this._getCustomerSupportEmail())
                                          this._passwordResetButton.show();
                                  } catch(e) {
                                      logError(e, 'Failed to determine if password reset is allowed');
                                  }
                              });
    }

    _respondToSessionWorker() {
         this.updateSensitivity(false);
         this.startSpinning();
         if (this._queryingService) {
             this._userVerifier.answerQuery(this._queryingService, this._entry.text);
         } else {
             this._preemptiveAnswer = this._entry.text;
         }
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
         this._message.text = _("Your unlock code was incorrect. Please try again.");
    }

    _showPasswordHint() {
        let userManager = AccountsService.UserManager.get_default();
        let user = userManager.get_user(this._username);
        this.setMessage(user.get_password_hint());
        this._displayingPasswordHint = true;
        this._passwordHintButton.hide();
        this._maybeShowPasswordResetButton();
    }
};
Signals.addSignalMethods(AuthPrompt.prototype);
