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
            return true;
        default:
            return false;
        }
    }

    handleMechanism(mechanism) {
        switch (mechanism.role) {
        case Const.PASSWORD_ROLE_NAME:
            return this._startPasswordLogin(mechanism);
        default:
            throw GObject.NotImplementedError(`handleMechanism: ${mechanism.id}`);
        }
    }

    _startPasswordLogin(mechanism) {
        const {prompt} = mechanism;

        this.emit('ask-question', prompt, true);
        return true;
    }

    handleQueryAnswer(role, answer) {
        switch (role) {
        case Const.PASSWORD_ROLE_NAME:
            this.emit('mechanism-response', role, {password: answer});
            break;
        default:
            throw GObject.NotImplementedError(`handleQueryAnswer: ${role}`);
        }
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
    }

    destroy() {
        super.destroy();

        this.clear();
        delete this._userVerifier;
    }
}
