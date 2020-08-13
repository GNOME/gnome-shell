// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported init connect disconnect */

const { GLib, Gio, GObject, Shell, St } = imports.gi;
const ByteArray = imports.byteArray;
const Signals = imports.signals;

const ExtensionDownloader = imports.ui.extensionDownloader;
const ExtensionUtils = imports.misc.extensionUtils;
const FileUtils = imports.misc.fileUtils;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

const { ExtensionState, ExtensionType } = ExtensionUtils;

const ENABLED_EXTENSIONS_KEY = 'enabled-extensions';
const DISABLED_EXTENSIONS_KEY = 'disabled-extensions';
const DISABLE_USER_EXTENSIONS_KEY = 'disable-user-extensions';
const EXTENSION_DISABLE_VERSION_CHECK_KEY = 'disable-extension-version-validation';

const UPDATE_CHECK_TIMEOUT = 24 * 60 * 60; // 1 day in seconds

var ExtensionManager = class {
    constructor() {
        this._initialized = false;
        this._enabled = false;
        this._updateNotified = false;

        this._extensions = new Map();
        this._unloadedExtensions = new Map();
        this._enabledExtensions = [];
        this._extensionOrder = [];

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
    }

    init() {
        // The following file should exist for a period of time when extensions
        // are enabled after start. If it exists, then the systemd unit will
        // disable extensions should gnome-shell crash.
        // Should the file already exist from a previous login, then this is OK.
        let disableFilename = GLib.build_filenamev([GLib.get_user_runtime_dir(), 'gnome-shell-disable-extensions']);
        let disableFile = Gio.File.new_for_path(disableFilename);
        try {
            disableFile.create(Gio.FileCreateFlags.REPLACE_DESTINATION, null);
        } catch (e) {
            log('Failed to create file %s: %s'.format(disableFilename, e.message));
        }

        GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 60, () => {
            disableFile.delete(null);
            return GLib.SOURCE_REMOVE;
        });

        this._installExtensionUpdates();
        this._sessionUpdated();

        GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, UPDATE_CHECK_TIMEOUT, () => {
            ExtensionDownloader.checkForUpdates();
            return GLib.SOURCE_CONTINUE;
        });
        ExtensionDownloader.checkForUpdates();
    }

    get updatesSupported() {
        const appSys = Shell.AppSystem.get_default();
        return appSys.lookup_app('org.gnome.Extensions.desktop') !== null;
    }

    lookup(uuid) {
        return this._extensions.get(uuid);
    }

    getUuids() {
        return [...this._extensions.keys()];
    }

    _callExtensionDisable(uuid) {
        let extension = this.lookup(uuid);
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

        let orderIdx = this._extensionOrder.indexOf(uuid);
        let order = this._extensionOrder.slice(orderIdx + 1);
        let orderReversed = order.slice().reverse();

        for (let i = 0; i < orderReversed.length; i++) {
            let otherUuid = orderReversed[i];
            try {
                this.lookup(otherUuid).stateObj.disable();
            } catch (e) {
                this.logExtensionError(otherUuid, e);
            }
        }

        try {
            extension.stateObj.disable();
        } catch (e) {
            this.logExtensionError(uuid, e);
        }

        if (extension.stylesheet) {
            let theme = St.ThemeContext.get_for_stage(global.stage).get_theme();
            theme.unload_stylesheet(extension.stylesheet);
            delete extension.stylesheet;
        }

        for (let i = 0; i < order.length; i++) {
            let otherUuid = order[i];
            try {
                this.lookup(otherUuid).stateObj.enable();
            } catch (e) {
                this.logExtensionError(otherUuid, e);
            }
        }

        this._extensionOrder.splice(orderIdx, 1);

        if (extension.state != ExtensionState.ERROR) {
            extension.state = ExtensionState.DISABLED;
            this.emit('extension-state-changed', extension);
        }
    }

    _callExtensionEnable(uuid) {
        if (!Main.sessionMode.allowExtensions)
            return;

        let extension = this.lookup(uuid);
        if (!extension)
            return;

        if (extension.state == ExtensionState.INITIALIZED)
            this._callExtensionInit(uuid);

        if (extension.state != ExtensionState.DISABLED)
            return;

        let stylesheetNames = ['%s.css'.format(global.session_mode), 'stylesheet.css'];
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
                this.logExtensionError(uuid, e);
                return;
            }
        }

        try {
            extension.stateObj.enable();
            extension.state = ExtensionState.ENABLED;
            this._extensionOrder.push(uuid);
            this.emit('extension-state-changed', extension);
        } catch (e) {
            if (extension.stylesheet) {
                theme.unload_stylesheet(extension.stylesheet);
                delete extension.stylesheet;
            }
            this.logExtensionError(uuid, e);
        }
    }

    enableExtension(uuid) {
        if (!this._extensions.has(uuid))
            return false;

        let enabledExtensions = global.settings.get_strv(ENABLED_EXTENSIONS_KEY);
        let disabledExtensions = global.settings.get_strv(DISABLED_EXTENSIONS_KEY);

        if (disabledExtensions.includes(uuid)) {
            disabledExtensions = disabledExtensions.filter(item => item !== uuid);
            global.settings.set_strv(DISABLED_EXTENSIONS_KEY, disabledExtensions);
        }

        if (!enabledExtensions.includes(uuid)) {
            enabledExtensions.push(uuid);
            global.settings.set_strv(ENABLED_EXTENSIONS_KEY, enabledExtensions);
        }

        return true;
    }

    disableExtension(uuid) {
        if (!this._extensions.has(uuid))
            return false;

        let enabledExtensions = global.settings.get_strv(ENABLED_EXTENSIONS_KEY);
        let disabledExtensions = global.settings.get_strv(DISABLED_EXTENSIONS_KEY);

        if (enabledExtensions.includes(uuid)) {
            enabledExtensions = enabledExtensions.filter(item => item !== uuid);
            global.settings.set_strv(ENABLED_EXTENSIONS_KEY, enabledExtensions);
        }

        if (!disabledExtensions.includes(uuid)) {
            disabledExtensions.push(uuid);
            global.settings.set_strv(DISABLED_EXTENSIONS_KEY, disabledExtensions);
        }

        return true;
    }

    openExtensionPrefs(uuid, parentWindow, options) {
        const extension = this.lookup(uuid);
        if (!extension || !extension.hasPrefs)
            return false;

        Gio.DBus.session.call(
            'org.gnome.Shell.Extensions',
            '/org/gnome/Shell/Extensions',
            'org.gnome.Shell.Extensions',
            'OpenExtensionPrefs',
            new GLib.Variant('(ssa{sv})', [uuid, parentWindow, options]),
            null,
            Gio.DBusCallFlags.NONE,
            -1,
            null);
        return true;
    }

    notifyExtensionUpdate(uuid) {
        let extension = this.lookup(uuid);
        if (!extension)
            return;

        extension.hasUpdate = true;
        this.emit('extension-state-changed', extension);

        if (!this._updateNotified) {
            this._updateNotified = true;

            let source = new ExtensionUpdateSource();
            Main.messageTray.add(source);

            let notification = new MessageTray.Notification(source,
                _('Extension Updates Available'),
                _('Extension updates are ready to be installed.'));
            source.showNotification(notification);
        }
    }

    logExtensionError(uuid, error) {
        let extension = this.lookup(uuid);
        if (!extension)
            return;

        const message = error instanceof Error
            ? error.message : error.toString();

        extension.error = message;
        extension.state = ExtensionState.ERROR;
        if (!extension.errors)
            extension.errors = [];
        extension.errors.push(message);

        logError(error, 'Extension %s'.format(uuid));
        this._updateCanChange(extension);
        this.emit('extension-state-changed', extension);
    }

    createExtensionObject(uuid, dir, type) {
        let metadataFile = dir.get_child('metadata.json');
        if (!metadataFile.query_exists(null))
            throw new Error('Missing metadata.json');

        let metadataContents, success_;
        try {
            [success_, metadataContents] = metadataFile.load_contents(null);
            metadataContents = ByteArray.toString(metadataContents);
        } catch (e) {
            throw new Error('Failed to load metadata.json: %s'.format(e.toString()));
        }
        let meta;
        try {
            meta = JSON.parse(metadataContents);
        } catch (e) {
            throw new Error('Failed to parse metadata.json: %s'.format(e.toString()));
        }

        let requiredProperties = ['uuid', 'name', 'description', 'shell-version'];
        for (let i = 0; i < requiredProperties.length; i++) {
            let prop = requiredProperties[i];
            if (!meta[prop])
                throw new Error('missing "%s" property in metadata.json'.format(prop));
        }

        if (uuid != meta.uuid)
            throw new Error('uuid "%s" from metadata.json does not match directory name "%s"'.format(meta.uuid, uuid));

        let extension = {
            metadata: meta,
            uuid: meta.uuid,
            type,
            dir,
            path: dir.get_path(),
            error: '',
            hasPrefs: dir.get_child('prefs.js').query_exists(null),
            hasUpdate: false,
            canChange: false,
        };
        this._extensions.set(uuid, extension);

        return extension;
    }

    _canLoad(extension) {
        if (!this._unloadedExtensions.has(extension.uuid))
            return true;

        const version = this._unloadedExtensions.get(extension.uuid);
        return extension.metadata.version === version;
    }

    loadExtension(extension) {
        // Default to error, we set success as the last step
        extension.state = ExtensionState.ERROR;

        let checkVersion = !global.settings.get_boolean(EXTENSION_DISABLE_VERSION_CHECK_KEY);

        if (checkVersion && ExtensionUtils.isOutOfDate(extension)) {
            extension.state = ExtensionState.OUT_OF_DATE;
        } else if (!this._canLoad(extension)) {
            this.logExtensionError(extension.uuid, new Error(
                'A different version was loaded previously. You need to log out for changes to take effect.'));
        } else {
            let enabled = this._enabledExtensions.includes(extension.uuid);
            if (enabled) {
                if (!this._callExtensionInit(extension.uuid))
                    return;
                if (extension.state == ExtensionState.DISABLED)
                    this._callExtensionEnable(extension.uuid);
            } else {
                extension.state = ExtensionState.INITIALIZED;
            }

            this._unloadedExtensions.delete(extension.uuid);
        }

        this._updateCanChange(extension);
        this.emit('extension-state-changed', extension);
    }

    unloadExtension(extension) {
        const { uuid, type } = extension;

        // Try to disable it -- if it's ERROR'd, we can't guarantee that,
        // but it will be removed on next reboot, and hopefully nothing
        // broke too much.
        this._callExtensionDisable(uuid);

        extension.state = ExtensionState.UNINSTALLED;
        this.emit('extension-state-changed', extension);

        // If we did install an importer, it is now cached and it's
        // impossible to load a different version
        if (type === ExtensionType.PER_USER && extension.imports)
            this._unloadedExtensions.set(uuid, extension.metadata.version);

        this._extensions.delete(uuid);
        return true;
    }

    reloadExtension(oldExtension) {
        // Grab the things we'll need to pass to createExtensionObject
        // to reload it.
        let { uuid, dir, type } = oldExtension;

        // Then unload the old extension.
        this.unloadExtension(oldExtension);

        // Now, recreate the extension and load it.
        let newExtension;
        try {
            newExtension = this.createExtensionObject(uuid, dir, type);
        } catch (e) {
            this.logExtensionError(uuid, e);
            return;
        }

        this.loadExtension(newExtension);
    }

    _callExtensionInit(uuid) {
        if (!Main.sessionMode.allowExtensions)
            return false;

        let extension = this.lookup(uuid);
        if (!extension)
            throw new Error("Extension was not properly created. Call createExtensionObject first");

        let dir = extension.dir;
        let extensionJs = dir.get_child('extension.js');
        if (!extensionJs.query_exists(null)) {
            this.logExtensionError(uuid, new Error('Missing extension.js'));
            return false;
        }

        let extensionModule;
        let extensionState = null;

        ExtensionUtils.installImporter(extension);
        try {
            extensionModule = extension.imports.extension;
        } catch (e) {
            this.logExtensionError(uuid, e);
            return false;
        }

        if (extensionModule.init) {
            try {
                extensionState = extensionModule.init(extension);
            } catch (e) {
                this.logExtensionError(uuid, e);
                return false;
            }
        }

        if (!extensionState)
            extensionState = extensionModule;
        extension.stateObj = extensionState;

        extension.state = ExtensionState.DISABLED;
        this.emit('extension-loaded', uuid);
        return true;
    }

    _getModeExtensions() {
        if (Array.isArray(Main.sessionMode.enabledExtensions))
            return Main.sessionMode.enabledExtensions;
        return [];
    }

    _updateCanChange(extension) {
        let hasError =
            extension.state == ExtensionState.ERROR ||
            extension.state == ExtensionState.OUT_OF_DATE;

        let isMode = this._getModeExtensions().includes(extension.uuid);
        let modeOnly = global.settings.get_boolean(DISABLE_USER_EXTENSIONS_KEY);

        let changeKey = isMode
            ? DISABLE_USER_EXTENSIONS_KEY
            : ENABLED_EXTENSIONS_KEY;

        extension.canChange =
            !hasError &&
            global.settings.is_writable(changeKey) &&
            (isMode || !modeOnly);
    }

    _getEnabledExtensions() {
        let extensions = this._getModeExtensions();

        if (!global.settings.get_boolean(DISABLE_USER_EXTENSIONS_KEY))
            extensions = extensions.concat(global.settings.get_strv(ENABLED_EXTENSIONS_KEY));

        // filter out 'disabled-extensions' which takes precedence
        let disabledExtensions = global.settings.get_strv(DISABLED_EXTENSIONS_KEY);
        return extensions.filter(item => !disabledExtensions.includes(item));
    }

    _onUserExtensionsEnabledChanged() {
        this._onEnabledExtensionsChanged();
        this._onSettingsWritableChanged();
    }

    _onEnabledExtensionsChanged() {
        let newEnabledExtensions = this._getEnabledExtensions();

        // Find and enable all the newly enabled extensions: UUIDs found in the
        // new setting, but not in the old one.
        newEnabledExtensions
            .filter(uuid => !this._enabledExtensions.includes(uuid))
            .forEach(uuid => this._callExtensionEnable(uuid));

        // Find and disable all the newly disabled extensions: UUIDs found in the
        // old setting, but not in the new one.
        this._extensionOrder
            .filter(uuid => !newEnabledExtensions.includes(uuid))
            .reverse().forEach(uuid => this._callExtensionDisable(uuid));

        this._enabledExtensions = newEnabledExtensions;
    }

    _onSettingsWritableChanged() {
        for (let extension of this._extensions.values()) {
            this._updateCanChange(extension);
            this.emit('extension-state-changed', extension);
        }
    }

    _onVersionValidationChanged() {
        // Disabling extensions modifies the order array, so use a copy
        let extensionOrder = this._extensionOrder.slice();

        // Disable enabled extensions in the reverse order first to avoid
        // the "rebasing" done in _callExtensionDisable...
        extensionOrder.slice().reverse().forEach(uuid => {
            this._callExtensionDisable(uuid);
        });

        // ...and then reload and enable extensions in the correct order again.
        [...this._extensions.values()].sort((a, b) => {
            return extensionOrder.indexOf(a.uuid) - extensionOrder.indexOf(b.uuid);
        }).forEach(extension => this.reloadExtension(extension));
    }

    _installExtensionUpdates() {
        if (!this.updatesSupported)
            return;

        FileUtils.collectFromDatadirs('extension-updates', true, (dir, info) => {
            let fileType = info.get_file_type();
            if (fileType !== Gio.FileType.DIRECTORY)
                return;
            let uuid = info.get_name();
            let extensionDir = Gio.File.new_for_path(
                GLib.build_filenamev([global.userdatadir, 'extensions', uuid]));

            try {
                FileUtils.recursivelyDeleteDir(extensionDir, false);
                FileUtils.recursivelyMoveDir(dir, extensionDir);
            } catch (e) {
                log('Failed to install extension updates for %s'.format(uuid));
            } finally {
                FileUtils.recursivelyDeleteDir(dir, true);
            }
        });
    }

    _loadExtensions() {
        global.settings.connect('changed::%s'.format(ENABLED_EXTENSIONS_KEY),
            this._onEnabledExtensionsChanged.bind(this));
        global.settings.connect('changed::%s'.format(DISABLED_EXTENSIONS_KEY),
            this._onEnabledExtensionsChanged.bind(this));
        global.settings.connect('changed::%s'.format(DISABLE_USER_EXTENSIONS_KEY),
            this._onUserExtensionsEnabledChanged.bind(this));
        global.settings.connect('changed::%s'.format(EXTENSION_DISABLE_VERSION_CHECK_KEY),
            this._onVersionValidationChanged.bind(this));
        global.settings.connect('writable-changed::%s'.format(ENABLED_EXTENSIONS_KEY),
            this._onSettingsWritableChanged.bind(this));
        global.settings.connect('writable-changed::%s'.format(DISABLED_EXTENSIONS_KEY),
            this._onSettingsWritableChanged.bind(this));

        this._enabledExtensions = this._getEnabledExtensions();

        let perUserDir = Gio.File.new_for_path(global.userdatadir);
        FileUtils.collectFromDatadirs('extensions', true, (dir, info) => {
            let fileType = info.get_file_type();
            if (fileType != Gio.FileType.DIRECTORY)
                return;
            let uuid = info.get_name();
            let existing = this.lookup(uuid);
            if (existing) {
                log('Extension %s already installed in %s. %s will not be loaded'.format(uuid, existing.path, dir.get_path()));
                return;
            }

            let extension;
            let type = dir.has_prefix(perUserDir)
                ? ExtensionType.PER_USER
                : ExtensionType.SYSTEM;
            try {
                extension = this.createExtensionObject(uuid, dir, type);
            } catch (e) {
                logError(e, 'Could not load extension %s'.format(uuid));
                return;
            }
            this.loadExtension(extension);
        });
    }

    _enableAllExtensions() {
        if (this._enabled)
            return;

        if (!this._initialized) {
            this._loadExtensions();
            this._initialized = true;
        } else {
            this._enabledExtensions.forEach(uuid => {
                this._callExtensionEnable(uuid);
            });
        }
        this._enabled = true;
    }

    _disableAllExtensions() {
        if (!this._enabled)
            return;

        if (this._initialized) {
            this._extensionOrder.slice().reverse().forEach(uuid => {
                this._callExtensionDisable(uuid);
            });
        }

        this._enabled = false;
    }

    _sessionUpdated() {
        // For now sessionMode.allowExtensions controls extensions from both the
        // 'enabled-extensions' preference and the sessionMode.enabledExtensions
        // property; it might make sense to make enabledExtensions independent
        // from allowExtensions in the future
        if (Main.sessionMode.allowExtensions) {
            // Take care of added or removed sessionMode extensions
            this._onEnabledExtensionsChanged();
            this._enableAllExtensions();
        } else {
            this._disableAllExtensions();
        }
    }
};
Signals.addSignalMethods(ExtensionManager.prototype);

const ExtensionUpdateSource = GObject.registerClass(
class ExtensionUpdateSource extends MessageTray.Source {
    _init() {
        let appSys = Shell.AppSystem.get_default();
        this._app = appSys.lookup_app('org.gnome.Extensions.desktop');

        super._init(this._app.get_name());
    }

    getIcon() {
        return this._app.app_info.get_icon();
    }

    _createPolicy() {
        return new MessageTray.NotificationApplicationPolicy(this._app.id);
    }

    open() {
        this._app.activate();
        Main.overview.hide();
        Main.panel.closeCalendar();
    }
});
