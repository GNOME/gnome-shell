import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import * as Signals from '../misc/signals.js';

import * as Batch from './batch.js';
import * as Constants from './constants.js';
import * as Main from '../ui/main.js';
import {logErrorUnlessCancelled} from '../misc/errorUtils.js';
import * as Params from '../misc/params.js';
import {AuthServicesLegacy} from './authServicesLegacy.js';
import {AuthServicesSSSDSwitchable} from './authServicesSSSDSwitchable.js';

export const CLONE_FADE_ANIMATION_TIME = 250;

export const LOGIN_SCREEN_SCHEMA = 'org.gnome.login-screen';
export const PASSWORD_AUTHENTICATION_KEY = 'enable-password-authentication';
export const FINGERPRINT_AUTHENTICATION_KEY = 'enable-fingerprint-authentication';
export const SMARTCARD_AUTHENTICATION_KEY = 'enable-smartcard-authentication';
export const PASSKEY_AUTHENTICATION_KEY = 'enable-passkey-authentication';
export const SWITCHABLE_AUTHENTICATION_KEY = 'enable-switchable-authentication';
export const WEB_AUTHENTICATION_KEY = 'enable-web-authentication';
export const BANNER_MESSAGE_KEY = 'banner-message-enable';
export const BANNER_MESSAGE_SOURCE_KEY = 'banner-message-source';
export const BANNER_MESSAGE_TEXT_KEY = 'banner-message-text';
export const BANNER_MESSAGE_PATH_KEY = 'banner-message-path';
export const ALLOWED_FAILURES_KEY = 'allowed-failures';

export const LOGO_KEY = 'logo';
export const DISABLE_USER_LIST_KEY = 'disable-user-list';

// Give user 48ms to read each character of a PAM message
// or 2 seconds, whichever is longer
const USER_READ_TIME = 48;
const USER_READ_TIME_MIN = 2000;
const MESSAGE_TIME_MULTIPLIER = (() => {
    const value = Number.parseFloat(GLib.getenv('GDM_MESSAGE_TIME_MULTIPLIER'));
    return Number.isFinite(value) && value > 0 ? value : 1;
})();

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

/**
 * Error thrown during the authentication initialization phase.
 *
 * This error is emitted when requesting user verifier proxies or starting
 * a service via beginVerification fails. It wraps the underlying error
 * and provides context about which service failed.
 */
export class InitError extends Error {
    constructor(error, message, serviceName) {
        super(message, {cause: error});
        this.serviceName = serviceName;
    }
}

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

    const [x, y] = actor.get_transformed_position();
    clone.set_position(x, y);

    const hold = new Batch.Hold();
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

/**
 * @param {object} mechanism
 * @returns {boolean}
 */
export function isSelectable(mechanism) {
    switch (mechanism.role) {
    case Constants.PASSWORD_ROLE_NAME:
    case Constants.SMARTCARD_ROLE_NAME:
    case Constants.PASSKEY_ROLE_NAME:
    case Constants.WEB_LOGIN_ROLE_NAME:
        return true;
    case Constants.FINGERPRINT_ROLE_NAME:
        return false;
    default:
        throw new Error(`Failed checking mechanism is selectable: ${mechanism.role}`);
    }
}

/**
 * @param {object} mechanism
 * @returns {string}
 */
export function getNonSelectableIconName(mechanism) {
    // This is only used for non selectable mechanisms.
    // Currently only fingerprint is non selectable
    if (isSelectable(mechanism))
        throw new Error(`Failed getting mechanism icon: ${mechanism.role}, is selectable`);

    switch (mechanism.role) {
    case Constants.FINGERPRINT_ROLE_NAME:
        return 'fingerprint-auth-symbolic';
    default:
        throw new Error(`Failed getting mechanism icon: ${mechanism.role}`);
    }
}

export class ShellUserVerifier extends Signals.EventEmitter {
    constructor(client, params) {
        super();
        params = Params.parse(params, {reauthenticationOnly: false});
        this._reauthOnly = params.reauthenticationOnly;

        this._client = client;
        this._cancellable = null;

        this._messageQueue = [];
        this._messageQueueTimeoutId = 0;

        this._settings = new Gio.Settings({schema_id: LOGIN_SCREEN_SCHEMA});
        this._settings.connect('changed', () => this._onSettingsChanged());
        this._updateAuthServices();
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

    async begin(userName, hold) {
        this._cancellable?.cancel();
        this._cancellable = new Gio.Cancellable();

        try {
            const proxies = await this._getUserVerifierProxies(userName, this._cancellable);
            this._setUserVerifier(proxies.userVerifier);
            await this._authServicesSSSDSwitchable?.beginVerification(userName, proxies);
            await this._authServicesLegacy?.beginVerification(userName, proxies);
        } catch (e) {
            if (e instanceof InitError)
                this._reportInitError(e);
        }

        hold?.release();
    }

    selectMechanism(mechanism) {
        let selected = false;
        selected |= this._authServicesSSSDSwitchable?.selectMechanism(mechanism);
        selected |= this._authServicesLegacy?.selectMechanism(mechanism);
        return selected;
    }

    needsUsername() {
        return this._authServicesSSSDSwitchable?.needsUsername() ||
            this._authServicesLegacy?.needsUsername();
    }

    reset() {
        this._authServicesSSSDSwitchable?.reset();
        this._authServicesLegacy?.reset();

        this._userVerifier?.call_cancel_sync(null);

        this.clear();
    }

    cancel() {
        this._authServicesSSSDSwitchable?.cancel();
        this._authServicesLegacy?.cancel();

        this._userVerifier?.call_cancel_sync(null);

        this.clear();
    }

    clear() {
        this._authServicesSSSDSwitchable?.clear();
        this._authServicesLegacy?.clear();

        if (this._authServicesSSSDSwitchable) {
            this._authServicesLegacy?.updateEnabledRoles(
                this._authServicesSSSDSwitchable.unsupportedRoles);
        }

        this._clearMessageQueue();

        this._cancellable?.cancel();
        this._cancellable = null;

        this._clearUserVerifier();
    }

    _setUserVerifier(userVerifier) {
        this._clearUserVerifier();
        this._userVerifier = userVerifier;
        this._userVerifier.get_connection().connectObject(
            'closed', () => this._clearUserVerifier(),
            this);
    }

    _clearUserVerifier() {
        this._userVerifier?.get_connection().disconnectObject(this);
        this._userVerifier = null;
    }

    destroy() {
        this._authServicesSSSDSwitchable?.destroy();
        this._authServicesSSSDSwitchable = null;

        this._authServicesLegacy?.destroy();
        this._authServicesLegacy = null;

        this.cancel();

        this._settings.run_dispose();
        this._settings = null;
    }

    selectChoice(serviceName, key) {
        this._authServicesSSSDSwitchable?.selectChoice(serviceName, key);
        this._authServicesLegacy?.selectChoice(serviceName, key);
    }

    async answerQuery(serviceName, answer) {
        // Wait for pending messages to be displayed before answering to
        // ensure no messages get lost
        await this._handlePendingMessages().catch(logErrorUnlessCancelled);

        this._authServicesSSSDSwitchable?.answerQuery(serviceName, answer);
        this._authServicesLegacy?.answerQuery(serviceName, answer);
    }

    addCredentialManager(serviceName, credentialManager) {
        this._authServicesLegacy?.addCredentialManager(serviceName, credentialManager);
    }

    removeCredentialManager(serviceName) {
        this._authServicesLegacy?.removeCredentialManager(serviceName);
    }

    _getIntervalForMessage(message) {
        if (!message)
            return 0;

        // We probably could be smarter here
        return Math.max(message.length * USER_READ_TIME, USER_READ_TIME_MIN) *
            MESSAGE_TIME_MULTIPLIER;
    }

    _finishMessageQueue() {
        if (!this.hasPendingMessages)
            return;

        this._messageQueue = [];

        this.emit('no-more-messages');
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

    async _queueMessageTimeout() {
        if (this._messageQueueTimeoutId !== 0 || this._showMessageResolver)
            return;

        const message = this.currentMessage;
        const {promise, resolve} = Promise.withResolvers();
        this._showMessageResolver = resolve;

        this.emit('show-message', message.serviceName, message.text, message.type, this._showMessageResolver);

        await promise.catch(logError);
        if (!this._showMessageResolver)
            return;
        this._showMessageResolver = null;

        this._messageQueueTimeoutId = GLib.timeout_add_once(GLib.PRIORITY_DEFAULT,
            message.interval, () => {
                this._messageQueueTimeoutId = 0;

                if (this._messageQueue.length > 1) {
                    this._messageQueue.shift();
                    this._queueMessageTimeout();
                } else {
                    this._finishMessageQueue();
                }
            });
        GLib.Source.set_name_by_id(this._messageQueueTimeoutId, '[gnome-shell] this._queueMessageTimeout');
    }

    _queueMessage(serviceName, message, messageType) {
        const interval = this._getIntervalForMessage(message);

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
        this._finishMessageQueue();

        if (this._messageQueueTimeoutId !== 0) {
            GLib.source_remove(this._messageQueueTimeoutId);
            this._messageQueueTimeoutId = 0;
        }

        if (this._showMessageResolver) {
            this._showMessageResolver();
            this._showMessageResolver = null;
        }

        this.emit('show-message', null, null, MessageType.NONE);
    }

    _reportInitError(initError) {
        const {cause, message, serviceName} = initError;

        logError(cause, message);

        this._queueMessage(serviceName, _('Authentication error'), MessageType.ERROR);
        this._verificationFailed(serviceName, false);
    }

    async _getUserVerifierProxies(userName, cancellable) {
        const proxies = {};

        if (userName) {
            try {
                proxies.userVerifier = await this._client.open_reauthentication_channel(
                    userName, cancellable);
            } catch (e) {
                if (e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                    throw e;
                if (e.matches(Gio.DBusError, Gio.DBusError.ACCESS_DENIED) &&
                    !this._reauthOnly) {
                    // Gdm emits org.freedesktop.DBus.Error.AccessDenied when there
                    // is no session to reauthenticate. Fall back to performing
                    // verification from this login session
                    return this._getUserVerifierProxies(null, cancellable);
                }
                throw new InitError(e, 'Failed to open reauthentication channel');
            }
        } else {
            try {
                proxies.userVerifier = await this._client.get_user_verifier(
                    cancellable);
            } catch (e) {
                if (e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                    throw e;
                throw new InitError(e, 'Failed to obtain user verifier');
            }
        }

        try {
            if (this._client.get_user_verifier_choice_list)
                proxies.userVerifierChoiceList = await this._client.get_user_verifier_choice_list();
            if (this._client.get_user_verifier_custom_json)
                proxies.userVerifierCustomJSON = await this._client.get_user_verifier_custom_json();
        } catch (e) {
            throw new InitError(e, 'Failed to obtain user verifier extensions');
        }

        return proxies;
    }

    serviceIsFingerprint(serviceName) {
        return serviceName === Constants.FINGERPRINT_SERVICE_NAME;
    }

    _onSettingsChanged() {
        this._updateAuthServices();
    }

    _updateAuthServices() {
        const enabledRoles = [];

        if (this._settings.get_boolean(PASSWORD_AUTHENTICATION_KEY))
            enabledRoles.push(Constants.PASSWORD_ROLE_NAME);
        if (this._settings.get_boolean(SMARTCARD_AUTHENTICATION_KEY))
            enabledRoles.push(Constants.SMARTCARD_ROLE_NAME);
        if (this._settings.get_boolean(PASSKEY_AUTHENTICATION_KEY))
            enabledRoles.push(Constants.PASSKEY_ROLE_NAME);
        if (this._settings.get_boolean(FINGERPRINT_AUTHENTICATION_KEY))
            enabledRoles.push(Constants.FINGERPRINT_ROLE_NAME);
        if (this._settings.get_boolean(WEB_AUTHENTICATION_KEY))
            enabledRoles.push(Constants.WEB_LOGIN_ROLE_NAME);

        const switchableAuthentication =
            this._settings.get_boolean(SWITCHABLE_AUTHENTICATION_KEY);

        if (JSON.stringify(enabledRoles) === JSON.stringify(this._enabledRoles) &&
            switchableAuthentication === this._switchableAuthenticationEnabled)
            return;

        this._enabledRoles = enabledRoles;
        this._switchableAuthenticationEnabled = switchableAuthentication;

        this._createAuthServices();
    }

    _createAuthServices() {
        this._clearAuthServices();

        const params = {
            client: this._client,
            enabledRoles: this._enabledRoles,
            allowedFailures: this.allowedFailures,
            reauthOnly: this._reauthOnly,
        };
        if (this._switchableAuthenticationEnabled &&
            AuthServicesSSSDSwitchable.supportsAny(this._enabledRoles)) {
            this._authServicesSSSDSwitchable = new AuthServicesSSSDSwitchable(params);

            params.enabledRoles = this._authServicesSSSDSwitchable.unsupportedRoles;
            this._authServicesLegacy = new AuthServicesLegacy(params);
        } else if (AuthServicesLegacy.supportsAny(this._enabledRoles)) {
            this._authServicesLegacy = new AuthServicesLegacy(params);
        }

        this._connectAuthServices();
    }

    _clearAuthServices() {
        this._authServicesSSSDSwitchable?.destroy();
        this._authServicesSSSDSwitchable = null;
        this._authServicesLegacy?.destroy();
        this._authServicesLegacy = null;
    }

    _connectAuthServices() {
        [
            this._authServicesSSSDSwitchable,
            this._authServicesLegacy,
        ].forEach(authServices => {
            authServices?.connectObject(
                'ask-question', (_, ...args) => this.emit('ask-question', ...args),
                'queue-message', (_, ...args) => this._queueMessage(...args),
                'queue-priority-message', (_, ...args) => this._queuePriorityMessage(...args),
                'wait-pending-messages', (_, ...args) => this._waitPendingMessages(...args),
                'filter-messages', (_, ...args) => this._filterServiceMessages(...args),
                'verification-failed', (_, ...args) => this._verificationFailed(...args),
                'verification-complete', (_, ...args) => this.emit('verification-complete', ...args),
                'reset', (_, ...args) => this.emit('reset', ...args),
                'show-choice-list', (_, ...args) => this.emit('show-choice-list', ...args),
                'mechanisms-changed', (_, ...args) => this._onMechanismsChanged(...args),
                'web-login', (_, ...args) => this.emit('web-login', ...args),
                this);
        });
    }

    _verificationFailed(serviceName, canRetry) {
        this._filterServiceMessages(serviceName, MessageType.ERROR);
        this.emit('verification-failed', serviceName, canRetry);
    }

    get selectedMechanism() {
        return this._authServicesSSSDSwitchable?.selectedMechanism ??
            this._authServicesLegacy?.selectedMechanism ??
            null;
    }

    _onMechanismsChanged() {
        if (this._enableFallbackMechanisms())
            return;

        const mechanismsSSSDSwitchable = this._authServicesSSSDSwitchable?.enabledMechanisms ?? [];
        const mechanismsLegacy = this._authServicesLegacy?.enabledMechanisms ?? [];
        const mechanisms = [...mechanismsSSSDSwitchable, ...mechanismsLegacy];

        const selectedMechanism = this.selectedMechanism ??
            mechanisms.find(m => isSelectable(m)) ??
            {};

        this.emit('mechanisms-changed', mechanisms, selectedMechanism);
    }

    _enableFallbackMechanisms() {
        if (!this._authServicesSSSDSwitchable || !this._authServicesLegacy)
            return false;

        return this._authServicesLegacy.updateEnabledRoles(
            this._authServicesSSSDSwitchable.unsupportedRoles);
    }

    async _waitPendingMessages(task) {
        try {
            await this._handlePendingMessages();
            task.return_boolean(true);
        } catch (e) {
            task.return_error(e);
        }
    }

    _handlePendingMessages() {
        if (!this.hasPendingMessages)
            return Promise.resolve();

        const cancellable = this._cancellable;
        return new Promise((resolve, reject) => {
            const signalId = this.connect('no-more-messages', () => {
                this.disconnect(signalId);
                if (cancellable.is_cancelled())
                    reject(new GLib.Error(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED, 'Operation was cancelled'));
                else
                    resolve();
            });
        });
    }
}
