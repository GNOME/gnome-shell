// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Signals = imports.signals;
const St = imports.gi.St;

const Animation = imports.ui.animation;
const Batch = imports.gdm.batch;
const Fprint = imports.gdm.fingerprint;
const Main = imports.ui.main;
const Params = imports.misc.params;
const Tweener = imports.ui.tweener;
const UserWidget = imports.ui.userWidget;

const PASSWORD_SERVICE_NAME = 'gdm-password';
const FINGERPRINT_SERVICE_NAME = 'gdm-fingerprint';
const FADE_ANIMATION_TIME = 0.16;
const CLONE_FADE_ANIMATION_TIME = 0.25;

const LOGIN_SCREEN_SCHEMA = 'org.gnome.login-screen';
const FINGERPRINT_AUTHENTICATION_KEY = 'enable-fingerprint-authentication';
const BANNER_MESSAGE_KEY = 'banner-message-enable';
const BANNER_MESSAGE_TEXT_KEY = 'banner-message-text';
const ALLOWED_FAILURES_KEY = 'allowed-failures';

const LOGO_KEY = 'logo';
const DISABLE_USER_LIST_KEY = 'disable-user-list';

// Give user 16ms to read each character of a PAM message
const USER_READ_TIME = 16

const DEFAULT_BUTTON_WELL_ICON_SIZE = 24;
const DEFAULT_BUTTON_WELL_ANIMATION_DELAY = 1.0;
const DEFAULT_BUTTON_WELL_ANIMATION_TIME = 0.3;

function fadeInActor(actor) {
    if (actor.opacity == 255 && actor.visible)
        return null;

    let hold = new Batch.Hold();
    actor.show();
    let [minHeight, naturalHeight] = actor.get_preferred_height(-1);

    actor.opacity = 0;
    actor.set_height(0);
    Tweener.addTween(actor,
                     { opacity: 255,
                       height: naturalHeight,
                       time: FADE_ANIMATION_TIME,
                       transition: 'easeOutQuad',
                       onComplete: function() {
                           this.set_height(-1);
                           hold.release();
                       },
                     });

    return hold;
}

function fadeOutActor(actor) {
    if (!actor.visible || actor.opacity == 0) {
        actor.opacity = 0;
        actor.hide();
        return null;
    }

    let hold = new Batch.Hold();
    Tweener.addTween(actor,
                     { opacity: 0,
                       height: 0,
                       time: FADE_ANIMATION_TIME,
                       transition: 'easeOutQuad',
                       onComplete: function() {
                           this.hide();
                           this.set_height(-1);
                           hold.release();
                       },
                     });
    return hold;
}

function cloneAndFadeOutActor(actor) {
    // Immediately hide actor so its sibling can have its space
    // and position, but leave a non-reactive clone on-screen,
    // so from the user's point of view it smoothly fades away
    // and reveals its sibling.
    actor.hide();

    let clone = new Clutter.Clone({ source: actor,
                                    reactive: false });

    Main.uiGroup.add_child(clone);

    let [x, y] = actor.get_transformed_position();
    clone.set_position(x, y);

    let hold = new Batch.Hold();
    Tweener.addTween(clone,
                     { opacity: 0,
                       time: CLONE_FADE_ANIMATION_TIME,
                       transition: 'easeOutQuad',
                       onComplete: function() {
                           clone.destroy();
                           hold.release();
                       }
                     });
    return hold;
}

const ShellUserVerifier = new Lang.Class({
    Name: 'ShellUserVerifier',

    _init: function(client, params) {
        params = Params.parse(params, { reauthenticationOnly: false });
        this._reauthOnly = params.reauthenticationOnly;

        this._client = client;

        this._settings = new Gio.Settings({ schema: LOGIN_SCREEN_SCHEMA });

        this._fprintManager = new Fprint.FprintManager();
        this._messageQueue = [];
        this._messageQueueTimeoutId = 0;
        this.hasPendingMessages = false;

        this._failCounter = 0;
    },

    begin: function(userName, hold) {
        this._cancellable = new Gio.Cancellable();
        this._hold = hold;
        this._userName = userName;

        this._checkForFingerprintReader();

        if (userName) {
            // If possible, reauthenticate an already running session,
            // so any session specific credentials get updated appropriately
            this._client.open_reauthentication_channel(userName, this._cancellable,
                                                       Lang.bind(this, this._reauthenticationChannelOpened));
        } else {
            this._client.get_user_verifier(this._cancellable, Lang.bind(this, this._userVerifierGot));
        }
    },

    cancel: function() {
        if (this._cancellable)
            this._cancellable.cancel();

        if (this._userVerifier)
            this._userVerifier.call_cancel_sync(null);
    },

    clear: function() {
        if (this._cancellable) {
            this._cancellable.cancel();
            this._cancellable = null;
        }

        if (this._userVerifier) {
            this._userVerifier.run_dispose();
            this._userVerifier = null;
        }

        this._clearMessageQueue();
    },

    answerQuery: function(serviceName, answer) {
        if (!this._userVerifier.hasPendingMessages) {
            this._clearMessageQueue();
            this._userVerifier.call_answer_query(serviceName, answer, this._cancellable, null);
        } else {
            let signalId = this._userVerifier.connect('no-more-messages',
                                                      Lang.bind(this, function() {
                                                          this._userVerifier.disconnect(signalId);
                                                          this._userVerifier.call_answer_query(serviceName, answer, this._cancellable, null);
                                                      }));
        }
    },

    _getIntervalForMessage: function(message) {
        // We probably could be smarter here
        return message.length * USER_READ_TIME;
    },

    finishMessageQueue: function() {
        if (!this.hasPendingMessages)
            return;

        this._messageQueue = [];

        this.hasPendingMessages = false;
        this.emit('no-more-messages');
    },

    _queueMessageTimeout: function() {
        if (this._messageQueue.length == 0) {
            this.finishMessageQueue();
            return;
        }

        if (this._messageQueueTimeoutId != 0)
            return;

        let message = this._messageQueue.shift();
        this.emit('show-message', message.text, message.iconName);

        this._messageQueueTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                                                       message.interval,
                                                       Lang.bind(this, function() {
                                                           this._messageQueueTimeoutId = 0;
                                                           this._queueMessageTimeout();
                                                       }));
    },

    _queueMessage: function(message, iconName) {
        let interval = this._getIntervalForMessage(message);

        this.hasPendingMessages = true;
        this._messageQueue.push({ text: message, interval: interval, iconName: iconName });
        this._queueMessageTimeout();
    },

    _clearMessageQueue: function() {
        this.finishMessageQueue();

        if (this._messageQueueTimeoutId != 0) {
            GLib.source_remove(this._messageQueueTimeoutId);
            this._messageQueueTimeoutId = 0;
        }
        this.emit('show-message', null, null);
    },

    _checkForFingerprintReader: function() {
        this._haveFingerprintReader = false;

        if (!this._settings.get_boolean(FINGERPRINT_AUTHENTICATION_KEY))
            return;

        this._fprintManager.GetDefaultDeviceRemote(Gio.DBusCallFlags.NONE, this._cancellable, Lang.bind(this,
            function(device, error) {
                if (!error && device)
                    this._haveFingerprintReader = true;
            }));
    },

    _reportInitError: function(where, error) {
        logError(error, where);
        this._hold.release();

        this._queueMessage(_("Authentication error"), 'login-dialog-message-warning');
        this._verificationFailed(false);
    },

    _reauthenticationChannelOpened: function(client, result) {
        try {
            this._userVerifier = client.open_reauthentication_channel_finish(result);
        } catch(e if e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
            return;
        } catch(e if e.matches(Gio.DBusError, Gio.DBusError.ACCESS_DENIED) &&
                !this._reauthOnly) {
            // Gdm emits org.freedesktop.DBus.Error.AccessDenied when there is
            // no session to reauthenticate. Fall back to performing verification
            // from this login session
            client.get_user_verifier(this._cancellable, Lang.bind(this, this._userVerifierGot));
            return;
        } catch(e) {
            this._reportInitError('Failed to open reauthentication channel', e);
            return;
        }

        this._connectSignals();
        this._beginVerification();
        this._hold.release();
    },

    _userVerifierGot: function(client, result) {
        try {
            this._userVerifier = client.get_user_verifier_finish(result);
        } catch(e if e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
            return;
        } catch(e) {
            this._reportInitError('Failed to obtain user verifier', e);
            return;
        }

        this._connectSignals();
        this._beginVerification();
        this._hold.release();
    },

    _connectSignals: function() {
        this._userVerifier.connect('info', Lang.bind(this, this._onInfo));
        this._userVerifier.connect('problem', Lang.bind(this, this._onProblem));
        this._userVerifier.connect('info-query', Lang.bind(this, this._onInfoQuery));
        this._userVerifier.connect('secret-info-query', Lang.bind(this, this._onSecretInfoQuery));
        this._userVerifier.connect('conversation-stopped', Lang.bind(this, this._onConversationStopped));
        this._userVerifier.connect('reset', Lang.bind(this, this._onReset));
        this._userVerifier.connect('verification-complete', Lang.bind(this, this._onVerificationComplete));
    },

    _beginVerification: function() {
        this._hold.acquire();

        if (this._userName) {
            this._userVerifier.call_begin_verification_for_user(PASSWORD_SERVICE_NAME,
                                                                this._userName,
                                                                this._cancellable,
                                                                Lang.bind(this, function(obj, result) {
                try {
                    obj.call_begin_verification_for_user_finish(result);
                } catch(e if e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
                    return;
                } catch(e) {
                    this._reportInitError('Failed to start verification for user', e);
                    return;
                }

                this._hold.release();
            }));

            if (this._haveFingerprintReader) {
                this._hold.acquire();

                this._userVerifier.call_begin_verification_for_user(FINGERPRINT_SERVICE_NAME,
                                                                    this._userName,
                                                                    this._cancellable,
                                                                    Lang.bind(this, function(obj, result) {
                    try {
                        obj.call_begin_verification_for_user_finish(result);
                    } catch(e if e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
                        return;
                    } catch(e) {
                        this._reportInitError('Failed to start fingerprint verification for user', e);
                        return;
                    }

                    this._hold.release();
                }));
            }
        } else {
            this._userVerifier.call_begin_verification(PASSWORD_SERVICE_NAME,
                                                       this._cancellable,
                                                       Lang.bind(this, function(obj, result) {
                try {
                    obj.call_begin_verification_finish(result);
                } catch(e if e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
                    return;
                } catch(e) {
                    this._reportInitError('Failed to start verification', e);
                    return;
                }

                this._hold.release();
            }));
        }
    },

    _onInfo: function(client, serviceName, info) {
        // We don't display fingerprint messages, because they
        // have words like UPEK in them. Instead we use the messages
        // as a cue to display our own message.
        if (serviceName == FINGERPRINT_SERVICE_NAME &&
            this._haveFingerprintReader) {

            // Translators: this message is shown below the password entry field
            // to indicate the user can swipe their finger instead
            this.emit('show-login-hint', _("(or swipe finger)"));
        } else if (serviceName == PASSWORD_SERVICE_NAME) {
            this._queueMessage(info, 'login-dialog-message-info');
        }
    },

    _onProblem: function(client, serviceName, problem) {
        // we don't want to show auth failed messages to
        // users who haven't enrolled their fingerprint.
        if (serviceName != PASSWORD_SERVICE_NAME)
            return;
        this._queueMessage(problem, 'login-dialog-message-warning');
    },

    _onInfoQuery: function(client, serviceName, question) {
        // We only expect questions to come from the main auth service
        if (serviceName != PASSWORD_SERVICE_NAME)
            return;

        this.emit('ask-question', serviceName, question, '');
    },

    _onSecretInfoQuery: function(client, serviceName, secretQuestion) {
        // We only expect secret requests to come from the main auth service
        if (serviceName != PASSWORD_SERVICE_NAME)
            return;

        this.emit('ask-question', serviceName, secretQuestion, '\u25cf');
    },

    _onReset: function() {
        // Clear previous attempts to authenticate
        this._failCounter = 0;

        this.emit('reset');
    },

    _onVerificationComplete: function() {
        this.emit('verification-complete');
    },

    _cancelAndReset: function() {
        this.cancel();
        this._onReset();
    },

    _retry: function() {
        this.begin(this._userName, new Batch.Hold());
    },

    _verificationFailed: function(retry) {
        // For Not Listed / enterprise logins, immediately reset
        // the dialog
        // Otherwise, we allow ALLOWED_FAILURES attempts. After that, we
        // go back to the welcome screen.

        this._failCounter++;
        let canRetry = retry && this._userName &&
            this._failCounter < this._settings.get_int(ALLOWED_FAILURES_KEY);

        if (canRetry) {
            if (!this._userVerifier.hasPendingMessages) {
                this._retry();
            } else {
                let signalId = this._userVerifier.connect('no-more-messages',
                                                          Lang.bind(this, function() {
                                                              this._userVerifier.disconnect(signalId);
                                                              this._retry();
                                                          }));
            }
        } else {
            if (!this._userVerifier.hasPendingMessages) {
                this._cancelAndReset();
            } else {
                let signalId = this._userVerifier.connect('no-more-messages',
                                                          Lang.bind(this, function() {
                                                              this._userVerifier.disconnect(signalId);
                                                              this._cancelAndReset();
                                                          }));
            }
        }

        this.emit('verification-failed');
    },

    _onConversationStopped: function(client, serviceName) {
        // if the password service fails, then cancel everything.
        // But if, e.g., fingerprint fails, still give
        // password authentication a chance to succeed
        if (serviceName == PASSWORD_SERVICE_NAME) {
            this._verificationFailed(true);
        }

        this.emit('hide-login-hint');
    },
});
Signals.addSignalMethods(ShellUserVerifier.prototype);

const AuthPrompt = new Lang.Class({
    Name: 'AuthPrompt',

    _init: function(params) {
        params = Params.parse(params,
                              { style_class: 'login-dialog-prompt-layout',
                                vertical: true }, true);
        this.actor = new St.BoxLayout(params);
        this.actor.connect('button-press-event',
                           Lang.bind(this, function(actor, event) {
                               if (event.get_key_symbol() == Clutter.KEY_Escape) {
                                   this.emit('cancel');
                               }
                           }));

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
                         x_fill: true,
                         y_fill: true,
                         x_align: St.Align.START });
        this._entry = new St.Entry({ style_class: 'login-dialog-prompt-entry',
                                     can_focus: true });
        this._entryTextChangedId = 0;
        this._entryActivateId = 0;
        this.actor.add(this._entry,
                       { expand: true,
                         x_fill: true,
                         y_fill: false,
                         x_align: St.Align.START });

        this._entry.grab_key_focus();

        this._message = new St.Label({ opacity: 0 });
        this.actor.add(this._message, { x_fill: true });

        this._loginHint = new St.Label({ style_class: 'login-dialog-prompt-login-hint-message' });
        this.actor.add(this._loginHint);

        this._buttonBox = new St.BoxLayout({ style_class: 'login-dialog-button-box',
                                             vertical: false });
        this.actor.add(this._buttonBox,
                       { expand:  true,
                         x_align: St.Align.MIDDLE,
                         y_align: St.Align.END });
        this._cancelButton = null;
        this._nextButton = null;

        this._defaultButtonWell = new St.Widget();
        this._defaultButtonWellActor = null;

        let spinnerIcon = global.datadir + '/theme/process-working.svg';
        this._spinner = new Animation.AnimatedIcon(spinnerIcon, DEFAULT_BUTTON_WELL_ICON_SIZE);
        this._spinner.actor.opacity = 0;
        this._spinner.actor.show();
        this._defaultButtonWell.add_child(this._spinner.actor);
    },

    addActorToDefaultButtonWell: function(actor) {
        this._defaultButtonWell.add_child(actor);

        actor.add_constraint(new Clutter.AlignConstraint({ source: this._spinner.actor,
                                                           align_axis: Clutter.AlignAxis.BOTH,
                                                           factor: 0.5 }));
    },

    setActorInDefaultButtonWell: function(actor, immediately) {
        if (!this._defaultButtonWellActor &&
            !actor)
            return;

        let oldActor = this._defaultButtonWellActor;

        if (oldActor)
            Tweener.removeTweens(oldActor);

        let isWorkSpinner;
        if (actor == this._spinner.actor)
            isWorkSpinner = true;
        else
            isWorkSpinner = false;

        if (this._defaultButtonWellActor != actor && oldActor) {
            if (immediately)
                oldActor.opacity = 0;
            else
                Tweener.addTween(oldActor,
                                 { opacity: 0,
                                   time: DEFAULT_BUTTON_WELL_ANIMATION_TIME,
                                   delay: DEFAULT_BUTTON_WELL_ANIMATION_DELAY,
                                   transition: 'linear',
                                   onCompleteScope: this,
                                   onComplete: function() {
                                       if (isWorkSpinner) {
                                           if (this._spinner)
                                               this._spinner.stop();
                                       }
                                   }
                                 });
        }

        if (actor) {
            if (isWorkSpinner)
                this._spinner.play();

            if (immediately)
                actor.opacity = 255;
            else
                Tweener.addTween(actor,
                                 { opacity: 255,
                                   time: DEFAULT_BUTTON_WELL_ANIMATION_TIME,
                                   delay: DEFAULT_BUTTON_WELL_ANIMATION_DELAY,
                                   transition: 'linear' });
        }

        this._defaultButtonWellActor = actor;
    },

    startSpinning: function() {
        this.setActorInDefaultButtonWell(this._spinner.actor, true);
    },

    stopSpinning: function() {
        this.setActorInDefaultButtonWell(null, true);
    },

    clear: function() {
        this._entry.text = '';
    },

    setPasswordChar: function(passwordChar) {
        this._entry.clutter_text.set_password_char(passwordChar);
    },

    setQuestion: function(question) {
        if (!this._initialAnswer) {
            this.clear();
        } else if (this._initialAnswer['activate-id']) {
            this._entry.clutter_text.disconnect(this._initialAnswer['activate-id']);
            delete this._initialAnswer['activate-id'];
        }

        this._label.set_text(question);

        this._label.show();
        this._entry.show();

        this._loginHint.opacity = 0;
        this._loginHint.show();

        this._entry.grab_key_focus();
    },

    getAnswer: function() {
        let text;

        if (this._initialAnswer && this._initialAnswer['text']) {
            text = this._initialAnswer['text'];
            this._initialAnswer = null;
        } else {
            text = this._entry.get_text();
        }

        return text;
    },

    setMessage: function(message, styleClass) {
        if (message) {
            this._message.text = message;
            this._message.styleClass = styleClass;
            this._message.opacity = 255;
        } else {
            this._message.opacity = 0;
        }
    },

    resetButtons: function(cancelLabel, nextLabel) {
        if (this._initialAnswer && this._initialAnswer['text']) {
            this.emit('next');
            return;
        }

        this._buttonBox.visible = true;
        this._buttonBox.remove_all_children();

        if (cancelLabel) {
            this._cancelButton = new St.Button({ style_class: 'modal-dialog-button',
                                                 button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                                 reactive: true,
                                                 can_focus: true,
                                                 label: cancelLabel });
            this._cancelButton.connect('clicked',
                                       Lang.bind(this, function() {
                                           this.emit('cancel');
                                       }));
            this._buttonBox.add(this._cancelButton,
                                { expand: false,
                                  x_fill: false,
                                  y_fill: false,
                                  x_align: St.Align.START,
                                  y_align: St.Align.END });
        }

        this._buttonBox.add(this._defaultButtonWell,
                            { expand: true,
                              x_fill: false,
                              y_fill: false,
                              x_align: St.Align.END,
                              y_align: St.Align.MIDDLE });
        this._nextButton = new St.Button({ style_class: 'modal-dialog-button',
                                             button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
                                             reactive: true,
                                             can_focus: true,
                                             label: nextLabel });
        this._nextButton.connect('clicked',
                                 Lang.bind(this, function() {
                                     this.emit('next');
                                 }));
        this._nextButton.add_style_pseudo_class('default');
        this._buttonBox.add(this._nextButton,
                            { expand: false,
                              x_fill: false,
                              y_fill: false,
                              x_align: St.Align.END,
                              y_align: St.Align.END });

        this._updateNextButtonSensitivity(this._entry.text.length > 0);

        this._entryTextChangedId =
            this._entry.clutter_text.connect('text-changed',
                                             Lang.bind(this, function() {
                                                 this._updateNextButtonSensitivity(this._entry.text.length > 0);
                                             }));

        this._entryActivateId =
            this._entry.clutter_text.connect('activate', Lang.bind(this, function() {
                this.emit('next');
            }));
    },

    _updateNextButtonSensitivity: function(sensitive) {
        if (this._nextButton) {
            this._nextButton.reactive = sensitive;
            this._nextButton.can_focus = sensitive;
        }
    },

    updateSensitivity: function(sensitive) {
        this._updateNextButtonSensitivity(sensitive);
        this._entry.reactive = sensitive;
        this._entry.clutter_text.editable = sensitive;
    },

    hide: function() {
        if (this._entryTextChangedId > 0) {
            this._entry.clutter_text.disconnect(this._entryTextChangedId);
            this._entryTextChangedId = 0;
        }

        if (this._entryActivateId > 0) {
            this._entry.clutter_text.disconnect(this._entryActivateId);
            this._entryActivateId = 0;
        }

        this.setActorInDefaultButtonWell(null, true);
        this.actor.hide();
        this._loginHint.opacity = 0;

        this.setUser(null);

        this.updateSensitivity(true);
        this._entry.set_text('');

        this._buttonBox.remove_all_children();
        this._nextButton = null;
        this._cancelButton = null;
    },

    setUser: function(user) {
        if (user) {
            let userWidget = new UserWidget.UserWidget(user);
            this._userWell.set_child(userWidget.actor);
        } else {
            this._userWell.set_child(null);
        }
    },

    setHint: function(message) {
        if (message) {
            this._loginHint.set_text(message)
            this._loginHint.opacity = 255;
        } else {
            this._loginHint.opacity = 0;
            this._loginHint.set_text('');
        }
    },

    reset: function() {
        this._message.opacity = 0;
        this.setUser(null);
    },

    addCharacter: function(unichar) {
        if (!this._entry.visible)
            return;

        if (!this._initialAnswer)
            this._initialAnswer = {};

        this._entry.grab_key_focus();
        this._entry.clutter_text.insert_unichar(unichar);

        if (!this._initialAnswer['activate-id'])
            this._initialAnswer['activate-id'] =
                this._entry.clutter_text.connect('activate', Lang.bind(this, function() {
                                                     this._entry.clutter_text.disconnect(this._initialAnswer['activate-id']);
                                                     delete this._initialAnswer['activate-id'];

                                                     this._initialAnswer['text'] = this._entry.get_text();
                                                 }));
    }
});
Signals.addSignalMethods(AuthPrompt.prototype);
