// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const System = imports.system;

const Params = imports.misc.params;

// This is an implementation of EcmaScript SameValue algorithm,
// which returns true if two values are not observably distinguishable.
// It was taken from http://wiki.ecmascript.org/doku.php?id=harmony:egal
//
// In the future, we may want to use the 'is' operator instead.
function _sameValue(x, y) {
    if (x === y) {
        // 0 === -0, but they are not identical
        return x !== 0 || 1 / x === 1 / y;
    }

    // NaN !== NaN, but they are identical.
    // NaNs are the only non-reflexive value, i.e., if x !== x,
    // then x is a NaN.
    // isNaN is broken: it converts its argument to number, so
    // isNaN("foo") => true
    return x !== x && y !== y;
}

const _hashers = {
    object: function(o) { return o ? System.addressOf(o) : 'null'; },
    function: function(f) { return System.addressOf(f); },
    string: function(s) { return s; },
    number: function(n) { return String(n); },
    undefined: function() { return 'undefined'; },
};

/* Map is meant to be similar in usage to ES6 Map, which is
   described at http://wiki.ecmascript.org/doku.php?id=harmony:simple_maps_and_sets,
   without requiring more than ES5 + Gjs extensions.

   Known differences from other implementations:
   Polyfills around the web usually implement HashMaps for
   primitive values and reversed WeakMaps for object keys,
   but we want real maps with real O(1) semantics in all cases,
   and the easiest way is to have different hashers for different
   types.

   Known differences from the ES6 specification:
   - Map is a Lang.Class, not a ES6 class, so inheritance,
     prototype, sealing, etc. work differently.
   - items(), keys() and values() don't return iterators,
     they return actual arrays, so they incur a full copy everytime
     they're called, and they don't see changes if you mutate
     the table while iterating
     (admittedly, the ES6 spec is a bit unclear on this, and
     the reference code would just blow up)
*/
const Map = new Lang.Class({
    Name: 'Map',

    _init: function(iterable) {
        this._pool = { };
        this._size = 0;

        if (iterable) {
            for (let i = 0; i < iterable.length; i++) {
                let [key, value] = iterable[i];
                this.set(key, value);
            }
        }
    },

    _hashKey: function(key) {
        let type = typeof(key);
        return type + ':' + _hashers[type](key);
    },

    _internalGet: function(key) {
        let hash = this._hashKey(key);
        let node = this._pool[hash];

        if (node && _sameValue(node.key, key))
            return [true, node.value];
        else
            return [false, null];
    },

    get: function(key) {
        return this._internalGet(key)[1];
    },

    has: function(key) {
        return this._internalGet(key)[0];
    },

    set: function(key, value) {
        let hash = this._hashKey(key);
        let node = this._pool[hash];

        if (node) {
            node.key = key;
            node.value = value;
        } else {
            this._pool[hash] = { key: key, value: value };
            this._size++;
        }
    },

    delete: function(key) {
        let hash = this._hashKey(key);
        let node = this._pool[hash];

        if (node && _sameValue(node.key, key)) {
            delete this._pool[hash];
            this._size--;
            return [node.key, node.value];
        } else {
            return [null, null];
        }
    },

    keys: function() {
        let pool = this._pool;
        return Object.getOwnPropertyNames(pool).map(function(hash) {
            return pool[hash].key;
        });
    },

    values: function() {
        let pool = this._pool;
        return Object.getOwnPropertyNames(pool).map(function(hash) {
            return pool[hash].value;
        });
    },

    items: function() {
        let pool = this._pool;
        return Object.getOwnPropertyNames(pool).map(function(hash) {
            return [pool[hash].key, pool[hash].value];
        });
    },

    size: function() {
        return this._size;
    },
});
