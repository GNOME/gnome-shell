// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ExtensionsService */

const { Gio, GLib, Shew } = imports.gi;

const ExtensionUtils = imports.misc.extensionUtils;

const { loadInterfaceXML } = imports.misc.dbusUtils;
const { ExtensionPrefsDialog } = imports.extensionPrefsDialog;
const { ServiceImplementation } = imports.dbusService;

const ExtensionsIface = loadInterfaceXML('org.gnome.Shell.Extensions');
const ExtensionsProxy = Gio.DBusProxy.makeProxyWrapper(ExtensionsIface);

var ExtensionsService = class extends ServiceImplementation {
    constructor() {
        super(ExtensionsIface, '/org/gnome/Shell/Extensions');

        this._proxy = new ExtensionsProxy(Gio.DBus.session,
            'org.gnome.Shell', '/org/gnome/Shell');

        this._proxy.connectSignal('ExtensionStateChanged',
            (proxy, sender, params) => {
                this._dbusImpl.emit_signal('ExtensionStateChanged',
                    new GLib.Variant('(sa{sv})', params));
            });

        this._proxy.connect('g-properties-changed', () => {
            this._dbusImpl.emit_property_changed('UserExtensionsEnabled',
                new GLib.Variant('b', this._proxy.UserExtensionsEnabled));
        });
    }

    get ShellVersion() {
        return this._proxy.ShellVersion;
    }

    get UserExtensionsEnabled() {
        return this._proxy.UserExtensionsEnabled;
    }

    set UserExtensionsEnabled(enable) {
        this._proxy.UserExtensionsEnabled = enable;
    }

    async ListExtensionsAsync(params, invocation) {
        try {
            const res = await this._proxy.ListExtensionsAsync(...params);
            invocation.return_value(new GLib.Variant('(a{sa{sv}})', res));
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    async GetExtensionInfoAsync(params, invocation) {
        try {
            const res = await this._proxy.GetExtensionInfoAsync(...params);
            invocation.return_value(new GLib.Variant('(a{sv})', res));
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    async GetExtensionErrorsAsync(params, invocation) {
        try {
            const res = await this._proxy.GetExtensionErrorsAsync(...params);
            invocation.return_value(new GLib.Variant('(as)', res));
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    async InstallRemoteExtensionAsync(params, invocation) {
        try {
            const res = await this._proxy.InstallRemoteExtensionAsync(...params);
            invocation.return_value(new GLib.Variant('(s)', res));
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    async UninstallExtensionAsync(params, invocation) {
        try {
            const res = await this._proxy.UninstallExtensionAsync(...params);
            invocation.return_value(new GLib.Variant('(b)', res));
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    async EnableExtensionAsync(params, invocation) {
        try {
            const res = await this._proxy.EnableExtensionAsync(...params);
            invocation.return_value(new GLib.Variant('(b)', res));
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    async DisableExtensionAsync(params, invocation) {
        try {
            const res = await this._proxy.DisableExtensionAsync(...params);
            invocation.return_value(new GLib.Variant('(b)', res));
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    LaunchExtensionPrefsAsync([uuid], invocation) {
        this.OpenExtensionPrefsAsync([uuid, '', {}], invocation);
    }

    async OpenExtensionPrefsAsync(params, invocation) {
        const [uuid, parentWindow, options] = params;

        try {
            const [serialized] = await this._proxy.GetExtensionInfoAsync(uuid);

            if (this._prefsDialog)
                throw new Error('Already showing a prefs dialog');

            const extension = ExtensionUtils.deserializeExtension(serialized);

            this._prefsDialog = new ExtensionPrefsDialog(extension);
            this._prefsDialog.connect('realize', () => {
                let externalWindow = null;

                if (parentWindow)
                    externalWindow = Shew.ExternalWindow.new_from_handle(parentWindow);

                if (externalWindow)
                    externalWindow.set_parent_of(this._prefsDialog.get_surface());
            });

            if (options.modal)
                this._prefsDialog.modal = options.modal.get_boolean();

            this._prefsDialog.connect('close-request', () => {
                delete this._prefsDialog;
                this.release();
                return false;
            });
            this.hold();

            this._prefsDialog.show();

            invocation.return_value(null);
        } catch (error) {
            this._handleError(invocation, error);
        }
    }

    async CheckForUpdatesAsync(params, invocation) {
        try {
            await this._proxy.CheckForUpdatesAsync(...params);
            invocation.return_value(null);
        } catch (error) {
            this._handleError(invocation, error);
        }
    }
};
