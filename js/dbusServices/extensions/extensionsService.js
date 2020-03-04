// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ExtensionsService */

const { Gio, GLib } = imports.gi;

const { loadInterfaceXML } = imports.misc.fileUtils;
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

    ListExtensionsAsync(params, invocation) {
        this._proxy.ListExtensionsRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(new GLib.Variant('(a{sa{sv}})', res));
        });
    }

    GetExtensionInfoAsync(params, invocation) {
        this._proxy.GetExtensionInfoRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(new GLib.Variant('(a{sv})', res));
        });
    }

    GetExtensionErrorsAsync(params, invocation) {
        this._proxy.GetExtensionErrorsRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(new GLib.Variant('(as)', res));
        });
    }

    InstallRemoteExtensionAsync(params, invocation) {
        this._proxy.InstallRemoteExtensionRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(new GLib.Variant('(s)', res));
        });
    }

    UninstallExtensionAsync(params, invocation) {
        this._proxy.UninstallExtensionRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(new GLib.Variant('(b)', res));
        });
    }

    EnableExtensionAsync(params, invocation) {
        this._proxy.EnableExtensionRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(new GLib.Variant('(b)', res));
        });
    }

    DisableExtensionAsync(params, invocation) {
        this._proxy.DisableExtensionRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(new GLib.Variant('(b)', res));
        });
    }

    LaunchExtensionPrefsAsync([uuid], invocation) {
        this.OpenExtensionPrefsAsync([uuid, '', {}], invocation);
    }

    OpenExtensionPrefsAsync(params, invocation) {
        this._proxy.OpenExtensionPrefsRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(null);
        });
    }

    CheckForUpdatesAsync(params, invocation) {
        this._proxy.CheckForUpdatesRemote(...params, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            invocation.return_value(null);
        });
    }
};
