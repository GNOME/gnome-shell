import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import Atk from 'gi://Atk';
import GObject from 'gi://GObject';
import Graphene from 'gi://Graphene';
import Pango from 'gi://Pango';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Animation from '../ui/animation.js';
import * as AuthList from './authList.js';
import * as Batch from './batch.js';
import * as Const from './const.js';
import * as GdmUtil from './util.js';
import * as Params from '../misc/params.js';
import * as ShellEntry from '../ui/shellEntry.js';
import * as UserWidget from '../ui/userWidget.js';
import {wiggle} from '../misc/animationUtils.js';

const DEFAULT_BUTTON_WELL_ICON_SIZE = 16;
const DEFAULT_BUTTON_WELL_ANIMATION_TIME = 300;

const MESSAGE_FADE_OUT_ANIMATION_TIME = 500;

/** @enum {number} */
export const AuthPromptMode = {
    UNLOCK_ONLY: 0,
    UNLOCK_OR_LOG_IN: 1,
};

/** @enum {number} */
export const AuthPromptStatus = {
    NOT_VERIFYING: 0,
    VERIFYING: 1,
    VERIFICATION_FAILED: 2,
    VERIFICATION_SUCCEEDED: 3,
    VERIFICATION_CANCELLED: 4,
    VERIFICATION_IN_PROGRESS: 5,
};

/** @enum {number} */
export const BeginRequestType = {
    PROVIDE_USERNAME: 0,
    DONT_PROVIDE_USERNAME: 1,
    REUSE_USERNAME: 2,
};

export const AuthPrompt = GObject.registerClass({
    Signals: {
        'cancelled': {},
        'failed': {},
        'next': {},
        'prompted': {},
        'mechanisms-changed': {param_types: [GObject.TYPE_JSOBJECT, GObject.TYPE_JSOBJECT]},
        'reset': {param_types: [GObject.TYPE_UINT]},
        'verification-complete': {},
        'loading': {param_types: [GObject.TYPE_BOOLEAN]},
    },
}, class AuthPrompt extends St.BoxLayout {
    _init(gdmClient, mode) {
        super._init({
            style_class: 'login-dialog-prompt-layout',
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
            reactive: true,
        });

        this.verificationStatus = AuthPromptStatus.NOT_VERIFYING;

        this._gdmClient = gdmClient;
        this._mode = mode;
        this._defaultButtonWellActor = null;
        this._cancelledRetries = 0;
        this._promptStep = 0;

        let reauthenticationOnly;
        if (this._mode === AuthPromptMode.UNLOCK_ONLY)
            reauthenticationOnly = true;
        else if (this._mode === AuthPromptMode.UNLOCK_OR_LOG_IN)
            reauthenticationOnly = false;

        this._userVerifier = this._createUserVerifier(this._gdmClient, {reauthenticationOnly});

        this._userVerifier.connect('ask-question', this._onAskQuestion.bind(this));
        this._userVerifier.connect('show-message', this._onShowMessage.bind(this));
        this._userVerifier.connect('show-choice-list', this._onShowChoiceList.bind(this));
        this._userVerifier.connect('mechanisms-changed', (_, ...args) => this.emit('mechanisms-changed', ...args));
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

        this._initInputRow();

        const capsLockPlaceholder = new St.Label();
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

    _createUserVerifier(gdmClient, params) {
        return new GdmUtil.ShellUserVerifier(gdmClient, params);
    }

    _onDestroy() {
        this._inactiveEntry.destroy();
        this._inactiveEntry = null;
        this._userVerifier.destroy();
        this._userVerifier = null;
        this._entry = null;
    }

    on_key_press_event(event) {
        if (event.get_key_symbol() === Clutter.KEY_Escape) {
            this.cancel();
            return Clutter.EVENT_STOP;
        }

        if (this._preemptiveInput && !this._pendingActivate) {
            const unichar = event.get_key_unicode();
            if (event.get_key_symbol() === Clutter.KEY_Return) {
                this._pendingActivate = true;
            } else if (GLib.unichar_isgraph(unichar)) {
                this._entry.clutter_text.insert_text(unichar,
                    this._entry.clutter_text.cursor_position);
            }
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    }

    _initInputRow() {
        this._mainBox = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            style_class: 'login-dialog-button-box',
            x_expand: true,
            y_expand: false,
        });
        this.add_child(this._mainBox);

        this.cancelButton = new St.Button({
            style_class: 'login-dialog-button cancel-button',
            accessible_name: _('Cancel'),
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: true,
            can_focus: true,
            x_expand: true,
            x_align: Clutter.ActorAlign.START,
            y_align: Clutter.ActorAlign.CENTER,
            icon_name: 'go-previous-symbolic',
        });
        this.cancelButton.add_constraint(new Clutter.AlignConstraint({
            source: this._mainBox,
            align_axis: Clutter.AlignAxis.X_AXIS,
            pivot_point: new Graphene.Point({x: 1, y: 0}),
        }));

        this.cancelButton.connect('clicked', () => this.cancel());
        this._updateCancelButton();
        this._mainBox.add_child(this.cancelButton);

        this._authList = new AuthList.AuthList();
        this._authList.set({
            visible: false,
            x_align: Clutter.ActorAlign.FILL,
            x_expand: true,
        });
        this._authList.connect('activate', (list, key) => {
            this._authList.reactive = false;
            this._authList.ease({
                opacity: 0,
                duration: MESSAGE_FADE_OUT_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_QUAD,
                onComplete: () => {
                    this._authListTitle.child.text = '';
                    this._authList.clear();
                    this._authList.hide();
                    this._userVerifier.selectChoice(this._queryingService, key);
                },
            });
        });
        this.add_child(this._authList);

        this._authListTitle = new St.Button({
            style_class: 'login-dialog-auth-list-title',
            x_expand: true,
            y_expand: true,
            child: new St.Label({style_class: 'login-dialog-auth-list-title-label'}),
            reactive: false,
            can_focus: false,
        });
        this._authList.bind_property('visible',
            this._authListTitle, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        this._authList.bind_property('opacity',
            this._authListTitle, 'opacity',
            GObject.BindingFlags.SYNC_CREATE);
        this._mainBox.add_child(this._authListTitle);

        this._entryArea = new St.Widget({
            style_class: 'login-dialog-prompt-entry-area',
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            y_expand: true,
            visible: false,
        });
        this._mainBox.add_child(this._entryArea);

        const entryParams = {
            style_class: 'login-dialog-prompt-entry',
            can_focus: true,
            x_expand: true,
            y_expand: true,
        };

        this._entry = null;

        this._textEntry = new St.Entry(entryParams);
        ShellEntry.addContextMenu(this._textEntry, {actionMode: Shell.ActionMode.NONE});

        this._passwordEntry = new St.PasswordEntry(entryParams);
        ShellEntry.addContextMenu(this._passwordEntry, {actionMode: Shell.ActionMode.NONE});

        this._entry = this._passwordEntry;
        this._entryArea.add_child(this._entry);
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

            entry.clutter_text.connect('activate', () => this._activateNext());
        });

        this._defaultButtonWell = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            style_class: 'login-dialog-default-button-well',
            x_expand: true,
            x_align: Clutter.ActorAlign.END,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._entryArea.add_child(this._defaultButtonWell);

        this._nextButton = new St.Button({
            style_class: 'login-dialog-button next-button',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: true,
            can_focus: false,
            icon_name: 'go-next-symbolic',
        });
        this._nextButton.connect('clicked', () => this._activateNext());
        this._nextButton.add_style_pseudo_class('default');
        this._defaultButtonWell.add_child(this._nextButton);

        this._spinner = new Animation.Spinner(DEFAULT_BUTTON_WELL_ICON_SIZE);
        this._defaultButtonWell.add_child(this._spinner);

        this.setActorInDefaultButtonWell(this._nextButton);
    }

    _updateCancelButton() {
        if (this._mode === AuthPromptMode.UNLOCK_OR_LOG_IN)
            return;

        const cancelVisible = this._promptStep > 1;
        this.cancelButton.visible = cancelVisible;
        this.cancelButton.reactive = cancelVisible;
        this.cancelButton.can_focus = cancelVisible;
    }

    showTimedLoginIndicator(time) {
        const hold = new Batch.Hold();

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

    _activateNext() {
        if (!this._entry.reactive)
            return;

        this.verificationStatus = AuthPromptStatus.VERIFICATION_IN_PROGRESS;
        this.updateSensitivity(false);

        if (this._entry === this._passwordEntry)
            this.startSpinning();

        if (this._queryingService)
            this._userVerifier.answerQuery(this._queryingService, this._entry.text);
        else
            this._preemptiveAnswer = this._entry.text;

        this.emit('next');
    }

    _updateEntry(secret) {
        let newEntry, inactiveEntry;

        if (secret && this._entry !== this._passwordEntry) {
            newEntry = this._passwordEntry;
            inactiveEntry = this._textEntry;
        } else if (!secret && this._entry !== this._textEntry) {
            newEntry = this._textEntry;
            inactiveEntry = this._passwordEntry;
        }

        if (newEntry) {
            this._entryArea.replace_child(this._entry, newEntry);
            this._entry = newEntry;
            this._inactiveEntry = inactiveEntry;

            const {text, cursorPosition, selectionBound} = inactiveEntry.clutterText;
            this._entry.clutterText.set({text, cursorPosition, selectionBound});
        }

        this._capsLockWarningLabel.visible = secret;
    }

    _onAskQuestion(verifier, serviceName, question, secret) {
        if (this._queryingService)
            this.clear();

        this._queryingService = serviceName;
        this._promptStep++;
        this._updateCancelButton();

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

        this.emit('prompted');
    }

    _onShowChoiceList(userVerifier, serviceName, promptMessage, choiceList) {
        if (this._queryingService)
            this.clear();

        this._queryingService = serviceName;
        this._promptStep++;
        this._updateCancelButton();

        if (this._preemptiveAnswer)
            this._preemptiveAnswer = null;

        this.setChoiceList(promptMessage, choiceList);
        this.updateSensitivity(true);
        this.emit('prompted');
    }

    _onCredentialManagerAuthenticated() {
        if (this.verificationStatus !== AuthPromptStatus.VERIFICATION_SUCCEEDED)
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
        if (this._userVerifier.serviceIsDefault(Const.SMARTCARD_SERVICE_NAME) &&
            (this.verificationStatus === AuthPromptStatus.VERIFYING ||
             this.verificationStatus === AuthPromptStatus.VERIFICATION_IN_PROGRESS) &&
            this.smartcardDetected)
            return;

        if (this.verificationStatus !== AuthPromptStatus.VERIFICATION_SUCCEEDED)
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

        // If we're showing a message and no auth widget is currently visible,
        // show the entry area to allow getting a preemptive answer
        if (message &&
            !this._entryArea.visible &&
            !this._authList.visible)
            this._fadeInElement(this._entryArea);

        this.emit('prompted');
    }

    _onVerificationFailed(userVerifier, serviceName, canRetry) {
        const wasQueryingService = this._queryingService === serviceName;

        if (wasQueryingService)
            this._queryingService = null;

        this.updateSensitivity(false);
        this.stopSpinning();

        if (!canRetry)
            this.verificationStatus = AuthPromptStatus.VERIFICATION_FAILED;

        if (wasQueryingService)
            wiggle(this._entryArea);
    }

    _onVerificationComplete() {
        this.stopSpinning(true);
        this.verificationStatus = AuthPromptStatus.VERIFICATION_SUCCEEDED;

        this._mainBox.reactive = false;
        this._mainBox.can_focus = false;
        this._mainBox.ease({
            opacity: 0,
            duration: MESSAGE_FADE_OUT_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });

        this.emit('verification-complete');
    }

    _onReset(_, resetParams) {
        if (this.verificationStatus === AuthPromptStatus.VERIFICATION_SUCCEEDED)
            return;

        this.reset(resetParams);
    }

    setActorInDefaultButtonWell(actor, animate) {
        if (!this._defaultButtonWellActor && !actor)
            return;

        const oldActor = this._defaultButtonWellActor;
        const wasSpinner = oldActor === this._spinner;

        if (oldActor)
            oldActor.remove_all_transitions();

        if (actor === this._spinner)
            this._spinner.play();

        if (!animate) {
            if (oldActor) {
                oldActor.opacity = 0;
                if (wasSpinner)
                    this._spinner.stop();
            }
            if (actor)
                actor.opacity = 255;

            this._defaultButtonWellActor = actor;
            return;
        }

        if (oldActor) {
            oldActor.opacity = 255;
            oldActor.ease({
                opacity: 0,
                duration: DEFAULT_BUTTON_WELL_ANIMATION_TIME,
                mode: Clutter.AnimationMode.LINEAR,
                onComplete: () => {
                    if (wasSpinner)
                        this._spinner.stop();
                },
            });
        }
        if (actor) {
            actor.opacity = 0;
            actor.ease({
                opacity: 255,
                duration: DEFAULT_BUTTON_WELL_ANIMATION_TIME,
                delay: oldActor ? DEFAULT_BUTTON_WELL_ANIMATION_TIME : 0,
                mode: Clutter.AnimationMode.LINEAR,
            });
        }

        this._defaultButtonWellActor = actor;
    }

    startSpinning() {
        this.emit('loading', true);
        this.setActorInDefaultButtonWell(this._spinner, true);
    }

    stopSpinning(animate) {
        this.emit('loading', false);
        this.setActorInDefaultButtonWell(this._nextButton, animate);
    }

    clear() {
        this._entryArea.hide();
        this._entry.text = '';
        this._inactiveEntry.text = '';
        this.stopSpinning();
        this._authListTitle.child.text = '';
        this._authList.clear();
        this._authList.hide();

        this._mainBox.opacity = 255;
        this._mainBox.reactive = true;
        this._mainBox.can_focus = true;
    }

    setQuestion(question) {
        this._entry.hint_text = question;

        this._authList.hide();

        this._fadeInElement(this._entryArea);
    }

    _fadeInElement(element) {
        element.set({
            opacity: 0,
            visible: true,
        });
        this.updateSensitivity(false);
        element.ease({
            opacity: 255,
            duration: MESSAGE_FADE_OUT_ANIMATION_TIME,
            transition: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => {
                this.updateSensitivity(true);
                this._endPreemptiveInput(element);
            },
        });
    }

    _endPreemptiveInput(element) {
        if (element === this._entryArea && this._pendingActivate)
            this._activateNext();
        this._preemptiveInput = false;
        this._pendingActivate = false;
    }

    setChoiceList(promptMessage, choiceList) {
        this._authList.clear();
        this._authListTitle.child.text = promptMessage;
        for (const key in choiceList) {
            const content = choiceList[key];
            this._authList.addItem(key, content);
        }

        this._entryArea.hide();
        if (this._message.text === '')
            this._message.hide();
        this._fadeInElement(this._authList);
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
        if (this._message.opacity === 0)
            return;
        this._message.remove_all_transitions();
        this._message.ease({
            opacity: 0,
            duration: MESSAGE_FADE_OUT_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        });
    }

    setMessage(message, type, wiggleParameters = {duration: 0}) {
        if (type === GdmUtil.MessageType.ERROR)
            this._message.add_style_class_name('login-dialog-message-warning');
        else
            this._message.remove_style_class_name('login-dialog-message-warning');

        if (type === GdmUtil.MessageType.HINT)
            this._message.add_style_class_name('login-dialog-message-hint');
        else
            this._message.remove_style_class_name('login-dialog-message-hint');

        this._message.show();
        if (message) {
            this._message.remove_all_transitions();
            this._message.text = message;
            this._message.opacity = 255;
            this.get_accessible().emit('notification', message, Atk.Live.ASSERTIVE);
        } else {
            this._message.opacity = 0;
        }

        wiggle(this._message, wiggleParameters);
    }

    updateSensitivity(sensitive) {
        let authWidget;

        if (this._authList.visible)
            authWidget = this._authList;
        else
            authWidget = this._entry;

        if (authWidget.reactive === sensitive)
            return;

        if (authWidget === this._entry)
            this._nextButton.reactive = sensitive;

        authWidget.reactive = sensitive;

        if (sensitive) {
            authWidget.grab_key_focus();
        } else {
            this.grab_key_focus();

            if (authWidget === this._passwordEntry)
                authWidget.password_visible = false;
        }
    }

    vfunc_hide() {
        this.stopSpinning();
        super.vfunc_hide();
        this._message.opacity = 0;

        this.setUser(null);

        this.updateSensitivity(true);
        this._entry.set_text('');
    }

    setUser(user) {
        const oldChild = this._userWell.get_child();
        if (oldChild)
            oldChild.destroy();

        const userWidget = new UserWidget.UserWidget(user, Clutter.Orientation.VERTICAL);
        this._userWell.set_child(userWidget);

        if (!user)
            this._updateEntry(false);
    }

    selectMechanism(mechanism) {
        const invalidStatus = [
            AuthPromptStatus.VERIFICATION_SUCCEEDED,
            AuthPromptStatus.VERIFICATION_IN_PROGRESS,
        ];
        if (invalidStatus.includes(this.verificationStatus))
            return false;

        return true;
    }

    reset(params) {
        let {beginRequestType, reuseEntryText, softReset} = Params.parse(params, {
            beginRequestType: null,
            reuseEntryText: false,
            softReset: false,
        });

        const oldStatus = this.verificationStatus;
        this.verificationStatus = AuthPromptStatus.NOT_VERIFYING;
        if (oldStatus !== AuthPromptStatus.VERIFICATION_IN_PROGRESS)
            this._preemptiveAnswer = null;
        this._promptStep = 0;
        this._updateCancelButton();

        const oldEntryText = this._textEntry.text;
        const oldPasswordText = this._passwordEntry.text;

        if (this._userVerifier)
            this._userVerifier.cancel();

        this._queryingService = null;
        this.clear();
        this._message.opacity = 0;
        this.setUser(null);
        this._updateEntry(true);
        this.stopSpinning();

        if (reuseEntryText || this._preemptiveInput) {
            this._textEntry.text = oldEntryText;
            this._passwordEntry.text = oldPasswordText;
        }

        if (oldStatus === AuthPromptStatus.VERIFICATION_FAILED)
            this.emit('failed');
        else if (oldStatus === AuthPromptStatus.VERIFICATION_CANCELLED)
            this.emit('cancelled');

        if (beginRequestType === null) {
            if (this._mode === AuthPromptMode.UNLOCK_ONLY) {
                // The user is constant at the unlock screen, so it will immediately
                // respond to the request with the username
                if (oldStatus === AuthPromptStatus.VERIFICATION_CANCELLED)
                    return;
                beginRequestType = BeginRequestType.PROVIDE_USERNAME;
            } else if (!this._userVerifier.needsUsername()) {
                // We don't need to know the username if the user preempted the login screen
                // with a smartcard or with preauthenticated oVirt credentials
                beginRequestType = BeginRequestType.DONT_PROVIDE_USERNAME;
            } else if (oldStatus === AuthPromptStatus.VERIFICATION_IN_PROGRESS ||
                softReset) {
                // We're going back to retry with current user
                beginRequestType = BeginRequestType.REUSE_USERNAME;
            } else {
                // In all other cases, we should get the username up front.
                beginRequestType = BeginRequestType.PROVIDE_USERNAME;
            }
        }

        this.emit('reset', beginRequestType);
    }

    startPreemptiveInput(unichar) {
        this._preemptiveInput = true;
        this._entry.clutter_text.insert_text(unichar, this._entry.clutter_text.cursor_position);
        this.grab_key_focus();
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

        const signalId = this._userVerifier.connect('no-more-messages', () => {
            this._userVerifier.disconnect(signalId);
            this._userVerifier.clear();
            onComplete();
        });
    }

    cancel() {
        if (this.verificationStatus === AuthPromptStatus.VERIFICATION_SUCCEEDED)
            return;

        // If we're in a multi-step flow (step > 1), go back to step 1 instead of full reset
        if (this._promptStep > 1) {
            this.reset({softReset: true});
            return;
        }

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
