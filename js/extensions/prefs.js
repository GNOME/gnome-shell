import GObject from 'gi://GObject';

import {ExtensionBase, GettextWrapper, setExtensionManager} from './sharedInternals.js';
import {extensionManager} from '../extensionsService.js';

setExtensionManager(extensionManager);

export class ExtensionPreferences extends ExtensionBase {
    static lookupByUUID(uuid) {
        return extensionManager.lookup(uuid)?.stateObj ?? null;
    }

    static defineTranslationFunctions(url) {
        const wrapper = new GettextWrapper(this, url);
        return wrapper.defineTranslationFunctions();
    }

    /**
     * Fill the preferences window with preferences.
     *
     * @param {Adw.PreferencesWindow} _window - the preferences window
     */
    fillPreferencesWindow(_window) {
        throw new GObject.NotImplementedError();
    }
}

export const {
    gettext, ngettext, pgettext,
} = ExtensionPreferences.defineTranslationFunctions();
