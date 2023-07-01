import {getCurrentExtension} from './sharedInternals.js';

export {
    getSettings,
    initTranslations,
    gettext,
    ngettext,
    pgettext
} from './sharedInternals.js';

/**
 * Open the preference dialog of the current extension
 */
export function openPrefs() {
    const extension = getCurrentExtension();

    if (!extension)
        throw new Error('openPrefs() can only be called from extensions');

    try {
        const extensionManager = imports.ui.main.extensionManager;
        extensionManager.openExtensionPrefs(extension.uuid, '', {});
    } catch (e) {
        if (e.name === 'ImportError')
            throw new Error('openPrefs() cannot be called from preferences');
        logError(e, 'Failed to open extension preferences');
    }
}
