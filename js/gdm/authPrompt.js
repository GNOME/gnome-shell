// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported AuthPrompt */

const { Clutter, GObject, Pango, Shell, St } = imports.gi;

const Animation = imports.ui.animation;
const Batch = imports.gdm.batch;
const GdmUtil = imports.gdm.util;
const Util = imports.misc.util;
const Params = imports.misc.params;
const ShellEntry = imports.ui.shellEntry;
const UserWidget = imports.ui.userWidget;

var DEFAULT_BUTTON_WELL_ICON_SIZE = 16;
var DEFAULT_BUTTON_WELL_ANIMATION_DELAY = 1000;
var DEFAULT_BUTTON_WELL_ANIMATION_TIME = 300;

var MESSAGE_FADE_OUT_ANIMATION_TIME = 500;

const WIGGLE_OFFSET = 6;
const WIGGLE_DURATION = 65;
const N_WIGGLES = 3;

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
        });

        this.verificationStatus = AuthPromptStatus.NOT_VERIFYING;

        this._gdmClient = gdmClient;
        this._mode = mode;

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

        this.connect('next', () => {
            this.updateSensitivity(false);
            this.startSpinning();
            if (this._queryingService)
                this._userVerifier.answerQuery(this._queryingService, this._entry.text);
            else
                this._preemptiveAnswer = this._entry.text;
        });

        this.connect('destroy', this._onDestroy.bind(this));

        this._userWell = new St.Bin({
            x_expand: true,
            y_expand: true,
        });
        this.add_child(this._userWell);
        this._label = new St.Label({
            style_class: 'login-dialog-prompt-label',
            x_expand: false,
            y_expand: true,
        });

        this.add_child(this._label);
        this._entry = new St.PasswordEntry({
            style_class: 'login-dialog-prompt-entry',
            can_focus: true,
            x_expand: false,
            y_expand: true,
        });
        ShellEntry.addContextMenu(this._entry, { actionMode: Shell.ActionMode.NONE });

        this.add_child(this._entry);

        this._entry.grab_key_focus();

        this._message = new St.Label({
            opacity: 0,
            styleClass: 'login-dialog-message',
            x_expand: false,
            y_expand: true,
            y_align: Clutter.ActorAlign.START,
        });
        this._message.clutter_text.line_wrap = true;
        this._message.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this.add_child(this._message);

        this._buttonBox = new St.BoxLayout({
            style_class: 'login-dialog-button-box',
            vertical: false,
            y_align: Clutter.ActorAlign.END,
        });
        this.add_child(this._buttonBox);

        this._defaultButtonWell = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            x_align: Clutter.ActorAlign.END,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this._initButtons();

        this._spinner = new Animation.Spinner(DEFAULT_BUTTON_WELL_ICON_SIZE);
        this._spinner.opacity = 0;
        this._spinner.show();
        this._defaultButtonWell.add_child(this._spinner);
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

    _initButtons() {
        this.cancelButton = new St.Button({
            style_class: 'modal-dialog-button button',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: true,
            can_focus: true,
            label: _("Cancel"),
            x_expand: true,
            x_align: Clutter.ActorAlign.START,
            y_align: Clutter.ActorAlign.END,
        });
        this.cancelButton.connect('clicked', () => this.cancel());
        this._buttonBox.add_child(this.cancelButton);

        this._buttonBox.add_child(this._defaultButtonWell);
        this.nextButton = new St.Button({
            style_class: 'modal-dialog-button button',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: true,
            can_focus: true,
            label: _("Next"),
            x_align: Clutter.ActorAlign.END,
            y_align: Clutter.ActorAlign.END,
        });
        this.nextButton.connect('clicked', () => this.emit('next'));
        this.nextButton.add_style_pseudo_class('default');
        this._buttonBox.add_child(this.nextButton);

        this._updateNextButtonSensitivity(this._entry.text.length > 0);

        this._entry.clutter_text.connect('text-changed', () => {
            if (!this._userVerifier.hasPendingMessages)
                this._fadeOutMessage();

            this._updateNextButtonSensitivity(this._entry.text.length > 0 || this.verificationStatus == AuthPromptStatus.VERIFYING);
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
        this._entry.set({
            password_visible: passwordChar === 0,
        });
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

        Util.wiggle(this._entry, {
            offset: WIGGLE_OFFSET,
            duration: WIGGLE_DURATION,
            wiggleCount: N_WIGGLES,
        });
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

        if (user) {
            let userWidget = new UserWidget.UserWidget(user);
            userWidget.x_align = Clutter.ActorAlign.START;
            this._userWell.set_child(userWidget);
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
        this.setUser(null);
        this.stopSpinning();

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

        this.reset();
        this.emit('cancelled');
    }
});
