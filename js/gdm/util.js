import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import * as Signals from '../misc/signals.js';

import * as Batch from './batch.js';
import * as Const from './const.js';
import * as Main from '../ui/main.js';
import * as Params from '../misc/params.js';
import {AuthServicesLegacy} from './authServicesLegacy.js';
import {AuthServicesSwitchable} from './authServicesSwitchable.js';

const CLONE_FADE_ANIMATION_TIME = 250;

export const LOGIN_SCREEN_SCHEMA = 'org.gnome.login-screen';
export const PASSWORD_AUTHENTICATION_KEY = 'enable-password-authentication';
export const FINGERPRINT_AUTHENTICATION_KEY = 'enable-fingerprint-authentication';
export const SMARTCARD_AUTHENTICATION_KEY = 'enable-smartcard-authentication';
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
const USER_READ_TIME = 48;

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

export class InitError extends Error {
    constructor(error, message, serviceName) {
        super(message);
        this.error = error;
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

        this._messageQueue = [];
        this._messageQueueTimeoutId = 0;

        this._settings = new Gio.Settings({schema_id: LOGIN_SCREEN_SCHEMA});
        this._settings.connect('changed', () => this._updateAuthServices());
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
        this._cancellable = new Gio.Cancellable();

        try {
            await this._authServicesSwitchable?.beginVerification(userName);
            await this._authServicesLegacy?.beginVerification(userName);
        } catch (e) {
            if (e instanceof InitError)
                this._reportInitError(e);
        }

        hold?.release();
    }

    selectMechanism(mechanism) {
        this._authServicesSwitchable?.selectMechanism(mechanism);
        this._authServicesLegacy?.selectMechanism(mechanism);
    }

    needsUsername() {
        return this._authServicesSwitchable?.needsUsername() ||
            this._authServicesLegacy?.needsUsername();
    }

    reset() {
        this._authServicesSwitchable?.reset();
        this._authServicesLegacy?.reset();

        this.cancel();
    }

    cancel() {
        this._authServicesSwitchable?.cancel();
        this._authServicesLegacy?.cancel();

        this.clear();
    }

    clear() {
        this._authServicesSwitchable?.clear();
        this._authServicesLegacy?.clear();

        if (this._authServicesSwitchable) {
            this._authServicesLegacy?.updateEnabledRoles(
                this._authServicesSwitchable.unsupportedRoles);
        }

        this._clearMessageQueue();

        this._cancellable?.cancel();
        this._cancellable = null;
    }

    destroy() {
        this.cancel();

        this._settings.run_dispose();
        this._settings = null;
    }

    selectChoice(serviceName, key) {
        this._authServicesSwitchable?.selectChoice(serviceName, key);
        this._authServicesLegacy?.selectChoice(serviceName, key);
    }

    answerQuery(serviceName, answer) {
        this._authServicesSwitchable?.answerQuery(serviceName, answer);
        this._authServicesLegacy?.answerQuery(serviceName, answer);
    }

    _getIntervalForMessage(message) {
        if (!message)
            return 0;

        // We probably could be smarter here
        return message.length * USER_READ_TIME;
    }

    _finishMessageQueue() {
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
                    this._finishMessageQueue();
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
        this._finishMessageQueue();

        if (this._messageQueueTimeoutId !== 0) {
            GLib.source_remove(this._messageQueueTimeoutId);
            this._messageQueueTimeoutId = 0;
        }
        this.emit('show-message', null, null, MessageType.NONE);
    }

    _reportInitError(initError) {
        const { error, where, serviceName } = initError;

        logError(error, where);

        this._queueMessage(serviceName, _('Authentication error'), MessageType.ERROR);
        this._verificationFailed(serviceName, false);
    }

    serviceIsFingerprint(serviceName) {
        return this._fingerprintReaderFound &&
            serviceName === Const.FINGERPRINT_SERVICE_NAME;
    }

    _updateAuthServices() {
        const enabledRoles = [];

        if (this._settings.get_boolean(PASSWORD_AUTHENTICATION_KEY))
            enabledRoles.push(Const.PASSWORD_ROLE_NAME);
        if (this._settings.get_boolean(SMARTCARD_AUTHENTICATION_KEY))
            enabledRoles.push(Const.SMARTCARD_ROLE_NAME);
        if (this._settings.get_boolean(FINGERPRINT_AUTHENTICATION_KEY))
            enabledRoles.push(Const.FINGERPRINT_ROLE_NAME);
        if (this._settings.get_boolean(WEB_AUTHENTICATION_KEY))
            enabledRoles.push(Const.WEB_LOGIN_ROLE_NAME);

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
            AuthServicesSwitchable.supports(this._enabledRoles)) {
            this._authServicesSwitchable = new AuthServicesSwitchable(params);

            params.enabledRoles = this._authServicesSwitchable.unsupportedRoles;
            this._authServicesLegacy = new AuthServicesLegacy(params);
        } else if (AuthServicesLegacy.supports(this._enabledRoles)) {
            this._authServicesLegacy = new AuthServicesLegacy(params);
        }

        this._connectAuthServices();
    }

    _clearAuthServices() {
        this._authServicesSwitchable?.disconnectObject(this);
        this._authServicesSwitchable?.clear();
        this._authServicesSwitchable = null;

        this._authServicesLegacy?.disconnectObject(this);
        this._authServicesLegacy?.clear();
        this._authServicesLegacy = null;
    }

    _connectAuthServices() {
        const connectAuthServices = (authServices) =>
            authServices?.connectObject(
                'ask-question', (_, ...args) => this.emit('ask-question', ...args),
                'queue-message', (_, ...args) => this._queueMessage(...args),
                'queue-priority-message', (_, ...args) => this._queuePriorityMessage(...args),
                'wait-pending-messages', (_, ...args) => this._waitPendingMessages(...args),
                'filter-messages', (_, ...args) => this._filterServiceMessages(...args),
                'verification-failed', (_, ...args) => this._verificationFailed(...args),
                'verification-complete', (_, ...args) => this.emit('verification-complete', ...args),
                'reset', (_, ...args) => this.emit('reset', ...args),
                'credential-manager-authenticated', (_, ...args) =>
                    this.emit('credential-manager-authenticated', ...args),
                'show-choice-list', (_, ...args) => this.emit('show-choice-list', ...args),
                'mechanisms-changed', (_, ...args) => this._onMechanismsChanged(...args),
                'web-login', (_, ...args) => this.emit('web-login', ...args),
                'web-login-failed', (_, ...args) => this.emit('web-login-failed', ...args),
                this);

        connectAuthServices(this._authServicesSwitchable);
        connectAuthServices(this._authServicesLegacy);
    }

    async _verificationFailed(serviceName, canRetry) {
        this._filterServiceMessages(serviceName, MessageType.ERROR);
        this.emit('verification-failed', serviceName, canRetry);
    }

    _onMechanismsChanged() {
        if (this._enableFallbackMechanisms())
            return;

        const mechanismsSwitchable = this._authServicesSwitchable?.enabledMechanisms ?? [];
        const mechanismsLegacy = this._authServicesLegacy?.enabledMechanisms ?? [];
        const mechanisms = [...mechanismsSwitchable, ...mechanismsLegacy];

        const selectedMechanism =
            this._authServicesSwitchable?.selectedMechanism ??
            this._authServicesLegacy?.selectedMechanism ??
            mechanisms.find(m => m.selectable) ??
            {};

        this.emit('mechanisms-changed', mechanisms, selectedMechanism);
    }

    _enableFallbackMechanisms() {
        if (!this._authServicesSwitchable || !this._authServicesLegacy)
            return false;

        return this._authServicesLegacy.updateEnabledRoles(
            this._authServicesSwitchable.unsupportedRoles);
    }

    async _waitPendingMessages(waiter) {
        try {
           await this._handlePendingMessages();
           waiter.resolve();
        } catch (e) {
           waiter.reject(e);
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
}
