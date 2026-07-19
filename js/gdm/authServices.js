import * as Constants from './constants.js';
import * as FingerprintManager from './fingerprintManager.js';
import * as Params from '../misc/params.js';
import {registerDestroyableType} from '../misc/signalTracker.js';
import * as PasskeyDeviceManager from './passkeyDeviceManager.js';
import * as SmartcardManager from './smartcardManager.js';
import {logErrorUnlessCancelled} from '../misc/errorUtils.js';
import * as Util from './util.js';
import Gdm from 'gi://Gdm';
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';

Gio._promisify(Gdm.Client.prototype, 'open_reauthentication_channel');
Gio._promisify(Gdm.Client.prototype, 'get_user_verifier');
Gio._promisify(Gdm.UserVerifierProxy.prototype, 'call_begin_verification_for_user');
Gio._promisify(Gdm.UserVerifierProxy.prototype, 'call_begin_verification');
Gio._promisify(Gdm.UserVerifierProxy.prototype, 'call_answer_query');
Gio._promisify(Gdm.UserVerifierChoiceListProxy.prototype, 'call_select_choice');
Gio._promisify(Gdm.UserVerifierCustomJSONProxy.prototype, 'call_reply');

export class AuthServices extends GObject.Object {
    static [GObject.signals] = {
        'destroy': {},
        'queue-message': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_UINT],
        },
        'queue-priority-message': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_UINT],
        },
        'wait-pending-messages': {
            param_types: [GObject.TYPE_JSOBJECT],
        },
        'filter-messages': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_UINT],
        },
        'verification-failed': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_BOOLEAN],
        },
        'verification-complete': {},
        'ask-question': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_BOOLEAN],
        },
        'reset': {
            param_types: [GObject.TYPE_JSOBJECT],
        },
        'show-choice-list': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_JSOBJECT],
        },
        'mechanisms-changed': {},
        'web-login': {
            param_types: [
                GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_STRING,
                GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_JSOBJECT,
            ],
        },
    };

    static {
        GObject.registerClass(this);
        registerDestroyableType(this);
    }

    static SupportedRoles = [];
    static RoleToService = {};

    static isEnabled(_settings) {
        return true;
    }

    constructor(params) {
        super();
        params = Params.parse(params, {
            client: null,
            allowedFailures: 3,
            reauthOnly: false,
        });

        this._client = params.client;
        this._enabledRoles = this.supportedRoles;
        this._allowedFailures = params.allowedFailures;
        this._reauthOnly = params.reauthOnly;

        this._failCounter = 0;
        this._activeServices = new Set();
        this._unavailableServices = new Set();

        this._cancellable = null;

        if (this.supportedRoles.includes(Constants.SMARTCARD_ROLE_NAME))
            this._connectSmartcardManager();
        if (this.supportedRoles.includes(Constants.PASSKEY_ROLE_NAME))
            this._connectPasskeyDeviceManager();
        if (this.supportedRoles.includes(Constants.FINGERPRINT_ROLE_NAME))
            this._connectFingerprintManager();
    }

    get selectedMechanism() {
        return this._selectedMechanism;
    }

    get enabledMechanisms() {
        return this._enabledMechanisms?.filter(m => m.ready !== false);
    }

    get _roleToService() {
        return this.constructor.RoleToService;
    }

    get supportedRoles() {
        return this._handleGetSupportedRoles();
    }

    selectChoice(serviceName, key) {
        this._handleSelectChoice(serviceName, key);
    }

    answerQuery(serviceName, answer) {
        this._handleAnswerQuery(serviceName, answer);
    }

    async beginVerification(userName, userVerifierProxies) {
        this._cancellable?.cancel();
        this._cancellable = new Gio.Cancellable();
        this._userName = userName;

        try {
            this._updateUserVerifier(userVerifierProxies);
            await this._startServices(this._cancellable);
        } catch (e) {
            if (e.cause?.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                return;

            this._failCounter++;
            throw e;
        }

        this._handleBeginVerification();
    }

    selectMechanism(mechanism) {
        if (this._selectedMechanism?.role === mechanism.role &&
            this._selectedMechanism?.serviceName === mechanism.serviceName)
            return false;

        this._selectedMechanism = this._enabledMechanisms?.find(m =>
            m.role === mechanism.role &&
            m.serviceName === mechanism.serviceName
        );

        this._handleSelectMechanism();

        return !!this._selectedMechanism;
    }

    needsUsername() {
        return this._handleNeedsUsername();
    }

    reset() {
        this._failCounter = 0;

        this._handleReset();
    }

    cancel() {
        this._handleCancel();
    }

    destroy() {
        this.reset();
        this.clear();
        this.emit('destroy');
    }

    clear() {
        this._cancellable?.cancel();
        this._cancellable = null;

        this._unavailableServices.clear();
        this._activeServices.clear();

        this._verificationComplete = false;

        this._clearUserVerifier();

        this._handleClear();
    }

    updateEnabledRoles({disableRoles}) {
        const updatedRoles = this.supportedRoles
            .filter(r => !disableRoles.includes(r));

        if (updatedRoles.length === this._enabledRoles.length &&
            updatedRoles.every(r => this._enabledRoles.includes(r)))
            return;

        this._enabledRoles = updatedRoles;

        this._handleUpdateEnabledRoles();
    }

    _clearUserVerifier() {
        this._disconnectUserVerifierSignals();
        this._userVerifier = null;
        this._userVerifierChoiceList = null;
        this._userVerifierCustomJSON = null;
    }

    _disconnectUserVerifierSignals() {
        this._userVerifier?.get_connection().disconnectObject(this);
        this._userVerifier?.disconnectObject(this);
        this._userVerifierChoiceList?.disconnectObject(this);
        this._userVerifierCustomJSON?.disconnectObject(this);
    }

    _updateEnabledMechanisms() {
        this._enabledMechanisms = [];

        this._handleUpdateEnabledMechanisms();

        this.emit('mechanisms-changed');
    }

    _connectSmartcardManager() {
        this._smartcardManager = SmartcardManager.getSmartcardManager();
        this._smartcardManager.connectObject(
            'smartcard-inserted', () => this._handleSmartcardChanged(),
            'smartcard-removed', () => this._handleSmartcardChanged(),
            this);
    }

    _connectPasskeyDeviceManager() {
        this._passkeyDeviceManager = PasskeyDeviceManager.getPasskeyDeviceManager();
        this._passkeyDeviceManager.connectObject(
            'passkey-inserted', () => this._handlePasskeyChanged(),
            'passkey-removed', () => this._handlePasskeyChanged(),
            this);
    }

    _connectFingerprintManager() {
        // Fingerprint can only work on lockscreen
        if (!this._reauthOnly)
            return;

        this._fingerprintManager = FingerprintManager.getFingerprintManager();
        this._fingerprintManager.connectObject(
            'reader-type-changed', () => this._handleFingerprintChanged(),
            this);
    }

    _waitPendingMessages() {
        const cancellable = this._cancellable;
        const timeoutId = GLib.timeout_add_seconds_once(GLib.PRIORITY_DEFAULT, 10,
            () => cancellable.cancel());

        const {promise, resolve, reject} = Promise.withResolvers();
        const task = Gio.Task.new(this, cancellable, () => {
            try {
                const res = task.propagate_boolean();
                if (!res)
                    throw new GLib.Error(Gio.IOErrorEnum, Gio.IOErrorEnum.FAILED, 'Operation failed');
                resolve();
            } catch (e) {
                reject(e);
            } finally {
                GLib.source_remove(timeoutId);
            }
        });

        this.emit('wait-pending-messages', task);

        return promise;
    }

    _updateUserVerifier(proxies) {
        this._disconnectUserVerifierSignals();
        this._userVerifier = proxies.userVerifier;
        this._userVerifierChoiceList = proxies.userVerifierChoiceList;
        this._userVerifierCustomJSON = proxies.userVerifierCustomJSON;
        this._connectUserVerifierSignals();
    }

    _connectUserVerifierSignals() {
        this._userVerifier.get_connection().connectObject(
            'closed', () => this._clearUserVerifier(),
            this);

        this._userVerifier.connectObject(
            'info', (_, ...args) => this._onInfo(...args),
            'problem', (_, ...args) => this._onProblem(...args),
            'info-query', (_, ...args) => this._onInfoQuery(...args),
            'secret-info-query', (_, ...args) => this._onSecretInfoQuery(...args),
            'conversation-started', (_, ...args) => this._onConversationStarted(...args),
            'conversation-stopped', (_, ...args) => this._onConversationStopped(...args),
            'service-unavailable', (_, ...args) => this._onServiceUnavailable(...args),
            'reset', () => this.emit('reset', {}),
            'verification-complete', (_, ...args) => this._onVerificationComplete(...args),
            this);

        this._userVerifierChoiceList?.connectObject(
            'choice-query', (_, ...args) => this._onChoiceListQuery(...args),
            this);

        this._userVerifierCustomJSON?.connectObject(
            'request', (_, ...args) => this._onCustomJSONRequest(...args),
            this);
    }

    _onInfo(serviceName, info) {
        this._handleOnInfo(serviceName, info);
    }

    _onProblem(serviceName, problem) {
        this._handleOnProblem(serviceName, problem);
    }

    _onInfoQuery(serviceName, question) {
        this._handleOnInfoQuery(serviceName, question);
    }

    _onSecretInfoQuery(serviceName, secretQuestion) {
        this._handleOnSecretInfoQuery(serviceName, secretQuestion);
    }

    _onConversationStarted(serviceName) {
        this._activeServices.add(serviceName);

        this._handleOnConversationStarted(serviceName);
    }

    _onConversationStopped(serviceName) {
        this._activeServices.delete(serviceName);

        this.emit('filter-messages', serviceName, Util.MessageType.ERROR);

        this._handleOnConversationStopped(serviceName);
    }

    _onServiceUnavailable(serviceName, errorMessage) {
        this._unavailableServices.add(serviceName);

        if (this._selectedMechanism?.serviceName === serviceName && errorMessage) {
            this.emit('queue-message',
                serviceName,
                errorMessage,
                Util.MessageType.ERROR);
        }

        this._handleOnServiceUnavailable(serviceName, errorMessage);
    }

    _onVerificationComplete(serviceName) {
        this._handleOnVerificationComplete(serviceName);
        this.emit('verification-complete');
    }

    _onChoiceListQuery(serviceName, promptMessage, list) {
        this._handleOnChoiceListQuery(serviceName, promptMessage, list);
    }

    _onCustomJSONRequest(serviceName, protocol, version, json) {
        this._handleOnCustomJSONRequest(serviceName, protocol, version, json);
    }

    _canRetry() {
        return this._userName &&
            (this._reauthOnly || this._failCounter < this._allowedFailures);
    }

    async _verificationFailed(serviceName, shouldRetry) {
        this._handleVerificationFailed(serviceName);

        const doneTrying = !shouldRetry || !this._canRetry();

        this.emit('verification-failed', serviceName, !doneTrying);

        try {
            await this._waitPendingMessages();
            this.emit('reset', {softReset: !doneTrying});
        } catch (e) {
            logErrorUnlessCancelled(e);
        }
    }

    async _startServices(cancellable) {
        for (const serviceName of this._getEnabledServices()) {
            if (this._canStartService(serviceName)) {
                // eslint-disable-next-line no-await-in-loop
                await this._startService(serviceName, cancellable);
            }
        }
    }

    _getEnabledServices() {
        const services = this._enabledRoles
            .map(r => this._roleToService[r])
            .filter(s => s); // filter undefined

        services.push(...this._getCredentialManagerServices());

        // Remove duplicates
        return [...new Set(services)];
    }

    _getCredentialManagerServices() {
        return this._handleGetCredentialManagerServices();
    }

    _canStartService(serviceName) {
        return !this._activeServices.has(serviceName) &&
            !this._unavailableServices.has(serviceName) &&
            this._handleCanStartService(serviceName);
    }

    async _startService(serviceName, cancellable) {
        try {
            this._activeServices.add(serviceName);
            if (this._userName) {
                await this._userVerifier.call_begin_verification_for_user(
                    serviceName, this._userName, cancellable);
            } else {
                await this._userVerifier.call_begin_verification(
                    serviceName, cancellable);
            }
        } catch (e) {
            this._activeServices.delete(serviceName);
            if (e instanceof GLib.Error &&
                Gio.DBusError.is_remote_error(e) &&
                Gio.DBusError.get_remote_error(e) ===
                'org.gnome.DisplayManager.SessionWorker.Error.ServiceUnavailable')
                this._unavailableServices.add(serviceName);

            throw new Util.InitError(e,
                this._userName
                    ? `Failed to start ${serviceName} verification for user`
                    : `Failed to start ${serviceName} verification`,
                serviceName);
        }
    }

    _handleGetSupportedRoles() {
        return this.constructor.SupportedRoles;
    }

    _handleSelectChoice() {}

    _handleAnswerQuery() {}

    _handleBeginVerification() {}

    _handleSelectMechanism() {}

    _handleNeedsUsername() {
        return true;
    }

    _handleReset() {}

    _handleCancel() {}

    _handleClear() {}

    _handleUpdateEnabledRoles() {}

    _handleUpdateEnabledMechanisms() {
        throw new GObject.NotImplementedError(
            `_handleUpdateEnabledMechanisms in ${this.constructor.name}`);
    }

    _handleSmartcardChanged() {}

    _handlePasskeyChanged() {}

    _handleFingerprintChanged() {}

    _handleOnInfo() {}

    _handleOnProblem() {}

    _handleOnInfoQuery() {}

    _handleOnSecretInfoQuery() {}

    _handleOnConversationStarted() {}

    _handleOnConversationStopped() {}

    _handleOnServiceUnavailable() {}

    _handleOnVerificationComplete() {}

    _handleOnChoiceListQuery() {}

    _handleOnCustomJSONRequest() {}

    _handleVerificationFailed() {}

    _handleGetCredentialManagerServices() {
        return [];
    }

    _handleCanStartService() {
        throw new GObject.NotImplementedError(
            `_handleCanStartService in ${this.constructor.name}`);
    }

    addCredentialManager(_serviceName, _credentialManager) {}

    removeCredentialManager(_serviceName) {}
}
