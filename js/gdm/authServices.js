import * as Params from '../misc/params.js';
import {registerDestroyableType} from '../misc/signalTracker.js';
import {logErrorUnlessCancelled} from '../misc/errorUtils.js';
import {MessageType} from './userVerifier.js';
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

export const Role = {
    PASSWORD: 'password',
    SMARTCARD: 'smartcard',
    FINGERPRINT: 'fingerprint',
    PASSKEY: 'passkey',
    WEB_LOGIN: 'eidp',
};

export const RoleProperties = {
    [Role.PASSWORD]: {
        selectable: true,
        preemptiveInput: true,
    },
    [Role.SMARTCARD]: {
        selectable: true,
        hint: _('Insert smartcard'),
    },
    [Role.PASSKEY]: {
        selectable: true,
        hint: _('Insert security key'),
    },
    [Role.WEB_LOGIN]: {
        selectable: true,
    },
    [Role.FINGERPRINT]: {
        iconName: 'fingerprint-auth-symbolic',
        description: _('Unlock with fingerprint'),
    },
};

export class AuthService extends GObject.Object {
    static [GObject.signals] = {
        'destroy': {},
        'queue-message': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_UINT, GObject.TYPE_BOOLEAN],
        },
        'queue-priority-message': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_UINT, GObject.TYPE_BOOLEAN],
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
            param_types: [GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_BOOLEAN, GObject.TYPE_JSOBJECT],
        },
        'reset': {
            param_types: [GObject.TYPE_JSOBJECT],
        },
        'show-choice-list': {
            param_types: [GObject.TYPE_STRING, GObject.TYPE_STRING, GObject.TYPE_JSOBJECT, GObject.TYPE_JSOBJECT],
        },
        'show-button': {
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

    static ServiceName = null;

    static Background = false;

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
    }

    get selectedMechanism() {
        return this._selectedMechanism;
    }

    get enabledMechanisms() {
        return this._enabledMechanisms?.filter(m => m.ready !== false);
    }

    get serviceName() {
        return this.constructor.ServiceName;
    }

    get isBackground() {
        return this.constructor.Background;
    }

    get _roleToService() {
        const {RoleToService, ServiceName, SupportedRoles} = this.constructor;

        if (Object.keys(RoleToService).length > 0)
            return RoleToService;

        if (ServiceName)
            return Object.fromEntries(SupportedRoles.map(r => [r, ServiceName]));

        return {};
    }

    get supportedRoles() {
        return this._handleGetSupportedRoles();
    }

    async beginVerification(userName, userVerifierProxies) {
        this._cancellable?.cancel();
        this._cancellable = new Gio.Cancellable();
        this._userName = userName;

        let started = false;
        try {
            this._updateUserVerifier(userVerifierProxies);
            started = await this._startServices(this._cancellable);
        } catch (e) {
            if (e.cause?.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                return false;

            this._failCounter++;
            throw e;
        }

        this._handleBeginVerification();
        return started;
    }

    // async startEnabledServices() {
    //     if (!this._cancellable || !this._userVerifier)
    //         return;

    //     await this._startServices(this._cancellable);
    // }

    _mechanismEquals(m1, m2) {
        return  m1?.serviceName === m2?.serviceName &&
            m1?.role === m2?.role;
    }

    selectMechanism(mechanism) {
        if (this._mechanismEquals(this._selectedMechanism, mechanism))
            return false;

        // log("Looking for ",mechanism, "Enabled mechanisms", this._enabledMechanisms);

        this._selectedMechanism = this._enabledMechanisms?.find(
            m => this._mechanismEquals(m, mechanism));

        log("Enabled mechanism", this._selectedMechanism);

        if (!this._selectedMechanism)
            logError(new Error(`Mechanism ${JSON.stringify(mechanism)} not found in enabled mechanisms: ${JSON.stringify(this._enabledMechanisms)}`));

        if (!this._selectedMechanism)
            return false;

        // log(this._enabledMechanisms?.map(m=>m.serviceName), "authService mechanism selection", mechanism, '=>', this.selectedMechanism)

        this._handleSelectMechanism();
        return true;
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

    cancelRequested() {}

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
        this._selectedMechanism = null;
        this._enabledMechanisms = [];

        this._handleUpdateEnabledMechanisms();

        this._enabledMechanisms = this._enabledMechanisms.map(m => ({
            ...m,
            ...RoleProperties[m.role],
        }));

        this.emit('mechanisms-changed');
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

        this.emit('filter-messages', serviceName, MessageType.ERROR);

        this._handleOnConversationStopped(serviceName);
    }

    _onServiceUnavailable(serviceName, errorMessage) {
        this._unavailableServices.add(serviceName);

        if (this._selectedMechanism?.serviceName === serviceName && errorMessage) {
            this.emit('queue-message',
                serviceName,
                errorMessage,
                MessageType.ERROR);
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

    // IMHO serviceName is not needed, it's alreayd one service
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
        let started = false;
        print("Starting service, Enabled services", this._getEnabledServices())
        for (const serviceName of this._getEnabledServices()) {
            // try {
            print("can start", serviceName, this._canStartService(serviceName))
            if (this._canStartService(serviceName)) {
                // eslint-disable-next-line no-await-in-loop
                print("Really starting...", serviceName)
                await this._startService(serviceName, cancellable);
                started = true;
            }
        // } catch (e) {logError(e)}
        }
        return started;
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

            // if (e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
            //     return;

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

    _handleGetSupportedRoles(...args) {
        print("_handleGetSupportedRoles", ...args);
        return this.constructor.SupportedRoles;
    }

    _handleBeginVerification(...args) {
        print("_handleBeginVerification", ...args);
    }

    _handleSelectMechanism(...args) {
        print("_handleSelectMechanism", ...args);
        throw new GObject.NotImplementedError(
            `_handleSelectMechanism in ${this.constructor.name}`);
    }

    _handleNeedsUsername(...args) {
        print("_handleNeedsUsername", ...args);
        return true;
    }

    _handleReset(...args) {
        print("_handleReset", ...args);
    }

    _handleCancel(...args) {
        print("_handleCancel", ...args);
    }

    _handleClear(...args) {
        print("_handleClear", ...args);
    }

    _handleUpdateEnabledRoles(...args) {
        print("_handleUpdateEnabledRoles", ...args);
    }

    _handleUpdateEnabledMechanisms(...args) {
        print("_handleUpdateEnabledMechanisms", ...args);
        throw new GObject.NotImplementedError(
            `_handleUpdateEnabledMechanisms in ${this.constructor.name}`);
    }

    _handleOnInfo(...args) {
        print("_handleOnInfo", ...args);
    }

    _handleOnProblem(...args) {
        print("_handleOnProblem", ...args);
    }

    _handleOnInfoQuery(...args) {
        print("_handleOnInfoQuery", ...args);
    }

    _handleOnSecretInfoQuery(...args) {
        print("_handleOnSecretInfoQuery", ...args);
    }

    _handleOnConversationStarted(...args) {
        print("_handleOnConversationStarted", ...args);
    }

    _handleOnConversationStopped(...args) {
        print("_handleOnConversationStopped", ...args);
    }

    _handleOnServiceUnavailable(...args) {
        print("_handleOnServiceUnavailable", ...args);
    }

    _handleOnVerificationComplete(...args) {
        print("_handleOnVerificationComplete", ...args);
    }

    _handleOnChoiceListQuery(...args) {
        print("_handleOnChoiceListQuery", ...args);
    }

    _handleOnCustomJSONRequest(...args) {
        // print("_handleOnCustomJSONRequest", ...args);
    }

    _handleVerificationFailed(...args) {
        print("_handleVerificationFailed", ...args);
    }

    _handleGetCredentialManagerServices(...args) {
        print("_handleGetCredentialManagerServices", ...args);
        return [];
    }

    _handleCanStartService(...args) {
        print("_handleCanStartService", ...args);
        throw new GObject.NotImplementedError(
            `_handleCanStartService in ${this.constructor.name}`);
    }

    addCredentialManager(_serviceName, _credentialManager) {}

    removeCredentialManager(_serviceName) {}
}

export class BackgroundAuthService extends AuthService {
    static {
        GObject.registerClass(this);
    }

    static Background = true;
}
