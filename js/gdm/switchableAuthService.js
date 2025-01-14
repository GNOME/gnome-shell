import GLib from 'gi://GLib';

import * as Const from './const.js';
import * as Util from './util.js';
import {AuthService} from './authService.js';

const AUTH_MECHANISM_PROTOCOL_NAME = 'auth-mechanisms';

const AUTH_SELECTION_COMPLETION_STATUS = 'Ok';

export class SwitchableAuthService extends AuthService {
    constructor(userVerifier) {
        super();

        this._userVerifier = userVerifier;
    }

    get protocolName() {
        return AUTH_MECHANISM_PROTOCOL_NAME;
    }

    get serviceName() {
        return Util.SWITCHABLE_AUTH_SERVICE_NAME;
    }

    getSupportedRoles() {
        return [
            Const.PASSWORD_ROLE_NAME,
            Const.SMARTCARD_ROLE_NAME,
            Const.WEB_LOGIN_ROLE_NAME,
        ];
    }

    handleProtocolRequest(_version, json) {
        this.emit('service-request');

        let requestObject;

        try {
            requestObject = JSON.parse(json);
        } catch (e) {
            logError(e);
            return;
        }

        const authSelection = requestObject['auth-selection'];
        if (authSelection)
            this._handleAuthSelection(authSelection);
    }

    _handleAuthSelection(authSelection) {
        const mechanisms = authSelection.mechanisms ?? {};
        this._priorityList = authSelection.priority;

        if (!mechanisms)
            return;

        this.emit('mechanisms-changed', mechanisms);
    }

    sortMechanisms(mechanismA, mechanismB) {
        const priorityA = this._priorityList?.indexOf(mechanismA.id) ?? -1;
        const priorityB = this._priorityList?.indexOf(mechanismB.id) ?? -1;

        if (priorityA !== -1 && priorityB !== -1)
            return priorityA - priorityB;

        if (priorityA !== -1)
            return -1;

        if (priorityB !== -1)
            return 1;

        return 0;
    }

    handlesMechanism(mechanism) {
        switch (mechanism.role) {
        case Const.PASSWORD_ROLE_NAME:
        case Const.SMARTCARD_ROLE_NAME:
        case Const.WEB_LOGIN_ROLE_NAME:
            return true;
        default:
            return false;
        }
    }

    handleMechanism(mechanism) {
        switch (mechanism.role) {
        case Const.PASSWORD_ROLE_NAME:
            return this._startPasswordLogin(mechanism);
        case Const.SMARTCARD_ROLE_NAME:
            return this._startSmartcardLogin(mechanism);
        case Const.WEB_LOGIN_ROLE_NAME:
            return this._startWebLogin(mechanism);
        default:
            throw GObject.NotImplementedError(`handleMechanism: ${mechanism.id}`);
        }
    }

    _startPasswordLogin(mechanism) {
        const {prompt} = mechanism;

        this.emit('ask-question', prompt, true);
        return true;
    }

    _startSmartcardLogin(mechanism) {
        const {pin_prompt: pinPrompt} = mechanism;

        this.emit('ask-question', pinPrompt, true);
        return true;
    }

    handleQueryAnswer(role, answer) {
        switch (role) {
        case Const.PASSWORD_ROLE_NAME:
            this.emit('mechanism-response', role, {password: answer});
            break;
        case Const.SMARTCARD_ROLE_NAME:
            this.emit('mechanism-response', role, {pin: answer});
            break;
        default:
            throw GObject.NotImplementedError(`handleQueryAnswer: ${role}`);
        }
    }

    _startWebLogin(mechanism) {
        const {
            init_prompt: initPrompt,
            link_prompt: linkPrompt,
            uri, code, timeout,
        } = mechanism;

        if (!linkPrompt || !uri)
            return true;

        if (timeout) {
            this._webLoginTimeoutId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT,
                timeout, () => {
                    this.emit('web-login-time-out');
                    this._webLoginTimeoutId = 0;
                    return GLib.SOURCE_REMOVE;
                });
        }

        const buttons = mechanism.buttons ?? [];
        const webLoginArgs = [initPrompt, linkPrompt, uri, code, buttons];

        if (buttons?.length) {
            this.emit('web-login', ...webLoginArgs);
            return true;
        }

        buttons.push({
            default: true,
            needsLoading: true,
            label: _('Done'),
            action: () => {
                if (this._doneButtonTracker) {
                    this._userVerifier.disconnectObject(this._doneButtonTracker);
                    this.disconnectObject(this._doneButtonTracker);
                } else {
                    this._doneButtonTracker = {};
                }

                this.connectObject('service-request', () => {
                    this._userVerifier.disconnectObject(this._doneButtonTracker);
                    this.emit('web-login-close');
                }, this._doneButtonTracker);

                this._userVerifier.connectObject(
                    'verification-complete', () => {
                        this._userVerifier.disconnectObject(this._doneButtonTracker);
                        this.emit('web-login-close');
                    },

                    'verification-failed', () => {
                        this._userVerifier.disconnectObject(this._doneButtonTracker);
                        this.emit('web-login-close');
                        this.emit('show-failed-notification');
                        this.emit('reset');
                    },
                    this._doneButtonTracker);

                this._webLoginDone(mechanism.role);
            },
        });

        this.emit('web-login', ...webLoginArgs);
        return true;
    }

    _clearWebLoginTimeout() {
        if (!this._webLoginTimeoutId)
            return;

        GLib.source_remove(this._webLoginTimeoutId);
        delete this._webLoginTimeoutId;
    }

    _webLoginDone(role) {
        this.emit('mechanism-response', role, {});
        this._clearWebLoginTimeout();
    }

    getProtocolResponse(mechanism, role, response) {
        return {
            'auth-selection': {
                status: AUTH_SELECTION_COMPLETION_STATUS,
                [mechanism.id]: response,
            },
        };
    }

    clear() {
        super.clear();

        this._clearWebLoginTimeout();
        this.disconnectObject(this._doneButtonTracker);
        this._userVerifier.disconnectObject(this._doneButtonTracker);
        delete this._doneButtonTracker;
    }

    destroy() {
        super.destroy();

        this.clear();
        delete this._userVerifier;
    }
}
