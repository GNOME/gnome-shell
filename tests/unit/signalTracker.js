// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

// Test cases for version comparison

const { GObject } = imports.gi;

const JsUnit = imports.jsUnit;
const Signals = imports.misc.signals;

const Environment = imports.ui.environment;
const { TransientSignalHolder, registerDestroyableType } = imports.misc.signalTracker;

Environment.init();

const Destroyable = GObject.registerClass({
    Signals: { 'destroy': {} },
}, class Destroyable extends GObject.Object {});
registerDestroyableType(Destroyable);

const GObjectEmitter = GObject.registerClass({
    Signals: { 'signal': {} },
}, class GObjectEmitter extends Destroyable {});

const emitter1 = new Signals.EventEmitter();
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

emitter1.connectObject('signal', handler, tracked1);
emitter2.connectObject('signal', handler, tracked1);

transientHolder = new TransientSignalHolder(tracked1);

emitter1.connectObject('signal', handler, transientHolder);
emitter2.connectObject('signal', handler, transientHolder);

emitter1.emit('signal');
emitter2.emit('signal');

JsUnit.assertEquals(count, 14);

transientHolder.destroy();

emitter1.emit('signal');
emitter2.emit('signal');

JsUnit.assertEquals(count, 16);

transientHolder = new TransientSignalHolder(tracked1);

emitter1.connectObject('signal', handler, transientHolder);
emitter2.connectObject('signal', handler, transientHolder);

emitter1.emit('signal');
emitter2.emit('signal');

JsUnit.assertEquals(count, 20);

tracked1.emit('destroy');
emitter1.emit('signal');
emitter2.emit('signal');

JsUnit.assertEquals(count, 20);
