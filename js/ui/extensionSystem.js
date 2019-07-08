// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Gio, St } = imports.gi;
const Signals = imports.signals;

const ExtensionUtils = imports.misc.extensionUtils;
const Main = imports.ui.main;

const { ExtensionState } = ExtensionUtils;

const ENABLED_EXTENSIONS_KEY = 'enabled-extensions';
const DISABLE_USER_EXTENSIONS_KEY = 'disable-user-extensions';
const EXTENSION_DISABLE_VERSION_CHECK_KEY = 'disable-extension-version-validation';

var ExtensionManager = class {
    constructor() {
        this._initted = false;
        this._enabled = false;

        this._enabledExtensions = [];
        this._extensionOrder = [];

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
    }

    _callExtensionDisable(uuid) {
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

        let orderIdx = this._extensionOrder.indexOf(uuid);
        let order = this._extensionOrder.slice(orderIdx + 1);
        let orderReversed = order.slice().reverse();

        for (let i = 0; i < orderReversed.length; i++) {
            let uuid = orderReversed[i];
            try {
                ExtensionUtils.extensions[uuid].stateObj.disable();
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
                ExtensionUtils.extensions[uuid].stateObj.enable();
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
        let extension = ExtensionUtils.extensions[uuid];
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
                log(`Failed to load stylesheet for extension ${uuid}: ${e.message}`);
                return;
            }
        }

        try {
            extension.stateObj.enable();
            extension.state = ExtensionState.ENABLED;
            this.emit('extension-state-changed', extension);
            return;
        } catch (e) {
            if (extension.stylesheet) {
                theme.unload_stylesheet(extension.stylesheet);
                delete extension.stylesheet;
            }
            this.logExtensionError(uuid, e);
            return;
        }
    }

    enableExtension(uuid) {
        if (!ExtensionUtils.extensions[uuid])
            return false;

        let enabledExtensions = global.settings.get_strv(ENABLED_EXTENSIONS_KEY);
        if (!enabledExtensions.includes(uuid)) {
            enabledExtensions.push(uuid);
            global.settings.set_strv(ENABLED_EXTENSIONS_KEY, enabledExtensions);
        }

        return true;
    }

    disableExtension(uuid) {
        if (!ExtensionUtils.extensions[uuid])
            return false;

        let enabledExtensions = global.settings.get_strv(ENABLED_EXTENSIONS_KEY);
        if (enabledExtensions.includes(uuid)) {
            enabledExtensions = enabledExtensions.filter(item => item !== uuid);
            global.settings.set_strv(ENABLED_EXTENSIONS_KEY, enabledExtensions);
        }

        return true;
    }

    logExtensionError(uuid, error) {
        let extension = ExtensionUtils.extensions[uuid];
        if (!extension)
            return;

        let message = `${error}`;

        extension.error = message;
        extension.state = ExtensionState.ERROR;
        if (!extension.errors)
            extension.errors = [];
        extension.errors.push(message);

        log('Extension "%s" had error: %s'.format(uuid, message));
        this.emit('extension-state-changed', extension);
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

        this.emit('extension-state-changed', extension);
    }

    unloadExtension(extension) {
        // Try to disable it -- if it's ERROR'd, we can't guarantee that,
        // but it will be removed on next reboot, and hopefully nothing
        // broke too much.
        this._callExtensionDisable(extension.uuid);

        extension.state = ExtensionState.UNINSTALLED;
        this.emit('extension-state-changed', extension);

        delete ExtensionUtils.extensions[extension.uuid];
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
            newExtension = ExtensionUtils.createExtensionObject(uuid, dir, type);
        } catch (e) {
            this.logExtensionError(uuid, e);
            return;
        }

        this.loadExtension(newExtension);
    }

    _callExtensionInit(uuid) {
        let extension = ExtensionUtils.extensions[uuid];
        let dir = extension.dir;

        if (!extension)
            throw new Error("Extension was not properly created. Call loadExtension first");

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

    _getEnabledExtensions() {
        let extensions;
        if (Array.isArray(Main.sessionMode.enabledExtensions))
            extensions = Main.sessionMode.enabledExtensions;
        else
            extensions = [];

        if (global.settings.get_boolean(DISABLE_USER_EXTENSIONS_KEY))
            return extensions;

        return extensions.concat(global.settings.get_strv(ENABLED_EXTENSIONS_KEY));
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

    _onVersionValidationChanged() {
        // we want to reload all extensions, but only enable
        // extensions when allowed by the sessionMode, so
        // temporarily disable them all
        this._enabledExtensions = [];
        for (let uuid in ExtensionUtils.extensions)
            this.reloadExtension(ExtensionUtils.extensions[uuid]);
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
        global.settings.connect(`changed::${DISABLE_USER_EXTENSIONS_KEY}`,
            this._onEnabledExtensionsChanged.bind(this));
        global.settings.connect(`changed::${EXTENSION_DISABLE_VERSION_CHECK_KEY}`,
            this._onVersionValidationChanged.bind(this));

        this._enabledExtensions = this._getEnabledExtensions();

        let finder = new ExtensionUtils.ExtensionFinder();
        finder.connect('extension-found', (finder, extension) => {
            this.loadExtension(extension);
        });
        finder.scanExtensions();
    }

    _enableAllExtensions() {
        if (this._enabled)
            return;

        if (!this._initted) {
            this._loadExtensions();
            this._initted = true;
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

        if (this._initted) {
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
            if (this._initted)
                this._enabledExtensions = this._getEnabledExtensions();
            this._enableAllExtensions();
        } else {
            this._disableAllExtensions();
        }
    }
};
Signals.addSignalMethods(ExtensionManager.prototype);
