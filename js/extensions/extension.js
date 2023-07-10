import {getCurrentExtension} from './sharedInternals.js';

export {
    getSettings,
    initTranslations,
    gettext,
    ngettext,
    pgettext
} from './sharedInternals.js';

const {extensionManager} = imports.ui.main;

/**
 * Open the preference dialog of the current extension
 */
export function openPrefs() {
    const extension = getCurrentExtension();

    if (!extension)
        throw new Error('openPrefs() can only be called from extensions');

    extensionManager.openExtensionPrefs(extension.uuid, '', {});
}
