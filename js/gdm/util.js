import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import * as Signals from '../misc/signals.js';

import * as Batch from './batch.js';
import * as Main from '../ui/main.js';
import {logErrorUnlessCancelled} from '../misc/errorUtils.js';
import * as Params from '../misc/params.js';
import {AuthServicesLegacy} from './authServicesLegacy.js';
import {AuthServicesSSSDSwitchable} from './authServicesSSSDSwitchable.js';

export const CLONE_FADE_ANIMATION_TIME = 250;

export const LOGIN_SCREEN_SCHEMA = 'org.gnome.login-screen';
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

// Priority-ordered: earlier entries take precedence for shared roles.
// Each authServices claims the roles it supports; unsupported roles
// cascade to the next authServices in the array.
const AuthServicesClasses = [
    AuthServicesSSSDSwitchable,
    AuthServicesLegacy,
];

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

export class ShellUserVerifier extends Signals.EventEmitter {
    constructor(client, params) {
        super();
        params = Params.parse(params, {reauthenticationOnly: false});
        this._reauthOnly = params.reauthenticationOnly;

        this._client = client;
        this._cancellable = null;
        this._authServices = [];

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
            for (const s of this._authServices) {
                // eslint-disable-next-line no-await-in-loop
                await s.beginVerification(userName, proxies);
            }
        } catch (e) {
            if (e instanceof InitError)
                this._reportInitError(e);
        }

        hold?.release();
    }

    selectMechanism(mechanism) {
        // Every authServices needs to update its selected mechanism
        return this._authServices
            .map(s => s.selectMechanism(mechanism))
            .some(Boolean);
    }

    needsUsername() {
        return this._authServices.some(s => s.needsUsername());
    }

    reset() {
        this._authServices.forEach(s => s.reset());

        this._userVerifier?.call_cancel_sync(null);

        this.clear();
    }

    cancel() {
        this._authServices.forEach(s => s.cancel());

        this._userVerifier?.call_cancel_sync(null);

        this.clear();
    }

    clear() {
        this._authServices.forEach(s => s.clear());
        this._redistributeRoles();

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
        this._authServices.forEach(s => s.destroy());
        this._authServices = [];

        this.cancel();

        this._settings.run_dispose();
        this._settings = null;
    }

    selectChoice(serviceName, key) {
        this._authServices.forEach(s => s.selectChoice(serviceName, key));
    }

    async answerQuery(serviceName, answer) {
        // Wait for pending messages to be displayed before answering to
        // ensure no messages get lost
        await this._handlePendingMessages().catch(logErrorUnlessCancelled);

        this._authServices.forEach(s => s.answerQuery(serviceName, answer));
    }

    addCredentialManager(serviceName, credentialManager) {
        this._authServices.forEach(s => s.addCredentialManager(serviceName, credentialManager));
    }

    removeCredentialManager(serviceName) {
        this._authServices.forEach(s => s.removeCredentialManager(serviceName));
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

        this.emit('show-message', message.serviceName, message.text, message.type, message.wiggle, this._showMessageResolver);

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

    _queueMessage(serviceName, message, messageType, wiggle) {
        const interval = this._getIntervalForMessage(message);

        this._messageQueue.push({serviceName, text: message, type: messageType, interval, wiggle});
        this._queueMessageTimeout();
    }

    _queuePriorityMessage(serviceName, message, messageType, wiggle) {
        const newQueue = this._messageQueue.filter(m => {
            if (m.serviceName !== serviceName || m.type >= messageType)
                return m.text !== message;
            return false;
        });

        if (!newQueue.includes(this.currentMessage))
            this._clearMessageQueue();

        this._messageQueue = newQueue;
        this._queueMessage(serviceName, message, messageType, wiggle);
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

    _onSettingsChanged() {
        this._updateAuthServices();
    }

    _updateAuthServices() {
        const enabledAuthServicesClasses = AuthServicesClasses
            .filter(C => C.isEnabled(this._settings));

        if (enabledAuthServicesClasses.length === this._enabledAuthServicesClasses?.length &&
            enabledAuthServicesClasses.every(c => this._enabledAuthServicesClasses.includes(c)))
            return;

        this._enabledAuthServicesClasses = enabledAuthServicesClasses;
        this._createAuthServices();
    }

    _createAuthServices() {
        this._clearAuthServices();

        const params = {
            client: this._client,
            allowedFailures: this.allowedFailures,
            reauthOnly: this._reauthOnly,
        };

        this._enabledAuthServicesClasses.forEach(AuthServicesClass => {
            this._authServices.push(new AuthServicesClass(params));
        });

        this._redistributeRoles();
        this._connectAuthServices();
    }

    _clearAuthServices() {
        this._authServices.forEach(s => s.destroy());
        this._authServices = [];
    }

    _connectAuthServices() {
        this._authServices.forEach(authServices => {
            authServices.connectObject(
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
        return this._authServices
            .find(s => s.selectedMechanism)?.selectedMechanism ?? null;
    }

    _redistributeRoles() {
        if (this._authServices.length < 2)
            return;

        this._redistributingRoles = true;

        // Each authServices disables the roles supported by the one
        // before it, cascading down the priority chain
        const authServices = this._authServices;
        for (let i = 1; i < authServices.length; i++) {
            const prev = authServices[i - 1];
            const current = authServices[i];
            current.updateEnabledRoles({disableRoles: prev.supportedRoles});
        }

        this._redistributingRoles = false;
    }

    _onMechanismsChanged() {
        if (this._redistributingRoles)
            return;

        this._redistributeRoles();

        // Collect mechanisms from all authServices in priority order,
        // keeping only the first mechanism per role
        const seenRoles = new Set();
        const mechanisms = this._authServices
            .flatMap(s => s.enabledMechanisms ?? [])
            .filter(m => !seenRoles.has(m.role) && seenRoles.add(m.role));

        const selectedMechanism = this.selectedMechanism ??
            mechanisms.find(m => m.selectable) ??
            {};

        this.emit('mechanisms-changed', mechanisms, selectedMechanism);
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
