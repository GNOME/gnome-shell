import GObject from 'gi://GObject';

import 'resource:///org/gnome/shell/ui/environment.js';

import {InjectionManager} from 'resource:///org/gnome/shell/extensions/extension.js';

const FIXED_NUMBER = 42;

class Object1 {
    count = 0;

    getNumber() {
        return FIXED_NUMBER;
    }

    getCount() {
        return ++this.count;
    }
}

class GObject1 extends GObject.Object {
    static [GObject.properties] = {
        'plonked': GObject.ParamSpec.boolean(
            'plonked', null, null,
            GObject.ParamFlags.READWRITE,
            false),
    };

    static {
        GObject.registerClass(this);
    }
}

describe('InjectionManager', () => {
    const INJECTIONS = {
        'getNumber': originalMethod => {
            return function () {
                // eslint-disable-next-line no-invalid-this
                const num = originalMethod.call(this);
                return 2 * num;
            };
        },
        'getCount': () => {
            return function () {
                return FIXED_NUMBER;
            };
        },
        'getOtherNumber': () => {
            return function () {
                return FIXED_NUMBER;
            };
        },
    };

    let injectionManager;

    function injectAll(obj) {
        for (const func in INJECTIONS) {
            injectionManager.overrideMethod(obj,
                func, INJECTIONS[func]);
        }
    }

    beforeEach(() => {
        injectionManager = new InjectionManager();
    });

    afterEach(() => {
        injectionManager.clear();
    });

    it('can extend prototype methods', () => {
        injectionManager.overrideMethod(Object1.prototype,
            'getNumber', INJECTIONS['getNumber']);

        const obj = new Object1();
        expect(obj.getNumber()).toEqual(2 * FIXED_NUMBER);
    });

    it('can replace prototype methods', () => {
        injectionManager.overrideMethod(Object1.prototype,
            'getCount', INJECTIONS['getCount']);

        const obj = new Object1();
        expect(obj.getCount()).toEqual(42);
        expect(obj.count).toEqual(0);
    });

    it('can inject methods into prototypes', () => {
        injectionManager.overrideMethod(Object1.prototype,
            'getOtherNumber', INJECTIONS['getOtherNumber']);

        const obj = new Object1();
        expect(obj.getOtherNumber()).toEqual(42);
    });

    it('can undo prototype injections', () => {
        injectAll(Object1.prototype);

        injectionManager.clear();

        const obj = new Object1();
        expect(obj.getNumber()).toEqual(FIXED_NUMBER);
        expect(obj.getCount()).toEqual(obj.count);
        expect(obj.count).toBeGreaterThan(0);
        expect(() => obj.getOtherNumber()).toThrow();
    });

    it('can extend instance methods', () => {
        const obj = new Object1();
        injectionManager.overrideMethod(obj,
            'getNumber', INJECTIONS['getNumber']);

        expect(obj.getNumber()).toEqual(2 * FIXED_NUMBER);
    });

    it('can replace instance methods', () => {
        const obj = new Object1();
        injectionManager.overrideMethod(obj,
            'getCount', INJECTIONS['getCount']);

        expect(obj.getCount()).toEqual(42);
        expect(obj.count).toEqual(0);
    });

    it('can inject methods into instances', () => {
        const obj = new Object1();
        injectionManager.overrideMethod(obj,
            'getOtherNumber', INJECTIONS['getOtherNumber']);

        expect(obj.getOtherNumber()).toEqual(42);
    });

    it('can undo instance injections', () => {
        const obj = new Object1();

        injectAll(obj);

        injectionManager.clear();

        expect(obj.getNumber()).toEqual(FIXED_NUMBER);
        expect(obj.getCount()).toEqual(obj.count);
        expect(obj.count).toBeGreaterThan(0);
        expect(() => obj.getOtherNumber()).toThrow();
    });

    it('can restore an original method', () => {
        const obj = new Object1();

        injectAll(obj);

        expect(obj.getNumber()).toEqual(2 * FIXED_NUMBER);

        injectionManager.restoreMethod(obj, 'getNumber');
        expect(obj.getNumber()).toEqual(FIXED_NUMBER);
    });

    it('can extend a GObject vfunc', () => {
        const gobj = new GObject1();
        let vfuncCalled;

        injectionManager.overrideMethod(
            GObject1.prototype, 'vfunc_set_property', originalMethod => {
                return function (...args) {
                    // eslint-disable-next-line no-invalid-this
                    originalMethod.apply(this, args);
                    vfuncCalled = true;
                };
            });

        vfuncCalled = false;
        gobj.set_property('plonked', true);
        expect(vfuncCalled).toEqual(true);
    });

    it('can restore a GObject vfunc', () => {
        const gobj = new GObject1();
        let vfuncCalled;

        injectionManager.overrideMethod(
            GObject1.prototype, 'vfunc_set_property', originalMethod => {
                return function (...args) {
                    // eslint-disable-next-line no-invalid-this
                    originalMethod.apply(this, args);
                    vfuncCalled = true;
                };
            });
        injectionManager.clear();

        vfuncCalled = false;
        gobj.set_property('plonked', true);
        expect(vfuncCalled).toEqual(false);
    });
});
