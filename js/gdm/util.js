// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported BANNER_MESSAGE_KEY, BANNER_MESSAGE_TEXT_KEY, LOGO_KEY,
            DISABLE_USER_LIST_KEY, fadeInActor, fadeOutActor, cloneAndFadeOutActor */

const { Clutter, Gdm, Gio, GLib } = imports.gi;
const Signals = imports.signals;

const Batch = imports.gdm.batch;
const OVirt = imports.gdm.oVirt;
const Vmware = imports.gdm.vmware;
const Main = imports.ui.main;
const { loadInterfaceXML } = imports.misc.fileUtils;
const Params = imports.misc.params;
const SmartcardManager = imports.misc.smartcardManager;

const FprintManagerIface = loadInterfaceXML('net.reactivated.Fprint.Manager');
const FprintManagerProxy = Gio.DBusProxy.makeProxyWrapper(FprintManagerIface);
const FprintDeviceIface = loadInterfaceXML('net.reactivated.Fprint.Device');
const FprintDeviceProxy = Gio.DBusProxy.makeProxyWrapper(FprintDeviceIface);

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
const FINGERPRINT_ERROR_TIMEOUT_WAIT = 15;

var MessageType = {
    // Keep messages in order by priority
    NONE: 0,
    HINT: 1,
    INFO: 2,
    ERROR: 3,
};

const FingerprintReaderType = {
    NONE: 0,
    PRESS: 1,
    SWIPE: 2,
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

        this._fprintManager = new FprintManagerProxy(Gio.DBus.system,
            'net.reactivated.Fprint',
            '/net/reactivated/Fprint/Manager',
            null,
            null,
            Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES);
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

    get hasPendingMessages() {
        return !!this._messageQueue.length;
    }

    get allowedFailures() {
        return this._settings.get_int(ALLOWED_FAILURES_KEY);
    }

    get currentMessage() {
        return this._messageQueue ? this._messageQueue[0] : null;
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
            this._disconnectSignals();
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
        if (!message)
            return 0;

        // We probably could be smarter here
        return message.length * USER_READ_TIME;
    }

    finishMessageQueue() {
        if (!this.hasPendingMessages)
            return;

        this._messageQueue = [];

        this.emit('no-more-messages');
    }

    increaseCurrentMessageTimeout(interval) {
        if (!this._messageQueueTimeoutId && interval > 0)
            this._currentMessageExtraInterval = interval;
    }

    _serviceHasPendingMessages(serviceName) {
        return this._messageQueue.some(m => m.serviceName === serviceName);
    }

    _filterServiceMessages(serviceName, messageType) {
        // This function allows to remove queued messages for the @serviceName
        // whose type has lower priority than @messageType, replacing them
        // with a null message that will lead to clearing the prompt once done.
        if (this._serviceHasPendingMessages(serviceName))
            this._queuePriorityMessage(serviceName, null, messageType);
    }

    _queueMessageTimeout() {
        if (this._messageQueueTimeoutId != 0)
            return;

        const message = this.currentMessage;

        delete this._currentMessageExtraInterval;
        this.emit('show-message', message.serviceName, message.text, message.type);

        this._messageQueueTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
            message.interval + (this._currentMessageExtraInterval | 0), () => {
                this._messageQueueTimeoutId = 0;

                if (this._messageQueue.length > 1) {
                    this._messageQueue.shift();
                    this._queueMessageTimeout();
                } else {
                    this.finishMessageQueue();
                }

                return GLib.SOURCE_REMOVE;
            });
        GLib.Source.set_name_by_id(this._messageQueueTimeoutId, '[gnome-shell] this._queueMessageTimeout');
    }

    _queueMessage(serviceName, message, messageType) {
        let interval = this._getIntervalForMessage(message);

        this._messageQueue.push({ serviceName, text: message, type: messageType, interval });
        this._queueMessageTimeout();
    }

    _queuePriorityMessage(serviceName, message, messageType) {
        const newQueue = this._messageQueue.filter(m => {
            if (m.serviceName !== serviceName || m.type >= messageType)
                return m.text !== message;
            return false;
        });

        if (!newQueue.includes(this.currentMessage))
            this._clearMessageQueue();

        this._messageQueue = newQueue;
        this._queueMessage(serviceName, message, messageType);
    }

    _clearMessageQueue() {
        this.finishMessageQueue();

        if (this._messageQueueTimeoutId != 0) {
            GLib.source_remove(this._messageQueueTimeoutId);
            this._messageQueueTimeoutId = 0;
        }
        this.emit('show-message', null, null, MessageType.NONE);
    }

    _checkForFingerprintReader() {
        this._fingerprintReaderType = FingerprintReaderType.NONE;

        if (!this._settings.get_boolean(FINGERPRINT_AUTHENTICATION_KEY) ||
            this._fprintManager == null) {
            this._updateDefaultService();
            return;
        }

        this._fprintManager.GetDefaultDeviceRemote(Gio.DBusCallFlags.NONE, this._cancellable,
            (params, error) => {
                if (!error && params) {
                    const [device] = params;
                    const fprintDeviceProxy = new FprintDeviceProxy(Gio.DBus.system,
                        'net.reactivated.Fprint',
                        device);
                    const fprintDeviceType = fprintDeviceProxy['scan-type'];

                    this._fingerprintReaderType = fprintDeviceType === 'swipe'
                        ? FingerprintReaderType.SWIPE
                        : FingerprintReaderType.PRESS;
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

    _reportInitError(where, error, serviceName) {
        logError(error, where);
        this._hold.release();

        this._queueMessage(serviceName, _('Authentication error'), MessageType.ERROR);
        this._failCounter++;
        this._verificationFailed(serviceName, false);
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
        this._disconnectSignals();
        this._signalIds = [];

        let id = this._userVerifier.connect('info', this._onInfo.bind(this));
        this._signalIds.push(id);
        id = this._userVerifier.connect('problem', this._onProblem.bind(this));
        this._signalIds.push(id);
        id = this._userVerifier.connect('info-query', this._onInfoQuery.bind(this));
        this._signalIds.push(id);
        id = this._userVerifier.connect('secret-info-query', this._onSecretInfoQuery.bind(this));
        this._signalIds.push(id);
        id = this._userVerifier.connect('conversation-stopped', this._onConversationStopped.bind(this));
        this._signalIds.push(id);
        id = this._userVerifier.connect('service-unavailable', this._onServiceUnavailable.bind(this));
        this._signalIds.push(id);
        id = this._userVerifier.connect('reset', this._onReset.bind(this));
        this._signalIds.push(id);
        id = this._userVerifier.connect('verification-complete', this._onVerificationComplete.bind(this));
        this._signalIds.push(id);
    }

    _disconnectSignals() {
        if (!this._signalIds || !this._userVerifier)
            return;

        this._signalIds.forEach(s => this._userVerifier.disconnect(s));
        this._signalIds = [];
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
        return this._fingerprintReaderType !== FingerprintReaderType.NONE &&
            serviceName === FINGERPRINT_SERVICE_NAME;
    }

    _updateDefaultService() {
        if (this._settings.get_boolean(PASSWORD_AUTHENTICATION_KEY))
            this._defaultService = PASSWORD_SERVICE_NAME;
        else if (this._settings.get_boolean(SMARTCARD_AUTHENTICATION_KEY))
            this._defaultService = SMARTCARD_SERVICE_NAME;
        else if (this._fingerprintReaderType !== FingerprintReaderType.NONE)
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
                ? 'Failed to start %s verification for user'.format(serviceName)
                : 'Failed to start %s verification'.format(serviceName), e,
            serviceName);
            return;
        }
        this._hold.release();
    }

    _beginVerification() {
        this._startService(this._getForegroundService());

        if (this._userName &&
            this._fingerprintReaderType !== FingerprintReaderType.NONE &&
            !this.serviceIsForeground(FINGERPRINT_SERVICE_NAME))
            this._startService(FINGERPRINT_SERVICE_NAME);
    }

    _onInfo(client, serviceName, info) {
        if (this.serviceIsForeground(serviceName)) {
            this._queueMessage(serviceName, info, MessageType.INFO);
        } else if (this.serviceIsFingerprint(serviceName)) {
            // We don't show fingerprint messages directly since it's
            // not the main auth service. Instead we use the messages
            // as a cue to display our own message.
            if (this._fingerprintReaderType === FingerprintReaderType.SWIPE) {
                // Translators: this message is shown below the password entry field
                // to indicate the user can swipe their finger on the fingerprint reader
                this._queueMessage(serviceName, _('(or swipe finger across reader)'),
                    MessageType.HINT);
            } else {
                // Translators: this message is shown below the password entry field
                // to indicate the user can place their finger on the fingerprint reader instead
                this._queueMessage(serviceName, _('(or place finger on reader)'),
                    MessageType.HINT);
            }
        }
    }

    _onProblem(client, serviceName, problem) {
        const isFingerprint = this.serviceIsFingerprint(serviceName);

        if (!this.serviceIsForeground(serviceName) && !isFingerprint)
            return;

        this._queuePriorityMessage(serviceName, problem, MessageType.ERROR);

        if (isFingerprint) {
            // pam_fprintd allows the user to retry multiple (maybe even infinite!
            // times before failing the authentication conversation.
            // We don't want this behavior to bypass the max-tries setting the user has set,
            // so we count the problem messages to know how many times the user has failed.
            // Once we hit the max number of failures we allow, it's time to failure the
            // conversation from our side. We can't do that right away, however, because
            // we may drop pending messages coming from pam_fprintd. In order to make sure
            // the user sees everything, we queue the failure up to get handled in the
            // near future, after we've finished up the current round of messages.
            this._failCounter++;

            if (!this._canRetry()) {
                if (this._fingerprintFailedId)
                    GLib.source_remove(this._fingerprintFailedId);

                const cancellable = this._cancellable;
                this._fingerprintFailedId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                    FINGERPRINT_ERROR_TIMEOUT_WAIT, () => {
                        this._fingerprintFailedId = 0;
                        if (!cancellable.is_cancelled())
                            this._verificationFailed(serviceName, false);
                        return GLib.SOURCE_REMOVE;
                    });
            }
        }
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

    _retry(serviceName) {
        this._hold = new Batch.Hold();
        this._connectSignals();
        this._startService(serviceName);
    }

    _canRetry() {
        return this._userName &&
            (this._reauthOnly || this._failCounter < this.allowedFailures);
    }

    _verificationFailed(serviceName, shouldRetry) {
        if (serviceName === FINGERPRINT_SERVICE_NAME) {
            if (this._fingerprintFailedId)
                GLib.source_remove(this._fingerprintFailedId);
        }

        // For Not Listed / enterprise logins, immediately reset
        // the dialog
        // Otherwise, when in login mode we allow ALLOWED_FAILURES attempts.
        // After that, we go back to the welcome screen.
        this._filterServiceMessages(serviceName, MessageType.ERROR);

        const doneTrying = !shouldRetry || !this._canRetry();

        if (doneTrying) {
            this._disconnectSignals();

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

        this.emit('verification-failed', serviceName, !doneTrying);

        if (!this.hasPendingMessages) {
            this._retry(serviceName);
        } else {
            const cancellable = this._cancellable;
            let signalId = this.connect('no-more-messages', () => {
                this.disconnect(signalId);
                if (!cancellable.is_cancelled())
                    this._retry(serviceName);
            });
        }
    }

    _onServiceUnavailable(_client, serviceName, errorMessage) {
        this._unavailableServices.add(serviceName);

        if (!errorMessage)
            return;

        if (this.serviceIsForeground(serviceName) || this.serviceIsFingerprint(serviceName))
            this._queueMessage(serviceName, errorMessage, MessageType.ERROR);
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
            this._verificationFailed(serviceName, false);
            return;
        }

        this._filterServiceMessages(serviceName, MessageType.ERROR);

        if (this._unavailableServices.has(serviceName))
            return;

        // if the password service fails, then cancel everything.
        // But if, e.g., fingerprint fails, still give
        // password authentication a chance to succeed
        if (this.serviceIsForeground(serviceName))
            this._failCounter++;

        this._verificationFailed(serviceName, true);
    }
};
Signals.addSignalMethods(ShellUserVerifier.prototype);
