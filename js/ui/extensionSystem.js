// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported init connect disconnect */

const { Gio, St } = imports.gi;
const Signals = imports.signals;

const ExtensionUtils = imports.misc.extensionUtils;
const FileUtils = imports.misc.fileUtils;
const Main = imports.ui.main;

const { ExtensionState, ExtensionType } = ExtensionUtils;

const ENABLED_EXTENSIONS_KEY = 'enabled-extensions';
const DISABLED_EXTENSIONS_KEY = 'disabled-extensions';
const DISABLE_USER_EXTENSIONS_KEY = 'disable-user-extensions';
const EXTENSION_DISABLE_VERSION_CHECK_KEY = 'disable-extension-version-validation';

var ExtensionManager = class {
    constructor() {
        this._initialized = false;
        this._enabled = false;

        this._extensions = new Map();
        this._enabledExtensions = [];
        this._extensionOrder = [];

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
    }

    init() {
        this._sessionUpdated();
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
            let uuid = orderReversed[i];
            try {
                this.lookup(uuid).stateObj.disable();
            } catch (e) {
                this.logExtensionError(uuid, e);
            }
        }

        if (extension.stylesheet) {
            let theme = St.ThemeContext.get_for_stage(global.stage).get_theme();
            theme.unload_stylesheet(extension.stylesheet);
            delete extension.stylesheet;
        }

        try {
            extension.stateObj.disable();
        } catch (e) {
            this.logExtensionError(uuid, e);
        }

        for (let i = 0; i < order.length; i++) {
            let uuid = order[i];
            try {
                this.lookup(uuid).stateObj.enable();
            } catch (e) {
                this.logExtensionError(uuid, e);
            }
        }

        this._extensionOrder.splice(orderIdx, 1);

        if (extension.state != ExtensionState.ERROR) {
            extension.state = ExtensionState.DISABLED;
            this.emit('extension-state-changed', extension);
        }
    }

    _callExtensionEnable(uuid) {
        let extension = this.lookup(uuid);
        if (!extension)
            return;

        if (extension.state == ExtensionState.INITIALIZED)
            this._callExtensionInit(uuid);

        if (extension.state != ExtensionState.DISABLED)
            return;

        this._extensionOrder.push(uuid);

        let stylesheetNames = [`${global.session_mode}.css`, 'stylesheet.css'];
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

    logExtensionError(uuid, error) {
        let extension = this.lookup(uuid);
        if (!extension)
            return;

        let message = `${error}`;

        extension.error = message;
        extension.state = ExtensionState.ERROR;
        if (!extension.errors)
            extension.errors = [];
        extension.errors.push(message);

        logError(error, `Extension ${uuid}`);
        this.emit('extension-state-changed', extension);
    }

    createExtensionObject(uuid, dir, type) {
        let metadataFile = dir.get_child('metadata.json');
        if (!metadataFile.query_exists(null)) {
            throw new Error('Missing metadata.json');
        }

        let metadataContents, success_;
        try {
            [success_, metadataContents] = metadataFile.load_contents(null);
            if (metadataContents instanceof Uint8Array)
                metadataContents = imports.byteArray.toString(metadataContents);
        } catch (e) {
            throw new Error(`Failed to load metadata.json: ${e}`);
        }
        let meta;
        try {
            meta = JSON.parse(metadataContents);
        } catch (e) {
            throw new Error(`Failed to parse metadata.json: ${e}`);
        }

        let requiredProperties = ['uuid', 'name', 'description', 'shell-version'];
        for (let i = 0; i < requiredProperties.length; i++) {
            let prop = requiredProperties[i];
            if (!meta[prop]) {
                throw new Error(`missing "${prop}" property in metadata.json`);
            }
        }

        if (uuid != meta.uuid) {
            throw new Error(`uuid "${meta.uuid}" from metadata.json does not match directory name "${uuid}"`);
        }

        let extension = {
            metadata: meta,
            uuid: meta.uuid,
            type,
            dir,
            path: dir.get_path(),
            error: '',
            hasPrefs: dir.get_child('prefs.js').query_exists(null),
            canChange: false
        };
        this._extensions.set(uuid, extension);

        return extension;
    }

    loadExtension(extension) {
        // Default to error, we set success as the last step
        extension.state = ExtensionState.ERROR;

        let checkVersion = !global.settings.get_boolean(EXTENSION_DISABLE_VERSION_CHECK_KEY);

        if (checkVersion && ExtensionUtils.isOutOfDate(extension)) {
            extension.state = ExtensionState.OUT_OF_DATE;
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
        }

        this._updateCanChange(extension);
        this.emit('extension-state-changed', extension);
    }

    unloadExtension(extension) {
        // Try to disable it -- if it's ERROR'd, we can't guarantee that,
        // but it will be removed on next reboot, and hopefully nothing
        // broke too much.
        this._callExtensionDisable(extension.uuid);

        extension.state = ExtensionState.UNINSTALLED;
        this.emit('extension-state-changed', extension);

        this._extensions.delete(extension.uuid);
        return true;
    }

    reloadExtension(oldExtension) {
        // Grab the things we'll need to pass to createExtensionObject
        // to reload it.
        let { uuid: uuid, dir: dir, type: type } = oldExtension;

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

        if (!this._enabled)
            return;

        // Find and enable all the newly enabled extensions: UUIDs found in the
        // new setting, but not in the old one.
        newEnabledExtensions.filter(
            uuid => !this._enabledExtensions.includes(uuid)
        ).forEach(uuid => {
            this._callExtensionEnable(uuid);
        });

        // Find and disable all the newly disabled extensions: UUIDs found in the
        // old setting, but not in the new one.
        this._enabledExtensions.filter(
            item => !newEnabledExtensions.includes(item)
        ).forEach(uuid => {
            this._callExtensionDisable(uuid);
        });

        this._enabledExtensions = newEnabledExtensions;
    }

    _onSettingsWritableChanged() {
        for (let extension of this._extensions.values()) {
            this._updateCanChange(extension);
            this.emit('extension-state-changed', extension);
        }
    }

    _onVersionValidationChanged() {
        // we want to reload all extensions, but only enable
        // extensions when allowed by the sessionMode, so
        // temporarily disable them all
        this._enabledExtensions = [];

        // The loop modifies the extensions map, so iterate over a copy
        let extensions = [...this._extensions.values()];
        for (let extension of extensions)
            this.reloadExtension(extension);
        this._enabledExtensions = this._getEnabledExtensions();

        if (Main.sessionMode.allowExtensions) {
            this._enabledExtensions.forEach(uuid => {
                this._callExtensionEnable(uuid);
            });
        }
    }

    _loadExtensions() {
        global.settings.connect(`changed::${ENABLED_EXTENSIONS_KEY}`,
            this._onEnabledExtensionsChanged.bind(this));
        global.settings.connect(`changed::${DISABLED_EXTENSIONS_KEY}`,
            this._onEnabledExtensionsChanged.bind(this));
        global.settings.connect(`changed::${DISABLE_USER_EXTENSIONS_KEY}`,
            this._onUserExtensionsEnabledChanged.bind(this));
        global.settings.connect(`changed::${EXTENSION_DISABLE_VERSION_CHECK_KEY}`,
            this._onVersionValidationChanged.bind(this));
        global.settings.connect(`writable-changed::${ENABLED_EXTENSIONS_KEY}`,
            this._onSettingsWritableChanged.bind(this));
        global.settings.connect(`writable-changed::${DISABLED_EXTENSIONS_KEY}`,
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
                log(`Extension ${uuid} already installed in ${existing.path}. ${dir.get_path()} will not be loaded`);
                return;
            }

            let extension;
            let type = dir.has_prefix(perUserDir)
                ? ExtensionType.PER_USER
                : ExtensionType.SYSTEM;
            try {
                extension = this.createExtensionObject(uuid, dir, type);
            } catch (e) {
                logError(e, `Could not load extension ${uuid}`);
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
