/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

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
var disabledExtensions;
var enabledExtensions;
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

function loadExtension(dir, enabled, type) {
    let info;
    let baseErrorString = 'While loading extension from "' + dir.get_parse_name() + '": ';

    let metadataFile = dir.get_child('metadata.json');
    if (!metadataFile.query_exists(null)) {
        global.logError(baseErrorString + 'Missing metadata.json');
        return;
    }

    let metadataContents;
    try {
        metadataContents = Shell.get_file_contents_utf8_sync(metadataFile.get_path());
    } catch (e) {
        global.logError(baseErrorString + 'Failed to load metadata.json: ' + e);
        return;
    }
    let meta;
    try {
        meta = JSON.parse(metadataContents);
    } catch (e) {
        global.logError(baseErrorString + 'Failed to parse metadata.json: ' + e);
        return;
    }
    let requiredProperties = ['uuid', 'name', 'description', 'shell-version'];
    for (let i = 0; i < requiredProperties.length; i++) {
        let prop = requiredProperties[i];
        if (!meta[prop]) {
            global.logError(baseErrorString + 'missing "' + prop + '" property in metadata.json');
            return;
        }
    }

    if (extensions[meta.uuid] != undefined) {
        global.logError(baseErrorString + "extension already loaded");
        return;
    }

    // Encourage people to add this
    if (!meta['url']) {
        global.log(baseErrorString + 'Warning: Missing "url" property in metadata.json');
    }

    let base = dir.get_basename();
    if (base != meta.uuid) {
        global.logError(baseErrorString + 'uuid "' + meta.uuid + '" from metadata.json does not match directory name "' + base + '"');
        return;
    }

    if (!versionCheck(meta['shell-version'], Config.PACKAGE_VERSION) ||
        (meta['js-version'] && !versionCheck(meta['js-version'], Config.GJS_VERSION))) {
        global.logError(baseErrorString + 'extension is not compatible with current GNOME Shell and/or GJS version');
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
        global.logError(baseErrorString + 'Missing extension.js');
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
            global.logError(baseErrorString + 'Stylesheet parse error: ' + e);
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
        global.logError(baseErrorString + e);
        return;
    }
    if (!extensionModule.main) {
        global.logError(baseErrorString + 'missing \'main\' function');
        return;
    }
    try {
        extensionModule.main(meta);
    } catch (e) {
        if (stylesheetPath != null)
            theme.unload_stylesheet(stylesheetPath);
        global.logError(baseErrorString + 'Failed to evaluate main function:' + e);
        return;
    }
    extensionMeta[meta.uuid].state = ExtensionState.ENABLED;
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

    disabledExtensions = global.settings.get_strv('disabled-extensions', -1);
    enabledExtensions = global.settings.get_strv('enabled-extensions', -1);
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
	// Enable all but disabled extensions if enabledExtensions is not set.
	// If it is set, enable one those, except they are disabled as well.
        let enabled = (enabledExtensions.length == 0 || enabledExtensions.indexOf(name) >= 0)
	    && disabledExtensions.indexOf(name) < 0;
        let child = dir.get_child(name);
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
