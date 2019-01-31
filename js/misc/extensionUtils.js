// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

// Common utils for the extension system and the extension
// preferences tool

const Gettext = imports.gettext;
const Signals = imports.signals;

const Gio = imports.gi.Gio;

const Config = imports.misc.config;
const FileUtils = imports.misc.fileUtils;

var ExtensionType = {
    SYSTEM: 1,
    PER_USER: 2
};

// Maps uuid -> metadata object
var extensions = {};

/**
 * getCurrentExtension:
 *
 * Returns the current extension, or null if not called from an extension.
 */
function getCurrentExtension() {
    let stack = (new Error()).stack.split('\n');
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

    let path = match[1];
    let file = Gio.File.new_for_path(path);

    // Walk up the directory tree, looking for an extension with
    // the same UUID as a directory name.
    while (file != null) {
        let extension = extensions[file.get_basename()];
        if (extension !== undefined)
            return extension;
        file = file.get_parent();
    }

    return null;
}

/**
 * initTranslations:
 * @domain: (optional): the gettext domain to use
 *
 * Initialize Gettext to load translations from extensionsdir/locale.
 * If @domain is not provided, it will be taken from metadata['gettext-domain']
 */
function initTranslations(domain) {
    let extension = getCurrentExtension();

    if (!extension)
        throw new Error('initTranslations() can only be called from extensions');

    domain = domain || extension.metadata['gettext-domain'];

    // Expect USER extensions to have a locale/ subfolder, otherwise assume a
    // SYSTEM extension that has been installed in the same prefix as the shell
    let localeDir = extension.dir.get_child('locale');
    if (localeDir.query_exists(null))
        Gettext.bindtextdomain(domain, localeDir.get_path());
    else
        Gettext.bindtextdomain(domain, Config.LOCALEDIR);
}

/**
 * getSettings:
 * @schema: (optional): the GSettings schema id
 *
 * Builds and returns a GSettings schema for @schema, using schema files
 * in extensionsdir/schemas. If @schema is omitted, it is taken from
 * metadata['settings-schema'].
 */
function getSettings(schema) {
    let extension = getCurrentExtension();

    if (!extension)
        throw new Error('getSettings() can only be called from extensions');

    schema = schema || extension.metadata['settings-schema'];

    const GioSSS = Gio.SettingsSchemaSource;

    // Expect USER extensions to have a schemas/ subfolder, otherwise assume a
    // SYSTEM extension that has been installed in the same prefix as the shell
    let schemaDir = extension.dir.get_child('schemas');
    let schemaSource;
    if (schemaDir.query_exists(null))
        schemaSource = GioSSS.new_from_directory(schemaDir.get_path(),
                                                 GioSSS.get_default(),
                                                 false);
    else
        schemaSource = GioSSS.get_default();

    let schemaObj = schemaSource.lookup(schema, true);
    if (!schemaObj)
        throw new Error(`Schema ${schema} could not be found for extension ${extension.metadata.uuid}. Please check your installation`);

    return new Gio.Settings({ settings_schema: schemaObj });
}

/**
 * versionCheck:
 * @required: an array of versions we're compatible with
 * @current: the version we have
 *
 * Check if a component is compatible for an extension.
 * @required is an array, and at least one version must match.
 * @current must be in the format <major>.<minor>.<point>.<micro>
 * <micro> is always ignored
 * <point> is ignored if <minor> is even (so you can target the
 * whole stable release)
 * <minor> and <major> must match
 * Each target version must be at least <major> and <minor>
 */
function versionCheck(required, current) {
    let currentArray = current.split('.');
    let major = currentArray[0];
    let minor = currentArray[1];
    let point = currentArray[2];
    for (let i = 0; i < required.length; i++) {
        let requiredArray = required[i].split('.');
        if (requiredArray[0] == major &&
            requiredArray[1] == minor &&
            (requiredArray[2] == point ||
             (requiredArray[2] == undefined && parseInt(minor) % 2 == 0)))
            return true;
    }
    return false;
}

function isOutOfDate(extension) {
    if (!versionCheck(extension.metadata['shell-version'], Config.PACKAGE_VERSION))
        return true;

    return false;
}

function createExtensionObject(uuid, dir, type) {
    let metadataFile = dir.get_child('metadata.json');
    if (!metadataFile.query_exists(null)) {
        throw new Error('Missing metadata.json');
    }

    let metadataContents, success, tag;
    try {
        [success, metadataContents, tag] = metadataFile.load_contents(null);
        if (metadataContents instanceof Uint8Array)
            metadataContents = imports.byteArray.toString(metadataContents);
    } catch (e) {
        throw new Error('Failed to load metadata.json: ' + e);
    }
    let meta;
    try {
        meta = JSON.parse(metadataContents);
    } catch (e) {
        throw new Error('Failed to parse metadata.json: ' + e);
    }

    let requiredProperties = ['uuid', 'name', 'description', 'shell-version'];
    for (let i = 0; i < requiredProperties.length; i++) {
        let prop = requiredProperties[i];
        if (!meta[prop]) {
            throw new Error('missing "' + prop + '" property in metadata.json');
        }
    }

    if (uuid != meta.uuid) {
        throw new Error('uuid "' + meta.uuid + '" from metadata.json does not match directory name "' + uuid + '"');
    }

    let extension = {};

    extension.metadata = meta;
    extension.uuid = meta.uuid;
    extension.type = type;
    extension.dir = dir;
    extension.path = dir.get_path();
    extension.error = '';
    extension.hasPrefs = dir.get_child('prefs.js').query_exists(null);

    extensions[uuid] = extension;

    return extension;
}

function installImporter(extension) {
    let oldSearchPath = imports.searchPath.slice();  // make a copy
    imports.searchPath = [extension.dir.get_parent().get_path()];
    // importing a "subdir" creates a new importer object that doesn't affect
    // the global one
    extension.imports = imports[extension.uuid];
    imports.searchPath = oldSearchPath;
}

var ExtensionFinder = class {
    _loadExtension(extensionDir, info, perUserDir) {
        let fileType = info.get_file_type();
        if (fileType != Gio.FileType.DIRECTORY)
            return;
        let uuid = info.get_name();
        let existing = extensions[uuid];
        if (existing) {
            log('Extension %s already installed in %s. %s will not be loaded'.format(uuid, existing.path, extensionDir.get_path()));
            return;
        }

        let extension;
        let type = extensionDir.has_prefix(perUserDir) ? ExtensionType.PER_USER
                                                       : ExtensionType.SYSTEM;
        try {
            extension = createExtensionObject(uuid, extensionDir, type);
        } catch(e) {
            logError(e, 'Could not load extension %s'.format(uuid));
            return;
        }
        this.emit('extension-found', extension);
    }

    scanExtensions() {
        let perUserDir = Gio.File.new_for_path(global.userdatadir);
        FileUtils.collectFromDatadirs('extensions', true, (dir, info) => {
            this._loadExtension(dir, info, perUserDir);
        });
    }
};
Signals.addSignalMethods(ExtensionFinder.prototype);
