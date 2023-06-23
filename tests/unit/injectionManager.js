const JsUnit = imports.jsUnit;

import '../../js/ui/environment.js';

import {InjectionManager} from '../../js/extensions/extension.js';

class Object1 {
    count = 0;

    getNumber() {
        return 42;
    }

    getCount() {
        return ++this.count;
    }
}

/**
 * @param {object} object to modify
 */
function addInjections(object) {
    // extend original method
    injectionManager.overrideMethod(
        object, 'getNumber', originalMethod => {
            return function () {
                // eslint-disable-next-line no-invalid-this
                const num = originalMethod.call(this);
                return 2 * num;
            };
        });

    // override original method
    injectionManager.overrideMethod(
        object, 'getCount', () => {
            return function () {
                return 42;
            };
        });

    // inject new method
    injectionManager.overrideMethod(
        object, 'getOtherNumber', () => {
            return function () {
                return 42;
            };
        });
}


const injectionManager = new InjectionManager();
let obj;

// Prototype injections
addInjections(Object1.prototype);

obj = new Object1();

// new obj is modified
JsUnit.assertEquals(obj.getNumber(), 84);
JsUnit.assertEquals(obj.getCount(), 42);
JsUnit.assertEquals(obj.count, 0);
JsUnit.assertEquals(obj.getOtherNumber(), 42);

injectionManager.clear();

obj = new Object1();

// new obj is unmodified
JsUnit.assertEquals(obj.getNumber(), 42);
JsUnit.assertEquals(obj.getCount(), obj.count);
JsUnit.assert(obj.count > 0);
JsUnit.assertRaises(() => obj.getOtherNumber());

// instance injections
addInjections(obj);

// obj is now modified
JsUnit.assertEquals(obj.getNumber(), 84);
JsUnit.assertEquals(obj.getCount(), 42);
JsUnit.assertEquals(obj.count, 1);
JsUnit.assertEquals(obj.getOtherNumber(), 42);

injectionManager.restoreMethod(obj, 'getNumber');
JsUnit.assertEquals(obj.getNumber(), 42);

injectionManager.clear();

// obj is unmodified again
JsUnit.assertEquals(obj.getNumber(), 42);
JsUnit.assertEquals(obj.getCount(), obj.count);
JsUnit.assert(obj.count > 0);
JsUnit.assertRaises(() => obj.getOtherNumber());
