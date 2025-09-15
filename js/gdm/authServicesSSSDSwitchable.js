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

export class AuthServicesSSSDSwitchable extends AuthServices {
    static [GObject.GTypeName] = 'AuthServicesSSSDSwitchable';

    static SupportedRoles = [
        Const.PASSWORD_ROLE_NAME,
        Const.SMARTCARD_ROLE_NAME,
        Const.PASSKEY_ROLE_NAME,
        Const.WEB_LOGIN_ROLE_NAME,
    ];

    static RoleToService = {
        [Const.PASSWORD_ROLE_NAME]: Const.SWITCHABLE_AUTH_SERVICE_NAME,
        [Const.SMARTCARD_ROLE_NAME]: Const.SWITCHABLE_AUTH_SERVICE_NAME,
        [Const.PASSKEY_ROLE_NAME]: Const.SWITCHABLE_AUTH_SERVICE_NAME,
        [Const.WEB_LOGIN_ROLE_NAME]: Const.SWITCHABLE_AUTH_SERVICE_NAME,
    };

    static {
        GObject.registerClass(this);
    }

    constructor(params) {
        super(params);

        this._mechanismsStatus = MechanismsStatus.WAITING;
    }

    _handleSelectChoice(serviceName, key) {
        if (serviceName !== this._selectedMechanism?.serviceName)
            return;

        if (this._selectedMechanism.role === Const.SMARTCARD_ROLE_NAME) {
            const certificates = this._selectedMechanism.certificates;
            const cert = certificates.find(c => c.keyId === key);
            this._selectedSmartcard = cert;
            this.emit('ask-question', serviceName, cert.pinPrompt, true);
        }
    }

    async _handleAnswerQuery(serviceName, answer) {
        if (serviceName !== this._selectedMechanism?.serviceName)
            return;

        if (this._selectedMechanism.role === Const.PASSWORD_ROLE_NAME &&
            this._resettingPassword) {
            await this._userVerifier.call_answer_query(serviceName,
                answer,
                this._cancellable,
                null);
            return;
        }

        let response;
        switch (this._selectedMechanism.role) {
        case Const.PASSWORD_ROLE_NAME:
        case Const.SMARTCARD_ROLE_NAME:
            response = this._formatResponse(answer);
            this._sendResponse(response);
            break;
        case Const.PASSKEY_ROLE_NAME:
            response = this._formatResponse(answer);
            this._sendResponse(response);

            this.emit('show-choice-list', serviceName,
                this._selectedMechanism.touchInstruction, {});
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
        case Const.PASSKEY_ROLE_NAME:
            this._startPasskeyLogin();
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
        default:
            throw new GObject.NotImplementedError(`invalid MechanismStatus: ${this._mechanismsStatus}`);
        }
    }

    _handleReset() {
        this._savedMechanism = null;
        this._mechanismsStatus = MechanismsStatus.WAITING;
    }

    _handleCancel() {
        if (this._selectedMechanism) {
            this._savedMechanism = this._selectedMechanism;
            this._mechanismsStatus = MechanismsStatus.WAITING;
        }
    }

    _handleClear() {
        this._mechanisms = null;
        this._priorityList = null;
        this._enabledMechanisms = null;
        this._selectedMechanism = null;
        this._selectedSmartcard = null;

        this._resettingPassword = false;

        this._clearWebLoginTimeout();
    }

    _handleOnCustomJSONRequest(_serviceName, _protocol, _version, json) {
        let requestObject;

        try {
            requestObject = JSON.parse(json);
        } catch (e) {
            logError(e);
            return;
        }

        const {authSelection} = requestObject;
        if (authSelection) {
            this._mechanisms = authSelection.mechanisms;
            this._priorityList = authSelection.priority;

            if (this._mechanisms)
                this._updateEnabledMechanisms();
        }

        this._mechanismsStatus = authSelection
            ? MechanismsStatus.FOUND
            : MechanismsStatus.NOT_FOUND;
    }

    _handleUpdateEnabledMechanisms() {
        this._enabledMechanisms.push(...Object.keys(this._mechanisms)
            .map(id => ({
                serviceName: Const.SWITCHABLE_AUTH_SERVICE_NAME,
                id,
                ...this._mechanisms[id],
            }))
            // filter out mechanisms with roles that are not enabled
            .filter(m => this._enabledRoles.includes(m.role)));

        this._trackWebLoginTimeout();

        const selectedMechanism =
            this._enabledMechanisms
                .find(m => this._savedMechanism?.role === m.role) ??
            this._priorityList
                .map(id => this._enabledMechanisms.find(m => m.id === id))[0] ??
            this._enabledMechanisms[0];
        this.selectMechanism(selectedMechanism);

        this._savedMechanism = null;
    }

    _trackWebLoginTimeout() {
        this._clearWebLoginTimeout();

        const webLoginMechanism = this._enabledMechanisms
            .find(m => m.role === Const.WEB_LOGIN_ROLE_NAME);
        if (!webLoginMechanism)
            return;

        const {timeout} = webLoginMechanism;
        if (!timeout)
            return;

        this._webLoginTimeoutId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT,
            timeout, () => {
                if (this._selectedMechanism?.role !== Const.WEB_LOGIN_ROLE_NAME)
                    webLoginMechanism.needsRefresh = true;
                else
                    this.emit('reset', {softReset: true});

                this._webLoginTimeoutId = 0;

                return GLib.SOURCE_REMOVE;
            });
    }

    _handleSmartcardChanged() {
        if (!this._selectedMechanism ||
            !this._enabledMechanisms.some(({role}) => role === Const.SMARTCARD_ROLE_NAME))
            return;

        this.emit('reset', {softReset: true, reuseEntryText: true});
    }

    _handlePasskeyChanged() {
        if (!this._selectedMechanism ||
            !this._enabledMechanisms.some(({role}) => role === Const.PASSKEY_ROLE_NAME))
            return;

        this.emit('reset', {softReset: true, reuseEntryText: true});
    }

    _handleOnInfo(serviceName, info) {
        if (!this._eventExpected())
            return;

        // sssd can't inform about expired password from JSON so it's needed
        // to check the info message and handle the reset using the old flow
        if (serviceName === this._selectedMechanism?.serviceName &&
            this._selectedMechanism.role === Const.PASSWORD_ROLE_NAME &&
            info.includes('Password expired. Change your password now'))
            this._resettingPassword = true;

        if (serviceName === this._selectedMechanism?.serviceName)
            this.emit('queue-message', serviceName, info, Util.MessageType.INFO);
    }

    _handleOnProblem(serviceName, problem) {
        if (!this._eventExpected())
            return;

        if (serviceName === this._selectedMechanism?.serviceName) {
            this.emit('queue-priority-message',
                serviceName,
                problem,
                Util.MessageType.ERROR);
        }
    }

    _handleOnInfoQuery() {
        if (!this._eventExpected())
            // eslint-disable-next-line no-useless-return
            return;
    }

    _handleOnSecretInfoQuery(serviceName, secretQuestion) {
        if (!this._eventExpected())
            return;

        if (serviceName === this._selectedMechanism?.serviceName &&
            this._selectedMechanism.role === Const.PASSWORD_ROLE_NAME &&
            this._resettingPassword)
            this.emit('ask-question', serviceName, secretQuestion, true);
    }

    _handleOnConversationStopped(serviceName) {
        if (serviceName !== this._selectedMechanism?.serviceName)
            return;

        if (this._unavailableServices.has(serviceName))
            return;

        this._failCounter++;
        this._verificationFailed(serviceName, true);
    }

    _handleCanStartService(serviceName) {
        return serviceName === Const.SWITCHABLE_AUTH_SERVICE_NAME &&
            this._mechanismsStatus === MechanismsStatus.WAITING;
    }

    _formatResponse(answer) {
        const {role, id, kerberos, cryptoChallenge} = this._selectedMechanism;

        let response;
        switch (role) {
        case Const.PASSWORD_ROLE_NAME: {
            response = {password: answer};
            break;
        }
        case Const.SMARTCARD_ROLE_NAME: {
            const {tokenName, moduleName, keyId, label} = this._selectedSmartcard;
            response = {pin: answer, tokenName, moduleName, keyId, label};
            break;
        }
        case Const.PASSKEY_ROLE_NAME: {
            response = {pin: answer, kerberos, cryptoChallenge};
            break;
        }
        case Const.WEB_LOGIN_ROLE_NAME: {
            response = {};
            break;
        }
        default:
            throw new GObject.NotImplementedError(`formatResponse: ${role}`);
        }

        return {
            authSelection: {
                status: 'Ok',
                [id]: response,
            },
        };
    }

    _sendResponse(response) {
        const {serviceName} = this._selectedMechanism;

        this._userVerifierCustomJSON.call_reply(
            serviceName, JSON.stringify(response), this._cancellable, null);
    }

    _eventExpected() {
        // If legacy PAM messages are received before receiving JSON PAM
        // messages informing about mechanisms, then pam_unix is being
        // used and the user is not supported by pam_sss using JSON.
        // Fallback to legacy authentication services.
        if (this._mechanismsStatus === MechanismsStatus.WAITING) {
            this._mechanismsStatus = MechanismsStatus.NOT_FOUND;
            this.emit('mechanisms-changed');
            return false;
        }

        return true;
    }

    _startPasswordLogin() {
        const {serviceName, prompt} = this._selectedMechanism;

        this.emit('ask-question', serviceName, prompt, true);
    }

    _startSmartcardLogin() {
        const {serviceName, certificates} = this._selectedMechanism;

        if (certificates.length === 1) {
            this._selectedSmartcard = certificates[0];
            this.emit('ask-question', serviceName, certificates[0].pinPrompt, true);
            return;
        }

        const choiceList = {};
        for (const cert of certificates)
            choiceList[cert.keyId] = this._parseCertInstruction(cert.certInstruction);

        const prompt = certificates.length === 0
            ? _('Insert Smartcard')
            : _('Select Identity');

        this.emit('show-choice-list', serviceName, prompt, choiceList);
    }

    _parseCertInstruction(certInstruction) {
        // Currently sssd can't split cert data in a more granular way
        // so it's parsed manually here
        const [description, subject] = certInstruction.split('\n');

        const fields = subject?.split(',').map(f => f.trim()) ?? [];
        const commonName = fields.find(f => f.startsWith('CN='))?.substring(3);
        const organization = fields.find(f => f.startsWith('O='))?.substring(2);

        return {
            title: commonName,
            subtitle: description,
            iconName: organization ? 'vcard-symbolic' : null,
            iconTitle: organization ? _('Organization') : null,
            iconSubtitle: organization,
        };
    }

    _startPasskeyLogin() {
        const {
            serviceName,
            keyConnected, initInstruction,
            pinPrompt, pinAttempts,
        } = this._selectedMechanism;

        if (!keyConnected) {
            this.emit('show-choice-list', serviceName, initInstruction, {});
            return;
        }

        this.emit('ask-question', serviceName, pinPrompt, true);

        if (pinAttempts <= 3 && pinAttempts > 0) {
            const message = _('You have %d attempts left. If the passkey gets locked, you may not able to access your account.').format(pinAttempts);
            this.emit('queue-message', serviceName, message, Util.MessageType.INFO);
        }
    }

    _startWebLogin() {
        const {
            serviceName,
            initPrompt, linkPrompt,
            uri, code, needsRefresh,
        } = this._selectedMechanism;

        if (!linkPrompt || !uri)
            return;

        if (needsRefresh) {
            this.emit('reset', {softReset: true});
            return;
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

        const response = this._formatResponse();
        this._sendResponse(response);

        this._clearWebLoginTimeout();
    }

    _clearWebLoginTimeout() {
        if (!this._webLoginTimeoutId)
            return;

        GLib.source_remove(this._webLoginTimeoutId);
        this._webLoginTimeoutId = 0;
    }
}
