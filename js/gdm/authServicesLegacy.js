import GLib from 'gi://GLib';
import GObject from 'gi://GObject';

import * as Const from './const.js';
import {FingerprintReaderType} from '../misc/fingerprintManager.js';
import * as OVirt from './oVirt.js';
import * as Util from './util.js';
import * as Vmware from './vmware.js';
import {AuthServices} from './authServices.js';

const FINGERPRINT_ERROR_TIMEOUT_WAIT = 15;

const Mechanisms = [
    {
        serviceName: Const.PASSWORD_SERVICE_NAME,
        role: Const.PASSWORD_ROLE_NAME,
        name: 'Password',
    },
    {
        serviceName: Const.SMARTCARD_SERVICE_NAME,
        role: Const.SMARTCARD_ROLE_NAME,
        name: 'Smartcard',
    },
    {
        serviceName: Const.FINGERPRINT_SERVICE_NAME,
        role: Const.FINGERPRINT_ROLE_NAME,
        name: 'Fingerprint',
    },
];

export const AuthServicesLegacy = GObject.registerClass({
}, class AuthServicesLegacy extends AuthServices {
    static SupportedRoles = [
        Const.PASSWORD_ROLE_NAME,
        Const.SMARTCARD_ROLE_NAME,
        Const.FINGERPRINT_ROLE_NAME,
    ];

    static RoleToService = {
        [Const.PASSWORD_ROLE_NAME]: Const.PASSWORD_SERVICE_NAME,
        [Const.SMARTCARD_ROLE_NAME]: Const.SMARTCARD_SERVICE_NAME,
        [Const.FINGERPRINT_ROLE_NAME]: Const.FINGERPRINT_SERVICE_NAME,
    };

    _init(params) {
        super._init(params);

        this._updateEnabledMechanisms();

        this._credentialManagers = {};
        this._addCredentialManager(OVirt.SERVICE_NAME, OVirt.getOVirtCredentialsManager());
        this._addCredentialManager(Vmware.SERVICE_NAME, Vmware.getVmwareCredentialsManager());
    }

    _handleSelectChoice(serviceName, key) {
        if (serviceName !== this._selectedMechanism?.serviceName)
            return;

        this._userVerifierChoiceList.call_select_choice(
            serviceName, key, this._cancellable, null);
    }

    async _handleAnswerQuery(serviceName, answer) {
        if (serviceName !== this._selectedMechanism?.serviceName)
            return;

        if (this._selectedMechanism.role === Const.SMARTCARD_ROLE_NAME)
            this._smartcardInProgress = true;

        await this._userVerifier.call_answer_query(serviceName,
            answer,
            this._cancellable,
            null);
    }

    _handleBeginVerification() {
        this._fingerprintManager?.checkReaderType(this._cancellable);

        this.emit('mechanisms-changed');
    }

    _handleSelectMechanism() {
        if (this._selectedMechanism)
            this.emit('reset', {softReset: true});
    }

    _handleNeedsUsername() {
        // Username won't be needed when there's only one mechanism and is
        // Smartcard, or if the selected mechanism is a credential manager
        return !(this._enabledMechanisms.length === 1 &&
            this._enabledMechanisms[0].role === Const.SMARTCARD_ROLE_NAME ||
            Object.keys(this._credentialManagers).includes(this._selectedMechanism?.serviceName));
    }

    _handleReset() {
        this._selectedMechanism = null;
    }

    _handleClear() {
        this._smartcardInProgress = false;
    }

    _handleUpdateEnabledMechanisms() {
        if (!this._fingerprintManager?.readerFound) {
            this._enabledMechanisms.push(...Mechanisms.filter(m =>
                this._enabledRoles.includes(m.role) &&
                m.role !== Const.FINGERPRINT_ROLE_NAME
            ));
        } else {
            this._enabledMechanisms.push(...Mechanisms.filter(m =>
                this._enabledRoles.includes(m.role)
            ));
        }
    }

    _handleSmartcardChanged() {
        if (this._selectedMechanism?.role !== Const.SMARTCARD_ROLE_NAME ||
            this._smartcardInProgress && this._smartcardManager.hasInsertedTokens())
            return;

        this.emit('reset', {softReset: true});
    }

    _handleFingerprintChanged() {
        if (!this._enabledRoles.includes(Const.FINGERPRINT_ROLE_NAME))
            return;

        this._updateEnabledMechanisms();
        this.emit('reset', {softReset: true, reuseEntryText: true});
    }

    _handleOnInfo(serviceName, info) {
        if (serviceName === this._selectedMechanism?.serviceName) {
            this.emit('queue-message', serviceName, info, Util.MessageType.INFO);
        } else if (serviceName === Const.FINGERPRINT_SERVICE_NAME &&
            this._enabledMechanisms.some(m => m.serviceName === serviceName)) {
            // We don't show fingerprint messages directly since it's
            // not the main auth service. Instead we use the messages
            // as a cue to display our own message.
            this.emit('queue-message',
                serviceName,
                this._fingerprintManager?.readerType === FingerprintReaderType.SWIPE
                    // Translators: this message is shown below the password entry field
                    // to indicate the user can swipe their finger on the fingerprint reader
                    ? _('(or swipe finger across reader)')
                    // Translators: this message is shown below the password entry field
                    // to indicate the user can place their finger on the fingerprint reader instead
                    : _('(or place finger on reader)'),
                Util.MessageType.HINT);
        }
    }

    _handleOnProblem(serviceName, problem) {
        if (serviceName === this._selectedMechanism?.serviceName ||
            (serviceName === Const.FINGERPRINT_SERVICE_NAME &&
            this._enabledMechanisms.some(m => m.serviceName === serviceName))) {
            this.emit('queue-priority-message',
                serviceName,
                problem,
                Util.MessageType.ERROR);
        }

        if (serviceName === Const.FINGERPRINT_SERVICE_NAME &&
            this._enabledMechanisms.some(m => m.serviceName === serviceName)) {
            // pam_fprintd allows the user to retry multiple (maybe even infinite!
            // times before failing the authentication conversation.
            // We don't want this behavior to bypass the max-tries setting the user has set,
            // so we count the problem messages to know how many times the user has failed.
            // Once we hit the max number of failures we allow, it's time to failure the
            // conversation from our side. We can't do that right away, however, because
            // we may drop pending messages coming from pam_fprintd. In order to make sure
            // the user sees everything, we queue the failure up to get handled in the
            // near future, after we've finished up the current round of messages.
            this._failCounter++;

            if (this._canRetry())
                return;

            if (this._fingerprintFailedId)
                GLib.source_remove(this._fingerprintFailedId);

            this._fingerprintFailedId = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                FINGERPRINT_ERROR_TIMEOUT_WAIT, () => {
                    this._fingerprintFailedId = 0;
                    if (!this._cancellable.is_cancelled())
                        this._verificationFailed(serviceName, false);
                    return GLib.SOURCE_REMOVE;
                });
        }
    }

    _handleOnInfoQuery(serviceName, question) {
        if (serviceName !== this._selectedMechanism?.serviceName)
            return;

        this.emit('ask-question', serviceName, question, false);
    }

    _handleOnSecretInfoQuery(serviceName, secretQuestion) {
        if (serviceName !== this._selectedMechanism?.serviceName)
            return;

        let token = null;
        if (this._credentialManagers[serviceName])
            token = this._credentialManagers[serviceName].token;

        if (token) {
            this.answerQuery(serviceName, token);
            return;
        }

        this.emit('ask-question', serviceName, secretQuestion, true);
    }

    _handleOnConversationStopped(serviceName) {
        if (serviceName !== this._selectedMechanism?.serviceName &&
            serviceName !== Const.FINGERPRINT_SERVICE_NAME)
            return;

        // If the login failed with the preauthenticated oVirt credentials
        // then discard the credentials and revert to default authentication
        // mechanism.
        if (this._credentialManagers[serviceName]) {
            this._credentialManagers[serviceName].token = null;
            this._selectedMechanism = null;
            this._verificationFailed(serviceName, false);
            return;
        }

        if (this._unavailableServices.has(serviceName)) {
            if (serviceName === Const.FINGERPRINT_SERVICE_NAME) {
                this._enabledMechanisms = this._enabledMechanisms
                    .filter(m => m.serviceName !== serviceName);
                this.emit('mechanisms-changed');
            }
            return;
        }

        // if the password service fails, then cancel everything.
        // But if, e.g., fingerprint fails, still give
        // password authentication a chance to succeed
        if (serviceName === this._selectedMechanism?.serviceName)
            this._failCounter++;

        this._verificationFailed(serviceName, true);
    }

    _handleOnServiceUnavailable(serviceName, errorMessage) {
        if (serviceName === Const.FINGERPRINT_SERVICE_NAME &&
            this._enabledMechanisms.some(m => m.serviceName === serviceName) &&
            errorMessage) {
            this.emit('queue-message',
                serviceName,
                errorMessage,
                Util.MessageType.ERROR);
        }
    }

    _handleVerificationFailed(serviceName) {
        if (serviceName === Const.FINGERPRINT_SERVICE_NAME &&
            this._enabledMechanisms.some(m => m.serviceName === serviceName) &&
            this._fingerprintFailedId)
            GLib.source_remove(this._fingerprintFailedId);
    }

    _handleOnVerificationComplete(serviceName) {
        if (serviceName !== this._selectedMechanism?.serviceName)
            return;

        if (this._credentialManagers[serviceName]) {
            this._credentialManagers[serviceName].token = null;
            this._selectedMechanism = null;
        }
    }

    _handleOnChoiceListQuery(serviceName, promptMessage, list) {
        if (serviceName !== this._selectedMechanism?.serviceName)
            return;

        const choiceList = Object.fromEntries(
            Object.entries(list.deepUnpack())
            .map(([key, value]) => [key, {description: value}]));

        this.emit('show-choice-list', serviceName, promptMessage, choiceList);
    }

    _handleGetCredentialManagerServices() {
        return Object.keys(this._credentialManagers);
    }

    _handleCanStartService(serviceName) {
        return serviceName === this._selectedMechanism?.serviceName ||
            (serviceName === Const.FINGERPRINT_SERVICE_NAME &&
            this._enabledMechanisms.some(m => m.serviceName === serviceName) &&
            this._userName);
    }

    _addCredentialManager(serviceName, credentialManager) {
        if (this._credentialManagers[serviceName])
            return;

        this._credentialManagers[serviceName] = credentialManager;
        if (credentialManager.token)
            this._onCredentialManagerAuthenticated(credentialManager);

        credentialManager.connectObject(
            'user-authenticated', () => this._onCredentialManagerAuthenticated(credentialManager),
            this);
    }

    _onCredentialManagerAuthenticated(credentialManager) {
        this._selectedMechanism = {
            serviceName: credentialManager.service,
            role: Const.PASSWORD_ROLE_NAME,
        };
        this.emit('reset', {softReset: true});
    }
});
