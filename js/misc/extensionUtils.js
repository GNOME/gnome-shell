// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

// Common utils for the extension system and the extension
// preferences tool

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const ShellJS = imports.gi.ShellJS;

const Config = imports.misc.config;

const ExtensionType = {
    SYSTEM: 1,
    PER_USER: 2
};

// GFile for user extensions
var userExtensionsDir = null;

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

function isOutOfDate(meta) {
    if (!versionCheck(meta['shell-version'], Config.PACKAGE_VERSION))
        return true;

    if (meta['js-version'] && !versionCheck(meta['js-version'], Config.GJS_VERSION))
        return true;

    return false;
}

function loadMetadata(uuid, dir, type) {
    let info;

    let metadataFile = dir.get_child('metadata.json');
    if (!metadataFile.query_exists(null)) {
        throw new Error('Missing metadata.json');
    }

    let metadataContents, success, tag;
    try {
        [success, metadataContents, tag] = metadataFile.load_contents(null);
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

    // Encourage people to add this
    if (!meta.url) {
        global.log('Warning: Missing "url" property in metadata.json');
    }

    if (uuid != meta.uuid) {
        throw new Error('uuid "' + meta.uuid + '" from metadata.json does not match directory name "' + uuid + '"');
    }

    meta.type = type;
    meta.dir = dir;
    meta.path = dir.get_path();
    meta.error = '';
    meta.hasPrefs = dir.get_child('prefs.js').query_exists(null);

    return meta;
}

var _meta = null;

function installImporter(meta) {
    _meta = meta;
    ShellJS.add_extension_importer('imports.misc.extensionUtils._meta', 'importer', meta.path);
    _meta = null;
}

function init() {
    let userExtensionsPath = GLib.build_filenamev([global.userdatadir, 'extensions']);
    userExtensionsDir = Gio.file_new_for_path(userExtensionsPath);
    try {
        if (!userExtensionsDir.query_exists(null))
            userExtensionsDir.make_directory_with_parents(null);
    } catch (e) {
        global.logError('' + e);
    }
}

function scanExtensionsInDirectory(callback, dir, type) {
    let fileEnum;
    let file, info;
    try {
        fileEnum = dir.enumerate_children('standard::*', Gio.FileQueryInfoFlags.NONE, null);
    } catch(e) {
        global.logError('' + e);
        return;
    }

    while ((info = fileEnum.next_file(null)) != null) {
        let fileType = info.get_file_type();
        if (fileType != Gio.FileType.DIRECTORY)
            continue;
        let uuid = info.get_name();
        let extensionDir = dir.get_child(uuid);
        callback(uuid, extensionDir, type);
    }
    fileEnum.close(null);
}

function scanExtensions(callback) {
    let systemDataDirs = GLib.get_system_data_dirs();
    for (let i = 0; i < systemDataDirs.length; i++) {
        let dirPath = GLib.build_filenamev([systemDataDirs[i], 'gnome-shell', 'extensions']);
        let dir = Gio.file_new_for_path(dirPath);
        if (dir.query_exists(null))
            scanExtensionsInDirectory(callback, dir, ExtensionType.SYSTEM);
    }
    scanExtensionsInDirectory(callback, userExtensionsDir, ExtensionType.PER_USER);
}
