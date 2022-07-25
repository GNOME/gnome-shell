// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ExtensionState, ExtensionType, getCurrentExtension,
   getSettings, initTranslations, gettext, ngettext, pgettext,
   openPrefs, isOutOfDate, installImporter, serializeExtension,
   deserializeExtension, setCurrentExtension */

// Common utils for the extension system and the extension
// preferences tool

const { Gio, GLib } = imports.gi;

const Gettext = imports.gettext;

const Config = imports.misc.config;

let Main = null;

try {
    Main = imports.ui.main;
} catch (error) {
    // Only log the error if it is not due to the
    // missing import.
    if (error?.name !== 'ImportError')
        console.error(error);
}

let _extension = null;

var ExtensionType = {
    SYSTEM: 1,
    PER_USER: 2,
};

var ExtensionState = {
    ENABLED: 1,
    DISABLED: 2,
    ERROR: 3,
    OUT_OF_DATE: 4,
    DOWNLOADING: 5,
    INITIALIZED: 6,
    DISABLING: 7,
    ENABLING: 8,

    // Used as an error state for operations on unknown extensions,
    // should never be in a real extensionMeta object.
    UNINSTALLED: 99,
};

const SERIALIZED_PROPERTIES = [
    'type',
    'state',
    'path',
    'error',
    'hasPrefs',
    'hasUpdate',
    'canChange',
];

/**
 * @param {object} extension the extension object to use in utilities like `initTranslations()`
 */
function setCurrentExtension(extension) {
    if (Main)
        throw new Error('setCurrentExtension() can only be called from outside the shell');

    _extension = extension;
}

/**
 * getCurrentExtension:
 *
 * @returns {?object} - The current extension, or null if not called from
 * an extension.
 */
function getCurrentExtension() {
    if (_extension)
        return _extension;

    let stack = new Error().stack.split('\n');
    let extensionStackLine;

    // Search for an occurrence of an extension stack frame
    // Start at 1 because 0 is the stack frame of this function
    for (let i = 1; i < stack.length; i++) {
        if (stack[i].includes('/gnome-shell/extensions/')) {
            extensionStackLine = stack[i];
            break;
        }
    }
    if (!extensionStackLine)
        return null;

    // The stack line is like:
    //   init([object Object])@/home/user/data/gnome-shell/extensions/u@u.id/prefs.js:8
    //
    // In the case that we're importing from
    // module scope, the first field is blank:
    //   @/home/user/data/gnome-shell/extensions/u@u.id/prefs.js:8
    let match = new RegExp('@(.+):\\d+').exec(extensionStackLine);
    if (!match)
        return null;

    // local import, as the module is used from outside the gnome-shell process
    // as well (not this function though)
    let extensionManager = imports.ui.main.extensionManager;

    let path = match[1];
    let file = Gio.File.new_for_path(path);

    // Walk up the directory tree, looking for an extension with
    // the same UUID as a directory name.
    while (file != null) {
        let extension = extensionManager.lookup(file.get_basename());
        if (extension !== undefined)
            return extension;
        file = file.get_parent();
    }

    return null;
}

/**
 * initTranslations:
 * @param {string=} domain - the gettext domain to use
 *
 * Initialize Gettext to load translations from extensionsdir/locale.
 * If @domain is not provided, it will be taken from metadata['gettext-domain']
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
 * gettext:
 * @param {string} str - the string to translate
 *
 * Translate @str using the extension's gettext domain
 *
 * @returns {string} - the translated string
 *
 */
function gettext(str) {
    return callExtensionGettextFunc('gettext', str);
}

/**
 * ngettext:
 * @param {string} str - the string to translate
 * @param {string} strPlural - the plural form of the string
 * @param {number} n - the quantity for which translation is needed
 *
 * Translate @str and choose plural form using the extension's
 * gettext domain
 *
 * @returns {string} - the translated string
 *
 */
function ngettext(str, strPlural, n) {
    return callExtensionGettextFunc('ngettext', str, strPlural, n);
}

/**
 * pgettext:
 * @param {string} context - context to disambiguate @str
 * @param {string} str - the string to translate
 *
 * Translate @str in the context of @context using the extension's
 * gettext domain
 *
 * @returns {string} - the translated string
 *
 */
function pgettext(context, str) {
    return callExtensionGettextFunc('pgettext', context, str);
}

function callExtensionGettextFunc(func, ...args) {
    const extension = getCurrentExtension();

    if (!extension)
        throw new Error(`${func}() can only be called from extensions`);

    if (!extension[func])
        throw new Error(`${func}() is used without calling initTranslations() first`);

    return extension[func](...args);
}

/**
 * getSettings:
 * @param {string=} schema - the GSettings schema id
 * @returns {Gio.Settings} - a new settings object for @schema
 *
 * Builds and returns a GSettings schema for @schema, using schema files
 * in extensionsdir/schemas. If @schema is omitted, it is taken from
 * metadata['settings-schema'].
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
        schemaSource = GioSSS.new_from_directory(schemaDir.get_path(),
                                                 GioSSS.get_default(),
                                                 false);
    } else {
        schemaSource = GioSSS.get_default();
    }

    let schemaObj = schemaSource.lookup(schema, true);
    if (!schemaObj)
        throw new Error(`Schema ${schema} could not be found for extension ${extension.metadata.uuid}. Please check your installation`);

    return new Gio.Settings({ settings_schema: schemaObj });
}

/**
 * openPrefs:
 *
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

function isOutOfDate(extension) {
    const [major] = Config.PACKAGE_VERSION.split('.');
    return !extension.metadata['shell-version'].some(v => v.startsWith(major));
}

function serializeExtension(extension) {
    let obj = { ...extension.metadata };

    SERIALIZED_PROPERTIES.forEach(prop => {
        obj[prop] = extension[prop];
    });

    let res = {};
    for (let key in obj) {
        let val = obj[key];
        let type;
        switch (typeof val) {
        case 'string':
            type = 's';
            break;
        case 'number':
            type = 'd';
            break;
        case 'boolean':
            type = 'b';
            break;
        default:
            continue;
        }
        res[key] = GLib.Variant.new(type, val);
    }

    return res;
}

function deserializeExtension(variant) {
    let res = { metadata: {} };
    for (let prop in variant) {
        let val = variant[prop].unpack();
        if (SERIALIZED_PROPERTIES.includes(prop))
            res[prop] = val;
        else
            res.metadata[prop] = val;
    }
    // add the 2 additional properties to create a valid extension object, as createExtensionObject()
    res.uuid = res.metadata.uuid;
    res.dir = Gio.File.new_for_path(res.path);
    return res;
}

function installImporter(extension) {
    let oldSearchPath = imports.searchPath.slice();  // make a copy
    imports.searchPath = [extension.dir.get_parent().get_path()];
    // importing a "subdir" creates a new importer object that doesn't affect
    // the global one
    extension.imports = imports[extension.uuid];
    imports.searchPath = oldSearchPath;
}
