// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported BANNER_MESSAGE_KEY, BANNER_MESSAGE_TEXT_KEY, LOGO_KEY,
            DISABLE_USER_LIST_KEY, fadeInActor, fadeOutActor, cloneAndFadeOutActor */

const { Clutter, Gdm, Gio, GLib } = imports.gi;
const Signals = imports.signals;

const Batch = imports.gdm.batch;
const Fprint = imports.gdm.fingerprint;
const OVirt = imports.gdm.oVirt;
const Vmware = imports.gdm.vmware;
const Main = imports.ui.main;
const Params = imports.misc.params;
const SmartcardManager = imports.misc.smartcardManager;

Gio._promisify(Gdm.Client.prototype,
    'open_reauthentication_channel', 'open_reauthentication_channel_finish');
Gio._promisify(Gdm.Client.prototype,
    'get_user_verifier', 'get_user_verifier_finish');
Gio._promisify(Gdm.UserVerifierProxy.prototype,
    'call_begin_verification_for_user', 'call_begin_verification_for_user_finish');
Gio._promisify(Gdm.UserVerifierProxy.prototype,
    'call_begin_verification', 'call_begin_verification_finish');

var PASSWORD_SERVICE_NAME = 'gdm-password';
var FINGERPRINT_SERVICE_NAME = 'gdm-fingerprint';
var SMARTCARD_SERVICE_NAME = 'gdm-smartcard';
var FADE_ANIMATION_TIME = 160;
var CLONE_FADE_ANIMATION_TIME = 250;

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
var USER_READ_TIME = 48;

var MessageType = {
    NONE: 0,
    ERROR: 1,
    INFO: 2,
    HINT: 3,
};

function fadeInActor(actor) {
    if (actor.opacity == 255 && actor.visible)
        return null;

    let hold = new Batch.Hold();
    actor.show();
    let [, naturalHeight] = actor.get_preferred_height(-1);

    actor.opacity = 0;
    actor.set_height(0);
    actor.ease({
        opacity: 255,
        height: naturalHeight,
        duration: FADE_ANIMATION_TIME,
        mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        onComplete: () => {
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
    actor.ease({
        opacity: 0,
        height: 0,
        duration: FADE_ANIMATION_TIME,
        mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        onComplete: () => {
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
    clone.ease({
        opacity: 0,
        duration: CLONE_FADE_ANIMATION_TIME,
        mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        onComplete: () => {
            clone.destroy();
            hold.release();
        },
    });
    return hold;
}

var ShellUserVerifier = class {
    constructor(client, params) {
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
        this._unavailableServices = new Set();

        this._credentialManagers = {};
        this._credentialManagers[OVirt.SERVICE_NAME] = OVirt.getOVirtCredentialsManager();
        this._credentialManagers[Vmware.SERVICE_NAME] = Vmware.getVmwareCredentialsManager();

        for (let service in this._credentialManagers) {
            if (this._credentialManagers[service].token) {
                this._onCredentialManagerAuthenticated(this._credentialManagers[service],
                    this._credentialManagers[service].token);
            }

            this._credentialManagers[service]._authenticatedSignalId =
                this._credentialManagers[service].connect('user-authenticated',
                                                          this._onCredentialManagerAuthenticated.bind(this));
        }
    }

    begin(userName, hold) {
        this._cancellable = new Gio.Cancellable();
        this._hold = hold;
        this._userName = userName;
        this.reauthenticating = false;

        this._checkForFingerprintReader();

        // If possible, reauthenticate an already running session,
        // so any session specific credentials get updated appropriately
        if (userName)
            this._openReauthenticationChannel(userName);
        else
            this._getUserVerifier();
    }

    cancel() {
        if (this._cancellable)
            this._cancellable.cancel();

        if (this._userVerifier) {
            this._userVerifier.call_cancel_sync(null);
            this.clear();
        }
    }

    _clearUserVerifier() {
        if (this._userVerifier) {
            this._userVerifier.run_dispose();
            this._userVerifier = null;
        }
    }

    clear() {
        if (this._cancellable) {
            this._cancellable.cancel();
            this._cancellable = null;
        }

        this._clearUserVerifier();
        this._clearMessageQueue();
    }

    destroy() {
        this.cancel();

        this._settings.run_dispose();
        this._settings = null;

        this._smartcardManager.disconnect(this._smartcardInsertedId);
        this._smartcardManager.disconnect(this._smartcardRemovedId);
        this._smartcardManager = null;

        for (let service in this._credentialManagers) {
            let credentialManager = this._credentialManagers[service];
            credentialManager.disconnect(credentialManager._authenticatedSignalId);
            credentialManager = null;
        }
    }

    answerQuery(serviceName, answer) {
        if (!this.hasPendingMessages) {
            this._userVerifier.call_answer_query(serviceName, answer, this._cancellable, null);
        } else {
            const cancellable = this._cancellable;
            let signalId = this.connect('no-more-messages', () => {
                this.disconnect(signalId);
                if (!cancellable.is_cancelled())
                    this._userVerifier.call_answer_query(serviceName, answer, cancellable, null);
            });
        }
    }

    _getIntervalForMessage(message) {
        // We probably could be smarter here
        return message.length * USER_READ_TIME;
    }

    finishMessageQueue() {
        if (!this.hasPendingMessages)
            return;

        this._messageQueue = [];

        this.hasPendingMessages = false;
        this.emit('no-more-messages');
    }

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
    }

    _queueMessage(message, messageType) {
        let interval = this._getIntervalForMessage(message);

        this.hasPendingMessages = true;
        this._messageQueue.push({ text: message, type: messageType, interval });
        this._queueMessageTimeout();
    }

    _clearMessageQueue() {
        this.finishMessageQueue();

        if (this._messageQueueTimeoutId != 0) {
            GLib.source_remove(this._messageQueueTimeoutId);
            this._messageQueueTimeoutId = 0;
        }
        this.emit('show-message', null, MessageType.NONE);
    }

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
    }

    _onCredentialManagerAuthenticated(credentialManager, _token) {
        this._preemptingService = credentialManager.service;
        this.emit('credential-manager-authenticated');
    }

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
    }

    _reportInitError(where, error) {
        logError(error, where);
        this._hold.release();

        this._queueMessage(_("Authentication error"), MessageType.ERROR);
        this._verificationFailed(false);
    }

    async _openReauthenticationChannel(userName) {
        try {
            this._clearUserVerifier();
            this._userVerifier = await this._client.open_reauthentication_channel(
                userName, this._cancellable);
        } catch (e) {
            if (e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                return;
            if (e.matches(Gio.DBusError, Gio.DBusError.ACCESS_DENIED) &&
                !this._reauthOnly) {
                // Gdm emits org.freedesktop.DBus.Error.AccessDenied when there
                // is no session to reauthenticate. Fall back to performing
                // verification from this login session
                this._getUserVerifier();
                return;
            }

            this._reportInitError('Failed to open reauthentication channel', e);
            return;
        }

        this.reauthenticating = true;
        this._connectSignals();
        this._beginVerification();
        this._hold.release();
    }

    async _getUserVerifier() {
        try {
            this._clearUserVerifier();
            this._userVerifier =
                await this._client.get_user_verifier(this._cancellable);
        } catch (e) {
            if (e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                return;
            this._reportInitError('Failed to obtain user verifier', e);
            return;
        }

        this._connectSignals();
        this._beginVerification();
        this._hold.release();
    }

    _connectSignals() {
        this._userVerifier.connect('info', this._onInfo.bind(this));
        this._userVerifier.connect('problem', this._onProblem.bind(this));
        this._userVerifier.connect('info-query', this._onInfoQuery.bind(this));
        this._userVerifier.connect('secret-info-query', this._onSecretInfoQuery.bind(this));
        this._userVerifier.connect('conversation-stopped', this._onConversationStopped.bind(this));
        this._userVerifier.connect('service-unavailable', this._onServiceUnavailable.bind(this));
        this._userVerifier.connect('reset', this._onReset.bind(this));
        this._userVerifier.connect('verification-complete', this._onVerificationComplete.bind(this));
    }

    _getForegroundService() {
        if (this._preemptingService)
            return this._preemptingService;

        return this._defaultService;
    }

    serviceIsForeground(serviceName) {
        return serviceName == this._getForegroundService();
    }

    serviceIsDefault(serviceName) {
        return serviceName == this._defaultService;
    }

    serviceIsFingerprint(serviceName) {
        return serviceName === FINGERPRINT_SERVICE_NAME &&
            this._haveFingerprintReader;
    }

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
    }

    async _startService(serviceName) {
        this._hold.acquire();
        try {
            if (this._userName) {
                await this._userVerifier.call_begin_verification_for_user(
                    serviceName, this._userName, this._cancellable);
            } else {
                await this._userVerifier.call_begin_verification(
                    serviceName, this._cancellable);
            }
        } catch (e) {
            if (e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                return;
            if (!this.serviceIsForeground(serviceName)) {
                logError(e, 'Failed to start %s for %s'.format(serviceName, this._userName));
                this._hold.release();
                return;
            }
            this._reportInitError(this._userName
                ? 'Failed to start verification for user'
                : 'Failed to start verification', e);
            return;
        }
        this._hold.release();
    }

    _beginVerification() {
        this._startService(this._getForegroundService());

        if (this._userName && this._haveFingerprintReader && !this.serviceIsForeground(FINGERPRINT_SERVICE_NAME))
            this._startService(FINGERPRINT_SERVICE_NAME);
    }

    _onInfo(client, serviceName, info) {
        if (this.serviceIsForeground(serviceName)) {
            this._queueMessage(info, MessageType.INFO);
        } else if (this.serviceIsFingerprint(serviceName)) {
            // We don't show fingerprint messages directly since it's
            // not the main auth service. Instead we use the messages
            // as a cue to display our own message.

            // Translators: this message is shown below the password entry field
            // to indicate the user can swipe their finger instead
            this._queueMessage(_("(or swipe finger)"), MessageType.HINT);
        }
    }

    _onProblem(client, serviceName, problem) {
        const isFingerprint = this.serviceIsFingerprint(serviceName);

        if (!this.serviceIsForeground(serviceName) && !isFingerprint)
            return;

        this._queueMessage(problem, MessageType.ERROR);
    }

    _onInfoQuery(client, serviceName, question) {
        if (!this.serviceIsForeground(serviceName))
            return;

        this.emit('ask-question', serviceName, question, false);
    }

    _onSecretInfoQuery(client, serviceName, secretQuestion) {
        if (!this.serviceIsForeground(serviceName))
            return;

        let token = null;
        if (this._credentialManagers[serviceName])
            token = this._credentialManagers[serviceName].token;

        if (token) {
            this.answerQuery(serviceName, token);
            return;
        }

        this.emit('ask-question', serviceName, secretQuestion, true);
    }

    _onReset() {
        // Clear previous attempts to authenticate
        this._failCounter = 0;
        this._unavailableServices.clear();
        this._updateDefaultService();

        this.emit('reset');
    }

    _onVerificationComplete() {
        this.emit('verification-complete');
    }

    _cancelAndReset() {
        this.cancel();
        this._onReset();
    }

    _retry() {
        this.cancel();
        this.begin(this._userName, new Batch.Hold());
    }

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
                const cancellable = this._cancellable;
                let signalId = this.connect('no-more-messages', () => {
                    this.disconnect(signalId);
                    if (!cancellable.is_cancelled())
                        this._retry();
                });
            }
        } else {
            // eslint-disable-next-line no-lonely-if
            if (!this.hasPendingMessages) {
                this._cancelAndReset();
            } else {
                const cancellable = this._cancellable;
                let signalId = this.connect('no-more-messages', () => {
                    this.disconnect(signalId);
                    if (!cancellable.is_cancelled())
                        this._cancelAndReset();
                });
            }
        }

        this.emit('verification-failed', canRetry);
    }

    _onServiceUnavailable(_client, serviceName, errorMessage) {
        this._unavailableServices.add(serviceName);

        if (!errorMessage)
            return;

        if (this.serviceIsForeground(serviceName) || this.serviceIsFingerprint(serviceName))
            this._queueMessage(errorMessage, MessageType.ERROR);
    }

    _onConversationStopped(client, serviceName) {
        // If the login failed with the preauthenticated oVirt credentials
        // then discard the credentials and revert to default authentication
        // mechanism.
        let foregroundService = Object.keys(this._credentialManagers).find(service =>
            this.serviceIsForeground(service));
        if (foregroundService) {
            this._credentialManagers[foregroundService].token = null;
            this._preemptingService = null;
            this._verificationFailed(false);
            return;
        }

        if (this._unavailableServices.has(serviceName))
            return;

        // if the password service fails, then cancel everything.
        // But if, e.g., fingerprint fails, still give
        // password authentication a chance to succeed
        if (this.serviceIsForeground(serviceName))
            this._verificationFailed(true);
    }
};
Signals.addSignalMethods(ShellUserVerifier.prototype);
