/* exported main */
imports.gi.versions.Gdk = '3.0';
imports.gi.versions.Gtk = '3.0';

const Gettext = imports.gettext;
const { Gdk, GLib, Gio, GObject, Gtk } = imports.gi;
const Format = imports.format;

const _ = Gettext.gettext;

const Config = imports.misc.config;
const ExtensionUtils = imports.misc.extensionUtils;

const { ExtensionState, ExtensionType } = ExtensionUtils;

const GnomeShellIface = loadInterfaceXML('org.gnome.Shell.Extensions');
const GnomeShellProxy = Gio.DBusProxy.makeProxyWrapper(GnomeShellIface);

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

function stripPrefix(string, prefix) {
    if (string.slice(0, prefix.length) == prefix)
        return string.slice(prefix.length);
    return string;
}

function toggleState(action) {
    let state = action.get_state();
    action.change_state(new GLib.Variant('b', !state.get_boolean()));
}

var Application = GObject.registerClass(
class Application extends Gtk.Application {
    _init() {
        GLib.set_prgname('gnome-shell-extension-prefs');
        super._init({
            application_id: 'org.gnome.Extensions',
            flags: Gio.ApplicationFlags.HANDLES_COMMAND_LINE,
        });
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

        this._shellProxy = new GnomeShellProxy(Gio.DBus.session, 'org.gnome.Shell', '/org/gnome/Shell');
        this._window = new ExtensionsWindow({ application: this });
    }

    vfunc_command_line(commandLine) {
        let [, uuid] = commandLine.get_arguments();

        if (uuid) {
            // Strip off "extension:///" prefix which fakes a URI, if it exists
            uuid = stripPrefix(uuid, 'extension:///');

            this._window.openPrefs(uuid);
        } else {
            this.activate();
        }
        return 0;
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

        this._startupUuid = null;
        this._loaded = false;
        this._prefsDialog = null;
        this._updatesCheckId = 0;

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

    openPrefs(uuid) {
        if (!this._loaded)
            this._startupUuid = uuid;
        else if (!this._showPrefs(uuid))
            this.present();
    }

    _showPrefs(uuid) {
        if (this._prefsDialog)
            return false;

        let row = this._findExtensionRow(uuid);
        if (!row || !row.hasPrefs)
            return false;

        let widget;

        try {
            widget = row.prefsModule.buildPrefsWidget();
        } catch (e) {
            widget = this._buildErrorUI(row, e);
        }

        this._prefsDialog = new Gtk.Window({
            application: this.application,
            default_width: 600,
            default_height: 400,
            modal: this.visible,
            type_hint: Gdk.WindowTypeHint.DIALOG,
            window_position: Gtk.WindowPosition.CENTER,
        });

        this._prefsDialog.set_titlebar(new Gtk.HeaderBar({
            show_close_button: true,
            title: row.name,
            visible: true,
        }));

        if (this.visible)
            this._prefsDialog.transient_for = this;

        this._prefsDialog.connect('destroy', () => {
            this._prefsDialog = null;

            if (!this.visible)
                this.destroy();
        });

        this._prefsDialog.add(widget);
        this._prefsDialog.show();

        return true;
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
            version: Config.PACKAGE_VERSION,

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

    _buildErrorUI(row, exc) {
        let scroll = new Gtk.ScrolledWindow({
            hscrollbar_policy: Gtk.PolicyType.NEVER,
            propagate_natural_height: true,
        });

        let box = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
            spacing: 12,
            margin: 100,
            margin_bottom: 60,
        });
        scroll.add(box);

        let label = new Gtk.Label({
            label: '<span size="x-large">%s</span>'.format(_("Something’s gone wrong")),
            use_markup: true,
        });
        label.get_style_context().add_class(Gtk.STYLE_CLASS_DIM_LABEL);
        box.add(label);

        label = new Gtk.Label({
            label: _("We’re very sorry, but there’s been a problem: the settings for this extension can’t be displayed. We recommend that you report the issue to the extension authors."),
            justify: Gtk.Justification.CENTER,
            wrap: true,
        });
        box.add(label);

        let expander = new Expander({
            label: _("Technical Details"),
            margin_top: 12,
        });
        box.add(expander);

        let errortext = '%s\n\nStack trace:\n'.format(exc);
        // Indent stack trace.
        errortext +=
            exc.stack.split('\n').map(line => '  %s'.format(line)).join('\n');

        let buffer = new Gtk.TextBuffer({ text: errortext });
        let textview = new Gtk.TextView({
            buffer,
            wrap_mode: Gtk.WrapMode.WORD,
            monospace: true,
            editable: false,
            top_margin: 12,
            bottom_margin: 12,
            left_margin: 12,
            right_margin: 12,
        });

        let toolbar = new Gtk.Toolbar();
        let provider = new Gtk.CssProvider();
        provider.load_from_data(`* {
            border: 0 solid @borders;
            border-top-width: 1px;
        }`);
        toolbar.get_style_context().add_provider(
            provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        );

        let copyButton = new Gtk.ToolButton({
            icon_name: 'edit-copy-symbolic',
            tooltip_text: _("Copy Error"),
        });
        toolbar.add(copyButton);

        copyButton.connect('clicked', w => {
            let clipboard = Gtk.Clipboard.get_default(w.get_display());
            // markdown for pasting in gitlab issues
            let lines = [
                'The settings of extension %s had an error:'.format(row.uuid),
                '```', // '`' (xgettext throws up on odd number of backticks)
                exc.toString(),
                '```', // '`'
                '',
                'Stack trace:',
                '```', // '`'
                exc.stack.replace(/\n$/, ''), // stack without trailing newline
                '```', // '`'
                '',
            ];
            clipboard.set_text(lines.join('\n'), -1);
        });

        let spacing = new Gtk.SeparatorToolItem({ draw: false });
        toolbar.add(spacing);
        toolbar.child_set_property(spacing, "expand", true);

        let urlButton = new Gtk.ToolButton({
            label: _("Homepage"),
            tooltip_text: _("Visit extension homepage"),
            no_show_all: true,
            visible: row.url !== '',
        });
        toolbar.add(urlButton);

        urlButton.connect('clicked', w => {
            let context = w.get_display().get_app_launch_context();
            Gio.AppInfo.launch_default_for_uri(row.url, context);
        });

        let expandedBox = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
        });
        expandedBox.add(textview);
        expandedBox.add(toolbar);

        expander.add(expandedBox);

        scroll.show_all();
        return scroll;
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
            return; // we only deal with new and deleted extensions here
        }
        this._addExtensionRow(extension);
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

    _checkUpdates() {
        let nUpdates = this._userList.get_children().filter(c => c.hasUpdate).length;

        this._updatesLabel.label = Gettext.ngettext(
            '%d extension will be updated on next login.',
            '%d extensions will be updated on next login.',
            nUpdates).format(nUpdates);
        this._updatesBar.visible = nUpdates > 0;
    }

    _extensionsLoaded() {
        this._userList.visible = this._userList.get_children().length > 0;
        this._systemList.visible = this._systemList.get_children().length > 0;

        if (this._userList.visible || this._systemList.visible)
            this._mainStack.visible_child_name = 'main';
        else
            this._mainStack.visible_child_name = 'placeholder';

        this._checkUpdates();

        if (this._startupUuid)
            this._showPrefs(this._startupUuid);
        this._startupUuid = null;
        this._loaded = true;
    }
});

var Expander = GObject.registerClass({
    Properties: {
        'label': GObject.ParamSpec.string(
            'label', 'label', 'label',
            GObject.ParamFlags.READWRITE,
            null
        ),
    },
}, class Expander extends Gtk.Box {
    _init(params = {}) {
        this._labelText = null;

        super._init(Object.assign(params, {
            orientation: Gtk.Orientation.VERTICAL,
            spacing: 0,
        }));

        this._frame = new Gtk.Frame({
            shadow_type: Gtk.ShadowType.IN,
            hexpand: true,
        });

        let eventBox = new Gtk.EventBox();
        this._frame.add(eventBox);

        let hbox = new Gtk.Box({
            spacing: 6,
            margin: 12,
        });
        eventBox.add(hbox);

        this._arrow = new Gtk.Image({
            icon_name: 'pan-end-symbolic',
        });
        hbox.add(this._arrow);

        this._label = new Gtk.Label({ label: this._labelText });
        hbox.add(this._label);

        this._revealer = new Gtk.Revealer();

        this._childBin = new Gtk.Frame({
            shadow_type: Gtk.ShadowType.IN,
        });
        this._revealer.add(this._childBin);

        // Directly chain up to parent for internal children
        super.add(this._frame);
        super.add(this._revealer);

        let provider = new Gtk.CssProvider();
        provider.load_from_data('* { border-top-width: 0; }');
        this._childBin.get_style_context().add_provider(
            provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        );

        this._gesture = new Gtk.GestureMultiPress({
            widget: this._frame,
            button: 0,
            exclusive: true,
        });
        this._gesture.connect('released', (gesture, nPress) => {
            if (nPress == 1)
                this._revealer.reveal_child = !this._revealer.reveal_child;
        });
        this._revealer.connect('notify::reveal-child', () => {
            if (this._revealer.reveal_child)
                this._arrow.icon_name = 'pan-down-symbolic';
            else
                this._arrow.icon_name = 'pan-end-symbolic';
        });
    }

    get label() {
        return this._labelText;
    }

    set label(text) {
        if (this._labelText == text)
            return;

        if (this._label)
            this._label.label = text;
        this._labelText = text;
        this.notify('label');
    }

    add(child) {
        // set expanded child
        this._childBin.get_children().forEach(c => {
            this._childBin.remove(c);
        });

        if (child)
            this._childBin.add(child);
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

    get prefsModule() {
        // give extension prefs access to their own extension object
        ExtensionUtils.getCurrentExtension = () => this._extension;

        if (!this._prefsModule) {
            ExtensionUtils.installImporter(this._extension);

            this._prefsModule = this._extension.imports.prefs;
            this._prefsModule.init(this._extension.metadata);
        }

        return this._prefsModule;
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

    String.prototype.format = Format.format;
}

function main(argv) {
    initEnvironment();

    new Application().run(argv);
}
