/** @type {import("environment").SignalsNamespace} */
const Signals = imports.signals;

export class EventEmitter {}

Signals.addSignalMethods(EventEmitter.prototype);
