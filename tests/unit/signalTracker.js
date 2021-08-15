// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

// Test cases for version comparison

const { GObject } = imports.gi;

const JsUnit = imports.jsUnit;
const Signals = imports.signals;

const Environment = imports.ui.environment;
Environment.init();

const Destroyable = GObject.registerClass({
    Signals: { 'destroy': {} },
}, class Destroyable extends GObject.Object {});

class PlainEmitter {}
Signals.addSignalMethods(PlainEmitter.prototype);

const GObjectEmitter = GObject.registerClass({
    Signals: { 'signal': {} },
}, class GObjectEmitter extends Destroyable {});

const emitter1 = new PlainEmitter();
const emitter2 = new GObjectEmitter();

const tracked1 = new Destroyable();
const tracked2 = {};

let count = 0;
const handler = () => count++;

emitter1.connectObject('signal', handler, tracked1);
emitter2.connectObject('signal', handler, tracked1);

emitter1.connectObject('signal', handler, tracked2);
emitter2.connectObject('signal', handler, tracked2);

JsUnit.assertEquals(count, 0);

emitter1.emit('signal');
emitter2.emit('signal');

JsUnit.assertEquals(count, 4);

tracked1.emit('destroy');

emitter1.emit('signal');
emitter2.emit('signal');

JsUnit.assertEquals(count, 6);

emitter1.disconnectObject(tracked2);
emitter2.emit('destroy');

emitter1.emit('signal');
emitter2.emit('signal');

JsUnit.assertEquals(count, 6);

emitter1.connectObject(
    'signal', handler,
    'signal', handler, GObject.ConnectFlags.AFTER,
    tracked1);
emitter2.connectObject(
    'signal', handler,
    'signal', handler, GObject.ConnectFlags.AFTER,
    tracked1);

emitter1.emit('signal');
emitter2.emit('signal');

JsUnit.assertEquals(count, 10);

tracked1.emit('destroy');
emitter1.emit('signal');
emitter2.emit('signal');

JsUnit.assertEquals(count, 10);
