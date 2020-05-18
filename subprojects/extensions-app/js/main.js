/* exported main */
imports.gi.versions.Gdk = '3.0';
imports.gi.versions.Gtk = '3.0';

const Gettext = imports.gettext;
const Package = imports.package;
const { Gdk, GLib, Gio, GObject, Gtk, Shew } = imports.gi;

Package.initFormat();

const ExtensionUtils = imports.misc.extensionUtils;

const { ExtensionState, ExtensionType } = ExtensionUtils;

const GnomeShellIface = loadInterfaceXML('org.gnome.Shell.Extensions');
const GnomeShellProxy = Gio.DBusProxy.makeProxyWrapper(GnomeShellIface);

Gio._promisify(Shew.WindowExporter.prototype, 'export', 'export_finish');

function loadInterfaceXML(iface) {
    const uri = 'resource:///org/gnome/Extensions/dbus-interfaces/%s.xml'.format(iface);
    const f = Gio.File.new_for_uri(uri);

    try {
        let [ok_, bytes] = f.load_contents(null);
        return imports.byteArray.toString(bytes);
    } catch (e) {
        log('Failed to load D-Bus interface %s'.format(iface));
    }

    return null;
}

function toggleState(action) {
    let state = action.get_state();
    action.change_state(new GLib.Variant('b', !state.get_boolean()));
}

var Application = GObject.registerClass(
class Application extends Gtk.Application {
    _init() {
        GLib.set_prgname('gnome-extensions-app');
        super._init({ application_id: 'org.gnome.Extensions' });
    }

    get shellProxy() {
        return this._shellProxy;
    }

    vfunc_activate() {
        this._shellProxy.CheckForUpdatesRemote();
        this._window.present();
    }

    vfunc_startup() {
        super.vfunc_startup();

        let provider = new Gtk.CssProvider();
        let uri = 'resource:///org/gnome/Extensions/css/application.css';
        try {
            provider.load_from_file(Gio.File.new_for_uri(uri));
        } catch (e) {
            logError(e, 'Failed to add application style');
        }
        Gtk.StyleContext.add_provider_for_screen(Gdk.Screen.get_default(),
            provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION);

        this._shellProxy = new GnomeShellProxy(Gio.DBus.session,
            'org.gnome.Shell.Extensions', '/org/gnome/Shell/Extensions');

        this._window = new ExtensionsWindow({ application: this });
    }
});

var ExtensionsWindow = GObject.registerClass({
    GTypeName: 'ExtensionsWindow',
    Template: 'resource:///org/gnome/Extensions/ui/extensions-window.ui',
    InternalChildren: [
        'userList',
        'systemList',
        'mainBox',
        'mainStack',
        'scrolledWindow',
        'updatesBar',
        'updatesLabel',
    ],
}, class ExtensionsWindow extends Gtk.ApplicationWindow {
    _init(params) {
        super._init(params);

        this._updatesCheckId = 0;

        this._exporter = new Shew.WindowExporter({ window: this });
        this._exportedHandle = '';

        this._mainBox.set_focus_vadjustment(this._scrolledWindow.vadjustment);

        let action;
        action = new Gio.SimpleAction({ name: 'show-about' });
        action.connect('activate', this._showAbout.bind(this));
        this.add_action(action);

        action = new Gio.SimpleAction({ name: 'logout' });
        action.connect('activate', this._logout.bind(this));
        this.add_action(action);

        action = new Gio.SimpleAction({
            name: 'user-extensions-enabled',
            state: new GLib.Variant('b', false),
        });
        action.connect('activate', toggleState);
        action.connect('change-state', (a, state) => {
            this._shellProxy.UserExtensionsEnabled = state.get_boolean();
        });
        this.add_action(action);

        this._userList.set_sort_func(this._sortList.bind(this));
        this._userList.set_header_func(this._updateHeader.bind(this));

        this._systemList.set_sort_func(this._sortList.bind(this));
        this._systemList.set_header_func(this._updateHeader.bind(this));

        this._shellProxy.connectSignal('ExtensionStateChanged',
            this._onExtensionStateChanged.bind(this));

        this._shellProxy.connect('g-properties-changed',
            this._onUserExtensionsEnabledChanged.bind(this));
        this._onUserExtensionsEnabledChanged();

        this._scanExtensions();
    }

    get _shellProxy() {
        return this.application.shellProxy;
    }

    uninstall(uuid) {
        let row = this._findExtensionRow(uuid);

        let dialog = new Gtk.MessageDialog({
            transient_for: this,
            modal: true,
            text: _('Remove “%s”?').format(row.name),
            secondary_text: _('If you remove the extension, you need to return to download it if you want to enable it again'),
        });

        dialog.add_button(_('Cancel'), Gtk.ResponseType.CANCEL);
        dialog.add_button(_('Remove'), Gtk.ResponseType.ACCEPT)
            .get_style_context().add_class('destructive-action');

        dialog.connect('response', (dlg, response) => {
            if (response === Gtk.ResponseType.ACCEPT)
                this._shellProxy.UninstallExtensionRemote(uuid);
            dialog.destroy();
        });
        dialog.present();
    }

    async openPrefs(uuid) {
        if (!this._exportedHandle) {
            try {
                this._exportedHandle = await this._exporter.export();
            } catch (e) {
                log('Failed to export window: %s'.format(e.message));
            }
        }

        this._shellProxy.OpenExtensionPrefsRemote(uuid,
            this._exportedHandle,
            { modal: new GLib.Variant('b', true) });
    }

    _showAbout() {
        let aboutDialog = new Gtk.AboutDialog({
            authors: [
                'Florian Müllner <fmuellner@gnome.org>',
                'Jasper St. Pierre <jstpierre@mecheye.net>',
                'Didier Roche <didrocks@ubuntu.com>',
            ],
            translator_credits: _('translator-credits'),
            program_name: _('Extensions'),
            comments: _('Manage your GNOME Extensions'),
            license_type: Gtk.License.GPL_2_0,
            logo_icon_name: 'org.gnome.Extensions',
            version: imports.package.version,

            transient_for: this,
            modal: true,
        });
        aboutDialog.present();
    }

    _logout() {
        this.application.get_dbus_connection().call(
            'org.gnome.SessionManager',
            '/org/gnome/SessionManager',
            'org.gnome.SessionManager',
            'Logout',
            new GLib.Variant('(u)', [0]),
            null,
            Gio.DBusCallFlags.NONE,
            -1,
            null,
            (o, res) => {
                o.call_finish(res);
            });
    }

    _sortList(row1, row2) {
        return row1.name.localeCompare(row2.name);
    }

    _updateHeader(row, before) {
        if (!before || row.get_header())
            return;

        let sep = new Gtk.Separator({ orientation: Gtk.Orientation.HORIZONTAL });
        row.set_header(sep);
    }

    _findExtensionRow(uuid) {
        return [
            ...this._userList.get_children(),
            ...this._systemList.get_children(),
        ].find(c => c.uuid === uuid);
    }

    _onUserExtensionsEnabledChanged() {
        let action = this.lookup_action('user-extensions-enabled');
        action.set_state(
            new GLib.Variant('b', this._shellProxy.UserExtensionsEnabled));
    }

    _onExtensionStateChanged(proxy, senderName, [uuid, newState]) {
        let extension = ExtensionUtils.deserializeExtension(newState);
        let row = this._findExtensionRow(uuid);

        this._queueUpdatesCheck();

        // the extension's type changed; remove the corresponding row
        // and reset the variable to null so that we create a new row
        // below and add it to the appropriate list
        if (row && row.type !== extension.type) {
            row.destroy();
            row = null;
        }

        if (row) {
            if (extension.state === ExtensionState.UNINSTALLED)
                row.destroy();
        } else {
            this._addExtensionRow(extension);
        }

        this._syncListVisibility();
    }

    _scanExtensions() {
        this._shellProxy.ListExtensionsRemote(([extensionsMap], e) => {
            if (e) {
                if (e instanceof Gio.DBusError) {
                    log('Failed to connect to shell proxy: %s'.format(e.toString()));
                    this._mainStack.visible_child_name = 'noshell';
                } else {
                    throw e;
                }
                return;
            }

            for (let uuid in extensionsMap) {
                let extension = ExtensionUtils.deserializeExtension(extensionsMap[uuid]);
                this._addExtensionRow(extension);
            }
            this._extensionsLoaded();
        });
    }

    _addExtensionRow(extension) {
        let row = new ExtensionRow(extension);
        row.show_all();

        if (row.type === ExtensionType.PER_USER)
            this._userList.add(row);
        else
            this._systemList.add(row);
    }

    _queueUpdatesCheck() {
        if (this._updatesCheckId)
            return;

        this._updatesCheckId = GLib.timeout_add_seconds(
            GLib.PRIORITY_DEFAULT, 1, () => {
                this._checkUpdates();

                this._updatesCheckId = 0;
                return GLib.SOURCE_REMOVE;
            });
    }

    _syncListVisibility() {
        this._userList.visible = this._userList.get_children().length > 0;
        this._systemList.visible = this._systemList.get_children().length > 0;

        if (this._userList.visible || this._systemList.visible)
            this._mainStack.visible_child_name = 'main';
        else
            this._mainStack.visible_child_name = 'placeholder';
    }

    _checkUpdates() {
        let nUpdates = this._userList.get_children().filter(c => c.hasUpdate).length;

        this._updatesLabel.label = Gettext.ngettext(
            '%d extension will be updated on next login.',
            '%d extensions will be updated on next login.',
            nUpdates).format(nUpdates);
        this._updatesBar.visible = nUpdates > 0;
    }

    _extensionsLoaded() {
        this._syncListVisibility();
        this._checkUpdates();
    }
});

var ExtensionRow = GObject.registerClass({
    GTypeName: 'ExtensionRow',
    Template: 'resource:///org/gnome/Extensions/ui/extension-row.ui',
    InternalChildren: [
        'nameLabel',
        'descriptionLabel',
        'versionLabel',
        'authorLabel',
        'updatesIcon',
        'switch',
        'revealButton',
        'revealer',
    ],
}, class ExtensionRow extends Gtk.ListBoxRow {
    _init(extension) {
        super._init();

        this._app = Gio.Application.get_default();
        this._extension = extension;
        this._prefsModule = null;

        this._actionGroup = new Gio.SimpleActionGroup();
        this.insert_action_group('row', this._actionGroup);

        let action;
        action = new Gio.SimpleAction({
            name: 'show-prefs',
            enabled: this.hasPrefs,
        });
        action.connect('activate', () => this.get_toplevel().openPrefs(this.uuid));
        this._actionGroup.add_action(action);

        action = new Gio.SimpleAction({
            name: 'show-url',
            enabled: this.url !== '',
        });
        action.connect('activate', () => {
            Gio.AppInfo.launch_default_for_uri(
                this.url, this.get_display().get_app_launch_context());
        });
        this._actionGroup.add_action(action);

        action = new Gio.SimpleAction({
            name: 'uninstall',
            enabled: this.type === ExtensionType.PER_USER,
        });
        action.connect('activate', () => this.get_toplevel().uninstall(this.uuid));
        this._actionGroup.add_action(action);

        action = new Gio.SimpleAction({
            name: 'enabled',
            state: new GLib.Variant('b', false),
        });
        action.connect('activate', toggleState);
        action.connect('change-state', (a, state) => {
            if (state.get_boolean())
                this._app.shellProxy.EnableExtensionRemote(this.uuid);
            else
                this._app.shellProxy.DisableExtensionRemote(this.uuid);
        });
        this._actionGroup.add_action(action);

        this._nameLabel.label = this.name;

        let desc = this._extension.metadata.description.split('\n')[0];
        this._descriptionLabel.label = desc;

        this._revealButton.connect('clicked', () => {
            this._revealer.reveal_child = !this._revealer.reveal_child;
        });
        this._revealer.connect('notify::reveal-child', () => {
            if (this._revealer.reveal_child)
                this._revealButton.get_style_context().add_class('expanded');
            else
                this._revealButton.get_style_context().remove_class('expanded');
        });

        this.connect('destroy', this._onDestroy.bind(this));

        this._extensionStateChangedId = this._app.shellProxy.connectSignal(
            'ExtensionStateChanged', (p, sender, [uuid, newState]) => {
                if (this.uuid !== uuid)
                    return;

                this._extension = ExtensionUtils.deserializeExtension(newState);
                this._updateState();
            });
        this._updateState();
    }

    get uuid() {
        return this._extension.uuid;
    }

    get name() {
        return this._extension.metadata.name;
    }

    get hasPrefs() {
        return this._extension.hasPrefs;
    }

    get hasUpdate() {
        return this._extension.hasUpdate || false;
    }

    get type() {
        return this._extension.type;
    }

    get creator() {
        return this._extension.metadata.creator || '';
    }

    get url() {
        return this._extension.metadata.url || '';
    }

    get version() {
        return this._extension.metadata.version || '';
    }

    _updateState() {
        let state = this._extension.state === ExtensionState.ENABLED;

        let action = this._actionGroup.lookup('enabled');
        action.set_state(new GLib.Variant('b', state));
        action.enabled = this._canToggle();

        if (!action.enabled)
            this._switch.active = state;

        this._updatesIcon.visible = this.hasUpdate;

        this._versionLabel.label = this.version.toString();
        this._versionLabel.visible = this.version !== '';

        this._authorLabel.label = this.creator.toString();
        this._authorLabel.visible = this.creator !== '';
    }

    _onDestroy() {
        if (!this._app.shellProxy)
            return;

        if (this._extensionStateChangedId)
            this._app.shellProxy.disconnectSignal(this._extensionStateChangedId);
        this._extensionStateChangedId = 0;
    }

    _canToggle() {
        return this._extension.canChange;
    }
});

function initEnvironment() {
    // Monkey-patch in a "global" object that fakes some Shell utilities
    // that ExtensionUtils depends on.
    window.global = {
        log(...args) {
            print(args.join(', '));
        },

        logError(s) {
            log('ERROR: %s'.format(s));
        },

        userdatadir: GLib.build_filenamev([GLib.get_user_data_dir(), 'gnome-shell']),
    };
}

function main(argv) {
    initEnvironment();
    Package.initGettext();

    new Application().run(argv);
}
