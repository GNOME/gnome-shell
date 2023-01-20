// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported AuthPrompt */

const { Clutter, GLib, GObject, Pango, Shell, St } = imports.gi;

const Animation = imports.ui.animation;
const AuthList = imports.gdm.authList;
const Batch = imports.gdm.batch;
const GdmUtil = imports.gdm.util;
const Params = imports.misc.params;
const ShellEntry = imports.ui.shellEntry;
const UserWidget = imports.ui.userWidget;
const Util = imports.misc.util;

var DEFAULT_BUTTON_WELL_ICON_SIZE = 16;
var DEFAULT_BUTTON_WELL_ANIMATION_DELAY = 1000;
var DEFAULT_BUTTON_WELL_ANIMATION_TIME = 300;

var MESSAGE_FADE_OUT_ANIMATION_TIME = 500;

var AuthPromptMode = {
    UNLOCK_ONLY: 0,
    UNLOCK_OR_LOG_IN: 1,
};

var AuthPromptStatus = {
    NOT_VERIFYING: 0,
    VERIFYING: 1,
    VERIFICATION_FAILED: 2,
    VERIFICATION_SUCCEEDED: 3,
    VERIFICATION_CANCELLED: 4,
    VERIFICATION_IN_PROGRESS: 5,
};

var BeginRequestType = {
    PROVIDE_USERNAME: 0,
    DONT_PROVIDE_USERNAME: 1,
    REUSE_USERNAME: 2,
};

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
            reactive: true,
        });

        this.verificationStatus = AuthPromptStatus.NOT_VERIFYING;

        this._gdmClient = gdmClient;
        this._mode = mode;
        this._defaultButtonWellActor = null;
        this._cancelledRetries = 0;

        let reauthenticationOnly;
        if (this._mode == AuthPromptMode.UNLOCK_ONLY)
            reauthenticationOnly = true;
        else if (this._mode == AuthPromptMode.UNLOCK_OR_LOG_IN)
            reauthenticationOnly = false;

        this._userVerifier = new GdmUtil.ShellUserVerifier(this._gdmClient, { reauthenticationOnly });

        this._userVerifier.connect('ask-question', this._onAskQuestion.bind(this));
        this._userVerifier.connect('show-message', this._onShowMessage.bind(this));
        this._userVerifier.connect('show-choice-list', this._onShowChoiceList.bind(this));
        this._userVerifier.connect('verification-failed', this._onVerificationFailed.bind(this));
        this._userVerifier.connect('verification-complete', this._onVerificationComplete.bind(this));
        this._userVerifier.connect('reset', this._onReset.bind(this));
        this._userVerifier.connect('smartcard-status-changed', this._onSmartcardStatusChanged.bind(this));
        this._userVerifier.connect('credential-manager-authenticated', this._onCredentialManagerAuthenticated.bind(this));
        this.smartcardDetected = this._userVerifier.smartcardDetected;

        this.connect('destroy', this._onDestroy.bind(this));

        this._userWell = new St.Bin({
            x_expand: true,
            y_expand: true,
        });
        this.add_child(this._userWell);

        this._hasCancelButton = this._mode === AuthPromptMode.UNLOCK_OR_LOG_IN;

        this._initInputRow();

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
    }

    _onDestroy() {
        this._inactiveEntry.destroy();
        this._inactiveEntry = null;
        this._userVerifier.destroy();
        this._userVerifier = null;
    }

    vfunc_key_press_event(keyPressEvent) {
        if (keyPressEvent.keyval == Clutter.KEY_Escape)
            this.cancel();
        return super.vfunc_key_press_event(keyPressEvent);
    }

    _initInputRow() {
        this._mainBox = new St.BoxLayout({
            style_class: 'login-dialog-button-box',
            vertical: false,
        });
        this.add_child(this._mainBox);

        this.cancelButton = new St.Button({
            style_class: 'login-dialog-button cancel-button',
            accessible_name: _('Cancel'),
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: this._hasCancelButton,
            can_focus: this._hasCancelButton,
            x_align: Clutter.ActorAlign.START,
            y_align: Clutter.ActorAlign.CENTER,
            icon_name: 'go-previous-symbolic',
        });
        if (this._hasCancelButton)
            this.cancelButton.connect('clicked', () => this.cancel());
        else
            this.cancelButton.opacity = 0;
        this._mainBox.add_child(this.cancelButton);

        this._authList = new AuthList.AuthList();
        this._authList.set({
            visible: false,
        });
        this._authList.connect('activate', (list, key) => {
            this._authList.reactive = false;
            this._authList.ease({
                opacity: 0,
                duration: MESSAGE_FADE_OUT_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    this._authList.clear();
                    this._authList.hide();
                    this._userVerifier.selectChoice(this._queryingService, key);
                },
            });
        });
        this._mainBox.add_child(this._authList);

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
        this._inactiveEntry = this._textEntry;

        this._timedLoginIndicator = new St.Bin({
            style_class: 'login-dialog-timed-login-indicator',
            scale_x: 0,
        });

        this.add_child(this._timedLoginIndicator);

        [this._textEntry, this._passwordEntry].forEach(entry => {
            entry.clutter_text.connect('text-changed', () => {
                if (!this._userVerifier.hasPendingMessages)
                    this._fadeOutMessage();
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
            coordinate: Clutter.BindCoordinate.WIDTH,
        }));
        this._mainBox.add_child(this._defaultButtonWell);

        this._spinner = new Animation.Spinner(DEFAULT_BUTTON_WELL_ICON_SIZE);
        this._defaultButtonWell.add_child(this._spinner);
    }

    showTimedLoginIndicator(time) {
        let hold = new Batch.Hold();

        this.hideTimedLoginIndicator();

        const startTime = GLib.get_monotonic_time();

        this._timedLoginTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 33,
            () => {
                const currentTime = GLib.get_monotonic_time();
                const elapsedTime = (currentTime - startTime) / GLib.USEC_PER_SEC;
                this._timedLoginIndicator.scale_x = elapsedTime / time;
                if (elapsedTime >= time) {
                    this._timedLoginTimeoutId = 0;
                    hold.release();
                    return GLib.SOURCE_REMOVE;
                }

                return GLib.SOURCE_CONTINUE;
            });

        GLib.Source.set_name_by_id(this._timedLoginTimeoutId, '[gnome-shell] this._timedLoginTimeoutId');

        return hold;
    }

    hideTimedLoginIndicator() {
        if (this._timedLoginTimeoutId) {
            GLib.source_remove(this._timedLoginTimeoutId);
            this._timedLoginTimeoutId = 0;
        }
        this._timedLoginIndicator.scale_x = 0.;
    }

    _activateNext(shouldSpin) {
        this.verificationStatus = AuthPromptStatus.VERIFICATION_IN_PROGRESS;
        this.updateSensitivity(false);

        if (shouldSpin)
            this.startSpinning();

        if (this._queryingService)
            this._userVerifier.answerQuery(this._queryingService, this._entry.text);
        else
            this._preemptiveAnswer = this._entry.text;

        this.emit('next');
    }

    _updateEntry(secret) {
        if (secret && this._entry !== this._passwordEntry) {
            this._mainBox.replace_child(this._entry, this._passwordEntry);
            this._entry = this._passwordEntry;
            this._inactiveEntry = this._textEntry;
        } else if (!secret && this._entry !== this._textEntry) {
            this._mainBox.replace_child(this._entry, this._textEntry);
            this._entry = this._textEntry;
            this._inactiveEntry = this._passwordEntry;
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
            this.setQuestion(question.replace(/[:：] *$/, '').trim());

        this.updateSensitivity(true);
        this.emit('prompted');
    }

    _onShowChoiceList(userVerifier, serviceName, promptMessage, choiceList) {
        if (this._queryingService)
            this.clear();

        this._queryingService = serviceName;

        if (this._preemptiveAnswer)
            this._preemptiveAnswer = null;

        this.setChoiceList(promptMessage, choiceList);
        this.updateSensitivity(true);
        this.emit('prompted');
    }

    _onCredentialManagerAuthenticated() {
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

    _onShowMessage(_userVerifier, serviceName, message, type) {
        let wiggleParameters = {duration: 0};

        if (type === GdmUtil.MessageType.ERROR &&
            this._userVerifier.serviceIsFingerprint(serviceName)) {
            // TODO: Use Await for wiggle to be over before unfreezing the user verifier queue
            wiggleParameters = {
                duration: 65,
                wiggleCount: 3,
            };
            this._userVerifier.increaseCurrentMessageTimeout(
                wiggleParameters.duration * (wiggleParameters.wiggleCount + 2));
        }

        this.setMessage(message, type, wiggleParameters);
        this.emit('prompted');
    }

    _onVerificationFailed(userVerifier, serviceName, canRetry) {
        const wasQueryingService = this._queryingService === serviceName;

        if (wasQueryingService) {
            this._queryingService = null;
            this.clear();
        }

        this.updateSensitivity(canRetry);
        this.setActorInDefaultButtonWell(null);

        if (!canRetry)
            this.verificationStatus = AuthPromptStatus.VERIFICATION_FAILED;

        if (wasQueryingService)
            Util.wiggle(this._entry);
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
        this._authList.clear();
        this._authList.hide();
    }

    setQuestion(question) {
        this._entry.hint_text = question;

        this._authList.hide();
        this._entry.show();
        this._entry.grab_key_focus();
    }

    _fadeInChoiceList() {
        this._authList.set({
            opacity: 0,
            visible: true,
            reactive: false,
        });
        this._authList.ease({
            opacity: 255,
            duration: MESSAGE_FADE_OUT_ANIMATION_TIME,
            transition: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => (this._authList.reactive = true),
        });
    }

    setChoiceList(promptMessage, choiceList) {
        this._authList.clear();
        this._authList.label.text = promptMessage;
        for (let key in choiceList) {
            let text = choiceList[key];
            this._authList.addItem(key, text);
        }

        this._entry.hide();
        if (this._message.text === '')
            this._message.hide();
        this._fadeInChoiceList();
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

    setMessage(message, type, wiggleParameters = {duration: 0}) {
        if (type == GdmUtil.MessageType.ERROR)
            this._message.add_style_class_name('login-dialog-message-warning');
        else
            this._message.remove_style_class_name('login-dialog-message-warning');

        if (type == GdmUtil.MessageType.HINT)
            this._message.add_style_class_name('login-dialog-message-hint');
        else
            this._message.remove_style_class_name('login-dialog-message-hint');

        this._message.show();
        if (message) {
            this._message.remove_all_transitions();
            this._message.text = message;
            this._message.opacity = 255;
        } else {
            this._message.opacity = 0;
        }

        Util.wiggle(this._message, wiggleParameters);
    }

    updateSensitivity(sensitive) {
        if (this._entry.reactive === sensitive)
            return;

        this._entry.reactive = sensitive;

        if (sensitive) {
            this._entry.grab_key_focus();
        } else {
            this.grab_key_focus();

            if (this._entry === this._passwordEntry)
                this._entry.password_visible = false;
        }
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
        this.setUser(null);
        this._updateEntry(true);
        this.stopSpinning();

        if (oldStatus == AuthPromptStatus.VERIFICATION_FAILED)
            this.emit('failed');
        else if (oldStatus === AuthPromptStatus.VERIFICATION_CANCELLED)
            this.emit('cancelled');

        let beginRequestType;

        if (this._mode == AuthPromptMode.UNLOCK_ONLY) {
            // The user is constant at the unlock screen, so it will immediately
            // respond to the request with the username
            if (oldStatus === AuthPromptStatus.VERIFICATION_CANCELLED)
                return;
            beginRequestType = BeginRequestType.PROVIDE_USERNAME;
        } else if (this._userVerifier.foregroundServiceDeterminesUsername()) {
            // We don't need to know the username if the user preempted the login screen
            // with a smartcard or with preauthenticated oVirt credentials
            beginRequestType = BeginRequestType.DONT_PROVIDE_USERNAME;
        } else if (oldStatus === AuthPromptStatus.VERIFICATION_IN_PROGRESS) {
            // We're going back to retry with current user
            beginRequestType = BeginRequestType.REUSE_USERNAME;
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
        params = Params.parse(params, {
            userName: null,
            hold: null,
        });

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
            onComplete();
            return;
        }

        let signalId = this._userVerifier.connect('no-more-messages', () => {
            this._userVerifier.disconnect(signalId);
            this._userVerifier.clear();
            onComplete();
        });
    }

    cancel() {
        if (this.verificationStatus == AuthPromptStatus.VERIFICATION_SUCCEEDED)
            return;

        if (this.verificationStatus === AuthPromptStatus.VERIFICATION_IN_PROGRESS) {
            this._cancelledRetries++;
            if (this._cancelledRetries > this._userVerifier.allowedFailures)
                this.verificationStatus = AuthPromptStatus.VERIFICATION_FAILED;
        } else {
            this.verificationStatus = AuthPromptStatus.VERIFICATION_CANCELLED;
        }

        this.reset();
    }
});
