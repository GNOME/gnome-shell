/* exported getCurrentExtension, setExtensionManager, getSettings,
   initTranslations, gettext, ngettext, pgettext, openPrefs */

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;

const Gettext = imports.gettext;

const Config = imports.misc.config;

let _extensionManager = null;

/**
 * @param {object} extensionManager to use in utilities like `initTranslations()`
 */
function setExtensionManager(extensionManager) {
    if (_extensionManager)
        throw new Error('Trying to override existing extension manager');

    _extensionManager = extensionManager;
}

/**
 * getCurrentExtension:
 *
 * @returns {?object} - The current extension, or null if not called from
 * an extension.
 */
function getCurrentExtension() {
    const basePath = '/gnome-shell/extensions/';

    // Search for an occurrence of an extension stack frame
    // Start at 1 because 0 is the stack frame of this function
    const [, ...stack] = new Error().stack.split('\n');
    const extensionLine = stack.find(
        line => line.includes(basePath));

    if (!extensionLine)
        return null;

    // local import, as the module is used from outside the gnome-shell process
    // as well
    if (!_extensionManager)
        setExtensionManager(imports.ui.main.extensionManager);

    // The exact stack line differs depending on where the function
    // was called (function or module scope), and whether it's called
    // from a module or legacy import (file:// URI vs. plain path).
    //
    // We don't have to care about the exact composition, all we need
    // is a string that can be traversed as path and contains the UUID
    let path = extensionLine.slice(extensionLine.indexOf(basePath));

    // Walk up the directory tree, looking for an extension with
    // the same UUID as a directory name.
    do {
        path = GLib.path_get_dirname(path);

        const dirName = GLib.path_get_basename(path);
        const extension = _extensionManager.lookup(dirName);
        if (extension !== undefined)
            return extension;
    } while (path !== '/');

    return null;
}

/**
 * Initialize Gettext to load translations from extensionsdir/locale.
 * If @domain is not provided, it will be taken from metadata['gettext-domain']
 *
 * @param {string=} domain - the gettext domain to use
 */
function initTranslations(domain) {
    let extension = getCurrentExtension();

    if (!extension)
        throw new Error('initTranslations() can only be called from extensions');

    domain ||= extension.metadata['gettext-domain'];

    // Expect USER extensions to have a locale/ subfolder, otherwise assume a
    // SYSTEM extension that has been installed in the same prefix as the shell
    let localeDir = extension.dir.get_child('locale');
    if (localeDir.query_exists(null))
        Gettext.bindtextdomain(domain, localeDir.get_path());
    else
        Gettext.bindtextdomain(domain, Config.LOCALEDIR);

    Object.assign(extension, Gettext.domain(domain));
}

/**
 * Translate @str using the extension's gettext domain
 *
 * @param {string} str - the string to translate
 *
 * @returns {string} - the translated string
 */
function gettext(str) {
    return callExtensionGettextFunc('gettext', str);
}

/**
 * Translate @str and choose plural form using the extension's
 * gettext domain
 *
 * @param {string} str - the string to translate
 * @param {string} strPlural - the plural form of the string
 * @param {number} n - the quantity for which translation is needed
 *
 * @returns {string} - the translated string
 */
function ngettext(str, strPlural, n) {
    return callExtensionGettextFunc('ngettext', str, strPlural, n);
}

/**
 * Translate @str in the context of @context using the extension's
 * gettext domain
 *
 * @param {string} context - context to disambiguate @str
 * @param {string} str - the string to translate
 *
 * @returns {string} - the translated string
 */
function pgettext(context, str) {
    return callExtensionGettextFunc('pgettext', context, str);
}

/**
 * @private
 * @param {string} func - function name
 * @param {*[]} args - function arguments
 */
function callExtensionGettextFunc(func, ...args) {
    const extension = getCurrentExtension();

    if (!extension)
        throw new Error(`${func}() can only be called from extensions`);

    if (!extension[func])
        throw new Error(`${func}() is used without calling initTranslations() first`);

    return extension[func](...args);
}

/**
 * Builds and returns a GSettings schema for @schema, using schema files
 * in extensionsdir/schemas. If @schema is omitted, it is taken from
 * metadata['settings-schema'].
 *
 * @param {string=} schema - the GSettings schema id
 * @returns {Gio.Settings} - a new settings object for @schema
 */
function getSettings(schema) {
    let extension = getCurrentExtension();

    if (!extension)
        throw new Error('getSettings() can only be called from extensions');

    schema ||= extension.metadata['settings-schema'];

    const GioSSS = Gio.SettingsSchemaSource;

    // Expect USER extensions to have a schemas/ subfolder, otherwise assume a
    // SYSTEM extension that has been installed in the same prefix as the shell
    let schemaDir = extension.dir.get_child('schemas');
    let schemaSource;
    if (schemaDir.query_exists(null)) {
        schemaSource = GioSSS.new_from_directory(
            schemaDir.get_path(), GioSSS.get_default(), false);
    } else {
        schemaSource = GioSSS.get_default();
    }

    let schemaObj = schemaSource.lookup(schema, true);
    if (!schemaObj)
        throw new Error(`Schema ${schema} could not be found for extension ${extension.metadata.uuid}. Please check your installation`);

    return new Gio.Settings({settings_schema: schemaObj});
}

/**
 * Open the preference dialog of the current extension
 */
function openPrefs() {
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
