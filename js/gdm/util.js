import Clutter from 'gi://Clutter';
import Gdm from 'gi://Gdm';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import * as Signals from '../misc/signals.js';

import * as Batch from './batch.js';
import * as OVirt from './oVirt.js';
import * as Vmware from './vmware.js';
import * as Main from '../ui/main.js';
import {loadInterfaceXML} from '../misc/fileUtils.js';
import * as Params from '../misc/params.js';
import * as SmartcardManager from '../misc/smartcardManager.js';

const FprintManagerInfo = Gio.DBusInterfaceInfo.new_for_xml(
    loadInterfaceXML('net.reactivated.Fprint.Manager'));
const FprintDeviceInfo = Gio.DBusInterfaceInfo.new_for_xml(
    loadInterfaceXML('net.reactivated.Fprint.Device'));

Gio._promisify(Gdm.Client.prototype, 'open_reauthentication_channel');
Gio._promisify(Gdm.Client.prototype, 'get_user_verifier');
Gio._promisify(Gdm.UserVerifierProxy.prototype,
    'call_begin_verification_for_user');
Gio._promisify(Gdm.UserVerifierProxy.prototype, 'call_begin_verification');

export const SWITCHABLE_AUTH_SERVICE_NAME = 'gdm-switchable-auth';
export const PASSWORD_SERVICE_NAME = 'gdm-password';
export const FINGERPRINT_SERVICE_NAME = 'gdm-fingerprint';
export const SMARTCARD_SERVICE_NAME = 'gdm-smartcard';
const CLONE_FADE_ANIMATION_TIME = 250;

export const PASSWORD_ROLE_NAME = 'password';
export const SMARTCARD_ROLE_NAME = 'smartcard';
export const FINGERPRINT_ROLE_NAME = 'fingerprint';
export const WEB_LOGIN_ROLE_NAME = 'eidp';

export const AUTH_MECHANISM_PROTOCOL = 'auth-mechanisms';

export const LOGIN_SCREEN_SCHEMA = 'org.gnome.login-screen';
export const SWITCHABLE_AUTHENTICATION_KEY = 'enable-switchable-authentication';
export const PASSWORD_AUTHENTICATION_KEY = 'enable-password-authentication';
export const WEB_LOGIN_AUTHENTICATION_KEY = 'enable-web-authentication';
export const FINGERPRINT_AUTHENTICATION_KEY = 'enable-fingerprint-authentication';
export const SMARTCARD_AUTHENTICATION_KEY = 'enable-smartcard-authentication';
export const BANNER_MESSAGE_KEY = 'banner-message-enable';
export const BANNER_MESSAGE_SOURCE_KEY = 'banner-message-source';
export const BANNER_MESSAGE_TEXT_KEY = 'banner-message-text';
export const BANNER_MESSAGE_PATH_KEY = 'banner-message-path';
export const ALLOWED_FAILURES_KEY = 'allowed-failures';

export const LOGO_KEY = 'logo';
export const DISABLE_USER_LIST_KEY = 'disable-user-list';

const AUTH_SELECTION_COMPLETION_STATUS = 'Ok';

const SWITCHABLE_AUTH_SUPPORTED_ROLES = [
    PASSWORD_ROLE_NAME,
    SMARTCARD_ROLE_NAME,
    WEB_LOGIN_ROLE_NAME,
];

// Give user 48ms to read each character of a PAM message
const USER_READ_TIME = 48;
const FINGERPRINT_SERVICE_PROXY_TIMEOUT = 5000;
const FINGERPRINT_ERROR_TIMEOUT_WAIT = 15;

// Note these are specified in order of preference
const DiscreteServiceMechanismDefinitions = [
    {
        serviceName: PASSWORD_SERVICE_NAME,
        mechanismId: 'password',
        mechanismName: _('Password'),
        iconName: 'dialog-password-symbolic',
        setting: PASSWORD_AUTHENTICATION_KEY,
        role: PASSWORD_ROLE_NAME,
        selectable: true,
    },

    {
        serviceName: SMARTCARD_SERVICE_NAME,
        mechanismId: 'smartcard',
        mechanismName: _('Smartcard'),
        iconName: 'application-certificate-symbolic',
        setting: SMARTCARD_AUTHENTICATION_KEY,
        role: SMARTCARD_ROLE_NAME,
        selectable: true,
    },

    {
        serviceName: FINGERPRINT_SERVICE_NAME,
        mechanismId: 'fingerprint',
        mechanismName: _('Fingerprint'),
        iconName: 'fingerprint-auth-symbolic',
        setting: FINGERPRINT_AUTHENTICATION_KEY,
        role: FINGERPRINT_ROLE_NAME,
        selectable: false,
    },
];

/**
 * Keep messages in order by priority
 *
 * @enum {number}
 */
export const MessageType = {
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

/**
 * @param {Clutter.Actor} actor
 */
export function cloneAndFadeOutActor(actor) {
    // Immediately hide actor so its sibling can have its space
    // and position, but leave a non-reactive clone on-screen,
    // so from the user's point of view it smoothly fades away
    // and reveals its sibling.
    actor.hide();

    const clone = new Clutter.Clone({
        source: actor,
        reactive: false,
    });

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

export class ShellUserVerifier extends Signals.EventEmitter {
    constructor(client, params) {
        super();
        params = Params.parse(params, {reauthenticationOnly: false});
        this._reauthOnly = params.reauthenticationOnly;

        this._client = client;
        this._cancellable = null;

        this._defaultService = null;
        this._preemptingService = null;
        this._foregroundMechanism = null;
        this._fingerprintReaderType = FingerprintReaderType.NONE;

        this._messageQueue = [];
        this._messageQueueTimeoutId = 0;

        this._failCounter = 0;
        this._activeServices = new Set();
        this._unavailableServices = new Set();
        this._publishedMechanisms = new Map();
        this._pendingMechanisms = new Map();

        this._credentialManagers = {};

        this.reauthenticating = false;
        this.smartcardDetected = false;

        this._settings = new Gio.Settings({schema_id: LOGIN_SCREEN_SCHEMA});
        this._settings.connect('changed', () => this._onSettingsChanged());
        this._updateEnabledServices();
        this._updateDefaultService();

        this.addCredentialManager(OVirt.SERVICE_NAME, OVirt.getOVirtCredentialsManager());
        this.addCredentialManager(Vmware.SERVICE_NAME, Vmware.getVmwareCredentialsManager());
    }

    addCredentialManager(serviceName, credentialManager) {
        if (this._credentialManagers[serviceName])
            return;

        this._credentialManagers[serviceName] = credentialManager;
        if (credentialManager.token) {
            this._onCredentialManagerAuthenticated(credentialManager,
                credentialManager.token);
        }

        credentialManager.connectObject('user-authenticated',
            this._onCredentialManagerAuthenticated.bind(this), this);
    }

    removeCredentialManager(serviceName) {
        let credentialManager = this._credentialManagers[serviceName];
        if (!credentialManager)
            return;

        credentialManager.disconnectObject(this);
        delete this._credentialManagers[serviceName];
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

        this._checkForFingerprintReader().catch(e =>
            this._handleFingerprintError(e));

        // If possible, reauthenticate an already running session,
        // so any session specific credentials get updated appropriately
        if (userName)
            this._openReauthenticationChannel(userName).catch(logError);
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
        this._pendingMechanisms.clear();
        this._publishedMechanisms.clear();
        this._unavailableServices.clear();
        if (this._userVerifier) {
            this._disconnectSignals();
            this._userVerifier.run_dispose();
            this._userVerifier = null;
            if (this._userVerifierChoiceList) {
                this._userVerifierChoiceList.run_dispose();
                this._userVerifierChoiceList = null;
            }
            if (this._userVerifierCustomJSON) {
                this._userVerifierCustomJSON.run_dispose();
                this._userVerifierCustomJSON = null;
            }
        }
    }

    clear() {
        if (this._cancellable) {
            this._cancellable.cancel();
            this._cancellable = null;
        }

        this._clearWebLoginTimeout();
        this._clearUserVerifier();
        this._clearMessageQueue();
        this._activeServices.clear();
    }

    destroy() {
        this.cancel();

        this._settings.run_dispose();
        this._settings = null;

        this._smartcardManager?.disconnectObject(this);
        this._smartcardManager = null;

        this._fingerprintManager = null;

        for (let service in this._credentialManagers)
            this.removeCredentialManager(service);
    }

    selectChoice(serviceName, key) {
        this._userVerifierChoiceList.call_select_choice(serviceName, key, this._cancellable, null);
    }

    async answerQuery(serviceName, answer) {
        try {
            await this._handlePendingMessages();
            if (this._pendingMechanisms.has(PASSWORD_ROLE_NAME))
                this._replyWithAuthSelectionResponse(serviceName, PASSWORD_ROLE_NAME, {password: answer});
            else if (this._pendingMechanisms.has(SMARTCARD_ROLE_NAME))
                this._replyWithAuthSelectionResponse(serviceName, SMARTCARD_ROLE_NAME, {pin: answer});
            else
                this._userVerifier.call_answer_query(serviceName, answer, this._cancellable, null);
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                logError(e);
        }
    }

    async replyWithJSON(serviceName, json) {
        log(`RAY: replyWithJSON: ${serviceName} ${json}`);
        try {
            await this._handlePendingMessages();
            this._userVerifierCustomJSON.call_reply(serviceName, json, this._cancellable, null);
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                logError(e);
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
        if (this._messageQueueTimeoutId !== 0)
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

        this._messageQueue.push({serviceName, text: message, type: messageType, interval});
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

        if (this._messageQueueTimeoutId !== 0) {
            GLib.source_remove(this._messageQueueTimeoutId);
            this._messageQueueTimeoutId = 0;
        }
        this.emit('show-message', null, null, MessageType.NONE);
    }

    async _initFingerprintManager() {
        if (this._fprintManager)
            return;

        const fprintManager = new Gio.DBusProxy({
            g_connection: Gio.DBus.system,
            g_name: 'net.reactivated.Fprint',
            g_object_path: '/net/reactivated/Fprint/Manager',
            g_interface_name: FprintManagerInfo.name,
            g_interface_info: FprintManagerInfo,
            g_flags: Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES |
                Gio.DBusProxyFlags.DO_NOT_AUTO_START_AT_CONSTRUCTION |
                Gio.DBusProxyFlags.DO_NOT_CONNECT_SIGNALS,
        });

        try {
            if (!this._getDetectedDefaultService()) {
                // Other authentication methods would have already been detected by
                // now as possibilities if they were available.
                // If we're here it means that FINGERPRINT_AUTHENTICATION_KEY is
                // true and so fingerprint authentication is our last potential
                // option, so go ahead a synchronously look for a fingerprint device
                // during startup or default service update.
                fprintManager.init(null);
                // Do not wait too much for fprintd to reply, as in case it hangs
                // we should fail early without having the shell to misbehave
                fprintManager.set_default_timeout(FINGERPRINT_SERVICE_PROXY_TIMEOUT);

                const [devicePath] = fprintManager.GetDefaultDeviceSync();
                this._fprintManager = fprintManager;

                const fprintDeviceProxy = this._getFingerprintDeviceProxy(devicePath);
                fprintDeviceProxy.init(null);
                this._setFingerprintReaderType(fprintDeviceProxy['scan-type']);
            } else {
                // Ensure fingerprint service starts, but do not wait for it
                const cancellable = this._cancellable;
                await fprintManager.init_async(GLib.PRIORITY_DEFAULT, cancellable);
                await this._updateFingerprintReaderType(fprintManager, cancellable);
                this._fprintManager = fprintManager;
            }
        } catch (e) {
            this._handleFingerprintError(e);
        }
    }

    _getFingerprintDeviceProxy(devicePath) {
        return new Gio.DBusProxy({
            g_connection: Gio.DBus.system,
            g_name: 'net.reactivated.Fprint',
            g_object_path: devicePath,
            g_interface_name: FprintDeviceInfo.name,
            g_interface_info: FprintDeviceInfo,
            g_flags: Gio.DBusProxyFlags.DO_NOT_CONNECT_SIGNALS,
        });
    }

    _handleFingerprintError(e) {
        this._fingerprintReaderType = FingerprintReaderType.NONE;

        if (e instanceof GLib.Error) {
            if (e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                return;
            if (e.matches(Gio.DBusError, Gio.DBusError.SERVICE_UNKNOWN))
                return;
            if (Gio.DBusError.is_remote_error(e) &&
                Gio.DBusError.get_remote_error(e) ===
                    'net.reactivated.Fprint.Error.NoSuchDevice')
                return;
        }

        logError(e, 'Failed to interact with fprintd service');
    }

    async _checkForFingerprintReader() {
        if (!this._fprintManager) {
            this._updateDefaultService();
            return;
        }

        if (this._fingerprintReaderType !== FingerprintReaderType.NONE)
            return;

        await this._updateFingerprintReaderType(this._fprintManager, this._cancellable);
    }

    async _updateFingerprintReaderType(fprintManager, cancellable) {
        // Wrappers don't support null cancellable, so let's ignore it in case
        const args = cancellable ? [cancellable] : [];
        const [devicePath] = await fprintManager.GetDefaultDeviceAsync(...args);
        const fprintDeviceProxy = this._getFingerprintDeviceProxy(devicePath);
        await fprintDeviceProxy.init_async(GLib.PRIORITY_DEFAULT, cancellable);
        this._setFingerprintReaderType(fprintDeviceProxy['scan-type']);
        this._updateDefaultService();

        if (this._userVerifier &&
            !this._activeServices.has(FINGERPRINT_SERVICE_NAME)) {
            if (!this._hold?.isAcquired())
                this._hold = new Batch.Hold();
            await this._maybeStartFingerprintVerification();
        }
    }

    _setFingerprintReaderType(fprintDeviceType) {
        this._fingerprintReaderType =
            FingerprintReaderType[fprintDeviceType.toUpperCase()];

        if (this._fingerprintReaderType === undefined)
            throw new Error(`Unexpected fingerprint device type '${fprintDeviceType}'`);
    }

    _onCredentialManagerAuthenticated(credentialManager, _token) {
        this.setForegroundService(credentialManager.service);
        this.emit('credential-manager-authenticated');
    }

    _initSmartcardManager() {
        if (this._smartcardManager)
            return;

        this._smartcardManager = SmartcardManager.getSmartcardManager();

        // We check for smartcards right away, since an inserted smartcard
        // at startup should result in immediately initiating authentication.
        // This is different than fingerprint readers, where we only check them
        // after a user has been picked.
        this.smartcardDetected = false;
        this._checkForSmartcard();

        this._smartcardManager.connectObject(
            'smartcard-inserted', () => this._checkForSmartcard(),
            'smartcard-removed', () => this._checkForSmartcard(), this);
    }

    _checkForSmartcard() {
        let smartcardDetected;

        log(`RAY: checking for smartcard`);

        if (!this._settings.get_boolean(SMARTCARD_AUTHENTICATION_KEY))
            smartcardDetected = false;
        else if (this._reauthOnly)
            smartcardDetected = this._smartcardManager.hasInsertedLoginToken();
        else
            smartcardDetected = this._smartcardManager.hasInsertedTokens();

        if (smartcardDetected !== this.smartcardDetected) {
            this.smartcardDetected = smartcardDetected;

            const smartcardService = SWITCHABLE_AUTH_SUPPORTED_ROLES.includes(SMARTCARD_ROLE_NAME)?
                SWITCHABLE_AUTH_SERVICE_NAME: SMARTCARD_SERVICE_NAME;

            // FIXME: It doesn't seem like pam_sss sends JSON if no username
            // is specified. Need to fix that before we can act on smartcard
            // insertion events with switchable auth. See also this message in
            // gdm/authPrompt.js
            if (smartcardService !== SWITCHABLE_AUTH_SERVICE_NAME) {
                if (this.smartcardDetected)
                    this.setForegroundService(smartcardService);
                else if (this._preemptingService === smartcardService)
                    this.setForegroundService(null);
            }

            this.emit('smartcard-status-changed');
        }
    }

    _reportInitError(where, error, serviceName) {
        logError(error, where);
        this._hold.release();
        this._hold = null;

        this._queueMessage(serviceName, _('Authentication error'), MessageType.ERROR);
        this._failCounter++;
        this._verificationFailed(serviceName, false);

        if (error instanceof GLib.Error &&
            Gio.DBusError.is_remote_error(error) &&
            Gio.DBusError.get_remote_error(error) ===
            'org.gnome.DisplayManager.SessionWorker.Error.ServiceUnavailable') {
            this._unavailableServices.add(serviceName);
            this._updateDefaultService();
        }
    }

    _getClientExtensionProxies() {
        if (this._client.get_user_verifier_choice_list)
            this._userVerifierChoiceList = this._client.get_user_verifier_choice_list();
        else
            this._userVerifierChoiceList = null;

        if (this._client.get_user_verifier_custom_json)
            this._userVerifierCustomJSON = this._client.get_user_verifier_custom_json();
        else
            this._userVerifierCustomJSON = null;
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

        this._getClientExtensionProxies();

        this.reauthenticating = true;
        this._connectSignals();
        this._beginVerification();
        this._hold.release();
        this._hold = null;
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

        this._getClientExtensionProxies();

        this._connectSignals();
        this._beginVerification();
        this._hold.release();
        this._hold = null;
    }

    _connectSignals() {
        this._disconnectSignals();

        this._userVerifier.connectObject(
            'info', this._onInfo.bind(this),
            'problem', this._onProblem.bind(this),
            'info-query', this._onInfoQuery.bind(this),
            'secret-info-query', this._onSecretInfoQuery.bind(this),
            'conversation-started', this._onConversationStarted.bind(this),
            'conversation-stopped', this._onConversationStopped.bind(this),
            'service-unavailable', this._onServiceUnavailable.bind(this),
            'reset', this._onReset.bind(this),
            'verification-complete', this._onVerificationComplete.bind(this),
            this);

        if (this._userVerifierChoiceList) {
            this._userVerifierChoiceList.connectObject('choice-query',
                this._onChoiceListQuery.bind(this), this);
        }

        if (this._userVerifierCustomJSON) {
            this._userVerifierCustomJSON.connectObject('request',
                this._onCustomJSONRequest.bind(this), this);
        }
    }

    _disconnectSignals() {
        this._userVerifier?.disconnectObject(this);
        this._userVerifierChoiceList?.disconnectObject(this);
        this._userVerifierCustomJSON?.disconnectObject(this);
    }

    _getForegroundService() {
        if (this._preemptingService)
            return this._preemptingService;

        return this._defaultService;
    }

    setForegroundService(serviceName) {
        if (this._getForegroundService() === serviceName)
            return;

        this._preemptingService = serviceName;
        this.emit('foreground-service-changed');
    }

    getForegroundMechanism() {
        if (this._foregroundMechanism)
            return this._foregroundMechanism;

        const foregroundService = this._getForegroundService();
        if (foregroundService === this._defaultService) {
            const definition = DiscreteServiceMechanismDefinitions.find(
                def => def.serviceName === foregroundService);

            if (!definition)
                return null;

            const {mechanismName, mechanismId, role, serviceName} = definition;

            return {name: mechanismName, id: mechanismId, role, serviceName};
        }

        return null;
    }

    setForegroundMechanism(mechanism) {
        const foregroundMechanism = this.getForegroundMechanism();

        this._foregroundMechanism = mechanism;

        if (foregroundMechanism === mechanism)
            return;

        if (foregroundMechanism?.role === mechanism?.role &&
            foregroundMechanism?.mechanismName === mechanism?.mechanismName &&
            foregroundMechanism?.serviceName === mechanism?.serviceName)
            return;

        this.emit('foreground-mechanism-changed');
    }

    serviceIsForeground(serviceName) {
        return serviceName === this._getForegroundService();
    }

    foregroundServiceDeterminesUsername() {
        for (let serviceName in this._credentialManagers) {
            if (this.serviceIsForeground(serviceName))
                return true;
        }

        const foregroundMechanism = this.getForegroundMechanism();
        if (foregroundMechanism?.role === SMARTCARD_ROLE_NAME)
            return true;

        const smartcardService =
            SWITCHABLE_AUTH_SUPPORTED_ROLES.includes(SMARTCARD_ROLE_NAME) &&
            this.smartcardDetected ? SWITCHABLE_AUTH_SERVICE_NAME:
            SMARTCARD_SERVICE_NAME;

        return this.serviceIsForeground(smartcardService);
    }

    serviceIsDefault(serviceName) {
        return serviceName === this._defaultService;
    }

    serviceIsFingerprint(serviceName) {
        return this._fingerprintReaderType !== FingerprintReaderType.NONE &&
            serviceName === FINGERPRINT_SERVICE_NAME;
    }

    _onSettingsChanged() {
        this._updateEnabledServices();
        this._updateDefaultService();
    }

    _updateEnabledServices() {
        let needsReset = false;

        if (this._settings.get_boolean(FINGERPRINT_AUTHENTICATION_KEY)) {
            this._initFingerprintManager().catch(logError);
        } else if (this._fingerprintManager) {
            this._fingerprintManager = null;
            this._fingerprintReaderType = FingerprintReaderType.NONE;

            if (this._activeServices.has(FINGERPRINT_SERVICE_NAME))
                needsReset = true;
        }

        if (this._settings.get_boolean(SMARTCARD_AUTHENTICATION_KEY)) {
            this._initSmartcardManager();
        } else if (this._smartcardManager) {
            this._smartcardManager.disconnectObject(this);
            this._smartcardManager = null;

            if (this._activeServices.has(SMARTCARD_SERVICE_NAME))
                needsReset = true;
        }

        if (needsReset)
            this._cancelAndReset();
    }

    _isDiscreteServiceEnabled(definition) {
        switch (definition.serviceName) {
        case PASSWORD_SERVICE_NAME:
            return this._settings.get_boolean(definition.setting);
        case FINGERPRINT_SERVICE_NAME:
            return this._fingerprintReaderType !== FingerprintReaderType.NONE;
        case SMARTCARD_SERVICE_NAME:
            return !!this._smartcardManager;
        default:
            return false;
        }
    }

    _getDetectedDefaultService() {
        if (this._settings.get_boolean(SWITCHABLE_AUTHENTICATION_KEY))
            return SWITCHABLE_AUTH_SERVICE_NAME;

        const definition = DiscreteServiceMechanismDefinitions.find(
            def => this._isDiscreteServiceEnabled(def));
        return definition?.serviceName ?? null;
    }

    _updateDefaultService() {
        const oldDefaultService = this._defaultService;
        this._defaultService = this._getDetectedDefaultService();

        if (!this._defaultService) {
            log('no authentication service is enabled, using password authentication');
            this._defaultService = PASSWORD_SERVICE_NAME;
        }

        if (oldDefaultService &&
            oldDefaultService !== this._defaultService &&
            this._activeServices.has(oldDefaultService))
            this._cancelAndReset();
    }

    async _startService(serviceName) {
        this._activeServices.add(serviceName);
        this._hold.acquire();
        try {
            this._activeServices.add(serviceName);
            if (this._userName) {
                await this._userVerifier.call_begin_verification_for_user(
                    serviceName, this._userName, this._cancellable);
            } else {
                await this._userVerifier.call_begin_verification(
                    serviceName, this._cancellable);
            }
        } catch (e) {
            this._activeServices.delete(serviceName);
            if (e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                return;
            if (!this.serviceIsForeground(serviceName)) {
                logError(e,
                    `Failed to start ${serviceName} for ${this._userName}`);
                this._hold.release();
                this._hold = null;
                return;
            }
            this._reportInitError(
                this._userName
                    ? `Failed to start ${serviceName} verification for user`
                    : `Failed to start ${serviceName} verification`,
                e, serviceName);
            return;
        }
        this._hold.release();
        this._hold = null;
    }

    async _maybeStartFingerprintVerification() {
        if (this._userName &&
            this._fingerprintReaderType !== FingerprintReaderType.NONE &&
            !this.serviceIsForeground(FINGERPRINT_SERVICE_NAME))
            await this._startService(FINGERPRINT_SERVICE_NAME);
    }

    _startBackgroundServices() {
        this._maybeStartFingerprintVerification().catch(logError);
    }

    _startMechanismFromSwitchableService(mechanism) {
        const roleHandlers = {
            [WEB_LOGIN_ROLE_NAME]: this._startWebLogin,
            [PASSWORD_ROLE_NAME]: this._startPasswordLogin,
            [SMARTCARD_ROLE_NAME]: this._startSmartcardLogin,
        };

        const { serviceName, role, id: mechanismId } = mechanism;
        const publishedMechanism = this._getPublishedMechanismById(
            serviceName, mechanismId);
        if (!publishedMechanism)
            return;

        const handler = roleHandlers[role];
        if (!handler)
            return;

        try {
            this._pendingMechanisms.set(role, mechanismId);
            handler.call(this, serviceName, publishedMechanism);
        } catch (e) {
            this._pendingMechanisms.delete(role);
            logError(e);
        }
    }

    _shouldStartBackgroundService(serviceName) {
        if (!this._userName)
            return false;

        if (this.serviceIsForeground(serviceName))
            return false;

        if (this._activeServices.has(serviceName))
            return false;

        if (this._unavailableServices.has(serviceName))
            return false;

        if (serviceName === FINGERPRINT_SERVICE_NAME)
            return this._fingerprintReaderType !== FingerprintReaderType.NONE;

        return true;
    }

    _onMechanismsListChanged(serviceName, mechanismsList) {
        log(`RAY: mechanisms list changed: ${JSON.stringify([...mechanismsList])}`);
        /* Start all non-selectable (background) mechanisms and the foreground
         * mechanism. Note for discrete auth (e.g. not the non-switchable services),
         * the foreground mechanism is already started explicitly in beginVerification
         */
        if (this._foregroundMechanism?.serviceName === SWITCHABLE_AUTH_SERVICE_NAME &&
            serviceName === this._foregroundMechanism?.serviceName)
            this._startMechanismFromSwitchableService(this._foregroundMechanism);

        if (this._unavailableServices.has(serviceName))
            return;

        for (const mechanism of mechanismsList) {
            /* If it's selectable we wait until it's selected to start it
             */
            if (mechanism.selectable)
                continue;

            if (this._shouldStartBackgroundService(serviceName))
                this._startService(serviceName).catch(logError);

            if (serviceName === SWITCHABLE_AUTH_SERVICE_NAME)
                this._startMechanismFromSwitchableService(mechanism);
        }
    }

    _beginVerification() {
        const foregroundService = this._getForegroundService();

        this._mechanismsListChangedSignalId =
            this.connect('mechanisms-list-changed',
                (_, ...args) => this._onMechanismsListChanged(...args));

        this._startService(foregroundService).catch(logError);
        this._generateMechanismsFromDiscreteServices();
    }

    _onChoiceListQuery(client, serviceName, promptMessage, list) {
        this.emit(`service-request::${serviceName}`);

        if (!this.serviceIsForeground(serviceName))
            return;

        this.emit('show-choice-list', serviceName, promptMessage, list.deepUnpack());
    }

    _startPasswordLogin(serviceName, mechanism) {
        const {prompt, role} = mechanism;

        this.emit('ask-question', serviceName, prompt, true);
    }

    _startSmartcardLogin(serviceName, mechanism) {
        const {pin_prompt, init_instruction, role} = mechanism;

        this.emit('ask-question', serviceName, pin_prompt, true);
    }

    async _replyWithAuthSelectionResponse(serviceName, role, response) {
        const mechanismId = this._pendingMechanisms.get(role);
        this._pendingMechanisms.delete(role);

        const reply = {
            'auth-selection': {
                'status': AUTH_SELECTION_COMPLETION_STATUS,
                [mechanismId]: response,
            },
        };

        await this.replyWithJSON(serviceName, JSON.stringify(reply));
    }

    _startWebLogin(serviceName, mechanism) {
        const {
            init_prompt: initPrompt,
            link_prompt: linkPrompt,
            uri, code, role, timeout,
        } = mechanism;

        if (!linkPrompt || !uri)
            return;

        if (timeout) {
            this._webLoginTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, timeout * 1000,
                () => {
                    this.emit('web-login-time-out', serviceName);
                    this._webLoginTimeoutId = 0;
                    return GLib.SOURCE_REMOVE;
                });
        }

        // FIXME: This must only happen when using the `AUTH_MECHANISM_PROTOCOL`
        // so it should be handled in the specific protocol implementation.
        let buttons = mechanism.buttons;
        if (!buttons) {
            buttons = [{
                action: () => {
                    const trackerObject = {};

                    this.connectObject(
                        `service-request::${serviceName}`, () => {
                            this.disconnectObject(trackerObject);
                            this.emit('web-login-close');
                        },

                        'verification-complete', () => {
                            this.disconnectObject(trackerObject);
                            this.emit('web-login-close');
                        },

                        'verification-failed', () => {
                            this.disconnectObject(trackerObject);
                            this.emit('web-login-close');
                            this.emit('show-failed-notification');
                            this.emit('reset');
                        },
                        trackerObject);

                    this._clearWebLoginTimeout();
                    this._replyWithAuthSelectionResponse(serviceName,
                        WEB_LOGIN_ROLE_NAME, {});
                },
                default: true,
                needsLoading: true,
                label: _('Done'),
            }];
        }

        this.emit('web-login', serviceName, initPrompt, linkPrompt, uri, code, buttons);
    }

    _clearWebLoginTimeout() {
        if (this._webLoginTimeoutId) {
            GLib.source_remove(this._webLoginTimeoutId);
            this._webLoginTimeoutId = 0;
        }
    }

    _filterAuthMechanisms(mechanismsList, filterFunc) {
        return mechanismsList.filter(mechanism => {
            const mapping = DiscreteServiceMechanismDefinitions.find(m =>
                m.role === mechanism.role);

            if (!mapping)
                return true;

            if (!mapping.selectable) {
                log(`RAY: Filtering ${JSON.stringify(mechanism)} because it is not selectable`);
                if (filterFunc)
                    filterFunc(mechanism);
                return false;
            }

            const enabled = this._settings.get_boolean(mapping.setting);

            if (!enabled && filterFunc) {
                log(`RAY: Filtering ${JSON.stringify(mechanism)} because it is not enabled`);
                filterFunc(mechanism);
            }

            return enabled;
        });
    }

    _handleAuthSelection(serviceName, authSelection) {
        let mechanisms = authSelection.mechanisms;
        const priorityList = authSelection.priority;

        if (!mechanisms)
            return;

        const ids = Object.keys(mechanisms);
        ids.sort((a, b) => {
            const priorityA = priorityList.indexOf(a);
            const priorityB = priorityList.indexOf(b);

            if (priorityA !== -1 && priorityB !== -1)
                return priorityA - priorityB;

            if (priorityA !== -1)
                return -1;

            if (priorityB !== -1)
                return 1;

            return 0;
        });

        let mechanismsList = [];
        for (const id of ids) {
            const name = mechanisms[id].name;
            const role = mechanisms[id].role;

            if (!name || !role)
                continue;

            const selectable = mechanisms[id].selectable ?? true;

            log(`RAY: Adding mechanism ${JSON.stringify(mechanisms[id])}`);
            mechanismsList.push({id, name, role, serviceName, selectable});
        }

        mechanismsList = this._filterAuthMechanisms(mechanismsList, mechanism => {
            delete mechanisms[mechanism.id];
        });

        this._publishedMechanisms.set(serviceName, mechanisms);

        this.emit('mechanisms-list-changed', serviceName, mechanismsList);
    }

    _getPublishedMechanismById(serviceName, mechanismId) {
        const mechanisms = this._publishedMechanisms.get(serviceName);

        if (!mechanisms)
            return;

        return mechanisms[mechanismId] ?? null;
    }

    _handleAuthMechanismsRequest(serviceName, requestObject) {
        const authSelection = requestObject['auth-selection'];

        if (authSelection)
            this._handleAuthSelection(serviceName, authSelection);
    }

    _onCustomJSONRequest(client, serviceName, protocol, version, json) {
        if (protocol === AUTH_MECHANISM_PROTOCOL) {
            this.emit(`service-request::${serviceName}`);

            let requestObject;
            try {
                requestObject = JSON.parse(json);
            } catch (e) {
                logError(e);
                return;
            }

            log (`RAY: incoming json request ${json}`);
            this._handleAuthMechanismsRequest(serviceName, requestObject);
        }
    }

    _onInfo(client, serviceName, info) {
        this.emit(`service-request::${serviceName}`);

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
        this.emit(`service-request::${serviceName}`);

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
        this.emit(`service-request::${serviceName}`);

        if (!this.serviceIsForeground(serviceName))
            return;

        this.emit('ask-question', serviceName, question, false);
    }

    _onSecretInfoQuery(client, serviceName, secretQuestion) {
        this.emit(`service-request::${serviceName}`);

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

    _generateMechanismsFromDiscreteServices() {
        // Switchable auth doesn't support all authentication mechanisms
        // (e.g. fingerprint) and also sometimes switchable auth isn't
        // available at all. Fill in the gaps in coverage with mechanisms
        // synthesized from non-switchable authentication services.
        const switchableAuthAvailable = this._activeServices.has(SWITCHABLE_AUTH_SERVICE_NAME) &&
            !this._unavailableServices.has(SWITCHABLE_AUTH_SERVICE_NAME);

        for (const definition of DiscreteServiceMechanismDefinitions) {
            const enabled = this._settings.get_boolean(definition.setting);
            const available = !this._activeServices.has(definition.serviceName) && !this._unavailableServices.has(definition.serviceName);
            const supportedBySwitchableAuth = switchableAuthAvailable &&
                SWITCHABLE_AUTH_SUPPORTED_ROLES.includes(definition.role);

            const mechanismsList = [];
            if (enabled && available && !supportedBySwitchableAuth) {
                mechanismsList.push({
                    iconName: definition.iconName,
                    id: definition.mechanismId,
                    name: definition.mechanismName,
                    role: definition.role,
                    selectable: definition.selectable,
                });
            }
            this.emit('mechanisms-list-changed', definition.serviceName, mechanismsList);
        }
    }

    _onReset() {
        // Clear previous attempts to authenticate
        this._failCounter = 0;
        this._activeServices.clear();

        if (this._mechanismsListChangedSignalId) {
            this.disconnect(this._mechanismsListChangedSignalId);
            this._mechanismsListChangedSignalId = 0;
        }
        this._updateDefaultService();

        this.emit('reset');
    }

    _onVerificationComplete(_client, serviceName) {
        const isCredentialManager = !!this._credentialManagers[serviceName];
        const isForeground = this.serviceIsForeground(serviceName);
        if (isCredentialManager && isForeground) {
            this._credentialManagers[serviceName].token = null;
            this._preemptingService = null;
        }

        this.emit('verification-complete');
    }

    _cancelAndReset() {
        this.cancel();
        this._onReset();
    }

    _retry(serviceName) {
        this._hold = new Batch.Hold();
        this._connectSignals();
        this._startService(serviceName).catch(logError);
    }

    _canRetry() {
        return this._userName &&
            (this._reauthOnly || this._failCounter < this.allowedFailures);
    }

    async _verificationFailed(serviceName, shouldRetry) {
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

        this.emit('verification-failed', serviceName, !doneTrying);

        if (!this._userVerifier)
            return;

        try {
            if (doneTrying) {
                this._disconnectSignals();
                await this._handlePendingMessages();
                this._cancelAndReset();
            } else {
                await this._handlePendingMessages();
                this._retry(serviceName);
            }
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                logError(e);
        }
    }

    _handlePendingMessages() {
        if (!this.hasPendingMessages)
            return Promise.resolve();

        const cancellable = this._cancellable;
        return new Promise((resolve, reject) => {
            let signalId = this.connect('no-more-messages', () => {
                this.disconnect(signalId);
                if (cancellable.is_cancelled())
                    reject(new GLib.Error(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED, 'Operation was cancelled'));
                else
                    resolve();
            });
        });
    }

    _onServiceUnavailable(_client, serviceName, errorMessage) {
        this._unavailableServices.add(serviceName);

        if (serviceName === SWITCHABLE_AUTH_SERVICE_NAME)
            this._generateMechanismsFromDiscreteServices();

        if (!errorMessage)
            return;

        if (this.serviceIsForeground(serviceName) || this.serviceIsFingerprint(serviceName))
            this._queueMessage(serviceName, errorMessage, MessageType.ERROR);
    }

    _onConversationStarted(client, serviceName) {
        this._activeServices.add(serviceName);
    }

    _onConversationStopped(client, serviceName) {
        this._activeServices.delete(serviceName);
        this.emit('mechanisms-list-changed', serviceName, []);

        // If the login failed with the preauthenticated oVirt credentials
        // then discard the credentials and revert to default authentication
        // mechanism.
        const isCredentialManager = !!this._credentialManagers[serviceName];
        const isForeground = this.serviceIsForeground(serviceName);
        if (isCredentialManager && isForeground) {
            this._credentialManagers[serviceName].token = null;
            this.setForegroundService(null);
            this._verificationFailed(serviceName, false);
            return;
        }

        this._filterServiceMessages(serviceName, MessageType.ERROR);

        if (this._unavailableServices.has(serviceName))
            return;

        // if the password service fails, then cancel everything.
        // But if, e.g., fingerprint fails, still give
        // password authentication a chance to succeed
        if (isForeground)
            this._failCounter++;

        this._verificationFailed(serviceName, true);
    }
}
