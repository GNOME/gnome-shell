// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

import * as FingerprintManager from '../misc/fingerprintManager.js';
import * as Params from '../misc/params.js';
import * as SmartcardManager from '../misc/smartcardManager.js';
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

export class AuthServices extends GObject.Object {
    static [GObject.GTypeName] = 'AuthServices';

    static [GObject.signals] = {
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
    };

    static {
        GObject.registerClass(this);
    }

    static SupportedRoles = [];
    static RoleToService = {};

    static supportsAny(roles) {
        return roles.some(r => this.SupportedRoles.includes(r));
    }

    constructor(params) {
        super();
        params = Params.parse(params, {
            client: null,
            enabledRoles: [],
            allowedFailures: 3,
            reauthOnly: false,
        });

        this._client = params.client;
        this._enabledRoles = params.enabledRoles;
        this._allowedFailures = params.allowedFailures;
        this._reauthOnly = params.reauthOnly;

        this._failCounter = 0;
        this._activeServices = new Set();
        this._unavailableServices = new Set();

        this._cancellable = null;

        this._connectSmartcardManager();
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
        return this.constructor.SupportedRoles;
    }

    selectChoice(serviceName, key) {
        this._handleSelectChoice(serviceName, key);
    }

    async answerQuery(serviceName, answer) {
        try {
            await this._waitPendingMessages();
            await this._handleAnswerQuery(serviceName, answer);
        } catch (e) {
            logErrorUnlessCancelled(e);
        }
    }

    async beginVerification(userName, userVerifierProxies) {
        this._cancellable?.cancel();
        this._cancellable = new Gio.Cancellable();
        this._userName = userName;

        try {
            this._updateUserVerifier(userVerifierProxies);
            await this._startServices(this._cancellable);
        } catch (e) {
            if (e.error?.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
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

    clear() {
        this._cancellable?.cancel();
        this._cancellable = null;

        this._unavailableServices.clear();
        this._activeServices.clear();

        this._verificationComplete = false;

        this._clearUserVerifier();

        this._handleClear();
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
        return new Promise((resolve, reject) => {
            let done = false;
            const safeResolve = () => {
                if (!done) {
                    done = true;
                    if (cancellable?.is_cancelled()) {
                        reject(new GLib.Error(
                            Gio.IOErrorEnum,
                            Gio.IOErrorEnum.CANCELLED,
                            'Operation was cancelled'));
                    } else {
                        resolve();
                    }
                }
            };
            const safeReject = err => {
                if (!done) {
                    done = true;
                    reject(err);
                }
            };
            const waiter = {
                resolve: safeResolve,
                reject: safeReject,
            };
            this.emit('wait-pending-messages', waiter);

            GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 10, () => {
                safeReject(new Error('Timed out waiting for pending messages'));
                return GLib.SOURCE_REMOVE;
            });
        });
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

    _handleSelectChoice() {}

    async _handleAnswerQuery() {}

    _handleBeginVerification() {}

    _handleSelectMechanism() {}

    _handleNeedsUsername() {
        return true;
    }

    _handleReset() {}

    _handleCancel() {}

    _handleClear() {}

    _handleUpdateEnabledMechanisms() {
        throw new GObject.NotImplementedError(
            `_handleUpdateEnabledMechanisms in ${this.constructor.name}`);
    }

    _handleSmartcardChanged() {}

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
}
