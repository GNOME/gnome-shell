const Gi = imports._gi;

import {ExtensionBase, GettextWrapper} from './sharedInternals.js';

import {extensionManager} from '../ui/main.js';

export class Extension extends ExtensionBase {
    static lookupByUUID(uuid) {
        return extensionManager.lookup(uuid)?.stateObj ?? null;
    }

    static defineTranslationFunctions(url) {
        const wrapper = new GettextWrapper(this, url);
        return wrapper.defineTranslationFunctions();
    }

    enable() {
    }

    disable() {
    }

    /**
     * Open the extension's preferences window
     */
    openPreferences() {
        extensionManager.openExtensionPrefs(this.uuid, '', {});
    }
}

export const {
    gettext, ngettext, pgettext,
} = Extension.defineTranslationFunctions();

export class InjectionManager {
    #savedMethods = new Map();

    /**
     * @callback CreateOverrideFunc
     * @param {Function?} originalMethod - the original method if it exists
     * @returns {Function} - a function to be used as override
     */

    /**
     * Modify, replace or inject a method
     *
     * @param {object} prototype - the object (or prototype) that is modified
     * @param {string} methodName - the name of the overwritten method
     * @param {CreateOverrideFunc} createOverrideFunc - function to call to create the override
     */
    overrideMethod(prototype, methodName, createOverrideFunc) {
        const originalMethod = this._saveMethod(prototype, methodName);
        this._installMethod(prototype, methodName, createOverrideFunc(originalMethod));
    }

    /**
     * Restore the original method
     *
     * @param {object} prototype - the object (or prototype) that is modified
     * @param {string} methodName - the name of the method to restore
     */
    restoreMethod(prototype, methodName) {
        const savedProtoMethods = this.#savedMethods.get(prototype);
        if (!savedProtoMethods)
            return;

        const originalMethod = savedProtoMethods.get(methodName);
        if (originalMethod === undefined)
            delete prototype[methodName];
        else
            this._installMethod(prototype, methodName, originalMethod);

        savedProtoMethods.delete(methodName);
        if (savedProtoMethods.size === 0)
            this.#savedMethods.delete(prototype);
    }

    /**
     * Restore all original methods and clear overrides
     */
    clear() {
        for (const [proto, map] of this.#savedMethods) {
            map.forEach(
                (_, methodName) => this.restoreMethod(proto, methodName));
        }
        console.assert(this.#savedMethods.size === 0,
            `${this.#savedMethods.size} overrides left after clear()`);
    }

    _saveMethod(prototype, methodName) {
        let savedProtoMethods = this.#savedMethods.get(prototype);
        if (!savedProtoMethods) {
            savedProtoMethods = new Map();
            this.#savedMethods.set(prototype, savedProtoMethods);
        }

        const originalMethod = prototype[methodName];
        savedProtoMethods.set(methodName, originalMethod);
        return originalMethod;
    }

    _installMethod(prototype, methodName, method) {
        if (methodName.startsWith('vfunc_')) {
            const giPrototype = prototype[Gi.gobject_prototype_symbol];
            giPrototype[Gi.hook_up_vfunc_symbol](methodName.slice(6), method);
        } else {
            prototype[methodName] = method;
        }
    }
}
