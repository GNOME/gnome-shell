// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

import * as Const from './const.js';
import * as Params from '../misc/params.js';
import * as SmartcardManager from '../misc/smartcardManager.js';
import * as Util from './util.js';
import Gdm from 'gi://Gdm';
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';

Gio._promisify(Gdm.Client.prototype, 'open_reauthentication_channel');
Gio._promisify(Gdm.Client.prototype, 'get_user_verifier');
Gio._promisify(Gdm.UserVerifierProxy.prototype, 'call_begin_verification_for_user');
Gio._promisify(Gdm.UserVerifierProxy.prototype, 'call_begin_verification');

export const AuthServices = GObject.registerClass({
    Signals: {
        'queue-message': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_UINT]
        },
        'queue-priority-message': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_UINT]
        },
        'wait-pending-messages': {
            param_types: [GObject.TYPE_JSOBJECT]
        },
        'filter-messages': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_UINT]
        },
        'verification-failed': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_BOOLEAN]
        },
        'verification-complete': {},
        'ask-question': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_BOOLEAN]
        },
        'reset': {
            param_types: [GObject.TYPE_JSOBJECT]
        },
        'credential-manager-authenticated': {},
        'show-choice-list': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_JSOBJECT]
        },
        'mechanisms-changed' : {},
        'web-login': {
            param_types: [
                GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_STRING,
                GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_JSOBJECT
            ]
        },
        'web-login-failed': {
            param_types: [GObject.TYPE_STRING]
        },
    },
}, class AuthServices extends GObject.Object {
    static SupportedRoles = [];
    static RoleToService = {};
    static supports(roles) {
        return roles.some(r => this.SupportedRoles.includes(r));
    }

    _init(params) {
        super._init();
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
    }

    get selectedMechanism() {
        return this._selectedMechanism;
    }

    get enabledMechanisms() {
        return this._enabledMechanisms;
    }

    get roleToService() {
        return this.constructor.RoleToService;
    }

    get supportedRoles() {
        return this.constructor.SupportedRoles;
    }

    get unsupportedRoles() {
        return this._handleGetUnsupportedRoles();
    }

    selectChoice(serviceName, key) {
        this._handleSelectChoice(serviceName, key);
    }

    async answerQuery(serviceName, answer) {
        try {
            await this._waitPendingMessages();
            await this._handleAnswerQuery(serviceName, answer);
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                logError(e);
        }
    }

    async beginVerification(userName) {
        if (!this._cancellable)
            this._cancellable = new Gio.Cancellable();
        this._userName = userName;

        try {
            await this._getUserVerifier(userName);
            await this._startServices();
        } catch (e) {
            if (e.error?.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                return;

            this._failCounter++;
            this._verificationFailed(serviceName, false);
            throw e;
        }

        this._handleBeginVerification();
    }

    selectMechanism(mechanism) {
        if (this._selectedMechanism?.role === mechanism.role &&
            this._selectedMechanism?.serviceName === mechanism.serviceName)
            return;

        this._selectedMechanism = this._enabledMechanisms?.find(m =>
            m.role === mechanism.role &&
            m.serviceName === mechanism.serviceName
        );

        this._handleSelectMechanism();
    }

    needsUsername() {
        return this._handleNeedsUsername();
    }

    reset() {
        this._failCounter = 0;

        this._handleReset();
    }

    cancel() {
        this._userVerifier?.call_cancel_sync(null);
    }

    clear() {
        this._cancellable?.cancel();
        this._cancellable = null;

        this._unavailableServices.clear();
        this._activeServices.clear();

        this._clearUserVerifier();

        this._handleClear();
    }

    updateEnabledRoles(roles) {
        if (this._enabledRoles.length === roles.length &&
            this._enabledRoles.every(r => roles.includes(r))) {
            return false
        }
        this._enabledRoles = roles;

        this._handleUpdateEnabledRoles();

        return true;
    }

    _clearUserVerifier() {
        this._disconnectUserVerifierSignals();
        this._userVerifier = null;
        this._userVerifierChoiceList = null;
        this._userVerifierCustomJSON = null;
    }

    _disconnectUserVerifierSignals() {
        this._userVerifier?.disconnectObject(this);
        this._userVerifierChoiceList?.disconnectObject(this);
        this._userVerifierCustomJSON?.disconnectObject(this);
    }

    _updateEnabledMechanisms() {
        this._enabledMechanisms = [];

        this._handleUpdateEnabledMechanisms();

        if (this._enabledMechanisms.some(m => m.role === Const.SMARTCARD_ROLE_NAME))
            this._connectSmartcardManager();
        else
            this._disconnectSmartcardManager();

        this.emit('mechanisms-changed');
    }

    _connectSmartcardManager() {
        if (this._smartcardManager)
            return;

        this._smartcardManager = SmartcardManager.getSmartcardManager();
        this._smartcardManager.connectObject(
            'smartcard-inserted', () => this._smartcardChanged(),
            'smartcard-removed', () => this._smartcardChanged(),
            this);

        this._smartcardDetected = this._smartcardManager.hasInsertedTokens();
    }

    _disconnectSmartcardManager() {
        this._smartcardManager?.disconnectObject(this);
        this._smartcardManager = null;
        this._smartcardDetected = false;
    }

    _smartcardChanged() {
        const smartcardDetected = this._smartcardManager.hasInsertedTokens();

        if (smartcardDetected !== this.smartcardDetected) {
            this._smartcardDetected = smartcardDetected;
            this._handleSmartcardChanged();
        }
    }

    _waitPendingMessages() {
        const cancellable = this._cancellable;
        return new Promise((resolve, reject) => {
            let done = false;
            const safeResolve = () => {
                if (!done) {
                    done = true;
                    if (cancellable?.is_cancelled())
                        reject(new GLib.Error(
                            Gio.IOErrorEnum,
                            Gio.IOErrorEnum.CANCELLED,
                            'Operation was cancelled'));
                    else
                        resolve();
                }
            };
            const safeReject = (err) => {
                if (!done) {
                    done = true;
                    reject(err);
                }
            };
            const waiter = {
                resolve: safeResolve,
                reject: safeReject
            };
            this.emit('wait-pending-messages', waiter);

            GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 10, () => {
                safeReject(new Error('Timed out waiting for pending messages'));
                return GLib.SOURCE_REMOVE;
            });
        });
    }

    async _getUserVerifier(userName) {
        this._clearUserVerifier();

        if (userName) {
            try {
                this._userVerifier = await this._client.open_reauthentication_channel(
                    userName, this._cancellable);
            } catch (e) {
                if (e.matches(Gio.DBusError, Gio.DBusError.ACCESS_DENIED) &&
                    !this._reauthOnly) {
                    // Gdm emits org.freedesktop.DBus.Error.AccessDenied when there
                    // is no session to reauthenticate. Fall back to performing
                    // verification from this login session
                    await this._getUserVerifier();
                    return;
                }

                throw new Util.InitError(e, 'Failed to open reauthentication channel');
            }
        } else {
            try {
                this._userVerifier = await this._client.get_user_verifier(
                    this._cancellable);
            } catch (e) {
                throw new Util.InitError(e, 'Failed to obtain user verifier');
            }
        }

        try {
            await this._getUserVerifierExtensions();
        } catch (e) {
            throw new Util.InitError(e, 'Failed to obtain user verifier extensions');
        }

        this._connectUserVerifierSignals();
    }

    async _getUserVerifierExtensions() {
        if (this._client.get_user_verifier_choice_list)
            this._userVerifierChoiceList = await this._client.get_user_verifier_choice_list();
        else
            this._userVerifierChoiceList = null;

        if (this._client.get_user_verifier_custom_json)
            this._userVerifierCustomJSON = await this._client.get_user_verifier_custom_json();
        else
            this._userVerifierCustomJSON = null;
    }

    _connectUserVerifierSignals() {
        this._userVerifier.connectObject(
            'info', (_, ...args) => this._onInfo(...args),
            'problem', (_, ...args) => this._onProblem(...args),
            'info-query', (_, ...args) => this._onInfoQuery(...args),
            'secret-info-query', (_, ...args) => this._onSecretInfoQuery(...args),
            'conversation-started', (_, ...args) => this._onConversationStarted(...args),
            'conversation-stopped', (_, ...args) => this._onConversationStopped(...args),
            'service-unavailable', (_, ...args) => this._onServiceUnavailable(...args),
            'reset', () => this.emit('reset'),
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

        if (this._selectedMechanism?.serviceName === serviceName && errorMessage)
            this.emit('queue-message',
                serviceName,
                errorMessage,
                Util.MessageType.ERROR);

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
            this.emit('reset', { softReset: !doneTrying });
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                logError(e);
        }
    }

    async _startServices() {
        for (const serviceName of this._getEnabledServices()) {
            if (this._canStartService(serviceName)) {
                await this._startService(serviceName);
            }
        }
    }

    _getEnabledServices() {
        const services = this._enabledRoles
            .map(r => this.roleToService[r])
            .filter(s => s);
        return [...new Set(services)];
    }

    _canStartService(serviceName) {
        return !this._activeServices.has(serviceName) &&
            !this._unavailableServices.has(serviceName) &&
            this._handleCanStartService(serviceName);
    }

    async _startService(serviceName) {
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
            if (error instanceof GLib.Error &&
                Gio.DBusError.is_remote_error(error) &&
                Gio.DBusError.get_remote_error(error) ===
                'org.gnome.DisplayManager.SessionWorker.Error.ServiceUnavailable') {
                this._unavailableServices.add(serviceName);
            }

            throw new Util.InitError(e,
                 this._userName
                     ? `Failed to start ${serviceName} verification for user`
                     : `Failed to start ${serviceName} verification`,
                serviceName);
        }
    }

    _handleGetUnsupportedRoles() {
        return [];
    }

    _handleSelectChoice() {}

    async _handleAnswerQuery() {}

    _handleBeginVerification() {}

    _handleSelectMechanism() {}

    _handleNeedsUsername() {
        return true;
    }

    _handleReset() {}

    _handleClear() {}

    _handleUpdateEnabledRoles() {
        return false;
    }

    _handleUpdateEnabledMechanisms() {}

    _handleSmartcardChanged() {}

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

    _handleCanStartService() {
        return false;
    }
});
