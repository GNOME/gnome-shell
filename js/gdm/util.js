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
const OVirt = imports.gdm.oVirt;
const Main = imports.ui.main;
const Params = imports.misc.params;
const ShellEntry = imports.ui.shellEntry;
const SmartcardManager = imports.misc.smartcardManager;
const Tweener = imports.ui.tweener;

var PASSWORD_SERVICE_NAME = 'gdm-password';
var FINGERPRINT_SERVICE_NAME = 'gdm-fingerprint';
var SMARTCARD_SERVICE_NAME = 'gdm-smartcard';
var OVIRT_SERVICE_NAME = 'gdm-ovirtcred';
var FADE_ANIMATION_TIME = 0.16;
var CLONE_FADE_ANIMATION_TIME = 0.25;

var LOGIN_SCREEN_SCHEMA = 'org.gnome.login-screen';
var PASSWORD_AUTHENTICATION_KEY = 'enable-password-authentication';
var FINGERPRINT_AUTHENTICATION_KEY = 'enable-fingerprint-authentication';
var SMARTCARD_AUTHENTICATION_KEY = 'enable-smartcard-authentication';
var BANNER_MESSAGE_KEY = 'banner-message-enable';
var BANNER_MESSAGE_TEXT_KEY = 'banner-message-text';
var ALLOWED_FAILURES_KEY = 'allowed-failures';

var LOGO_KEY = 'logo';
var DISABLE_USER_LIST_KEY = 'disable-user-list';

// Give user 48ms to read each character of a PAM message
var USER_READ_TIME = 48

var MessageType = {
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
                       onComplete() {
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
                       onComplete() {
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
                       onComplete() {
                           clone.destroy();
                           hold.release();
                       }
                     });
    return hold;
}

var ShellUserVerifier = new Lang.Class({
    Name: 'ShellUserVerifier',

    _init(client, params) {
        params = Params.parse(params, { reauthenticationOnly: false });
        this._reauthOnly = params.reauthenticationOnly;

        this._client = client;

        this._defaultService = null;
        this._preemptingService = null;

        this._settings = new Gio.Settings({ schema_id: LOGIN_SCREEN_SCHEMA });
        this._settings.connect('changed',
                               this._updateDefaultService.bind(this));
        this._updateDefaultService();

        this._fprintManager = Fprint.FprintManager();
        this._smartcardManager = SmartcardManager.getSmartcardManager();

        // We check for smartcards right away, since an inserted smartcard
        // at startup should result in immediately initiating authentication.
        // This is different than fingerprint readers, where we only check them
        // after a user has been picked.
        this.smartcardDetected = false;
        this._checkForSmartcard();

        this._smartcardInsertedId = this._smartcardManager.connect('smartcard-inserted',
                                                                   this._checkForSmartcard.bind(this));
        this._smartcardRemovedId = this._smartcardManager.connect('smartcard-removed',
                                                                  this._checkForSmartcard.bind(this));

        this._messageQueue = [];
        this._messageQueueTimeoutId = 0;
        this.hasPendingMessages = false;
        this.reauthenticating = false;

        this._failCounter = 0;

        this._oVirtCredentialsManager = OVirt.getOVirtCredentialsManager();

        if (this._oVirtCredentialsManager.hasToken())
            this._oVirtUserAuthenticated(this._oVirtCredentialsManager.getToken());

        this._oVirtUserAuthenticatedId = this._oVirtCredentialsManager.connect('user-authenticated',
                                                                               this._oVirtUserAuthenticated.bind(this));
    },

    begin(userName, hold) {
        this._cancellable = new Gio.Cancellable();
        this._hold = hold;
        this._userName = userName;
        this.reauthenticating = false;

        this._checkForFingerprintReader();

        if (userName) {
            // If possible, reauthenticate an already running session,
            // so any session specific credentials get updated appropriately
            this._client.open_reauthentication_channel(userName, this._cancellable,
                                                       this._reauthenticationChannelOpened.bind(this));
        } else {
            this._client.get_user_verifier(this._cancellable, this._userVerifierGot.bind(this));
        }
    },

    cancel() {
        if (this._cancellable)
            this._cancellable.cancel();

        if (this._userVerifier) {
            this._userVerifier.call_cancel_sync(null);
            this.clear();
        }
    },

    _clearUserVerifier() {
        if (this._userVerifier) {
            this._userVerifier.run_dispose();
            this._userVerifier = null;
        }
    },

    clear() {
        if (this._cancellable) {
            this._cancellable.cancel();
            this._cancellable = null;
        }

        this._clearUserVerifier();
        this._clearMessageQueue();
    },

    destroy() {
        this.clear();

        this._settings.run_dispose();
        this._settings = null;

        this._smartcardManager.disconnect(this._smartcardInsertedId);
        this._smartcardManager.disconnect(this._smartcardRemovedId);
        this._smartcardManager = null;

        this._oVirtCredentialsManager.disconnect(this._oVirtUserAuthenticatedId);
        this._oVirtCredentialsManager = null;
    },

    answerQuery(serviceName, answer) {
        if (!this.hasPendingMessages) {
            this._userVerifier.call_answer_query(serviceName, answer, this._cancellable, null);
        } else {
            let signalId = this.connect('no-more-messages', () => {
                this.disconnect(signalId);
                this._userVerifier.call_answer_query(serviceName, answer, this._cancellable, null);
            });
        }
    },

    _getIntervalForMessage(message) {
        // We probably could be smarter here
        return message.length * USER_READ_TIME;
    },

    finishMessageQueue() {
        if (!this.hasPendingMessages)
            return;

        this._messageQueue = [];

        this.hasPendingMessages = false;
        this.emit('no-more-messages');
    },

    _queueMessageTimeout() {
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
                                                       () => {
                                                           this._messageQueueTimeoutId = 0;
                                                           this._queueMessageTimeout();
                                                           return GLib.SOURCE_REMOVE;
                                                       });
        GLib.Source.set_name_by_id(this._messageQueueTimeoutId, '[gnome-shell] this._queueMessageTimeout');
    },

    _queueMessage(message, messageType) {
        let interval = this._getIntervalForMessage(message);

        this.hasPendingMessages = true;
        this._messageQueue.push({ text: message, type: messageType, interval: interval });
        this._queueMessageTimeout();
    },

    _clearMessageQueue() {
        this.finishMessageQueue();

        if (this._messageQueueTimeoutId != 0) {
            GLib.source_remove(this._messageQueueTimeoutId);
            this._messageQueueTimeoutId = 0;
        }
        this.emit('show-message', null, MessageType.NONE);
    },

    _checkForFingerprintReader() {
        this._haveFingerprintReader = false;

        if (!this._settings.get_boolean(FINGERPRINT_AUTHENTICATION_KEY) ||
            this._fprintManager == null) {
            this._updateDefaultService();
            return;
        }

        this._fprintManager.GetDefaultDeviceRemote(Gio.DBusCallFlags.NONE, this._cancellable,
            (device, error) => {
                if (!error && device) {
                    this._haveFingerprintReader = true;
                    this._updateDefaultService();
                }
            });
    },

    _oVirtUserAuthenticated(token) {
        this._preemptingService = OVIRT_SERVICE_NAME;
        this.emit('ovirt-user-authenticated');
    },

    _checkForSmartcard() {
        let smartcardDetected;

        if (!this._settings.get_boolean(SMARTCARD_AUTHENTICATION_KEY))
            smartcardDetected = false;
        else if (this._reauthOnly)
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

    _reportInitError(where, error) {
        logError(error, where);
        this._hold.release();

        this._queueMessage(_("Authentication error"), MessageType.ERROR);
        this._verificationFailed(false);
    },

    _reauthenticationChannelOpened(client, result) {
        try {
            this._clearUserVerifier();
            this._userVerifier = client.open_reauthentication_channel_finish(result);
        } catch(e if e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
            return;
        } catch(e if e.matches(Gio.DBusError, Gio.DBusError.ACCESS_DENIED) &&
                !this._reauthOnly) {
            // Gdm emits org.freedesktop.DBus.Error.AccessDenied when there is
            // no session to reauthenticate. Fall back to performing verification
            // from this login session
            client.get_user_verifier(this._cancellable, this._userVerifierGot.bind(this));
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

    _userVerifierGot(client, result) {
        try {
            this._clearUserVerifier();
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

    _connectSignals() {
        this._userVerifier.connect('info', this._onInfo.bind(this));
        this._userVerifier.connect('problem', this._onProblem.bind(this));
        this._userVerifier.connect('info-query', this._onInfoQuery.bind(this));
        this._userVerifier.connect('secret-info-query', this._onSecretInfoQuery.bind(this));
        this._userVerifier.connect('conversation-stopped', this._onConversationStopped.bind(this));
        this._userVerifier.connect('reset', this._onReset.bind(this));
        this._userVerifier.connect('verification-complete', this._onVerificationComplete.bind(this));
    },

    _getForegroundService() {
        if (this._preemptingService)
            return this._preemptingService;

        return this._defaultService;
    },

    serviceIsForeground(serviceName) {
        return serviceName == this._getForegroundService();
    },

    serviceIsDefault(serviceName) {
        return serviceName == this._defaultService;
    },

    _updateDefaultService() {
        if (this._settings.get_boolean(PASSWORD_AUTHENTICATION_KEY))
            this._defaultService = PASSWORD_SERVICE_NAME;
        else if (this._settings.get_boolean(SMARTCARD_AUTHENTICATION_KEY))
            this._defaultService = SMARTCARD_SERVICE_NAME;
        else if (this._haveFingerprintReader)
            this._defaultService = FINGERPRINT_SERVICE_NAME;

        if (!this._defaultService) {
            log("no authentication service is enabled, using password authentication");
            this._defaultService = PASSWORD_SERVICE_NAME;
        }
    },

    _startService(serviceName) {
        this._hold.acquire();
        if (this._userName) {
           this._userVerifier.call_begin_verification_for_user(serviceName,
                                                               this._userName,
                                                               this._cancellable,
                                                               (obj, result) => {
               try {
                   obj.call_begin_verification_for_user_finish(result);
               } catch(e if e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
                   return;
               } catch(e) {
                   this._reportInitError('Failed to start verification for user', e);
                   return;
               }

               this._hold.release();
           });
        } else {
           this._userVerifier.call_begin_verification(serviceName,
                                                      this._cancellable,
                                                      (obj, result) => {
               try {
                   obj.call_begin_verification_finish(result);
               } catch(e if e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED)) {
                   return;
               } catch(e) {
                   this._reportInitError('Failed to start verification', e);
                   return;
               }

               this._hold.release();
           });
        }
    },

    _beginVerification() {
        this._startService(this._getForegroundService());

        if (this._userName && this._haveFingerprintReader && !this.serviceIsForeground(FINGERPRINT_SERVICE_NAME))
            this._startService(FINGERPRINT_SERVICE_NAME);
    },

    _onInfo(client, serviceName, info) {
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

    _onProblem(client, serviceName, problem) {
        if (!this.serviceIsForeground(serviceName))
            return;

        this._queueMessage(problem, MessageType.ERROR);
    },

    _onInfoQuery(client, serviceName, question) {
        if (!this.serviceIsForeground(serviceName))
            return;

        this.emit('ask-question', serviceName, question, '');
    },

    _onSecretInfoQuery(client, serviceName, secretQuestion) {
        if (!this.serviceIsForeground(serviceName))
            return;

        if (serviceName == OVIRT_SERVICE_NAME) {
            // The only question asked by this service is "Token?"
            this.answerQuery(serviceName, this._oVirtCredentialsManager.getToken());
            return;
        }

        this.emit('ask-question', serviceName, secretQuestion, '\u25cf');
    },

    _onReset() {
        // Clear previous attempts to authenticate
        this._failCounter = 0;
        this._updateDefaultService();

        this.emit('reset');
    },

    _onVerificationComplete() {
        this.emit('verification-complete');
    },

    _cancelAndReset() {
        this.cancel();
        this._onReset();
    },

    _retry() {
        this.begin(this._userName, new Batch.Hold());
    },

    _verificationFailed(retry) {
        // For Not Listed / enterprise logins, immediately reset
        // the dialog
        // Otherwise, when in login mode we allow ALLOWED_FAILURES attempts.
        // After that, we go back to the welcome screen.

        this._failCounter++;
        let canRetry = retry && this._userName &&
            (this._reauthOnly ||
             this._failCounter < this._settings.get_int(ALLOWED_FAILURES_KEY));

        if (canRetry) {
            if (!this.hasPendingMessages) {
                this._retry();
            } else {
                let signalId = this.connect('no-more-messages', () => {
                    this.disconnect(signalId);
                    if (this._cancellable && !this._cancellable.is_cancelled())
                        this._retry();
                });
            }
        } else {
            if (!this.hasPendingMessages) {
                this._cancelAndReset();
            } else {
                let signalId = this.connect('no-more-messages', () => {
                    this.disconnect(signalId);
                    this._cancelAndReset();
                });
            }
        }

        this.emit('verification-failed', canRetry);
    },

    _onConversationStopped(client, serviceName) {
        // If the login failed with the preauthenticated oVirt credentials
        // then discard the credentials and revert to default authentication
        // mechanism.
        if (this.serviceIsForeground(OVIRT_SERVICE_NAME)) {
            this._oVirtCredentialsManager.resetToken();
            this._preemptingService = null;
            this._verificationFailed(false);
            return;
        }

        // if the password service fails, then cancel everything.
        // But if, e.g., fingerprint fails, still give
        // password authentication a chance to succeed
        if (this.serviceIsForeground(serviceName)) {
            this._verificationFailed(true);
        }
    },
});
Signals.addSignalMethods(ShellUserVerifier.prototype);
