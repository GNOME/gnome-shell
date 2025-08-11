import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import St from 'gi://St';
import Shell from 'gi://Shell';
import * as Signals from '../misc/signals.js';

import * as Config from '../misc/config.js';
import * as ExtensionDownloader from './extensionDownloader.js';
import {formatError} from '../misc/errorUtils.js';
import {
    ExtensionState, ExtensionType, loadExtensionMetadata,
} from '../misc/extensionUtils.js';
import * as FileUtils from '../misc/fileUtils.js';
import * as Main from './main.js';
import * as MessageTray from './messageTray.js';

const ENABLED_EXTENSIONS_KEY = 'enabled-extensions';
const DISABLED_EXTENSIONS_KEY = 'disabled-extensions';
const DISABLE_USER_EXTENSIONS_KEY = 'disable-user-extensions';
const EXTENSION_DISABLE_VERSION_CHECK_KEY = 'disable-extension-version-validation';

const UPDATE_CHECK_TIMEOUT = 24 * 60 * 60; // 1 day in seconds

function stateToString(state) {
    return Object.keys(ExtensionState).find(k => ExtensionState[k] === state);
}

export class ExtensionManager extends Signals.EventEmitter {
    constructor() {
        super();

        this._initializationPromise = null;
        this._updateNotified = false;
        this._updateInProgress = false;
        this._updatedUUIDS = [];

        this._extensions = new Map();
        this._unloadedExtensions = new Map();
        this._enabledExtensions = [];
        this._extensionOrder = [];
        this._checkVersion = false;

        St.Settings.get().connect('notify::color-scheme',
            () => this._reloadExtensionStylesheets());

        Main.sessionMode.connect('updated', () => {
            this._sessionUpdated().catch(logError);
        });
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
            log(`Failed to create file ${disableFilename}: ${e.message}`);
        }

        const shutdownId = global.connect('shutdown',
            () => disableFile.delete(null));

        GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, 60, () => {
            global.disconnect(shutdownId);

            disableFile.delete(null);
            return GLib.SOURCE_REMOVE;
        });

        this._installExtensionUpdates();
        this._sessionUpdated().then(() => {
            ExtensionDownloader.checkForUpdates();
        }).catch(logError);

        GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, UPDATE_CHECK_TIMEOUT, () => {
            ExtensionDownloader.checkForUpdates();

            return GLib.SOURCE_CONTINUE;
        });
    }

    get updatesSupported() {
        const appSys = Shell.AppSystem.get_default();
        const hasUpdatesApp =
            appSys.lookup_app('org.gnome.Extensions.desktop') !== null ||
            appSys.lookup_app('com.mattjakeman.ExtensionManager.desktop') !== null;
        const allowed = global.settings.get_boolean('allow-extension-installation');
        return allowed && hasUpdatesApp;
    }

    lookup(uuid) {
        return this._extensions.get(uuid);
    }

    getUuids() {
        return [...this._extensions.keys()];
    }

    _reloadExtensionStylesheets() {
        for (const ext of this._extensions.values()) {
            // No stylesheet, nothing to reload
            if (!ext.stylesheet)
                continue;

            // No variants, so skip reloading
            const path = ext.stylesheet.get_path();
            if (!path.endsWith('-dark.css') && !path.endsWith('-light.css'))
                continue;

            try {
                this._unloadExtensionStylesheet(ext);
                this._loadExtensionStylesheet(ext);
            } catch (e) {
                this._callExtensionDisable(ext.uuid);
                this.logExtensionError(ext.uuid, e);
            }
        }
    }

    _loadExtensionStylesheet(extension) {
        if (extension.state !== ExtensionState.ACTIVE &&
            extension.state !== ExtensionState.ACTIVATING)
            return;

        const variant = Main.getStyleVariant();
        const stylesheetNames = [
            `${global.sessionMode}-${variant}.css`,
            `stylesheet-${variant}.css`,
            `${global.sessionMode}.css`,
            'stylesheet.css',
        ];
        const theme = St.ThemeContext.get_for_stage(global.stage).get_theme();
        for (const name of stylesheetNames) {
            try {
                const stylesheetFile = extension.dir.get_child(name);
                theme.load_stylesheet(stylesheetFile);
                extension.stylesheet = stylesheetFile;
                break;
            } catch (e) {
                if (e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_FOUND))
                    continue; // not an error
                throw e;
            }
        }
    }

    _unloadExtensionStylesheet(extension) {
        if (!extension.stylesheet)
            return;

        const theme = St.ThemeContext.get_for_stage(global.stage).get_theme();
        theme.unload_stylesheet(extension.stylesheet);
        delete extension.stylesheet;
    }

    _changeExtensionState(extension, newState) {
        const strState = stateToString(newState);
        console.debug(`Changing state of extension ${extension.uuid} to ${strState}`);

        extension.state = newState;
        this.emit('extension-state-changed', extension);
    }

    _extensionSupportsSessionMode(uuid) {
        const extension = this.lookup(uuid);

        if (!extension)
            return false;

        if (extension.sessionModes.includes(Main.sessionMode.currentMode))
            return true;

        if (extension.sessionModes.includes(Main.sessionMode.parentMode))
            return true;

        return false;
    }

    async _callExtensionDisable(uuid) {
        let extension = this.lookup(uuid);
        if (!extension)
            return;

        if (extension.state !== ExtensionState.ACTIVE)
            return;

        this._changeExtensionState(extension, ExtensionState.DEACTIVATING);

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
                console.debug(`Temporarily disable extension ${otherUuid}`);
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

        this._unloadExtensionStylesheet(extension);

        for (let i = 0; i < order.length; i++) {
            let otherUuid = order[i];
            try {
                console.debug(`Re-enable extension ${otherUuid}`);
                // eslint-disable-next-line no-await-in-loop
                await this.lookup(otherUuid).stateObj.enable();
            } catch (e) {
                this.logExtensionError(otherUuid, e);
            }
        }

        this._extensionOrder.splice(orderIdx, 1);

        if (extension.state !== ExtensionState.ERROR)
            this._changeExtensionState(extension, ExtensionState.INACTIVE);
    }

    async _callExtensionEnable(uuid) {
        if (!this._extensionSupportsSessionMode(uuid))
            return;

        let extension = this.lookup(uuid);
        if (!extension)
            return;

        if (extension.state === ExtensionState.INITIALIZED)
            await this._callExtensionInit(uuid);


        if (extension.state !== ExtensionState.INACTIVE)
            return;

        this._changeExtensionState(extension, ExtensionState.ACTIVATING);

        try {
            this._loadExtensionStylesheet(extension);
        } catch (e) {
            this.logExtensionError(uuid, e);
            return;
        }

        try {
            await extension.stateObj.enable();
            this._changeExtensionState(extension, ExtensionState.ACTIVE);
            this._extensionOrder.push(uuid);
        } catch (e) {
            this._unloadExtensionStylesheet(extension);
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
        if (this._updateInProgress) {
            this._updatedUUIDS.push(uuid);
            return;
        }

        let extension = this.lookup(uuid);
        if (!extension)
            return;

        extension.hasUpdate = true;
        this.emit('extension-state-changed', extension);

        if (!this._updateNotified) {
            this._updateNotified = true;

            let source = new ExtensionUpdateSource();
            Main.messageTray.add(source);

            const notification = new MessageTray.Notification({
                source,
                title: _('Extension Updates Available'),
                body: _('Extension updates are ready to be installed'),
            });
            notification.connect('activated',
                () => source.open());
            source.addNotification(notification);
        }
    }

    logExtensionError(uuid, error) {
        let extension = this.lookup(uuid);
        if (!extension)
            return;

        const message = formatError(error, {showStack: false});

        console.debug(`Changing state of extension ${uuid} to ERROR`);
        extension.error = message;
        extension.state = ExtensionState.ERROR;
        if (!extension.errors)
            extension.errors = [];
        extension.errors.push(message);

        logError(error, `Extension ${uuid}`);
        this.emit('extension-state-changed', extension);
    }

    createExtensionObject(uuid, dir, type) {
        const metadata = loadExtensionMetadata(uuid, dir);
        const extension = {
            metadata,
            uuid,
            type,
            dir,
            path: dir.get_path(),
            error: '',
            hasPrefs: dir.get_child('prefs.js').query_exists(null),
            enabled: this._enabledExtensions.includes(uuid),
            hasUpdate: false,
            canChange: false,
            sessionModes: metadata['session-modes'] ?? ['user'],
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

    _isOutOfDate(extension) {
        const [major] = Config.PACKAGE_VERSION.split('.');
        return !extension.metadata['shell-version'].some(v => v.startsWith(major));
    }

    async loadExtension(extension) {
        const {uuid} = extension;
        console.debug(`Loading extension ${uuid}`);
        // Default to error, we set success as the last step
        extension.state = ExtensionState.ERROR;

        if (this._checkVersion && this._isOutOfDate(extension)) {
            extension.state = ExtensionState.OUT_OF_DATE;
        } else if (!this._canLoad(extension)) {
            this.logExtensionError(uuid, new Error(
                'A different version was loaded previously. You need to log out for changes to take effect.'));
        } else {
            const enabled = this._enabledExtensions.includes(uuid) &&
                            this._extensionSupportsSessionMode(uuid);
            if (enabled) {
                if (!await this._callExtensionInit(uuid))
                    return;

                if (extension.state === ExtensionState.INACTIVE)
                    await this._callExtensionEnable(uuid);
            } else {
                extension.state = ExtensionState.INITIALIZED;
            }

            this._unloadedExtensions.delete(uuid);
        }

        console.debug(`Extension ${uuid} in state ${stateToString(extension.state)} after loading`);
        this._updateCanChange(extension);
        this.emit('extension-state-changed', extension);
    }

    async unloadExtension(extension) {
        const {uuid, type} = extension;

        // Try to disable it -- if it's ERROR'd, we can't guarantee that,
        // but it will be removed on next reboot, and hopefully nothing
        // broke too much.
        await this._callExtensionDisable(uuid);

        this._changeExtensionState(extension, ExtensionState.UNINSTALLED);

        // The extension is now cached and it's impossible to load a different version
        if (type === ExtensionType.PER_USER && extension.isImported)
            this._unloadedExtensions.set(uuid, extension.metadata.version);

        this._extensions.delete(uuid);
        return true;
    }

    async reloadExtension(oldExtension) {
        // Grab the things we'll need to pass to createExtensionObject
        // to reload it.
        let {uuid, dir, type} = oldExtension;

        // Then unload the old extension.
        await this.unloadExtension(oldExtension);

        // Now, recreate the extension and load it.
        let newExtension;
        try {
            newExtension = this.createExtensionObject(uuid, dir, type);
        } catch (e) {
            this.logExtensionError(uuid, e);
            return;
        }

        await this.loadExtension(newExtension);
    }

    async _callExtensionInit(uuid) {
        if (!this._extensionSupportsSessionMode(uuid))
            return false;

        let extension = this.lookup(uuid);
        if (!extension)
            throw new Error('Extension was not properly created. Call createExtensionObject first');

        let dir = extension.dir;
        let extensionJs = dir.get_child('extension.js');
        if (!extensionJs.query_exists(null)) {
            this.logExtensionError(uuid, new Error('Missing extension.js'));
            return false;
        }

        let extensionModule;
        let extensionState = null;

        try {
            extensionModule = await import(extensionJs.get_uri());

            // Extensions can only be imported once, so add a property to avoid
            // attempting to re-import an extension.
            extension.isImported = true;
        } catch (e) {
            this.logExtensionError(uuid, e);
            return false;
        }

        try {
            const {metadata, path} = extension;
            extensionState =
                new extensionModule.default({...metadata, dir, path});
        } catch (e) {
            this.logExtensionError(uuid, e);
            return false;
        }

        extension.stateObj = extensionState;
        this._changeExtensionState(extension, ExtensionState.INACTIVE);
        return true;
    }

    _getModeExtensions() {
        if (Array.isArray(Main.sessionMode.enabledExtensions))
            return Main.sessionMode.enabledExtensions;
        return [];
    }

    _updateCanChange(extension) {
        let isMode = this._getModeExtensions().includes(extension.uuid);
        let modeOnly = global.settings.get_boolean(DISABLE_USER_EXTENSIONS_KEY);

        let changeKey = isMode
            ? DISABLE_USER_EXTENSIONS_KEY
            : ENABLED_EXTENSIONS_KEY;

        extension.canChange =
            global.settings.is_writable(changeKey) &&
            (isMode || !modeOnly);
    }

    _getEnabledExtensions() {
        let extensions = this._getModeExtensions();

        if (!global.settings.get_boolean(DISABLE_USER_EXTENSIONS_KEY))
            extensions = extensions.concat(global.settings.get_strv(ENABLED_EXTENSIONS_KEY));

        extensions.sort((a, b) => this._compareExtensions(this.lookup(a), this.lookup(b)));

        // filter out 'disabled-extensions' which takes precedence
        let disabledExtensions = global.settings.get_strv(DISABLED_EXTENSIONS_KEY);
        return extensions.filter(item => !disabledExtensions.includes(item));
    }

    async _onUserExtensionsEnabledChanged() {
        await this._onEnabledExtensionsChanged();
        this._onSettingsWritableChanged();
    }

    async _onEnabledExtensionsChanged() {
        let newEnabledExtensions = this._getEnabledExtensions();

        for (const extension of this._extensions.values()) {
            const wasEnabled = extension.enabled;
            extension.enabled = newEnabledExtensions.includes(extension.uuid);
            if (wasEnabled !== extension.enabled)
                this.emit('extension-state-changed', extension);
        }

        // Find and disable all the newly disabled extensions: UUIDs found in the
        // old setting, but not in the new one.
        const extensionsToDisable = this._extensionOrder
            .filter(uuid => !newEnabledExtensions.includes(uuid) ||
                            !this._extensionSupportsSessionMode(uuid));
        // Reverse mutates the original array, but .filter() creates a new array.
        extensionsToDisable.reverse();

        for (const uuid of extensionsToDisable) {
            // eslint-disable-next-line no-await-in-loop
            await this._callExtensionDisable(uuid);
        }

        // Find and enable all the newly enabled extensions: UUIDs found in the
        // new setting, but not in the old one.
        const extensionsToEnable = newEnabledExtensions
            .filter(uuid => !this._enabledExtensions.includes(uuid) &&
                             this._extensionSupportsSessionMode(uuid));
        for (const uuid of extensionsToEnable) {
            // eslint-disable-next-line no-await-in-loop
            await this._callExtensionEnable(uuid);
        }

        this._enabledExtensions = newEnabledExtensions;
    }

    _onSettingsWritableChanged() {
        for (let extension of this._extensions.values()) {
            this._updateCanChange(extension);
            this.emit('extension-state-changed', extension);
        }
    }

    async _onVersionValidationChanged() {
        const checkVersion = !global.settings.get_boolean(EXTENSION_DISABLE_VERSION_CHECK_KEY);
        if (checkVersion === this._checkVersion)
            return;

        this._checkVersion = checkVersion;

        // Disabling extensions modifies the order array, so use a copy
        let extensionOrder = this._extensionOrder.slice();

        // Disable enabled extensions first to avoid
        // the "rebasing" done in _callExtensionDisable...
        this._disableAllExtensions();

        // ...and then reload and enable extensions in the correct order again.
        const extensionsToReload = [...this._extensions.values()].sort((a, b) => {
            return extensionOrder.indexOf(a.uuid) - extensionOrder.indexOf(b.uuid);
        });
        for (const extension of extensionsToReload) {
            // eslint-disable-next-line no-await-in-loop
            await this.reloadExtension(extension);
        }
    }

    async _handleMajorUpdate() {
        const [majorVersion] = Config.PACKAGE_VERSION.split('.');
        const path = `${global.userdatadir}/update-check-${majorVersion}`;
        const file = Gio.File.new_for_path(path);

        try {
            if (!await file.touch_async())
                return;
        } catch (e) {
            logError(e);
        }

        this._updateInProgress = true;

        await ExtensionDownloader.checkForUpdates();
        this._installExtensionUpdates();

        this._updatedUUIDS.map(uuid => this.lookup(uuid)).forEach(
            ext => this.reloadExtension(ext));
        this._updatedUUIDS = [];

        this._updateInProgress = false;
    }

    _installExtensionUpdates() {
        if (!this.updatesSupported)
            return;

        for (const {file: dir, info} of FileUtils.collectFromDatadirs('extension-updates', true)) {
            let fileType = info.get_file_type();
            if (fileType !== Gio.FileType.DIRECTORY)
                continue;
            let uuid = info.get_name();
            let extensionDir = Gio.File.new_for_path(
                GLib.build_filenamev([global.userdatadir, 'extensions', uuid]));

            try {
                FileUtils.recursivelyDeleteDir(extensionDir, false);
                FileUtils.recursivelyMoveDir(dir, extensionDir);
            } catch {
                log(`Failed to install extension updates for ${uuid}`);
            }

            try {
                FileUtils.recursivelyDeleteDir(dir, true);
            } catch (e) {
                console.error(`Failed to delete extension update: ${e.message}`);
            }
        }
    }

    _compareExtensions(a, b) {
        const modesA = a?.sessionModes ?? [];
        const modesB = b?.sessionModes ?? [];
        return modesB.length - modesA.length;
    }

    async _loadExtensions() {
        global.settings.connect(`changed::${ENABLED_EXTENSIONS_KEY}`, () => {
            this._onEnabledExtensionsChanged();
        });
        global.settings.connect(`changed::${DISABLED_EXTENSIONS_KEY}`, () => {
            this._onEnabledExtensionsChanged();
        });
        global.settings.connect(`changed::${DISABLE_USER_EXTENSIONS_KEY}`, () => {
            this._onUserExtensionsEnabledChanged();
        });
        global.settings.connect(`changed::${EXTENSION_DISABLE_VERSION_CHECK_KEY}`, () => {
            this._onVersionValidationChanged();
        });
        global.settings.connect(`writable-changed::${ENABLED_EXTENSIONS_KEY}`, () =>
            this._onSettingsWritableChanged());
        global.settings.connect(`writable-changed::${DISABLED_EXTENSIONS_KEY}`, () =>
            this._onSettingsWritableChanged());

        await this._onVersionValidationChanged();

        this._enabledExtensions = this._getEnabledExtensions();

        let perUserDir = Gio.File.new_for_path(global.userdatadir);

        const includeUserDir = global.settings.get_boolean('allow-extension-installation');
        const extensionFiles = [...FileUtils.collectFromDatadirs('extensions', includeUserDir)];
        const extensionObjects = extensionFiles.map(({file: dir, info}) => {
            let fileType = info.get_file_type();
            if (fileType !== Gio.FileType.DIRECTORY)
                return null;
            let uuid = info.get_name();
            let existing = this.lookup(uuid);
            if (existing) {
                log(`Extension ${uuid} already installed in ${existing.path}. ${dir.get_path()} will not be loaded`);
                return null;
            }

            let extension;
            let type = dir.has_prefix(perUserDir)
                ? ExtensionType.PER_USER
                : ExtensionType.SYSTEM;
            try {
                extension = this.createExtensionObject(uuid, dir, type);
            } catch (error) {
                logError(error, `Could not load extension ${uuid}`);
                return null;
            }

            return extension;
        }).filter(extension => extension !== null).sort(this._compareExtensions.bind(this));

        // after updating to a new major version,
        // update extensions before loading them
        await this._handleMajorUpdate();

        for (const extension of extensionObjects) {
            // eslint-disable-next-line no-await-in-loop
            await this.loadExtension(extension);
        }
    }

    async _enableAllExtensions() {
        if (!this._initializationPromise)
            this._initializationPromise = this._loadExtensions();

        await this._initializationPromise;

        for (const uuid of this._enabledExtensions) {
            // eslint-disable-next-line no-await-in-loop
            await this._callExtensionEnable(uuid);
        }
    }


    /**
     * Disables all currently enabled extensions.
     */
    async _disableAllExtensions() {
        // Wait for extensions to finish loading before starting
        // to disable, otherwise some extensions may enable after
        // this function.
        if (this._initializationPromise)
            await this._initializationPromise;

        const extensionsToDisable = this._extensionOrder.slice();
        // Extensions are disabled in the reverse order
        // from when they were enabled.
        extensionsToDisable.reverse();

        for (const uuid of extensionsToDisable) {
            // eslint-disable-next-line no-await-in-loop
            await this._callExtensionDisable(uuid);
        }
    }

    async _sessionUpdated() {
        // Take care of added or removed sessionMode extensions
        await this._onEnabledExtensionsChanged();
        await this._enableAllExtensions();
    }
}

const ExtensionUpdateSource = GObject.registerClass(
class ExtensionUpdateSource extends MessageTray.Source {
    constructor() {
        const appSys = Shell.AppSystem.get_default();
        const app =
            appSys.lookup_app('org.gnome.Extensions.desktop') ||
            appSys.lookup_app('com.mattjakeman.ExtensionManager.desktop');

        super({
            title: app.get_name(),
            icon: app.get_icon(),
            policy: MessageTray.NotificationPolicy.newForApp(app),
        });

        this._app = app;
    }

    open() {
        this._app.activate();
        Main.overview.hide();
        Main.panel.closeCalendar();
    }
});
