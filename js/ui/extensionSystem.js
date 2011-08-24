/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Lang = imports.lang;
const Signals = imports.signals;

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const St = imports.gi.St;
const Shell = imports.gi.Shell;
const Soup = imports.gi.Soup;

const Config = imports.misc.config;

const ExtensionState = {
    ENABLED: 1,
    DISABLED: 2,
    ERROR: 3,
    OUT_OF_DATE: 4,

    // Used as an error state for operations on unknown extensions,
    // should never be in a real extensionMeta object.
    UNINSTALLED: 99
};

const ExtensionType = {
    SYSTEM: 1,
    PER_USER: 2
};

const _httpSession = new Soup.SessionAsync();

// The unfortunate state of gjs, gobject-introspection and libsoup
// means that I have to do a hack to add a feature.
// See: https://bugzilla.gnome.org/show_bug.cgi?id=655189 for context.

if (Soup.Session.prototype.add_feature != null)
    Soup.Session.prototype.add_feature.call(_httpSession, new Soup.ProxyResolverDefault());

// Maps uuid -> metadata object
const extensionMeta = {};
// Maps uuid -> importer object (extension directory tree)
const extensions = {};
// Maps uuid -> extension state object (returned from init())
const extensionStateObjs = {};
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

function installExtensionFromManifestURL(uuid, url) {
    _httpSession.queue_message(
        Soup.Message.new('GET', url),
        function(session, message) {
            if (message.status_code != Soup.KnownStatusCode.OK) {
                logExtensionError(uuid, 'downloading manifest: ' + message.status_code.toString());
                return;
            }

            let manifest;
            try {
                manifest = JSON.parse(message.response_body.data);
            } catch (e) {
                logExtensionError(uuid, 'parsing: ' + e.toString());
                return;
            }

            if (uuid != manifest['uuid']) {
                logExtensionError(uuid, 'manifest: manifest uuids do not match');
                return;
            }

            installExtensionFromManifest(manifest, meta);
        });
}

function installExtensionFromManifest(manifest, meta) {
    let uuid = manifest['uuid'];
    let name = manifest['name'];

    let url = manifest['__installer'];
    _httpSession.queue_message(Soup.Message.new('GET', url),
                               function(session, message) {
                                   gotExtensionZipFile(session, message, uuid);
                               });
}

function gotExtensionZipFile(session, message, uuid) {
    if (message.status_code != Soup.KnownStatusCode.OK) {
        logExtensionError(uuid, 'downloading extension: ' + message.status_code);
        return;
    }

    // FIXME: use a GFile mkstemp-type method once one exists
    let fd, tmpzip;
    try {
        [fd, tmpzip] = GLib.file_open_tmp('XXXXXX.shell-extension.zip');
    } catch (e) {
        logExtensionError(uuid, 'tempfile: ' + e.toString());
        return;
    }

    let stream = new Gio.UnixOutputStream({ fd: fd });
    let dir = userExtensionsDir.get_child(uuid);
    Shell.write_soup_message_to_stream(stream, message);
    stream.close(null);
    let [success, pid] = GLib.spawn_async(null,
                                          ['unzip', '-uod', dir.get_path(), '--', tmpzip],
                                          null,
                                          GLib.SpawnFlags.SEARCH_PATH | GLib.SpawnFlags.DO_NOT_REAP_CHILD,
                                          null);

    if (!success) {
        logExtensionError(uuid, 'extract: could not extract');
        return;
    }

    GLib.child_watch_add(GLib.PRIORITY_DEFAULT, pid, function(pid, status) {
        GLib.spawn_close_pid(pid);
        loadExtension(dir, true, ExtensionType.PER_USER);
    });
}

function disableExtension(uuid) {
    let meta = extensionMeta[uuid];
    if (!meta)
        return;

    if (meta.state != ExtensionState.ENABLED)
        return;

    let extensionState = extensionStateObjs[uuid];

    try {
        extensionState.disable();
    } catch(e) {
        logExtensionError(uuid, e.toString());
        return;
    }

    meta.state = ExtensionState.DISABLED;
}

function enableExtension(uuid) {
    let meta = extensionMeta[uuid];
    if (!meta)
        return;

    if (meta.state != ExtensionState.DISABLED)
        return;

    let extensionState = extensionStateObjs[uuid];

    try {
        extensionState.enable();
    } catch(e) {
        logExtensionError(uuid, e.toString());
        return;
    }

    meta.state = ExtensionState.ENABLED;
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

    extensionMeta[uuid] = meta;
    meta.type = type;
    meta.path = dir.get_path();

    // Default to error, we set success as the last step
    meta.state = ExtensionState.ERROR;

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
    let extensionState = null;
    try {
        global.add_extension_importer('imports.ui.extensionSystem.extensions', meta.uuid, dir.get_path());
        extensionModule = extensions[meta.uuid].extension;
    } catch (e) {
        if (stylesheetPath != null)
            theme.unload_stylesheet(stylesheetPath);
        logExtensionError(uuid, e);
        return;
    }

    if (!extensionModule.init) {
        logExtensionError(uuid, 'missing \'init\' function');
        return;
    }

    try {
        extensionState = extensionModule.init(meta);
    } catch (e) {
        if (stylesheetPath != null)
            theme.unload_stylesheet(stylesheetPath);
        logExtensionError(uuid, 'Failed to evaluate init function:' + e);
        return;
    }

    if (!extensionState)
        extensionState = extensionModule;
    extensionStateObjs[uuid] = extensionState;

    if (!extensionState.enable) {
        logExtensionError(uuid, 'missing \'enable\' function');
        return;
    }
    if (!extensionState.disable) {
        logExtensionError(uuid, 'missing \'disable\' function');
        return;
    }

    meta.state = ExtensionState.DISABLED;

    if (enabled)
        enableExtension(uuid);

    _signals.emit('extension-loaded', meta.uuid);
    global.log('Loaded extension ' + meta.uuid);
}

function onEnabledExtensionsChanged() {
    let newEnabledExtensions = global.settings.get_strv(ENABLED_EXTENSIONS_KEY);

    // Find and enable all the newly enabled extensions: UUIDs found in the
    // new setting, but not in the old one.
    newEnabledExtensions.filter(function(uuid) {
        return enabledExtensions.indexOf(uuid) == -1;
    }).forEach(function(uuid) {
        enableExtension(uuid);
    });

    // Find and disable all the newly disabled extensions: UUIDs found in the
    // old setting, but not in the new one.
    enabledExtensions.filter(function(item) {
        return newEnabledExtensions.indexOf(item) == -1;
    }).forEach(function(uuid) {
        disableExtension(uuid);
    });

    enabledExtensions = newEnabledExtensions;
}

function init() {
    let userExtensionsPath = GLib.build_filenamev([global.userdatadir, 'extensions']);
    userExtensionsDir = Gio.file_new_for_path(userExtensionsPath);
    try {
        userExtensionsDir.make_directory_with_parents(null);
    } catch (e) {
        global.logError('' + e);
    }

    global.settings.connect('changed::' + ENABLED_EXTENSIONS_KEY, onEnabledExtensionsChanged);
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
