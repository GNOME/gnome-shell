const Signals = imports.signals;

var EventEmitter = class EventEmitter {};

Signals.addSignalMethods(EventEmitter.prototype);
