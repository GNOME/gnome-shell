// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Signals = imports.signals;
const St = imports.gi.St;

const Batch = imports.gdm.batch;
const Fprint = imports.gdm.fingerprint;
const Main = imports.ui.main;
const Params = imports.misc.params;
const ShellEntry = imports.ui.shellEntry;
const SmartcardManager = imports.misc.smartcardManager;
const Tweener = imports.ui.tweener;

const PASSWORD_SERVICE_NAME = 'gdm-password';
const FINGERPRINT_SERVICE_NAME = 'gdm-fingerprint';
const SMARTCARD_SERVICE_NAME = 'gdm-smartcard';
const FADE_ANIMATION_TIME = 0.16;
const CLONE_FADE_ANIMATION_TIME = 0.25;

const LOGIN_SCREEN_SCHEMA = 'org.gnome.login-screen';
const PASSWORD_AUTHENTICATION_KEY = 'enable-password-authentication';
const FINGERPRINT_AUTHENTICATION_KEY = 'enable-fingerprint-authentication';
const SMARTCARD_AUTHENTICATION_KEY = 'enable-smartcard-authentication';
const BANNER_MESSAGE_KEY = 'banner-message-enable';
const BANNER_MESSAGE_TEXT_KEY = 'banner-message-text';
const ALLOWED_FAILURES_KEY = 'allowed-failures';

const LOGO_KEY = 'logo';
const DISABLE_USER_LIST_KEY = 'disable-user-list';

// Give user 16ms to read each character of a PAM message
const USER_READ_TIME = 16

const MessageType = {
    NONE: 0,
    ERROR: 1,
    INFO: 2,
    HINT: 3
};

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
        this._settings.connect('changed',
                               Lang.bind(this, this._updateDefaultService));
        this._updateDefaultService();

        this._fprintManager = new Fprint.FprintManager();
        this._smartcardManager = SmartcardManager.getSmartcardManager();

        // We check for smartcards right away, since an inserted smartcard
        // at startup should result in immediately initiating authentication.
        // This is different than fingeprint readers, where we only check them
        // after a user has been picked.
        this._checkForSmartcard();

        this._smartcardManager.connect('smartcard-inserted',
                                       Lang.bind(this, this._checkForSmartcard));
        this._smartcardManager.connect('smartcard-removed',
                                       Lang.bind(this, this._checkForSmartcard));

        this._messageQueue = [];
        this._messageQueueTimeoutId = 0;
        this.hasPendingMessages = false;
        this.reauthenticating = false;

        this._failCounter = 0;
    },

    begin: function(userName, hold) {
        this._cancellable = new Gio.Cancellable();
        this._hold = hold;
        this._userName = userName;
        this.reauthenticating = false;

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

        if (this._userVerifier) {
            this._userVerifier.call_cancel_sync(null);
            this.clear();
        }
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
        if (!this.hasPendingMessages) {
            this._userVerifier.call_answer_query(serviceName, answer, this._cancellable, null);
        } else {
            let signalId = this.connect('no-more-messages',
                                        Lang.bind(this, function() {
                                            this.disconnect(signalId);
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

        this.emit('show-message', message.text, message.type);

        this._messageQueueTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                                                       message.interval,
                                                       Lang.bind(this, function() {
                                                           this._messageQueueTimeoutId = 0;
                                                           this._queueMessageTimeout();
                                                       }));
    },

    _queueMessage: function(message, messageType) {
        let interval = this._getIntervalForMessage(message);

        this.hasPendingMessages = true;
        this._messageQueue.push({ text: message, type: messageType, interval: interval });
        this._queueMessageTimeout();
    },

    _clearMessageQueue: function() {
        this.finishMessageQueue();

        if (this._messageQueueTimeoutId != 0) {
            GLib.source_remove(this._messageQueueTimeoutId);
            this._messageQueueTimeoutId = 0;
        }
        this.emit('show-message', null, MessageType.NONE);
    },

    _checkForFingerprintReader: function() {
        this._haveFingerprintReader = false;

        if (!this._settings.get_boolean(FINGERPRINT_AUTHENTICATION_KEY)) {
            this._updateDefaultService();
            return;
        }

        this._fprintManager.GetDefaultDeviceRemote(Gio.DBusCallFlags.NONE, this._cancellable, Lang.bind(this,
            function(device, error) {
                if (!error && device)
                    this._haveFingerprintReader = true;
                    this._updateDefaultService();
            }));
    },

    _checkForSmartcard: function() {
        let smartcardDetected;

        if (!this._settings.get_boolean(SMARTCARD_AUTHENTICATION_KEY))
            smartcardDetected = false;
        else if (this.reauthenticating)
            smartcardDetected = this._smartcardManager.hasInsertedLoginToken();
        else
            smartcardDetected = this._smartcardManager.hasInsertedTokens();

        if (smartcardDetected != this.smartcardDetected) {
            this.smartcardDetected = smartcardDetected;

            if (this.smartcardDetected)
                this._preemptingService = SMARTCARD_SERVICE_NAME;
            else if (this._preemptingService == SMARTCARD_SERVICE_NAME)
                this._preemptingService = null;

            this.emit('smartcard-status-changed');
        }
    },

    _reportInitError: function(where, error) {
        logError(error, where);
        this._hold.release();

        this._queueMessage(_("Authentication error"), MessageType.ERROR);
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

        this.reauthenticating = true;
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

    _getForegroundService: function() {
        if (this._preemptingService)
            return this._preemptingService;

        return this._defaultService;
    },

    serviceIsForeground: function(serviceName) {
        return serviceName == this._getForegroundService();
    },

    serviceIsDefault: function(serviceName) {
        return serviceName == this._defaultService;
    },

    _updateDefaultService: function() {
        if (this._settings.get_boolean(PASSWORD_AUTHENTICATION_KEY))
            this._defaultService = PASSWORD_SERVICE_NAME;
        else if (this.smartcardDetected)
            this._defaultService = SMARTCARD_SERVICE_NAME;
        else if (this._haveFingerprintReader)
            this._defaultService = FINGERPRINT_SERVICE_NAME;
    },

    _startService: function(serviceName) {
        this._hold.acquire();
        this._userVerifier.call_begin_verification_for_user(serviceName,
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
    },

    _beginVerification: function() {
        this._startService(this._getForegroundService());

        if (this._userName && this._haveFingerprintReader && !this.serviceIsForeground(FINGERPRINT_SERVICE_NAME))
            this._startService(FINGERPRINT_SERVICE_NAME);
    },

    _onInfo: function(client, serviceName, info) {
        if (this.serviceIsForeground(serviceName)) {
            this._queueMessage(info, MessageType.INFO);
        } else if (serviceName == FINGERPRINT_SERVICE_NAME &&
            this._haveFingerprintReader) {
            // We don't show fingerprint messages directly since it's
            // not the main auth service. Instead we use the messages
            // as a cue to display our own message.

            // Translators: this message is shown below the password entry field
            // to indicate the user can swipe their finger instead
            this._queueMessage(_("(or swipe finger)"), MessageType.HINT);
        }
    },

    _onProblem: function(client, serviceName, problem) {
        if (!this.serviceIsForeground(serviceName))
            return;

        this._queueMessage(problem, MessageType.ERROR);
    },

    _onInfoQuery: function(client, serviceName, question) {
        if (!this.serviceIsForeground(serviceName))
            return;

        this.emit('ask-question', serviceName, question, '');
    },

    _onSecretInfoQuery: function(client, serviceName, secretQuestion) {
        if (!this.serviceIsForeground(serviceName))
            return;

        this.emit('ask-question', serviceName, secretQuestion, '\u25cf');
    },

    _onReset: function() {
        // Clear previous attempts to authenticate
        this._failCounter = 0;
        this._updateDefaultService();

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
            if (!this.hasPendingMessages) {
                this._retry();
            } else {
                let signalId = this.connect('no-more-messages',
                                            Lang.bind(this, function() {
                                                this.disconnect(signalId);
                                                this._retry();
                                            }));
            }
        } else {
            if (!this.hasPendingMessages) {
                this._cancelAndReset();
            } else {
                let signalId = this.connect('no-more-messages',
                                            Lang.bind(this, function() {
                                                this.disconnect(signalId);
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
        if (this.serviceIsForeground(serviceName)) {
            this._verificationFailed(true);
        }
    },
});
Signals.addSignalMethods(ShellUserVerifier.prototype);
