import {ExtensionBase, GettextWrapper} from './sharedInternals.js';

const {extensionManager} = imports.ui.main;

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
