import {ExtensionBase, GettextWrapper, setExtensionManager} from './sharedInternals.js';

export {gettext, ngettext, pgettext} from './sharedInternals.js';

const {extensionManager} = imports.ui.main;
setExtensionManager(extensionManager);

export class Extension extends ExtensionBase {
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
