// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Gio, St } = imports.gi;
const Signals = imports.signals;

const ExtensionUtils = imports.misc.extensionUtils;
const Main = imports.ui.main;

var ExtensionState = {
    ENABLED: 1,
    DISABLED: 2,
    ERROR: 3,
    OUT_OF_DATE: 4,
    DOWNLOADING: 5,
    INITIALIZED: 6,

    // Used as an error state for operations on unknown extensions,
    // should never be in a real extensionMeta object.
    UNINSTALLED: 99
};

// Array of UUIDs
var _enabledExtensions = [];
// Array of UUIDs in order that extensions were enabled in
var _extensionOrder = [];
// Array with IDs of signals connected to
var _settingsConnections = [];

var _initialized = false;

// We don't really have a class to add signals on. So, create
// a simple dummy object, add the signal methods, and export those
// publically.
var _signals = {};
Signals.addSignalMethods(_signals);

var connect = _signals.connect.bind(_signals);
var disconnect = _signals.disconnect.bind(_signals);

const ENABLED_EXTENSIONS_KEY = 'enabled-extensions';
const DISABLE_USER_EXTENSIONS_KEY = 'disable-user-extensions';
const EXTENSION_DISABLE_VERSION_CHECK_KEY = 'disable-extension-version-validation';

function _disableExtension(uuid) {
    let extension = ExtensionUtils.extensions[uuid];
    if (!extension)
        return;

    if (extension.state != ExtensionState.ENABLED)
        return;

    // "Rebase" the extension order by disabling and then enabling extensions
    // in order to help prevent conflicts.

    // Example:
    //   order = [A, B, C, D, E]
    //   user disables C
    //   this should: disable E, disable D, disable C, enable D, enable E

    let orderIdx = _extensionOrder.indexOf(uuid);
    let order = _extensionOrder.slice(orderIdx + 1);
    let orderReversed = order.slice().reverse();

    for (let i = 0; i < orderReversed.length; i++) {
        let uuid = orderReversed[i];
        try {
            ExtensionUtils.extensions[uuid].stateObj.disable();
        } catch(e) {
            logExtensionError(uuid, e);
        }
    }

    if (extension.stylesheet) {
        let theme = St.ThemeContext.get_for_stage(global.stage).get_theme();
        theme.unload_stylesheet(extension.stylesheet);
        delete extension.stylesheet;
    }

    try {
        extension.stateObj.disable();
    } catch(e) {
        logExtensionError(uuid, e);
    }

    for (let i = 0; i < order.length; i++) {
        let uuid = order[i];
        try {
            ExtensionUtils.extensions[uuid].stateObj.enable();
        } catch(e) {
            logExtensionError(uuid, e);
        }
    }

    _extensionOrder.splice(orderIdx, 1);

    if (extension.state != ExtensionState.ERROR) {
        extension.state = ExtensionState.DISABLED;
        _signals.emit('extension-state-changed', extension);
    }
}

function _enableExtension(uuid) {
    let extension = ExtensionUtils.extensions[uuid];
    if (!extension)
        return;

    if (extension.state == ExtensionState.INITIALIZED)
        _initExtension(uuid);

    if (extension.state != ExtensionState.DISABLED)
        return;

    _extensionOrder.push(uuid);

    let stylesheetNames = [global.session_mode + '.css', 'stylesheet.css'];
    let theme = St.ThemeContext.get_for_stage(global.stage).get_theme();
    for (let i = 0; i < stylesheetNames.length; i++) {
        try {
            let stylesheetFile = extension.dir.get_child(stylesheetNames[i]);
            theme.load_stylesheet(stylesheetFile);
            extension.stylesheet = stylesheetFile;
            break;
        } catch (e) {
            if (e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_FOUND))
                continue; // not an error
            log(`Failed to load stylesheet for extension ${uuid}: ${e.message}`);
            return;
        }
    }

    try {
        extension.stateObj.enable();
        extension.state = ExtensionState.ENABLED;
        _signals.emit('extension-state-changed', extension);
        return;
    } catch(e) {
        if (extension.stylesheet) {
            theme.unload_stylesheet(extension.stylesheet);
            delete extension.stylesheet;
        }
        logExtensionError(uuid, e);
        return;
    }
}

function logExtensionError(uuid, error) {
    let extension = ExtensionUtils.extensions[uuid];
    if (!extension)
        return;

    let message = '' + error;

    extension.state = ExtensionState.ERROR;
    if (!extension.errors)
        extension.errors = [];
    extension.errors.push(message);

    log('Extension "%s" had error: %s'.format(uuid, message));
    _signals.emit('extension-state-changed', { uuid: uuid,
                                               error: message,
                                               state: extension.state });
}

function loadExtension(extension) {
    // Default to error, we set success as the last step
    extension.state = ExtensionState.ERROR;

    let checkVersion = !global.settings.get_boolean(EXTENSION_DISABLE_VERSION_CHECK_KEY);

    if (checkVersion && ExtensionUtils.isOutOfDate(extension)) {
        extension.state = ExtensionState.OUT_OF_DATE;
    } else {
        let enabled = _enabledExtensions.indexOf(extension.uuid) != -1;
        if (enabled) {
            if (!_initExtension(extension.uuid))
                return;
            if (extension.state == ExtensionState.DISABLED)
                _enableExtension(extension.uuid);
        } else {
            extension.state = ExtensionState.INITIALIZED;
        }
    }

    _signals.emit('extension-state-changed', extension);
}

function unloadExtension(extension) {
    // Try to disable it -- if it's ERROR'd, we can't guarantee that,
    // but it will be removed on next reboot, and hopefully nothing
    // broke too much.
    _disableExtension(extension.uuid);

    extension.state = ExtensionState.UNINSTALLED;
    _signals.emit('extension-state-changed', extension);

    delete ExtensionUtils.extensions[extension.uuid];
    return true;
}

function reloadExtension(oldExtension) {
    // Grab the things we'll need to pass to createExtensionObject
    // to reload it.
    let { uuid: uuid, dir: dir, type: type } = oldExtension;

    // Then unload the old extension.
    unloadExtension(oldExtension);

    // Now, recreate the extension and load it.
    let newExtension;
    try {
        newExtension = ExtensionUtils.createExtensionObject(uuid, dir, type);
    } catch(e) {
        logExtensionError(uuid, e);
        return;
    }

    loadExtension(newExtension);
}

function _initExtension(uuid) {
    let extension = ExtensionUtils.extensions[uuid];
    let dir = extension.dir;

    if (!extension)
        throw new Error("Extension was not properly created. Call ExtensionUtils.createExtensionObject first");

    let extensionJs = dir.get_child('extension.js');
    if (!extensionJs.query_exists(null)) {
        logExtensionError(uuid, new Error('Missing extension.js'));
        return false;
    }

    let extensionModule;
    let extensionState = null;

    ExtensionUtils.installImporter(extension);
    try {
        extensionModule = extension.imports.extension;
    } catch(e) {
        logExtensionError(uuid, e);
        return false;
    }

    if (extensionModule.init) {
        try {
            extensionState = extensionModule.init(extension);
        } catch(e) {
            logExtensionError(uuid, e);
            return false;
        }
    }

    if (!extensionState)
        extensionState = extensionModule;
    extension.stateObj = extensionState;

    extension.state = ExtensionState.DISABLED;
    _signals.emit('extension-loaded', uuid);
    return true;
}

function getEnabledExtensions() {
    let extensions;
    if (Array.isArray(Main.sessionMode.enabledExtensions))
        extensions = Main.sessionMode.enabledExtensions;
    else
        extensions = [];

    if (global.settings.get_boolean(DISABLE_USER_EXTENSIONS_KEY))
        return extensions;

    return extensions.concat(global.settings.get_strv(ENABLED_EXTENSIONS_KEY));
}

function _onEnabledExtensionsChanged() {
    let newEnabledExtensions = getEnabledExtensions();

    // Find and enable all the newly enabled extensions: UUIDs found in the
    // new setting, but not in the old one.
    newEnabledExtensions.filter(
        uuid => !_enabledExtensions.includes(uuid)
    ).forEach(uuid => {
        _enableExtension(uuid);
    });

    // Find and disable all the newly disabled extensions: UUIDs found in the
    // old setting, but not in the new one.
    _extensionOrder.filter(
        uuid => !newEnabledExtensions.includes(uuid)
    ).slice().reverse().forEach(uuid => {
        _disableExtension(uuid);
    });

    _enabledExtensions = newEnabledExtensions;
}

function _onVersionValidationChanged() {
    // Disable the extensions in a fast way before reloading
    // them. While reloadExtension would also disable them,
    // it can't be called using the reversed _extensionOrder
    // array, which avoids disabling and re-enabling a lot
    // extensions during the process.
    _extensionOrder.slice().reverse().forEach(uuid => _disableExtension(uuid));

    for (let uuid in ExtensionUtils.extensions)
        reloadExtension(ExtensionUtils.extensions[uuid]);
}

function _loadExtensions() {
    _enabledExtensions = getEnabledExtensions();

    ExtensionUtils.scanExtensions((extension) => loadExtension(extension));
    _initialized = true;
}

function _connectSettingsChanged() {
    if (_settingsConnections.length > 0)
        return;

    _settingsConnections.push(global.settings.connect('changed::' + ENABLED_EXTENSIONS_KEY, _onEnabledExtensionsChanged));
    _settingsConnections.push(global.settings.connect('changed::' + DISABLE_USER_EXTENSIONS_KEY, _onEnabledExtensionsChanged));
    _settingsConnections.push(global.settings.connect('changed::' + EXTENSION_DISABLE_VERSION_CHECK_KEY, _onVersionValidationChanged));
}

function _disconnectSettingsChanged() {
    _settingsConnections.forEach(id => {
        global.settings.disconnect(id);
    });

    _settingsConnections = [];
}

function _sessionUpdated() {
    // For now sessionMode.allowExtensions controls extensions from both the
    // 'enabled-extensions' preference and the sessionMode.enabledExtensions
    // property; it might make sense to make _enabledExtensions independent
    // from allowExtensions in the future
    if (Main.sessionMode.allowExtensions) {
        if (!_initialized)
            _loadExtensions();
        else
            _onEnabledExtensionsChanged();

        _connectSettingsChanged();
    } else {
        _disconnectSettingsChanged();

        _extensionOrder.slice().reverse().forEach(uuid => _disableExtension(uuid));
        _enabledExtensions = [];
    }
}

function init() {
    Main.sessionMode.connect('updated', _sessionUpdated);
    _sessionUpdated();
}
