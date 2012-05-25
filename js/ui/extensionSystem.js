// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Signals = imports.signals;

const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const St = imports.gi.St;

const ExtensionUtils = imports.misc.extensionUtils;

const ExtensionState = {
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

// Arrays of uuids
var enabledExtensions;
// Contains the order that extensions were enabled in.
const extensionOrder = [];

// We don't really have a class to add signals on. So, create
// a simple dummy object, add the signal methods, and export those
// publically.
var _signals = {};
Signals.addSignalMethods(_signals);

const connect = Lang.bind(_signals, _signals.connect);
const disconnect = Lang.bind(_signals, _signals.disconnect);

const ENABLED_EXTENSIONS_KEY = 'enabled-extensions';

function disableExtension(uuid) {
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

    let orderIdx = extensionOrder.indexOf(uuid);
    let order = extensionOrder.slice(orderIdx + 1);
    let orderReversed = order.slice().reverse();

    for (let i = 0; i < orderReversed.length; i++) {
        let uuid = orderReversed[i];
        try {
            ExtensionUtils.extensions[uuid].stateObj.disable();
        } catch(e) {
            logExtensionError(uuid, e.toString());
        }
    }

    try {
        extension.stateObj.disable();
    } catch(e) {
        logExtensionError(uuid, e.toString());
        return;
    }

    for (let i = 0; i < order.length; i++) {
        let uuid = order[i];
        try {
            ExtensionUtils.extensions[uuid].stateObj.enable();
        } catch(e) {
            logExtensionError(uuid, e.toString());
        }
    }

    extensionOrder.splice(orderIdx, 1);

    extension.state = ExtensionState.DISABLED;
    _signals.emit('extension-state-changed', extension);
}

function enableExtension(uuid) {
    let extension = ExtensionUtils.extensions[uuid];
    if (!extension)
        return;

    if (extension.state == ExtensionState.INITIALIZED)
        initExtension(uuid);

    if (extension.state != ExtensionState.DISABLED)
        return;

    extensionOrder.push(uuid);

    try {
        extension.stateObj.enable();
    } catch(e) {
        logExtensionError(uuid, e.toString());
        return;
    }

    extension.state = ExtensionState.ENABLED;
    _signals.emit('extension-state-changed', extension);
}

function logExtensionError(uuid, message, state) {
    let extension = ExtensionUtils.extensions[uuid];
    if (!extension)
        return;

    if (!extension.errors)
        extension.errors = [];

    extension.errors.push(message);
    log('Extension "%s" had error: %s'.format(uuid, message));
    state = state || ExtensionState.ERROR;
    _signals.emit('extension-state-changed', { uuid: uuid,
                                               error: message,
                                               state: state });
}

function loadExtension(dir, type, enabled) {
    let uuid = dir.get_basename();
    let extension;

    if (ExtensionUtils.extensions[uuid] != undefined) {
        log('Extension "%s" is already loaded'.format(uuid));
        return;
    }

    try {
        extension = ExtensionUtils.createExtensionObject(uuid, dir, type);
    } catch(e) {
        logExtensionError(uuid, e.message);
        return;
    }

    // Default to error, we set success as the last step
    extension.state = ExtensionState.ERROR;

    if (ExtensionUtils.isOutOfDate(extension)) {
        logExtensionError(uuid, 'extension is not compatible with current GNOME Shell and/or GJS version', ExtensionState.OUT_OF_DATE);
        extension.state = ExtensionState.OUT_OF_DATE;
        return;
    }

    if (enabled) {
        initExtension(uuid);
        if (extension.state == ExtensionState.DISABLED)
            enableExtension(uuid);
    } else {
        extension.state = ExtensionState.INITIALIZED;
    }

    _signals.emit('extension-state-changed', extension);
}

function initExtension(uuid) {
    let extension = ExtensionUtils.extensions[uuid];
    let dir = extension.dir;

    if (!extension)
        throw new Error("Extension was not properly created. Call loadExtension first");

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
        ExtensionUtils.installImporter(extension);
        extensionModule = extension.imports.extension;
    } catch (e) {
        if (stylesheetPath != null)
            theme.unload_stylesheet(stylesheetPath);
        logExtensionError(uuid, '' + e);
        return;
    }

    if (!extensionModule.init) {
        logExtensionError(uuid, 'missing \'init\' function');
        return;
    }

    try {
        extensionState = extensionModule.init(extension);
    } catch (e) {
        if (stylesheetPath != null)
            theme.unload_stylesheet(stylesheetPath);
        logExtensionError(uuid, 'Failed to evaluate init function:' + e);
        return;
    }

    if (!extensionState)
        extensionState = extensionModule;
    extension.stateObj = extensionState;

    if (!extensionState.enable) {
        logExtensionError(uuid, 'missing \'enable\' function');
        return;
    }
    if (!extensionState.disable) {
        logExtensionError(uuid, 'missing \'disable\' function');
        return;
    }

    extension.state = ExtensionState.DISABLED;

    _signals.emit('extension-loaded', uuid);
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
    ExtensionUtils.init();

    global.settings.connect('changed::' + ENABLED_EXTENSIONS_KEY, onEnabledExtensionsChanged);
    enabledExtensions = global.settings.get_strv(ENABLED_EXTENSIONS_KEY);
}

function loadExtensions() {
    ExtensionUtils.scanExtensions(function(uuid, dir, type) {
        let enabled = enabledExtensions.indexOf(uuid) != -1;
        loadExtension(dir, type, enabled);
    });
}
