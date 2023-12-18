import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';

import {
    ExtensionState, ExtensionType, deserializeExtension
}  from './misc/extensionUtils.js';

const GnomeShellIface = loadInterfaceXML('org.gnome.Shell.Extensions');
const GnomeShellProxy = Gio.DBusProxy.makeProxyWrapper(GnomeShellIface);

let shellVersion;

function loadInterfaceXML(iface) {
    const uri = `resource:///org/gnome/Extensions/dbus-interfaces/${iface}.xml`;
    const f = Gio.File.new_for_uri(uri);

    try {
        let [ok_, bytes] = f.load_contents(null);
        return new TextDecoder().decode(bytes);
    } catch (e) {
        console.error(`Failed to load D-Bus interface ${iface}`);
    }

    return null;
}

class Extension {
    constructor(variant) {
        this.update(variant);
    }

    update(variant) {
        const deserialized = deserializeExtension(variant);

        const {
            uuid, type, state, error, hasPrefs, hasUpdate, canChange, metadata,
        } = deserialized;

        if (!this._uuid)
            this._uuid = uuid;

        if (this._uuid !== uuid)
            throw new Error(`Invalid update of extension ${this._uuid} with data from ${uuid}`);

        const {name} = metadata;
        this._name = name;
        [this._keywords] = GLib.str_tokenize_and_fold(name, null);

        const [desc] = metadata.description.split('\n');
        this._description = desc;

        this._type = type;
        this._errorDetail = error;
        this._state = state;

        const creator = metadata.creator ?? '';
        this._creator = creator;

        const url = metadata.url ?? '';
        this._url = url;

        const version = String(
            metadata['version-name'] || metadata['version'] || '');
        this._version = version;

        this._hasPrefs = hasPrefs;
        this._hasUpdate = hasUpdate;
        this._canChange = canChange;
    }

    get uuid() {
        return this._uuid;
    }

    get name() {
        return this._name;
    }

    get description() {
        return this._description;
    }

    get state() {
        return this._state;
    }

    get creator() {
        return this._creator;
    }

    get url() {
        return this._url;
    }

    get version() {
        return this._version;
    }

    get keywords() {
        return this._keywords;
    }

    get error() {
        if (!this.hasError)
            return '';

        if (this.state === ExtensionState.OUT_OF_DATE) {
            return this.version !== ''
                ? _('The installed version of this extension (%s) is incompatible with the current version of GNOME (%s). The extension has been disabled.').format(this.version, shellVersion)
                : _('The installed version of this extension is incompatible with the current version of GNOME (%s). The extension has been disabled.').format(shellVersion);
        }

        const message = [
            _('An error has occurred in this extension. This could cause issues elsewhere in the system. It is recommended to turn the extension off until the error is resolved.'),
        ];

        if (this._errorDetail) {
            message.push(
                // translators: Details for an extension error
                _('Error details:'), this._errorDetail);
        }

        return message.join('\n\n');
    }

    get hasError() {
        return this.state === ExtensionState.OUT_OF_DATE ||
               this.state === ExtensionState.ERROR;
    }

    get hasPrefs() {
        return this._hasPrefs;
    }

    get hasUpdate() {
        return this._hasUpdate;
    }

    get hasVersion() {
        return this._version !== '';
    }

    get canChange() {
        return this._canChange;
    }

    get isUser() {
        return this._type === ExtensionType.PER_USER;
    }
}

export const ExtensionManager = GObject.registerClass({
    Properties: {
        'user-extensions-enabled': GObject.ParamSpec.boolean(
            'user-extensions-enabled', null, null,
            GObject.ParamFlags.READWRITE,
            true),
        'n-updates': GObject.ParamSpec.int(
            'n-updates', null, null,
            GObject.ParamFlags.READABLE,
            0, 999, 0),
        'failed': GObject.ParamSpec.boolean(
            'failed', null, null,
            GObject.ParamFlags.READABLE,
            false),
    },
    Signals: {
        'extension-added': {param_types: [GObject.TYPE_JSOBJECT]},
        'extension-removed': {param_types: [GObject.TYPE_JSOBJECT]},
        'extension-changed': {param_types: [GObject.TYPE_JSOBJECT], flags: GObject.SignalFlags.DETAILED},
        'extensions-loaded': {},
    },
}, class ExtensionManager extends GObject.Object {
    constructor() {
        super();

        this._extensions = new Map();

        this._proxyReady = false;
        this._shellProxy = new GnomeShellProxy(Gio.DBus.session,
            'org.gnome.Shell.Extensions', '/org/gnome/Shell/Extensions',
            () => {
                this._proxyReady = true;
                shellVersion = this._shellProxy.ShellVersion;

                this._shellProxy.connect('notify::g-name-owner',
                    () => this.notify('failed'));
                this.notify('failed');
            });

        this._shellProxy.connect('g-properties-changed', (proxy, properties) => {
            const enabledChanged = !!properties.lookup_value('UserExtensionsEnabled', null);
            if (enabledChanged)
                this.notify('user-extensions-enabled');
        });
        this._shellProxy.connectSignal(
            'ExtensionStateChanged', this._onExtensionStateChanged.bind(this));

        this._loadExtensions().catch(console.error);
    }

    get userExtensionsEnabled() {
        return this._shellProxy.UserExtensionsEnabled ?? false;
    }

    set userExtensionsEnabled(enabled) {
        this._shellProxy.UserExtensionsEnabled = enabled;
    }

    get nUpdates() {
        let nUpdates = 0;
        for (const ext of this._extensions.values()) {
            if (ext.isUser && ext.hasUpdate)
                nUpdates++;
        }
        return nUpdates;
    }

    get failed() {
        return this._proxyReady && this._shellProxy.gNameOwner === null;
    }

    enableExtension(uuid) {
        this._shellProxy.EnableExtensionAsync(uuid).catch(console.error);
    }

    disableExtension(uuid) {
        this._shellProxy.DisableExtensionAsync(uuid).catch(console.error);
    }

    uninstallExtension(uuid) {
        this._shellProxy.UninstallExtensionAsync(uuid).catch(console.error);
    }

    openExtensionPrefs(uuid, parentHandle) {
        this._shellProxy.OpenExtensionPrefsAsync(uuid,
            parentHandle,
            {modal: new GLib.Variant('b', true)}).catch(console.error);
    }

    checkForUpdates() {
        this._shellProxy.CheckForUpdatesAsync().catch(console.error);
    }

    _addExtension(extension) {
        const {uuid} = extension;
        if (this._extensions.has(uuid))
            return;

        this._extensions.set(uuid, extension);
        this.emit('extension-added', extension);
    }

    _removeExtension(extension) {
        const {uuid} = extension;
        if (this._extensions.delete(uuid))
            this.emit('extension-removed', extension);
    }

    async _loadExtensions() {
        const [extensionsMap] = await this._shellProxy.ListExtensionsAsync();

        for (let uuid in extensionsMap) {
            const extension = new Extension(extensionsMap[uuid]);
            this._addExtension(extension);
        }
        this.emit('extensions-loaded');
    }

    _onExtensionStateChanged(p, sender, [uuid, newState]) {
        const extension = this._extensions.get(uuid);

        if (extension)
            extension.update(newState);

        if (!extension)
            this._addExtension(new Extension(newState));
        else if (extension.state === ExtensionState.UNINSTALLED)
            this._removeExtension(extension);
        else
            this.emit(`extension-changed::${uuid}`, extension);

        if (this._updatesCheckId)
            return;

        this._updatesCheckId = GLib.timeout_add_seconds(
            GLib.PRIORITY_DEFAULT, 1, () => {
                this.notify('n-updates');

                delete this._updatesCheckId;
                return GLib.SOURCE_REMOVE;
            });
    }
});
