/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Lang = imports.lang;
const Signals = imports.signals;

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

const Config = imports.misc.config;

const ExtensionState = {
    ENABLED: 1,
    DISABLED: 2,
    ERROR: 3,
    OUT_OF_DATE: 4
};

const ExtensionType = {
    SYSTEM: 1,
    PER_USER: 2
};

// Maps uuid -> metadata object
const extensionMeta = {};
// Maps uuid -> importer object (extension directory tree)
const extensions = {};
// Arrays of uuids
var enabledExtensions;
// GFile for user extensions
var userExtensionsDir = null;

// We don't really have a class to add signals on. So, create
// a simple dummy object, add the signal methods, and export those
// publically.
var _signals = {};
Signals.addSignalMethods(_signals);

const connect = Lang.bind(_signals, _signals.connect);
const disconnect = Lang.bind(_signals, _signals.disconnect);

// UUID => Array of error messages
var errors = {};

const ENABLED_EXTENSIONS_KEY = 'enabled-extensions';

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

function logExtensionError(uuid, message) {
    if (!errors[uuid]) errors[uuid] = [];
    errors[uuid].push(message);
    global.logError('Extension "%s" had error: %s'.format(uuid, message));
}

function loadExtension(dir, enabled, type) {
    let info;
    let uuid = dir.get_basename();

    let metadataFile = dir.get_child('metadata.json');
    if (!metadataFile.query_exists(null)) {
        logExtensionError(uuid, 'Missing metadata.json');
        return;
    }

    let metadataContents;
    try {
        metadataContents = Shell.get_file_contents_utf8_sync(metadataFile.get_path());
    } catch (e) {
        logExtensionError(uuid, 'Failed to load metadata.json: ' + e);
        return;
    }
    let meta;
    try {
        meta = JSON.parse(metadataContents);
    } catch (e) {
        logExtensionError(uuid, 'Failed to parse metadata.json: ' + e);
        return;
    }

    let requiredProperties = ['uuid', 'name', 'description', 'shell-version'];
    for (let i = 0; i < requiredProperties.length; i++) {
        let prop = requiredProperties[i];
        if (!meta[prop]) {
            logExtensionError(uuid, 'missing "' + prop + '" property in metadata.json');
            return;
        }
    }

    if (extensions[uuid] != undefined) {
        logExtensionError(uuid, "extension already loaded");
        return;
    }

    // Encourage people to add this
    if (!meta['url']) {
        global.log('Warning: Missing "url" property in metadata.json');
    }

    if (uuid != meta.uuid) {
        logExtensionError(uuid, 'uuid "' + meta.uuid + '" from metadata.json does not match directory name "' + uuid + '"');
        return;
    }

    if (!versionCheck(meta['shell-version'], Config.PACKAGE_VERSION) ||
        (meta['js-version'] && !versionCheck(meta['js-version'], Config.GJS_VERSION))) {
        logExtensionError(uuid, 'extension is not compatible with current GNOME Shell and/or GJS version');
        return;
    }

    extensionMeta[meta.uuid] = meta;
    extensionMeta[meta.uuid].type = type;
    extensionMeta[meta.uuid].path = dir.get_path();
    if (!enabled) {
        extensionMeta[meta.uuid].state = ExtensionState.DISABLED;
        return;
    }

    // Default to error, we set success as the last step
    extensionMeta[meta.uuid].state = ExtensionState.ERROR;

    let extensionJs = dir.get_child('extension.js');
    if (!extensionJs.query_exists(null)) {
        logExtensionError(uuid, 'Missing extension.js');
        return;
    }
    let stylesheetPath = null;
    let themeContext = St.ThemeContext.get_for_stage(global.stage);
    let theme = themeContext.get_theme();
    let stylesheetFile = dir.get_child('stylesheet.css');
    if (stylesheetFile.query_exists(null)) {
        try {
            theme.load_stylesheet(stylesheetFile.get_path());
        } catch (e) {
            logExtensionError(uuid, 'Stylesheet parse error: ' + e);
            return;
        }
    }

    let extensionModule;
    try {
        global.add_extension_importer('imports.ui.extensionSystem.extensions', meta.uuid, dir.get_path());
        extensionModule = extensions[meta.uuid].extension;
    } catch (e) {
        if (stylesheetPath != null)
            theme.unload_stylesheet(stylesheetPath);
        logExtensionError(uuid, e);
        return;
    }
    if (!extensionModule.main) {
        logExtensionError(uuid, 'missing \'main\' function');
        return;
    }
    try {
        extensionModule.main(meta);
    } catch (e) {
        if (stylesheetPath != null)
            theme.unload_stylesheet(stylesheetPath);
        logExtensionError(uuid, 'Failed to evaluate init function:' + e);
        return;
    }
    extensionMeta[meta.uuid].state = ExtensionState.ENABLED;

    _signals.emit('extension-loaded', meta.uuid);
    global.log('Loaded extension ' + meta.uuid);
}

function init() {
    let userExtensionsPath = GLib.build_filenamev([global.userdatadir, 'extensions']);
    userExtensionsDir = Gio.file_new_for_path(userExtensionsPath);
    try {
        userExtensionsDir.make_directory_with_parents(null);
    } catch (e) {
        global.logError('' + e);
    }

    enabledExtensions = global.settings.get_strv(ENABLED_EXTENSIONS_KEY);
}

function _loadExtensionsIn(dir, type) {
    let fileEnum;
    let file, info;
    try {
        fileEnum = dir.enumerate_children('standard::*', Gio.FileQueryInfoFlags.NONE, null);
    } catch (e) {
        global.logError('' + e);
       return;
    }

    while ((info = fileEnum.next_file(null)) != null) {
        let fileType = info.get_file_type();
        if (fileType != Gio.FileType.DIRECTORY)
            continue;
        let name = info.get_name();
        let child = dir.get_child(name);
        let enabled = enabledExtensions.indexOf(name) != -1;
        loadExtension(child, enabled, type);
    }
    fileEnum.close(null);
}

function loadExtensions() {
    _loadExtensionsIn(userExtensionsDir, ExtensionType.PER_USER);
    let systemDataDirs = GLib.get_system_data_dirs();
    for (let i = 0; i < systemDataDirs.length; i++) {
        let dirPath = systemDataDirs[i] + '/gnome-shell/extensions';
        let dir = Gio.file_new_for_path(dirPath);
        if (dir.query_exists(null))
            _loadExtensionsIn(dir, ExtensionType.SYSTEM);
    }
}
