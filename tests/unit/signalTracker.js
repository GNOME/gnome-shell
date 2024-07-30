import 'resource:///org/gnome/shell/ui/environment.js';
import GObject from 'gi://GObject';

import * as Signals from 'resource:///org/gnome/shell/misc/signals.js';

import {TransientSignalHolder, registerDestroyableType} from 'resource:///org/gnome/shell/misc/signalTracker.js';

const Destroyable = GObject.registerClass({
    Signals: {'destroy': {}},
}, class Destroyable extends GObject.Object {});
registerDestroyableType(Destroyable);

const GObjectEmitter = GObject.registerClass({
    Signals: {
        'signal1': {},
        'signal2': {},
    },
}, class GObjectEmitter extends Destroyable {});

describe('connectObject()', () => {
    let gobjectEmitter, eventEmitter;
    let trackedObject;

    beforeEach(() => {
        gobjectEmitter = new GObjectEmitter();
        eventEmitter = new Signals.EventEmitter();

        trackedObject = {};
    });

    it('connects to GObject signals', () => {
        const handler = jasmine.createSpy();

        gobjectEmitter.connectObject('signal1', handler, trackedObject);
        gobjectEmitter.emit('signal1');

        expect(handler).toHaveBeenCalled();
    });

    it('connects to EventEmitter signals', () => {
        const handler = jasmine.createSpy();

        eventEmitter.connectObject('signal1', handler, trackedObject);
        eventEmitter.emit('signal1');

        expect(handler).toHaveBeenCalled();
    });

    it('can connect multiple GObject signals', () => {
        const handler1 = jasmine.createSpy();
        const handler2 = jasmine.createSpy();

        gobjectEmitter.connectObject(
            'signal1', handler1,
            'signal2', handler2,
            trackedObject);
        gobjectEmitter.emit('signal1');
        gobjectEmitter.emit('signal2');

        expect(handler1).toHaveBeenCalled();
        expect(handler2).toHaveBeenCalled();
    });

    it('can connect multiple EventEmitter signals', () => {
        const handler1 = jasmine.createSpy();
        const handler2 = jasmine.createSpy();

        eventEmitter.connectObject(
            'signal1', handler1,
            'signal2', handler2,
            trackedObject);
        eventEmitter.emit('signal1');
        eventEmitter.emit('signal2');

        expect(handler1).toHaveBeenCalled();
        expect(handler2).toHaveBeenCalled();
    });

    it('supports ConnectFlags for GObject signals', () => {
        const handler1 = jasmine.createSpy();
        const handler2 = jasmine.createSpy();

        gobjectEmitter.connectObject(
            'signal1', handler1,
            'signal2', handler2, GObject.ConnectFlags.AFTER,
            trackedObject);
        gobjectEmitter.emit('signal1');
        gobjectEmitter.emit('signal2');

        expect(handler1).toHaveBeenCalled();
        expect(handler2).toHaveBeenCalled();
    });

    it('supports ConnectFlags for EventEmitter signals', () => {
        const handler1 = jasmine.createSpy();
        const handler2 = jasmine.createSpy();

        eventEmitter.connectObject(
            'signal1', handler1,
            'signal2', handler2, GObject.ConnectFlags.AFTER,
            trackedObject);
        eventEmitter.emit('signal1');
        eventEmitter.emit('signal2');

        expect(handler1).toHaveBeenCalled();
        expect(handler2).toHaveBeenCalled();
    });
});

describe('disconnectObject()', () => {
    let emitter;
    let trackedDestroyable, trackedObject;

    beforeEach(() => {
        emitter = new GObjectEmitter();

        trackedDestroyable = new Destroyable();
        trackedObject = {};
    });

    it('disconnects signals by tracked object', () => {
        const handler = jasmine.createSpy();

        emitter.connectObject(
            'signal1', handler,
            'signal2', handler,
            trackedObject);
        emitter.disconnectObject(trackedObject);
        emitter.emit('signal1');
        emitter.emit('signal2');

        expect(handler).not.toHaveBeenCalled();
    });

    it('is called when a tracked destroyable is destroyed', () => {
        const handler = jasmine.createSpy();

        emitter.connectObject('signal1', handler, trackedDestroyable);
        trackedDestroyable.emit('destroy');
        emitter.emit('signal1');

        expect(handler).not.toHaveBeenCalled();
    });
});

describe('TransientSignalHolder', () => {
    let emitter;
    let tracked, transientHolder;

    beforeEach(() => {
        emitter = new GObjectEmitter();
        tracked = new Destroyable();
        transientHolder = new TransientSignalHolder(tracked);
    });

    it('destroys with its owner', () => {
        const handler = jasmine.createSpy();

        emitter.connectObject('signal1', handler, transientHolder);
        tracked.emit('destroy');
        emitter.emit('signal1');

        expect(handler).not.toHaveBeenCalled();
    });

    it('can be destroyed without affecting its owner', () => {
        const handler1 = jasmine.createSpy();
        const handler2 = jasmine.createSpy();

        emitter.connectObject('signal1', handler1, tracked);
        emitter.connectObject('signal1', handler2, transientHolder);
        transientHolder.destroy();
        emitter.emit('signal1');

        expect(handler1).toHaveBeenCalled();
        expect(handler2).not.toHaveBeenCalled();
    });
});
