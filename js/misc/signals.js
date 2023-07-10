import * as SignalTracker from './signalTracker.js';

const Signals = imports.signals;

export class EventEmitter {
    connectObject(...args) {
        return SignalTracker.connectObject(this, ...args);
    }

    disconnectObject(...args) {
        return SignalTracker.disconnectObject(this, ...args);
    }

    connect_object(...args) {
        return this.connectObject(...args);
    }

    disconnect_object(...args) {
        return this.disconnectObject(...args);
    }
}

Signals.addSignalMethods(EventEmitter.prototype);
