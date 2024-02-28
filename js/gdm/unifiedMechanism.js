// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

import Clutter from 'gi://Clutter';
import Gdm from 'gi://Gdm';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import * as Signals from '../misc/signals.js';
import {
    PASSWORD_ROLE_NAME,
    WEB_LOGIN_ROLE_NAME
} from './consts.js';

export const MECHANISM_PROTOCOL = 'auth-mechanisms';

const AUTH_SELECTION_COMPLETION_STATUS = 'Ok';

export const SUPPORTED_ROLES = [
    PASSWORD_ROLE_NAME,
    WEB_LOGIN_ROLE_NAME,
];

export class UnifiedAuthService extends Signals.EventEmitter {
    // FIXME USE GObject so we have proper signal definitions
    // 'mechanisms-changed'

    get protocolName() {
        throw new GObject.NotImplementedError(
            `protocolName in ${this.constructor.name}`);
    }

    // get serviceName() {
    //     throw new GObject.NotImplementedError(
    //         `protocolName in ${this.constructor.name}`);
    // }

    getSupportedRoles() {
        throw new GObject.NotImplementedError(
            `getSupportedRoles in ${this.constructor.name}`);
    }

    getMechanisms() {
        throw new GObject.NotImplementedError(
            `getMechanisms in ${this.constructor.name}`);
    }

    handleProtocolRequest(_protocol, _version, _json) {
        throw new GObject.NotImplementedError(
            `getMechanisms in ${this.constructor.name}`);
    }

    handleMechanism(_mechanism) {
        return false;
    }

    getProtocolResponse(_mechanism) {
        throw new GObject.NotImplementedError(
            `getMechanisms in ${this.constructor.name}`);
    }
}

export class UnifiedMechanismProtocolHandler extends  UnifiedAuthService/* Signals.EventEmitter */ {
    get protocolName() {
        return MECHANISM_PROTOCOL;
    }

    getSupportedRoles() {
        return SUPPORTED_ROLES;
    }

    _startMechanismFromUnifiedService(mechanism) {
        const roleHandlers = {
            [WEB_LOGIN_ROLE_NAME]: 'start-web-login',
            [PASSWORD_ROLE_NAME]: 'start-password-login',
        };

        const handler = roleHandlers[mechanism.role];
        if (handler)
            this.emit(handler);
    }

    handleProtocolRequest(_protocol, _version, json) {
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
        const mechanisms = authSelection.mechanisms;
        const priorityList = authSelection.priority;

        if (!mechanisms)
            return;

        const ids = Object.keys(mechanisms);
        ids.sort((a, b) => {
            const priorityA = priorityList.indexOf(a);
            const priorityB = priorityList.indexOf(b);

            if (priorityA !== -1 && priorityB !== -1)
                return priorityA - priorityB;

            if (priorityA !== -1)
                return -1;

            if (priorityB !== -1)
                return 1;

            return 0;
        });

        this.emit('mechanisms-changed', mechanisms);
    }

    getProtocolResponse(_mechanism, role, response) {
        return {
            'auth-selection': {
                status: AUTH_SELECTION_COMPLETION_STATUS,
                mechanismId: response,
            },
        };
    }
}
