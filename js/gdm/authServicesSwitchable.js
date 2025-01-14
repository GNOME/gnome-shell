import GLib from 'gi://GLib';
import GObject from 'gi://GObject';

import * as Const from './const.js';
import * as Util from './util.js';
import {AuthServices} from './authServices.js';

const MechanismsStatus = {
    WAITING: 0,
    NOT_FOUND: 1,
    FOUND: 2,
};

export const AuthServicesSwitchable = GObject.registerClass({
}, class AuthServicesSwitchable extends AuthServices {
    static SupportedRoles = [
        Const.PASSWORD_ROLE_NAME,
        Const.SMARTCARD_ROLE_NAME,
        Const.WEB_LOGIN_ROLE_NAME,
    ];
    static RoleToService = {
        [Const.PASSWORD_ROLE_NAME]: Const.SWITCHABLE_AUTH_SERVICE_NAME,
        [Const.SMARTCARD_ROLE_NAME]: Const.SWITCHABLE_AUTH_SERVICE_NAME,
        [Const.WEB_LOGIN_ROLE_NAME]: Const.SWITCHABLE_AUTH_SERVICE_NAME,
    };

    _init(params) {
        super._init(params);

        this._mechanismsStatus = MechanismsStatus.WAITING;
    }

    _handleSelectChoice(serviceName, key) {
        if (serviceName !== this._selectedMechanism?.serviceName)
            return;

        switch (this._selectedMechanism.role) {
        case Const.SMARTCARD_ROLE_NAME:
            this._selectedSmartcard =
                    this._selectedMechanism.certs.find(({ id }) => id === key);
            const pinPrompt = this._selectedSmartcard.pin_prompt;
            this.emit('ask-question', serviceName, pinPrompt, true);
            break;
        }
    }

    _handleAnswerQuery(serviceName, answer) {
        if (serviceName !== this._selectedMechanism?.serviceName)
            return;

        let response;
        switch (this._selectedMechanism.role) {
        case Const.PASSWORD_ROLE_NAME:
            response = this._formatResponse(this._selectedMechanism, answer);
            this._sendResponse(response);
            break;
        case Const.SMARTCARD_ROLE_NAME:
            response = this._formatResponse(this._selectedSmartcard, answer);
            this._sendResponse(response);
            break;
        }
    }

    _handleSelectMechanism() {
        switch (this._selectedMechanism?.role) {
        case Const.PASSWORD_ROLE_NAME:
            this._startPasswordLogin();
            break;
        case Const.SMARTCARD_ROLE_NAME:
            this._startSmartcardLogin();
            break;
        case Const.WEB_LOGIN_ROLE_NAME:
            this._startWebLogin();
            break;
        }
    }

    _handleGetUnsupportedRoles() {
        // Until we know the mechanisms or wait for them,
        // consider supportedRoles as supported
        switch (this._mechanismsStatus) {
        case MechanismsStatus.WAITING:
        case MechanismsStatus.FOUND:
            return this._enabledRoles.filter(r => !this.supportedRoles.includes(r));
        case MechanismsStatus.NOT_FOUND:
            return this._enabledRoles;
        }
    }

    _handleReset() {
        this._savedMechanism = null;
        this._mechanismsStatus = MechanismsStatus.WAITING;
    }

    _handleClear() {
        this._mechanisms = null;
        this._priorityList = null;
        this._enabledMechanisms = null;
        this._selectedMechanism = null;
        this._selectedSmartcard = null;
    }

    _handleOnCustomJSONRequest(_serviceName, _protocol, _version, json) {
        let requestObject;

        try {
            requestObject = JSON.parse(json);
        } catch (e) {
            logError(e);
            return;
        }

        const authSelection = requestObject['auth-selection'];
        if (authSelection) {
            this._mechanisms = authSelection.mechanisms;
            this._priorityList = authSelection.priority;

            if (this._mechanisms)
                this._updateEnabledMechanisms();
        }

        this._mechanismsStatus = authSelection ?
            MechanismsStatus.FOUND :
            MechanismsStatus.NOT_FOUND;
    }

    _handleUpdateEnabledMechanisms() {
        this._reformatSmartcardMechanism(this._mechanisms);

        this._enabledMechanisms.push(...Object.keys(this._mechanisms)
            .map(id => ({
                serviceName: Const.SWITCHABLE_AUTH_SERVICE_NAME,
                id,
                ...this._mechanisms[id],
            }))
            // filter out mechanisms with roles that are not enabled
            .filter(m => this._enabledRoles.includes(m.role)));

        const selectedMechanism =
            this._enabledMechanisms
                .find(m => this._savedMechanism?.role === m.role) ??
            this._priorityList
                .map(id => this._enabledMechanisms.find(m => m.id === id))[0] ??
            this._enabledMechanisms[0];
        this.selectMechanism(selectedMechanism);

        this._savedMechanism = null;
    }

    _handleSmartcardChanged() {
        this._savedMechanism = this._selectedMechanism;
        this._mechanismsStatus = MechanismsStatus.WAITING;
        this.emit('reset', { softReset: true, reuseEntryText: true });
    }

    _handleOnInfo(serviceName, info) {
        if (!this._eventExpected())
            return;

        if (serviceName === this._selectedMechanism?.serviceName)
            this.emit('queue-message', serviceName, info, Util.MessageType.INFO);
    }

    _handleOnProblem(serviceName, problem) {
        if (!this._eventExpected())
            return;

        if (serviceName === this._selectedMechanism?.serviceName)
            this.emit('queue-priority-message',
                serviceName,
                problem,
                Util.MessageType.ERROR);
    }

    _handleOnInfoQuery() {
        if (!this._eventExpected())
            return;
    }

    _handleOnSecretInfoQuery() {
        if (!this._eventExpected())
            return;
    }

    _handleOnConversationStopped(serviceName) {
        if (serviceName !== this._selectedMechanism?.serviceName)
            return;

        if (this._unavailableServices.has(serviceName))
            return;

        if (this._selectedMechanism.role === Const.WEB_LOGIN_ROLE_NAME) {
            this.emit('web-login-failed', serviceName);
            this._verificationFailed(serviceName, false);
            return;
        }

        this._savedMechanism = this._selectedMechanism;
        this._mechanismsStatus = MechanismsStatus.WAITING;

        this._failCounter++;
        this._verificationFailed(serviceName, true);
    }

    _handleOnServiceUnavailable(serviceName) {
        if (serviceName !== this._selectedMechanism?.serviceName)
            return;

        this._selectedMechanism = null;
        this._mechanismsStatus = MechanismsStatus.WAITING;

        this.emit('mechanisms-changed');
    }

    _handleCanStartService(serviceName) {
        return serviceName === Const.SWITCHABLE_AUTH_SERVICE_NAME &&
            this._mechanismsStatus === MechanismsStatus.WAITING;
    }

    _formatResponse(mechanism, answer) {
        const { role, id, name, module_name, key_id, label } = mechanism;

        let response
        switch (role) {
        case Const.PASSWORD_ROLE_NAME:
            response = { password: answer };
            break;
        case Const.SMARTCARD_ROLE_NAME:
            response = { pin: answer, name, module_name, key_id, label };
            break;
        case Const.WEB_LOGIN_ROLE_NAME:
            response = {};
            break;
        default:
            throw new GObject.NotImplementedError(`formatResponse: ${role}`);
        }

        return {
            'auth-selection': {
                status: 'Ok',
                [id]: response,
            },
        };
    }

    _sendResponse(response) {
        const { serviceName } = this._selectedMechanism;

        this._userVerifierCustomJSON.call_reply(
            serviceName, JSON.stringify(response), this._cancellable, null);
    }

    _reformatSmartcardMechanism(mechanisms) {
        const certs = Object.keys(mechanisms)
            .map(id => ({ id, ...mechanisms[id] }))
            .filter((cert) => cert.role === Const.SMARTCARD_ROLE_NAME);

        for (const cert of certs)
            delete mechanisms[cert.id];

        const selectPrompt = certs.length > 0 ?
            'Select Identity' :
            'Insert Smartcard';

        mechanisms['smartcard'] = {
            role: Const.SMARTCARD_ROLE_NAME,
            name: 'Smartcard',
            selectable: true,
            selectPrompt,
            certs,
        };
    }

    _eventExpected() {
        // If legacy PAM messages are received before receiving json PAM
        // messages informing about mechanisms, then pam_unix is being
        // used and the user is not supported by pam_sss using json.
        // Fallback to legacy authentication services.
        if (this._mechanismsStatus === MechanismsStatus.WAITING) {
            this._mechanismsStatus = MechanismsStatus.NOT_FOUND;
            this.emit('mechanisms-changed');
            return false;
        }

        return true;
    }

    _startPasswordLogin() {
        const { serviceName, prompt } = this._selectedMechanism;

        this.emit('ask-question', serviceName, prompt, true);
    }

    _startSmartcardLogin() {
        const {
            serviceName,
            selectPrompt,
            certs,
        } = this._selectedMechanism;

        if (certs.length === 1) {
            this._selectedSmartcard = certs[0];
            this.emit('ask-question', serviceName, certs[0].pin_prompt, true);
            return;
        }

        const choiceList = {};
        for (const cert of certs)
            choiceList[cert.id] = cert.cert_instruction;

        this.emit('show-choice-list', serviceName, selectPrompt, choiceList);
    }

    _startWebLogin() {
        const {
            serviceName,
            init_prompt: initPrompt,
            link_prompt: linkPrompt,
            uri, code, timeout,
        } = this._selectedMechanism;

        if (!linkPrompt || !uri)
            return;

        if (timeout) {
            this._webLoginTimeoutId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT,
                timeout, () => {
                    this._webLoginTimeoutId = 0;

                    this._savedMechanism = this._selectedMechanism;
                    this._mechanismsStatus = MechanismsStatus.WAITING;
                    this.emit('reset', { softReset: true });

                    return GLib.SOURCE_REMOVE;
                });
        }

        const buttons = [{
            default: true,
            needsLoading: true,
            label: _('Done'),
            action: () => this._webLoginDone(),
        }];

        this.emit('web-login', serviceName, initPrompt, linkPrompt, uri, code, buttons);
    }

    _webLoginDone() {
        if (this._selectedMechanism?.role !== Const.WEB_LOGIN_ROLE_NAME)
            return;

        const response = this._formatResponse(this._selectedMechanism);
        this._sendResponse(response);

        this._clearWebLoginTimeout();
    }

    _clearWebLoginTimeout() {
        if (!this._webLoginTimeoutId)
            return;

        GLib.source_remove(this._webLoginTimeoutId);
        this._webLoginTimeoutId = 0;
    }
});
