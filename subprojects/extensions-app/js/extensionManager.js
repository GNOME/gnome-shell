import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';

import {
    ExtensionState, ExtensionType, deserializeExtension,
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
    } catch {
        console.error(`Failed to load D-Bus interface ${iface}`);
    }

    return null;
}

const Extension = GObject.registerClass({
    GTypeName: 'Extension',
    Properties: {
        'uuid': GObject.ParamSpec.string(
            'uuid', null, null,
            GObject.ParamFlags.READABLE,
            ''),
        'name': GObject.ParamSpec.string(
            'name', null, null,
            GObject.ParamFlags.READABLE,
            ''),
        'description': GObject.ParamSpec.string(
            'description', null, null,
            GObject.ParamFlags.READABLE,
            ''),
        'state': GObject.ParamSpec.int(
            'state', null, null,
            GObject.ParamFlags.READABLE,
            1, 99, ExtensionState.INITIALIZED),
        'enabled': GObject.ParamSpec.boolean(
            'enabled', null, null,
            GObject.ParamFlags.READABLE,
            false),
        'creator': GObject.ParamSpec.string(
            'creator', null, null,
            GObject.ParamFlags.READABLE,
            ''),
        'url': GObject.ParamSpec.string(
            'url', null, null,
            GObject.ParamFlags.READABLE,
            ''),
        'version': GObject.ParamSpec.string(
            'version', null, null,
            GObject.ParamFlags.READABLE,
            ''),
        'error': GObject.ParamSpec.string(
            'error', null, null,
            GObject.ParamFlags.READABLE,
            ''),
        'has-error': GObject.ParamSpec.boolean(
            'has-error', null, null,
            GObject.ParamFlags.READABLE,
            false),
        'has-prefs': GObject.ParamSpec.boolean(
            'has-prefs', null, null,
            GObject.ParamFlags.READABLE,
            false),
        'has-update': GObject.ParamSpec.boolean(
            'has-update', null, null,
            GObject.ParamFlags.READABLE,
            false),
        'has-version': GObject.ParamSpec.boolean(
            'has-version', null, null,
            GObject.ParamFlags.READABLE,
            false),
        'can-change': GObject.ParamSpec.boolean(
            'can-change', null, null,
            GObject.ParamFlags.READABLE,
            false),
        'is-user': GObject.ParamSpec.boolean(
            'is-user', null, null,
            GObject.ParamFlags.READABLE,
            false),
    },
}, class Extension extends GObject.Object {
    constructor(variant) {
        super();
        this.update(variant);
    }

    update(variant) {
        const deserialized = deserializeExtension(variant);

        const {
            uuid, type, state, enabled, error, hasPrefs, hasUpdate, canChange, metadata,
        } = deserialized;

        if (!this._uuid)
            this._uuid = uuid;

        if (this._uuid !== uuid)
            throw new Error(`Invalid update of extension ${this._uuid} with data from ${uuid}`);

        this.freeze_notify();

        const {name} = metadata;
        if (this._name !== name) {
            this._name = name;
            this.notify('name');
        }

        const [desc] = metadata.description.split('\n');
        if (this._description !== desc) {
            this._description = desc;
            this.notify('description');
        }

        if (this._type !== type) {
            this._type = type;
            this.notify('is-user');
        }

        if (this._errorDetail !== error) {
            this._errorDetail = error;
            this.notify('error');
        }

        if (this._enabled !== enabled) {
            this._enabled = enabled;
            this.notify('enabled');
        }

        if (this._state !== state) {
            const hadError = this.hasError;
            this._state = state;
            this.notify('state');

            // Compat with older shell versions
            if (this._enabled === undefined)
                this.notify('enabled');

            if (this.hasError !== hadError) {
                this.notify('has-error');
                this.notify('error');
            }
        }

        const creator = metadata.creator ?? '';
        if (this._creator !== creator) {
            this._creator = creator;
            this.notify('creator');
        }

        const url = metadata.url ?? '';
        if (this._url !== url) {
            this._url = url;
            this.notify('url');
        }

        const version = String(
            metadata['version-name'] || metadata['version'] || '');
        if (this._version !== version) {
            this._version = version;
            this.notify('version');
            this.notify('has-version');
        }

        if (this._hasPrefs !== hasPrefs) {
            this._hasPrefs = hasPrefs;
            this.notify('has-prefs');
        }

        if (this._hasUpdate !== hasUpdate) {
            this._hasUpdate = hasUpdate;
            this.notify('has-update');
        }

        if (this._canChange !== canChange) {
            this._canChange = canChange;
            this.notify('can-change');
        }

        this.thaw_notify();
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

    get enabled() {
        // Compat with older shell versions
        if (this._enabled === undefined) {
            return this.state === ExtensionState.ACTIVE ||
                   this.state === ExtensionState.ACTIVATING;
        }

        return this._enabled;
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
});
const {$gtype: TYPE_EXTENSION} = Extension;
export {TYPE_EXTENSION as Extension};

export const ExtensionManager = GObject.registerClass({
    Properties: {
        'user-extensions-enabled': GObject.ParamSpec.boolean(
            'user-extensions-enabled', null, null,
            GObject.ParamFlags.READWRITE,
            true),
        'extensions': GObject.ParamSpec.object(
            'extensions', null, null,
            GObject.ParamFlags.READABLE,
            Gio.ListModel),
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
        'extensions-loaded': {},
    },
}, class ExtensionManager extends GObject.Object {
    constructor() {
        super();

        this._extensions = new Gio.ListStore({itemType: Extension});

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

    get extensions() {
        return this._extensions;
    }

    get userExtensionsEnabled() {
        return this._shellProxy.UserExtensionsEnabled ?? false;
    }

    set userExtensionsEnabled(enabled) {
        this._shellProxy.UserExtensionsEnabled = enabled;
    }

    get nUpdates() {
        let nUpdates = 0;
        for (const ext of this._extensions) {
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

    async _loadExtensions() {
        const [extensionsMap] = await this._shellProxy.ListExtensionsAsync();

        for (let uuid in extensionsMap) {
            const extension = new Extension(extensionsMap[uuid]);
            this._extensions.append(extension);
        }
        this.emit('extensions-loaded');
    }

    _findExtension(uuid) {
        const len = this._extensions.get_n_items();
        for (let pos = 0; pos < len; pos++) {
            const extension = this._extensions.get_item(pos);
            if (extension.uuid === uuid)
                return [extension, pos];
        }

        return [null, -1];
    }

    _onExtensionStateChanged(p, sender, [uuid, newState]) {
        const [extension, pos] = this._findExtension(uuid);

        if (extension)
            extension.update(newState);

        if (!extension)
            this._extensions.append(new Extension(newState));
        else if (extension.state === ExtensionState.UNINSTALLED)
            this._extensions.remove(pos);

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
