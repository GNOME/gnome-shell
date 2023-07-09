import {setExtensionManager} from './sharedInternals.js';
import {extensionManager} from '../extensionsService.js';

setExtensionManager(extensionManager);

export {
    getSettings,
    initTranslations,
    gettext,
    ngettext,
    pgettext
} from './sharedInternals.js';
