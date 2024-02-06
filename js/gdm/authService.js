// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

import GObject from 'gi://GObject';

import * as Signals from '../misc/signals.js';

export class AuthService extends Signals.EventEmitter {
    // FIXME: USE GObject so we have proper signal definitions
    // signals:
    // : 'mechanisms-changed'
    // : 'protocol-request-handled'
    // : 'queue-message'
    // : 'queue-priority-message'
    // : 'clear-message-queue'
    // : 'reset'
    // : 'service-request'
    // : 'ask-question'
    // : 'cancel'
    // : 'destroy'

    get protocolName() {
        throw new GObject.NotImplementedError(
            `protocolName in ${this.constructor.name}`);
    }

    get serviceName() {
        throw new GObject.NotImplementedError(
            `serviceName in ${this.constructor.name}`);
    }

    getSupportedRoles() {
        throw new GObject.NotImplementedError(
            `getSupportedRoles in ${this.constructor.name}`);
    }

    handleProtocolRequest(_version, _json) {
        throw new GObject.NotImplementedError(
            `handleProtocolRequest in ${this.constructor.name}`);
    }

    handlesMechanism(_mechanism) {
        return false;
    }

    handleMechanismResponse(_mechanism, _role, _response) {
        return false;
    }

    handleQueryAnswer(_role, _answer) {
        throw new GObject.NotImplementedError(
            `handleQueryAnswer in ${this.constructor.name}`);
    }

    handleProblem(_problem) {
        return false;
    }

    sendProtocolResponse(reply, params = {}) {
        this.emit('protocol-request-handled', reply, params);
    }

    getProtocolResponse(_mechanism, _role, _response) {
        throw new GObject.NotImplementedError(
            `getProtocolResponse in ${this.constructor.name}`);
    }

    setForegroundMechanism(_mechanism) {
        return false;
    }

    sortMechanisms(_mechanismA, _mechanismB) {
        throw new GObject.NotImplementedError(
            `sortMechanisms in ${this.constructor.name}`);
    }

    cancelRequested() {
        return false;
    }

    cancel() {}

    clear() {}

    reset() {}

    destroy() {
        this.emit('destroy');
    }
}
