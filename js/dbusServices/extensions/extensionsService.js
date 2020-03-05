// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ExtensionsService */

const { Gdk, Gio, GLib, GObject, Gtk, Shew } = imports.gi;

const ExtensionUtils = imports.misc.extensionUtils;

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
        const [uuid, parentWindow, options] = params;

        this._proxy.GetExtensionInfoRemote(uuid, (res, error) => {
            if (this._handleError(invocation, error))
                return;

            const [serialized] = res;
            const extension = ExtensionUtils.deserializeExtension(serialized);

            const window = new ExtensionPrefsDialog(extension);
            window.realize();

            let externalWindow = null;

            if (parentWindow)
                externalWindow = Shew.ExternalWindow.new_from_handle(parentWindow);

            if (externalWindow)
                externalWindow.set_parent_of(window.window);

            if (options.modal)
                window.modal = options.modal.get_boolean();

            window.connect('destroy', () => this.release());
            this.hold();

            window.show();

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

var ExtensionPrefsDialog = GObject.registerClass({
    GTypeName: 'ExtensionPrefsDialog',
    Template: 'resource:///org/gnome/Shell/Extensions/ui/extension-prefs-dialog.ui',
    InternalChildren: [
        'headerBar',
        'stack',
        'expander',
        'expanderArrow',
        'revealer',
        'errorView',
    ],
}, class ExtensionPrefsDialog extends Gtk.Window {
    _init(extension) {
        super._init();

        this._uuid = extension.uuid;
        this._url = extension.metadata.url || '';

        this._headerBar.title = extension.metadata.name;

        this._actionGroup = new Gio.SimpleActionGroup();
        this.insert_action_group('win', this._actionGroup);

        this._initActions();
        this._addCustomStylesheet();

        this._gesture = new Gtk.GestureMultiPress({
            widget: this._expander,
            button: 0,
            exclusive: true,
        });

        this._gesture.connect('released', (gesture, nPress) => {
            if (nPress === 1)
                this._revealer.reveal_child = !this._revealer.reveal_child;
        });

        this._revealer.connect('notify::reveal-child', () => {
            this._expanderArrow.icon_name = this._revealer.reveal_child
                ? 'pan-down-symbolic'
                : 'pan-end-symbolic';
        });

        try {
            ExtensionUtils.installImporter(extension);

            // give extension prefs access to their own extension object
            ExtensionUtils.getCurrentExtension = () => extension;

            const prefsModule = extension.imports.prefs;
            prefsModule.init(extension.metadata);

            const widget = prefsModule.buildPrefsWidget();
            this._stack.add(widget);
            this._stack.visible_child = widget;
        } catch (e) {
            this._setError(e);
        }
    }

    _setError(exc) {
        this._errorView.buffer.text = `${exc}\n\nStack trace:\n`;
        // Indent stack trace.
        this._errorView.buffer.text +=
            exc.stack.split('\n').map(line => `  ${line}`).join('\n');

        // markdown for pasting in gitlab issues
        let lines = [
            `The settings of extension ${this._uuid} had an error:`,
            '```',
            `${exc}`,
            '```',
            '',
            'Stack trace:',
            '```',
            exc.stack.replace(/\n$/, ''), // stack without trailing newline
            '```',
            '',
        ];
        this._errorMarkdown = lines.join('\n');
        this._actionGroup.lookup('copy-error').enabled = true;
    }

    _initActions() {
        let action;

        action = new Gio.SimpleAction({
            name: 'copy-error',
            enabled: false,
        });
        action.connect('activate', () => {
            const clipboard = Gtk.Clipboard.get_default(this.get_display());
            clipboard.set_text(this._errorMarkdown, -1);
        });
        this._actionGroup.add_action(action);

        action = new Gio.SimpleAction({
            name: 'show-url',
            enabled: this._url !== '',
        });
        action.connect('activate', () => {
            Gio.AppInfo.launch_default_for_uri(this._url,
                this.get_display().get_app_launch_context());
        });
        this._actionGroup.add_action(action);
    }

    _addCustomStylesheet() {
        let provider = new Gtk.CssProvider();
        let uri = 'resource:///org/gnome/Shell/Extensions/css/application.css';
        try {
            provider.load_from_file(Gio.File.new_for_uri(uri));
        } catch (e) {
            logError(e, 'Failed to add application style');
        }
        Gtk.StyleContext.add_provider_for_screen(Gdk.Screen.get_default(),
            provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
});
