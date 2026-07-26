// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';

import {MessageType} from './userVerifier.js';
import {authd as Authd, gdm as AuthdGdm, pam as AuthdPam} from './authdProtocol.js';
import {AuthService} from './authServices.js';
import * as Constants from './constants.js';
import {logErrorUnlessCancelled} from '../misc/errorUtils.js';

export const SERVICE_NAME = 'gdm-authd-dev';

const STRING_PROTOCOL_NAME_ = 'com.ubuntu.authd.gdm';
const STRING_PROTOCOL_VERSION_ = 1;

const AUTO_SELECTION_MAX_WAIT = 1000;

const AuthMechanismIDs = Object.freeze({
    BrokerSelection: 'authd-broker-selection',
    AuthModeSelection: 'authd-auth-mode-selection',
});


// FIXME: Add to proto!
const AuthResult = Object.freeze({
    Granted: 'granted',
    Denied: 'denied',
    Cancelled: 'cancelled',
    Retry: 'retry',
    Next: 'next',
});

// MOVE To proto?!
const UILayoutTypes = Object.freeze({
    Form: 'form',
    NewPassword: 'newpassword',
    QrCode: 'qrcode',
});

const EntryTypes = Object.freeze({
    Chars: 'chars',
    CharsPassword: 'chars_password',
    Digits: 'digits',
    DigitsPassword: 'digits_password',
});

function isEntryPassword(entryType) {
    switch (entryType) {
    case EntryTypes.Chars:
    case EntryTypes.Digits:
        return false;
    default:
        return true;
    }
}

function isEntryPIN(entryType) {
    switch (entryType) {
    case EntryTypes.Digits:
    case EntryTypes.DigitsPassword:
        return true;
    default:
        return false;
    }
}

export class AuthServicesAuthd extends AuthService {
    static {
        GObject.registerClass(this);
    }

    // get protocolName() {
    //     return STRING_PROTOCOL_NAME;
    // }

    static SupportedRoles = [
        Constants.PASSWORD_ROLE_NAME,
        Constants.WEB_LOGIN_ROLE_NAME,
        Constants.CHOICE_LIST_ROLE_NAME,
        Constants.PLAIN_TEXT_ROLE_NAME,
        Constants.MESSAGE_ROLE_NAME,
    ];

    static ServiceName = SERVICE_NAME;

    constructor(params) {
        super(params);
        this._brokers = {};
        this._authModes = {};
        this._authMechanisms = {};
        this._enabledMechanisms = [];
        this._pendingEvents = [];
        this._stage = undefined;
        this._pendingStage = undefined;
        this._pendingNewChallenge = null;
    }

    _handleCanStartService(serviceName) {
        return serviceName === SERVICE_NAME;
    }

    _handleOnCustomJSONRequest(serviceName, protocol, version, json) {
        if (json !== '{"type":"poll"}')
            print('->', json);

        const authdData = AuthdGdm.Data.fromObject(JSON.parse(json));
        const reply = this._handleAuthdProtocol(authdData);
        if (JSON.stringify(reply.toJSON()) !== '{"type":"pollResponse"}')
            print('<-', JSON.stringify(reply.toJSON()));
        // this.sendProtocolResponse(reply.toJSON(), {ignoreMessageWait: true});
        this._userVerifierCustomJSON.call_reply(
            serviceName, JSON.stringify(reply.toJSON()), this._cancellable).catch(
            logErrorUnlessCancelled);
    }

    _handleOnProblem(serviceName, problem) {
        console.log(`authd: Got a problem: ${problem}`);

        this.emit('queue-priority-message', serviceName, problem, MessageType.ERROR, false);
        this.reset();

        return true;
    }

    getProtocolResponse(_mechanism, _role, _response) {
        throw new Error('This should not be called!');
    }

    _handleAuthdProtocol(authdData) {
        switch (authdData.type) {
        case AuthdGdm.DataType.hello:
            console.log('authd: Starting authd protocol');
            return new AuthdGdm.Data({
                type: AuthdGdm.DataType.hello,
                hello: new AuthdGdm.HelloData({version: 1}),
            });
        case AuthdGdm.DataType.poll: {
            /* gather the events happened meanwhile awaiting... here... */
            const pendingEvents = this._pendingEvents;
            this._pendingEvents = [];
            return new AuthdGdm.Data({
                type: AuthdGdm.DataType.pollResponse,
                pollResponse: pendingEvents,
            });
        }
        case AuthdGdm.DataType.request:
            return new AuthdGdm.Data({
                type: AuthdGdm.DataType.response,
                response: new AuthdGdm.ResponseData({
                    type: authdData.request.type,
                    ...this._handleAuthdRequest(authdData.request),
                }),
            });
        case AuthdGdm.DataType.event:
            this._handleAuthdEvent(authdData.event);
            return new AuthdGdm.Data({
                type: AuthdGdm.DataType.eventAck,
            });
        default:
            throw new Error(`Unhandled type ${authdData.type}`);
        }
    }

    // get _blockedRoles() {
    //     return [Constants.PASSWORD_ROLE_NAME];
    //     // return this._authenticationStarted
    //     //     ? [Constants.PASSWORD_ROLE_NAME]
    //     //     : [];
    // }

    // _handleGetUnsupportedRoles() {
    //     return this._enabledRoles.filter(role =>
    //         !this._blockedRoles.includes(role));
    // }

    _handleAuthdRequest(request) {
        switch (request.type) {
        case AuthdGdm.RequestType.uiLayoutCapabilities: {
            const supportedEntries = Object.values(EntryTypes).join(',');
            const supportedWait = [true, false].join(',');
            return {
                uiLayoutCapabilities: new AuthdGdm.Responses.UiLayoutCapabilities({
                    supportedUiLayouts: [
                        new Authd.UILayout({
                            type: UILayoutTypes.Form,
                            label: 'required',
                            wait: `optional:${supportedWait}`,
                            entry: `optional:${supportedEntries}`,
                        }),
                        new Authd.UILayout({
                            type: UILayoutTypes.NewPassword,
                            label: 'required',
                            entry: `optional:${supportedEntries}`,
                        }),
                        new Authd.UILayout({
                            type: UILayoutTypes.QrCode,
                            content: 'required',
                            wait: `required:${supportedWait}`,
                            label: 'required',
                            button: 'optional',
                            code: 'optional',
                        }),
                    ],
                }),
            };
        }

        case AuthdGdm.RequestType.changeStage: {
            // FIXME: Check is valid value of AuthdPam.Stage
            this._onStageChanged(request.changeStage.stage);
            return {ack: new AuthdGdm.Responses.Ack()};
        }

        default:
            throw new Error(`Unhandled request type ${request.type}`);
        }
    }

    // FIXME: qrcode to other service fails...
    _mechanismEquals(m1, m2) {
        return m1?.id === m2?.id &&
            m1?.serviceName === m2?.serviceName;
        // return super._mechanismEquals(m1, m2) && m1?.id === m2?.id;
    }

    // selectMechanism(mechanism) {
    //     if (this._selectedMechanism?.id === mechanism.id &&
    //         this._selectedMechanism?.serviceName === mechanism.serviceName)
    //         return false;

    //     this._selectedMechanism = this._enabledMechanisms?.find(m =>
    //         m.id === mechanism.id &&
    //         m.serviceName === mechanism.serviceName
    //     );

    //     if (!this._selectedMechanism)
    //         return false;

    //     this._handleSelectMechanism();
    //     return true;
    // }

    _handleSelectMechanism() {
        const mechanism = this.selectedMechanism;

        log("AUTHD: selecting mechanism", mechanism, "current mode", this._selectedAuthMode,
            "authMechanism", this._authMechanisms[mechanism.id]
        )

        if (this._selectedAuthMode === mechanism.id)
            return;
        if (!this._authMechanisms[mechanism.id])
            return;

        const maybeSwitchMechanism = () => {
            if (this._stage !== AuthdPam.Stage.challenge)
                return;

            this._pendingStage = AuthdPam.Stage.challenge;
            this._uiLayout = null;
            this._selectAuthMode(mechanism.id);
        };

        if (this._authenticationStarted && this._isWaitingLayout()) {
            this._authCancelledAction = maybeSwitchMechanism;
            this._pendingEvents.push(
                new AuthdGdm.EventData({
                    type: AuthdGdm.EventType.isAuthenticatedCancelled,
                    isAuthenticatedCancelled: new AuthdGdm.Events.IsAuthenticatedCancelled(),
                }));

            return;
        }

        maybeSwitchMechanism();
    }

    _startSelectedMechanism() {
        const mechanism = this.selectedMechanism;
        log('authd: Selecting mechanism', mechanism);

        // if (!mechanism)
        //     logError(new Error());
        // print('Starting selected mechanism', mechanism.id);
        // print('Starting selected mechanism', JSON.stringify(mechanism));

        switch (mechanism.role) {
        case Constants.PASSWORD_ROLE_NAME:
            this.emit('ask-question', SERVICE_NAME, mechanism.prompt, true, answer => {
                if (this.selectedMechanism?.id !== mechanism.id)
                    return;

                this.handleAuthSelectionResponse(this.selectedMechanism,
                    mechanism.role,
                    {password: answer});
            });
            break;
        case Constants.PLAIN_TEXT_ROLE_NAME:
            this.emit('ask-question', SERVICE_NAME, mechanism.prompt, false, answer => {
                if (this.selectedMechanism?.id !== mechanism.id)
                    return;

                this.handleAuthSelectionResponse(this.selectedMechanism, mechanism.role,
                    {text: answer});
            });
            break;
        case Constants.WEB_LOGIN_ROLE_NAME:
            this.emit('web-login', SERVICE_NAME, mechanism.init_prompt,
                mechanism.link_prompt, mechanism.uri, mechanism.code, mechanism.buttons);
            break;
        case Constants.MESSAGE_ROLE_NAME:
            this.emit('show-choice-list', SERVICE_NAME, mechanism.name, [], () => {});
            this.emit('queue-priority-message', SERVICE_NAME, mechanism.prompt, MessageType.INFO, false);
            // this.emit('show-choice-list', SERVICE_NAME, mechanism.prompt, []);
            break;
        default:
            throw new Error(
                `Unhandled role ${mechanism.role} for selected mechanism ${mechanism.id}`);
        }
    }

    // handleMechanism(mechanism) {
    //     if (!this._selectedAuthMode || this._stage !== AuthdPam.Stage.challenge)
    //         return mechanism.role !== Constants.CHOICE_LIST_ROLE_NAME;

    //     if (this._selectedAuthMode !== mechanism.id)
    //         return true;

    //     return false;
    // }

    _newPasswordConfirmed(newPassword) {
        if (this._pendingNewChallenge === newPassword)
            return true;

        if (!this._pendingNewChallenge?.length) {
            let msg = _('Please, type the new passphrase again');
            if (isEntryPIN(this._uiLayout.entry))
                msg = _('Please, type the new PIN again');
            this.emit('queue-priority-message', SERVICE_NAME, msg, MessageType.INFO, false);
            this._pendingNewChallenge = newPassword;
        } else {
            let msg = _('The provided passphrases do not match, please try again');
            if (isEntryPIN(this._uiLayout.entry))
                msg = _('The provided PIN numbers do not match, please try again');
            this.emit('queue-priority-message', SERVICE_NAME, msg, MessageType.ERROR, false);
            this._pendingNewChallenge = null;
        }

        return false;
    }

    handleAuthSelectionResponse(mechanism, role, response) {
        if (role !== Constants.CHOICE_LIST_ROLE_NAME &&
            role !== Constants.MESSAGE_ROLE_NAME) {
            this._pendingEvents = this._pendingEvents.filter(ev =>
                ev.type !== AuthdGdm.EventType.isAuthenticatedRequested);
        }

        switch (role) {
        // case Constants.CHOICE_LIST_ROLE_NAME:
        //     this._onChoiceSelected(mechanism, response);
        //     break;

        case Constants.PASSWORD_ROLE_NAME:
            if (this._uiLayout.type === UILayoutTypes.NewPassword &&
                !this._newPasswordConfirmed(response.password)) {
                this._showChallenge();
                break;
            }

            this._pendingEvents.push(
                new AuthdGdm.EventData({
                    type: AuthdGdm.EventType.isAuthenticatedRequested,
                    isAuthenticatedRequested: new AuthdGdm.Events.IsAuthenticatedRequested({
                        authenticationData: new Authd.IARequest.AuthenticationData({
                            secret: response.password,
                        }),
                    }),
                }));
            break;

        case Constants.PLAIN_TEXT_ROLE_NAME:
            if (this._uiLayout.type === UILayoutTypes.NewPassword &&
                !this._newPasswordConfirmed(response.text)) {
                this._showChallenge();
                break;
            }

            this._pendingEvents.push(
                new AuthdGdm.EventData({
                    type: AuthdGdm.EventType.isAuthenticatedRequested,
                    isAuthenticatedRequested: new AuthdGdm.Events.IsAuthenticatedRequested({
                        authenticationData: new Authd.IARequest.AuthenticationData({
                            secret: response.text,
                        }),
                    }),
                }));
            break;

        case Constants.WEB_LOGIN_ROLE_NAME:
            this._pendingEvents.push(
                new AuthdGdm.EventData({
                    type: AuthdGdm.EventType.isAuthenticatedRequested,
                    isAuthenticatedRequested: new AuthdGdm.Events.IsAuthenticatedRequested({
                        authenticationData: new Authd.IARequest.AuthenticationData({
                            wait: 'true',
                        }),
                    }),
                }));
            break;
        }


        return true;
    }

    _doStageChange(stage) {
        this._onStageChanged(stage);
        this._requestStageChange(stage);
    }

    _requestStageChange(stage) {
        this._pendingEvents.push(new AuthdGdm.EventData({
            type: AuthdGdm.EventType.stageChanged,
            stageChanged: new AuthdGdm.Events.StageChanged({stage}),
        }));
    }

    _maybeStartBrokerSelection() {
        if (this._pendingStage !== AuthdPam.Stage.brokerSelection)
            return;

        if (this._stage === AuthdPam.Stage.brokerSelection)
            return;

        this._stage = this._pendingStage;

        const brokerIDs = Object.keys(this._brokers);
        switch (brokerIDs.length) {
        case 0:
            return;
        case 1:
            if (this._selectedBroker !== brokerIDs[0])
                this._selectBroker(brokerIDs[0]);
            return;
        }

        this._maybeShowChoiceList(AuthMechanismIDs.BrokerSelection,
            _('Select the broker'), this._brokers);
    }

    _selectBroker(brokerId) {
        this._pendingEvents.push(new AuthdGdm.EventData({
            type: AuthdGdm.EventType.brokerSelected,
            brokerSelected: new AuthdGdm.Events.BrokerSelected({
                brokerId,
            }),
        }));
    }

    _maybeStartAuthModeSelection() {
        print('maybeStartAuthModeSelection', this._pendingStage, this._stage, this._authenticationStarted);
        if (this._pendingStage !== AuthdPam.Stage.authModeSelection)
            return;

        if (this._stage === AuthdPam.Stage.authModeSelection)
            return;

        if (this._authenticationStarted)
            return;

        this._stage = this._pendingStage;

        const authModesIDs = Object.keys(this._authModes);
        switch (authModesIDs.length) {
        case 0:
            return;
        case 1:
            if (this._selectedAuthMode !== authModesIDs[0])
                this._selectAuthMode(authModesIDs[0]);
            return;
        }

        this._maybeShowChoiceList(AuthMechanismIDs.AuthModeSelection,
            _('Select the authentication mode'), this._authModes);
    }

    _selectAuthMode(authModeId) {
        this._pendingEvents.push(new AuthdGdm.EventData({
            type: AuthdGdm.EventType.authModeSelected,
            authModeSelected: new AuthdGdm.Events.AuthModeSelected({
                authModeId,
            }),
        }));
    }

    _maybeCancelPendingAutoSelection() {
        if (!this._autoSelectTimeoutID)
            return;

        GLib.source_remove(this._autoSelectTimeoutID);
        delete this._autoSelectTimeoutID;
    }

    _maybeShowChoiceList(id, prompt, choices) {
        // This is a workaround to handle the cases in which authd is now fast
        // enough to provide us a choice together with the list.
        // So let's wait a bit, before asking the user for a choice.

        this._maybeCancelPendingAutoSelection();

        if (this._skipNextAutoSelection) {
            this._showChoiceList(id, prompt, choices);
            delete this._skipNextAutoSelection;
            return;
        }

        this._autoSelectTimeoutID = GLib.timeout_add(GLib.PRIORITY_DEFAULT,
            AUTO_SELECTION_MAX_WAIT, () => {
                delete this._autoSelectTimeoutID;
                this._showChoiceList(id, prompt, choices);
                return GLib.SOURCE_REMOVE;
            });
    }

    _showChoiceList(id, prompt, choices) {
        this.emit('show-choice-list', SERVICE_NAME, prompt, choices, key => {
            // FIXME: Do it inline...
            this._handleSelectChoice(SERVICE_NAME, key);
        });

        // this._authMechanisms[id] = choiceListMechanism;
        // this._enabledMechanisms = [choiceListMechanism];
        // // this._selectedMechanism = choiceListMechanism;
        // this.selectMechanism(choiceListMechanism);
        // // this.emit('mechanisms-changed', {[id]: choiceListMechanism});

        // // use _updateEnabledMechanisms + _handleUpdateEnabledMechanisms
        // this.emit('mechanisms-changed');
    }

    _handleSelectChoice(serviceName, key) {
        if (serviceName !== SERVICE_NAME)
            return;

        switch (this._stage) {
        case AuthdPam.Stage.brokerSelection:
            this._selectBroker(key);
            break;

        case AuthdPam.Stage.authModeSelection:
            this._selectAuthMode(key);
            break;
        }
    }

    // FIXME: for local broker!
    // _handleOnSecretInfoQuery()

    // _onChoiceSelected(mechanism, key) {
    //     switch (mechanism.id) {
    //     case AuthMechanismIDs.BrokerSelection:
    //         this._selectBroker(key);
    //         break;

    //     case AuthMechanismIDs.AuthModeSelection:
    //         this._selectAuthMode(key);
    //         break;
    //     }
    // }

    cancelRequested() {
        log('authd: Cancel requested');

        let prevStage = this._stage - 1;

        switch (this._stage) {
        case AuthdPam.Stage.challenge:
            if (Object.keys(this._authModes).length > 1) {
                this._skipNextAutoSelection = true;
                break;
            }

            prevStage--;
            // fallthrough

        case AuthdPam.Stage.authModeSelection:
            if (Object.keys(this._brokers).length > 1) {
                this._skipNextAutoSelection = true;
                break;
            }

            prevStage--;
            // fallthrough

        default:
            // Mark the verification as failed, as we do not to want
            // to rely on the default allowedFailures value.
            this.emit('verification-failed', SERVICE_NAME, /* should retry */ false);
            return false;
        }

        this._requestStageChange(prevStage);
        return true;
    }

    _onStageChanged(stage) {
        if (this._pendingStage !== stage) {
            // this.emit('filter-messages', SERVICE_NAME, MessageType.ERROR);
            this.emit('queue-priority-message', SERVICE_NAME, null, MessageType.ERROR, false);
            this._pendingStage = stage;

            this._maybeCancelPendingAutoSelection();
        }

        switch (stage) {
        case AuthdPam.Stage.userSelection:
            this._cancelAndReset();
            break;

        case AuthdPam.Stage.brokerSelection:
            this._authModes = {};
            this._uiLayout = null;
            this._authenticationStarted = false;
            this._selectedAuthMode = null;
            this._maybeStartBrokerSelection();
            break;

        case AuthdPam.Stage.authModeSelection:
            this._uiLayout = null;
            this._authenticationStarted = false;
            this._selectedAuthMode = null;
            this._maybeStartAuthModeSelection();
            break;

        case AuthdPam.Stage.challenge:
            this._maybeStartChallenge();
            break;
        }
    }

    _handleAuthdEvent(event) {
        switch (event.type) {
        case AuthdGdm.EventType.userSelected:
            break;

        case AuthdGdm.EventType.brokersReceived:
            this._brokers = {};
            event.brokersReceived.brokersInfos.forEach(brokerInfo => {
                this._brokers[brokerInfo.id] = {
                    title: brokerInfo.name,
                    // FIXME: We need to support the icon, but it requires Gio.Icon()
                    // support in StButton.
                    // iconName: brokerInfo.brandIcon,
                };
            });
            this._maybeStartBrokerSelection();

            break;

        case AuthdGdm.EventType.brokerSelected:
            this._maybeCancelPendingAutoSelection();

            this._selectedBroker = event.brokerSelected.brokerId;
            // if (this._stage === AuthdPam.Stage.brokerSelection)
            //     this.emit('choice-list-selected', this._selectedBroker);

            console.log('authd: Broker selected', this._selectedBroker);
            break;

        case AuthdGdm.EventType.authModesReceived:
            this._authMechanisms = {};
            this._authModes = {};
            event.authModesReceived.authModes.forEach(authMode => {
                this._authModes[authMode.id] = {
                    title: authMode.label,
                };
                this._authMechanisms[authMode.id] = {
                    name: authMode.label,
                    selectable: true,
                    /* FIXME: we can't really define the real role until the mechanism is selected */
                    role: Constants.PASSWORD_ROLE_NAME,
                    // FIXME: We can probably just use the ID as role now, prefixed to avoid collisions, but we need to check if the role is used in other places.
                    // Since a role is an unique string.
                    id: authMode.id,
                    prompt: authMode.label,
                    serviceName: SERVICE_NAME,
                    protocol: this.protocolName,
                };
            });

            this._updateEnabledMechanisms();
            this._maybeStartAuthModeSelection();
            break;

        case AuthdGdm.EventType.authModeSelected:
            this._maybeCancelPendingAutoSelection();

            if (this._selectedAuthMode === event.authModeSelected.authModeId)
                return;

            this._selectedAuthMode = event.authModeSelected.authModeId;
            // this.emit('filter-messages', SERVICE_NAME, MessageType.ERROR);
            this.emit('queue-priority-message', SERVICE_NAME, null, MessageType.ERROR, false);

            if (this._stage >= AuthdPam.Stage.brokerSelection)
                this._maybeStartChallenge();
            break;

        case AuthdGdm.EventType.uiLayoutReceived:
            this._uiLayout = event.uiLayoutReceived.uiLayout;
            this._maybeStartChallenge();
            break;

        case AuthdGdm.EventType.startAuthentication:
            this._authenticationStarted = true;
            this._pendingNewChallenge = null;
            this._maybeStartChallenge();

            break;

        case AuthdGdm.EventType.authEvent:
            if (!this._authenticationStarted)
                return;

            this._authenticationStarted = false;
            this._handleAuthResponse(event.authEvent.response);
            break;

        default:
            throw new Error(`Unhandled event type ${event.type}`);
        }
    }

    reset() {
        this.cancel();
    }

    cancel() {
        if (this._stage !== undefined)
            console.log('authd: Cancelling authentication');

        this._maybeCancelPendingAutoSelection();

        this._stage = undefined;
        this._uiLayout = null;
        this._pendingNewChallenge = null;
        this._selectedAuthMode = null;
        this._skipNextAutoSelection = false;
        this._authenticationStarted = false;
        this._authMechanisms = {};
        this._authModes = {};
        this._authCancelledAction = null;
        this._updateEnabledMechanisms();
    }

    _handleOnConversationStopped(serviceName) {
        print(`_handleOnConversationStopped for service ${serviceName}`);
        if (serviceName !== SERVICE_NAME)
            return;
        // if (serviceName !== this._selectedMechanism?.serviceName)
        //     return;

        // if (this._unavailableServices.has(serviceName))
        //     return;

        this._verificationFailed(serviceName, true);
    }

    _handleClear() {
        print('_handleClear');
        this.cancel();
    }

    _handleUpdateEnabledRoles() {
        print("_handleUpdateEnabledRoles")
        this._selectedMechanism = null;
        this._updateEnabledMechanisms();
    }

    _handleUpdateEnabledMechanisms() {
        print("_handleUpdateEnabledMechanisms")
        // const mechanism = this._authMechanisms[this._selectedAuthMode];
        // if (mechanism && this._enabledRoles.includes(Constants.PASSWORD_ROLE_NAME))
            // this._enabledMechanisms.push(mechanism);
        // this._enabledMechanisms = Object.values(this._authMechanisms)
        //     .filter(m => this._enabledRoles.includes(m.role));

        this._enabledMechanisms = Object.values(this._authMechanisms);

        print("AUTHD: Enabled mechanisms", JSON.stringify(this._enabledMechanisms));
    }

    _maybeStartChallenge() {
        if (this._pendingStage !== AuthdPam.Stage.challenge)
            return;

        if (!this._selectedAuthMode)
            return;

        if (!this._authenticationStarted)
            return;

        if (!this._uiLayout)
            return;

        if (!this._authModes[this._selectedAuthMode])
            return;

        this._stage = this._pendingStage;
        this._showChallenge();
    }

    sortMechanisms(mA, mB) {
        if (mA.id === this._selectedAuthMode)
            return -1;
        if (mB.id === this._selectedAuthMode)
            return 1;
        return 0;
    }

    _isWaitingLayout() {
        return this._uiLayout?.wait === 'true';
    }

    _showChallenge() {
        if (!this._selectedAuthMode)
            throw new Error('No authentication mode selected');
        if (!this._authModes[this._selectedAuthMode])
            throw new Error(`Selected authentication mode '${this._selectedAuthMode}' is not supported`);
        if (!this._authMechanisms[this._selectedAuthMode])
            throw new Error(`Selected authentication mode '${this._selectedAuthMode}' is not supported mechanism`);
        if (!this._uiLayout)
            throw new Error('No ui layouts defined');

        console.log('authd: Starting challenge request', this._uiLayout.type, this._uiLayout.label);

        switch (this._uiLayout.type) {
        case UILayoutTypes.Form:
        case UILayoutTypes.NewPassword: {
            const label = this._uiLayout.label;
            let infoMsg;
            let prompt;
            const mechanism = this._authMechanisms[this._selectedAuthMode];
            if (this._isWaitingLayout())
                infoMsg = label;

            if (this._uiLayout.entry?.length) {
                prompt = label;

                if (!prompt?.length) {
                    switch (this._uiLayout.entry) {
                    case EntryTypes.CharsPassword:
                        prompt = _('Password');
                        break;
                    case EntryTypes.Chars:
                        prompt = _('Input value');
                        break;
                    case EntryTypes.DigitsPassword:
                        // FIXME: Add another role for validating them
                        prompt = _('Input numbers');
                        break;
                    case EntryTypes.Digits:
                        // FIXME: Add another role for validating them
                        prompt = _('Input numbers');
                        break;
                    }
                }

                if (isEntryPassword(this._uiLayout.entry))
                    mechanism.preemptiveInput = true;
                else
                    mechanism.role = Constants.PLAIN_TEXT_ROLE_NAME;
            } else {
                // FIXME: We need to add other roles to show wait actions.
                // mechanism.role = Const.PLAIN_TEXT_ROLE_NAME;
                // delete mechanism.role;
                mechanism.role = Constants.MESSAGE_ROLE_NAME;
                prompt = infoMsg;
            }

            mechanism.prompt = prompt ?? '';
            // this._updateEnabledMechanisms();
            // this._authMechanisms[mechanism.id] = mechanism;
            this._handleUpdateEnabledMechanisms();
            // this._updateEnabledMechanisms();
            this.selectMechanism(mechanism);
            this._startSelectedMechanism();

            if (infoMsg?.length && mechanism.prompt !== infoMsg)
                this.emit('queue-message', SERVICE_NAME, infoMsg, MessageType.INFO);

            if (!this._isWaitingLayout())
                break;

            this._pendingEvents = this._pendingEvents.filter(ev =>
                ev.type !== AuthdGdm.EventType.isAuthenticatedRequested);

            this._pendingEvents.push(new AuthdGdm.EventData({
                type: AuthdGdm.EventType.isAuthenticatedRequested,
                isAuthenticatedRequested: new AuthdGdm.Events.IsAuthenticatedRequested({
                    authenticationData: new Authd.IARequest.AuthenticationData({
                        wait: 'true',
                    }),
                }),
            }));
            break;
        }

        case UILayoutTypes.QrCode: {
            const mechanism = this._authMechanisms[this._selectedAuthMode];
            mechanism.role = Constants.WEB_LOGIN_ROLE_NAME;
            mechanism.link_prompt = this._uiLayout.label;
            mechanism.init_prompt = null;
            mechanism.uri = this._uiLayout.content;
            mechanism.code = this._uiLayout.code;
            mechanism.autoAck = true;

            if (this._uiLayout.button) {
                mechanism.buttons = [{
                    default: true,
                    label: this._uiLayout.button,
                    action: () => {
                        this._pendingEvents.push(
                            new AuthdGdm.EventData({
                                type: AuthdGdm.EventType.reselectAuthMode,
                                reselectAuthMode: new AuthdGdm.Events.ReselectAuthMode(),
                            }));
                    },
                }];
            }

            print('AUTHD: Starting QR code mechanism', JSON.stringify(mechanism));

            // this._authMechanisms[mechanism.id] = mechanism;
            this._handleUpdateEnabledMechanisms();
            // this._updateEnabledMechanisms();

            // this._enabledMechanisms[mechanism.id] = mechanism;
            this.selectMechanism(mechanism);
            this._startSelectedMechanism();
            // this._updateEnabledMechanisms();
            break;
        }

        default:
            throw new Error(`UI Layout ${this._uiLayout.type} is not handled`);
        }
    }

    _handleAuthResponse(response) {
        console.log('authd: Access response:', response.access);

        this._pendingNewChallenge = null;
        const authCancelledAction = this._authCancelledAction;
        this._authCancelledAction = null;

        if (response.access !== AuthResult.Retry)
            this._uiLayout = null;

        switch (response.access) {
        case AuthResult.Granted:
            this.emit('verification-complete');
            break;

        case AuthResult.Retry:
        case AuthResult.Denied: {
            if (response?.msg) {
                this.emit('queue-priority-message', SERVICE_NAME,
                    response.msg, MessageType.ERROR, false);
            }

            if (response.access === AuthResult.Denied)
                this._failVerification();
            else if (response.access === AuthResult.Retry)
                this._maybeStartAuthModeSelection();

            break;
        }

        case AuthResult.Cancelled: {
            authCancelledAction?.();
            break;
        }

        case AuthResult.Next: {
            this.emit('filter-messages', SERVICE_NAME, MessageType.ERROR);
            this.emit('queue-priority-message', SERVICE_NAME, response.msg, MessageType.INFO, false);
            this._authModes = {};
            // FIXME: Add emit update mechanissms?!
            break;
        }
        }
    }

    _failVerification() {
        this._authMechanisms = {};
        this._authModes = {};
        this._selectedAuthMode = null;
        this._updateEnabledMechanisms();

        this.emit('verification-failed', SERVICE_NAME, /* should retry */ false);
    }

    _cancelAndReset() {
        this.emit('cancel');
        this.emit('reset');
    }
}
