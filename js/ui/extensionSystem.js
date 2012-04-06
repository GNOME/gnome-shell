// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Signals = imports.signals;

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gio = imports.gi.Gio;
const St = imports.gi.St;
const Shell = imports.gi.Shell;
const Soup = imports.gi.Soup;

const Config = imports.misc.config;
const ExtensionUtils = imports.misc.extensionUtils;
const FileUtils = imports.misc.fileUtils;
const ModalDialog = imports.ui.modalDialog;

const API_VERSION = 1;

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

const REPOSITORY_URL_BASE = 'https://extensions.gnome.org';
const REPOSITORY_URL_DOWNLOAD = REPOSITORY_URL_BASE + '/download-extension/%s.shell-extension.zip';
const REPOSITORY_URL_INFO =     REPOSITORY_URL_BASE + '/extension-info/';

const _httpSession = new Soup.SessionAsync();

// The unfortunate state of gjs, gobject-introspection and libsoup
// means that I have to do a hack to add a feature.
// See: https://bugzilla.gnome.org/show_bug.cgi?id=655189 for context.

if (Soup.Session.prototype.add_feature != null)
    Soup.Session.prototype.add_feature.call(_httpSession, new Soup.ProxyResolverDefault());

function _getCertFile() {
    let localCert = GLib.build_filenamev([global.userdatadir, 'extensions.gnome.org.crt']);
    if (GLib.file_test(localCert, GLib.FileTest.EXISTS))
        return localCert;
    else
        return Config.SHELL_SYSTEM_CA_FILE;
}

_httpSession.ssl_ca_file = _getCertFile();

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

function installExtensionFromUUID(uuid, version_tag) {
    let params = { uuid: uuid,
                   version_tag: version_tag,
                   shell_version: Config.PACKAGE_VERSION,
                   api_version: API_VERSION.toString() };

    let message = Soup.form_request_new_from_hash('GET', REPOSITORY_URL_INFO, params);

    _httpSession.queue_message(message,
                               function(session, message) {
                                   let info = JSON.parse(message.response_body.data);
                                   let dialog = new InstallExtensionDialog(uuid, version_tag, info.name);
                                   dialog.open(global.get_current_time());
                               });
}

function uninstallExtensionFromUUID(uuid) {
    let extension = ExtensionUtils.extensions[uuid];
    if (!extension)
        return false;

    // Try to disable it -- if it's ERROR'd, we can't guarantee that,
    // but it will be removed on next reboot, and hopefully nothing
    // broke too much.
    disableExtension(uuid);

    // Don't try to uninstall system extensions
    if (extension.type != ExtensionUtils.ExtensionType.PER_USER)
        return false;

    extension.state = ExtensionState.UNINSTALLED;
    _signals.emit('extension-state-changed', extension);

    delete ExtensionUtils.extensions[uuid];

    FileUtils.recursivelyDeleteDir(Gio.file_new_for_path(extension.path));

    return true;
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
    let dir = ExtensionUtils.userExtensionsDir.get_child(uuid);
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

        // Add extension to 'enabled-extensions' for the user, always...
        let enabledExtensions = global.settings.get_strv(ENABLED_EXTENSIONS_KEY);
        if (enabledExtensions.indexOf(uuid) == -1) {
            enabledExtensions.push(uuid);
            global.settings.set_strv(ENABLED_EXTENSIONS_KEY, enabledExtensions);
        }

        loadExtension(dir, ExtensionUtils.ExtensionType.PER_USER, true);
    });
}

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
    global.logError('Extension "%s" had error: %s'.format(uuid, message));
    state = state || ExtensionState.ERROR;
    _signals.emit('extension-state-changed', { uuid: uuid,
                                               error: message,
                                               state: state });
}

function loadExtension(dir, type, enabled) {
    let uuid = dir.get_basename();
    let extension;

    if (ExtensionUtils.extensions[uuid] != undefined) {
        global.logError('Extension "%s" is already loaded'.format(uuid));
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
    global.log('Loaded extension ' + uuid);
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

const InstallExtensionDialog = new Lang.Class({
    Name: 'InstallExtensionDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function(uuid, version_tag, name) {
        this.parent({ styleClass: 'extension-dialog' });

        this._uuid = uuid;
        this._version_tag = version_tag;
        this._name = name;

        this.setButtons([{ label: _("Cancel"),
                           action: Lang.bind(this, this._onCancelButtonPressed),
                           key:    Clutter.Escape
                         },
                         { label:  _("Install"),
                           action: Lang.bind(this, this._onInstallButtonPressed)
                         }]);

        let message = _("Download and install '%s' from extensions.gnome.org?").format(name);

        this._descriptionLabel = new St.Label({ text: message });

        this.contentLayout.add(this._descriptionLabel,
                               { y_fill:  true,
                                 y_align: St.Align.START });
    },

    _onCancelButtonPressed: function(button, event) {
        this.close(global.get_current_time());

        // Even though the extension is already "uninstalled", send through
        // a state-changed signal for any users who want to know if the install
        // went through correctly -- using proper async DBus would block more
        // traditional clients like the plugin
        let meta = { uuid: this._uuid,
                     state: ExtensionState.UNINSTALLED,
                     error: '' };

        _signals.emit('extension-state-changed', meta);
    },

    _onInstallButtonPressed: function(button, event) {
        let state = { uuid: this._uuid,
                      state: ExtensionState.DOWNLOADING,
                      error: '' };

        _signals.emit('extension-state-changed', state);

        let params = { version_tag: this._version_tag,
                       shell_version: Config.PACKAGE_VERSION,
                       api_version: API_VERSION.toString() };

        let url = REPOSITORY_URL_DOWNLOAD.format(this._uuid);
        let message = Soup.form_request_new_from_hash('GET', url, params);

        _httpSession.queue_message(message,
                                   Lang.bind(this, function(session, message) {
                                       gotExtensionZipFile(session, message, this._uuid);
                                   }));

        this.close(global.get_current_time());
    }
});
