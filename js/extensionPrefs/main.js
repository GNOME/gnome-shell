
const Lang = imports.lang;
const Gettext = imports.gettext;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Gdk = imports.gi.Gdk;
const Pango = imports.gi.Pango;
const Format = imports.format;

const _ = Gettext.gettext;

const Config = imports.misc.config;
const ExtensionUtils = imports.misc.extensionUtils;
const { loadInterfaceXML } = imports.misc.fileUtils;

const GnomeShellIface = loadInterfaceXML('org.gnome.Shell.Extensions');
const GnomeShellProxy = Gio.DBusProxy.makeProxyWrapper(GnomeShellIface);

function stripPrefix(string, prefix) {
    if (string.slice(0, prefix.length) == prefix)
        return string.slice(prefix.length);
    return string;
}

let app;

var Application = new Lang.Class({
    Name: 'Application',
    _init() {
        GLib.set_prgname('gnome-shell-extension-prefs');
        this.application = new Gtk.Application({
            application_id: 'org.gnome.shell.ExtensionPrefs',
            flags: Gio.ApplicationFlags.HANDLES_COMMAND_LINE
        });

        this.application.connect('activate', this._onActivate.bind(this));
        this.application.connect('command-line', this._onCommandLine.bind(this));
        this.application.connect('startup', this._onStartup.bind(this));

        this._startupUuid = null;
        this._loaded = false;
        this._skipMainWindow = false;
        this.shellProxy = null;
    },

    _showPrefs(uuid) {
        let row;
        for (let item of this._extensionSelector.get_children()) {
            if (item.uuid === uuid) {
                if (item.hasPrefs)
                    row = item;
                break;
            }
        }
        if (!row)
            return false;

        let widget;

        try {
            widget = row.prefsModule.buildPrefsWidget();
        } catch (e) {
            widget = this._buildErrorUI(row, e);
        }

        let dialog = new Gtk.Window({
            modal: !this._skipMainWindow,
            type_hint: Gdk.WindowTypeHint.DIALOG
        });
        dialog.set_titlebar(new Gtk.HeaderBar({
            show_close_button: true,
            title: row.name,
            visible: true
        }));

        if (this._skipMainWindow) {
            this.application.add_window(dialog);
            if (this._window)
                this._window.destroy();
            this._window = dialog;
            this._window.window_position = Gtk.WindowPosition.CENTER;
        } else {
            dialog.transient_for = this._window;
        }

        dialog.set_default_size(600, 400);
        dialog.add(widget);
        dialog.show();
        return true;
    },

    _buildErrorUI(row, exc) {
        let box = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL });
        let label = new Gtk.Label({
            label: _("There was an error loading the preferences dialog for %s:").format(row.name)
        });
        box.add(label);

        let errortext = '';
        errortext += exc;
        errortext += '\n\n';
        errortext += 'Stack trace:\n';

        // Indent stack trace.
        errortext += exc.stack.split('\n').map(line => '  ' + line).join('\n');

        let scroll = new Gtk.ScrolledWindow({ vexpand: true });
        let buffer = new Gtk.TextBuffer({ text: errortext });
        let textview = new Gtk.TextView({ buffer: buffer });
        textview.override_font(Pango.font_description_from_string('monospace'));
        scroll.add(textview);
        box.add(scroll);

        box.show_all();
        return box;
    },

    _buildUI(app) {
        this._window = new Gtk.ApplicationWindow({ application: app,
                                                   window_position: Gtk.WindowPosition.CENTER });

        this._window.set_default_size(800, 500);

        this._titlebar = new Gtk.HeaderBar({ show_close_button: true,
                                             title: _("Shell Extensions") });
        this._window.set_titlebar(this._titlebar);

        let killSwitch = new Gtk.Switch({ valign: Gtk.Align.CENTER });
        this._titlebar.pack_end(killSwitch);

        this._UISwitch = new Gtk.Notebook();
        this._UISwitch.set_show_tabs(false);
        this._window.add(this._UISwitch);

        this._settings = new Gio.Settings({ schema_id: 'org.gnome.shell' });
        this._settings.bind('disable-user-extensions', killSwitch, 'active',
                            Gio.SettingsBindFlags.DEFAULT |
                            Gio.SettingsBindFlags.INVERT_BOOLEAN);

        let scroll = new Gtk.ScrolledWindow({ hscrollbar_policy: Gtk.PolicyType.NEVER });
        this._UISwitch.append_page(scroll, null);

        this._extensionSelector = new Gtk.ListBox({ selection_mode: Gtk.SelectionMode.NONE });
        this._extensionSelector.set_sort_func(this._sortList.bind(this));
        this._extensionSelector.set_header_func(this._updateHeader.bind(this));

        scroll.add(this._extensionSelector);

        this._globalerror = new Gtk.Label({
            use_markup: true,
            wrap: true,
            halign: Gtk.Align.FILL
        });
        this._UISwitch.append_page(this._globalerror, null);

        this.shellProxy = new GnomeShellProxy(Gio.DBus.session, 'org.gnome.Shell', '/org/gnome/Shell');
        this.shellProxy.connectSignal('ExtensionStatusChanged', (proxy, senderName, [uuid, state, error]) => {
            // we only deal with new and deleted extensions here
            let row = null;
            for (let item of this._extensionSelector.get_children()) {
                if (item.uuid === uuid) {
                    row = item;
                    break;
                }
            }

            if (row && (state === ExtensionUtils.ExtensionState.UNINSTALLED)) {
                row.disconnectSignals();
                row.destroy();
                return;
            } else if (!row) {
                this.shellProxy.GetExtensionInfoRemote(uuid, ([extensionProxy]) => {
                    let extension = ExtensionUtils.deserializeExtension(extensionProxy);
                    if (!extension)
                        return;
                    // check the extension wasn't added in between (due to asyncness)
                    for (let item of this._extensionSelector.get_children()) {
                        if (item.uuid === extension.uuid)
                            return;
                    }
                    this._extensionFound(extension);
                });
            }
        });

        this._window.show_all();
    },

    _showErrorUI(text) {
        this._globalerror.set_markup(text);
        this._UISwitch.set_current_page(1);
    },

    _sortList(row1, row2) {
        return row1.name.localeCompare(row2.name);
    },

    _updateHeader(row, before) {
        if (!before || row.get_header())
            return;

        let sep = new Gtk.Separator({ orientation: Gtk.Orientation.HORIZONTAL });
        row.set_header(sep);
    },

    _scanExtensions() {
        this.shellProxy.ListExtensionsRemote(([extensionsProxy], e) => {
            if (e) {
                if (e instanceof Gio.DBusError)
                    this._showErrorUI("<b>" + _("Can't connect to the Shell: are you running this tool in the correct session?") + "</b>");
                else
                    throw e;
                return;
            }

            for (let uuid in extensionsProxy) {
                let extension = ExtensionUtils.deserializeExtension(extensionsProxy[uuid]);
                this._extensionFound(extension);
            }
            this._extensionsLoaded();
        });
    },

    _extensionFound(extension) {
        let row = new ExtensionRow(extension);

        row.prefsButton.connect('clicked', () => {
            this._showPrefs(row.uuid);
        });

        row.show_all();
        this._extensionSelector.add(row);
    },

    _extensionsLoaded() {
        if (this._startupUuid)
            this._showPrefs(this._startupUuid);
        this._startupUuid = null;
        this._skipMainWindow = false;
        this._loaded = true;
    },

    _onActivate() {
        this._window.present();
    },

    _onStartup(app) {
        this._buildUI(app);
        this._scanExtensions();
        },

    _onCommandLine(app, commandLine) {
        app.activate();
        let args = commandLine.get_arguments();

        if (args.length) {
            let uuid = args[0];

            this._skipMainWindow = true;

            // Strip off "extension:///" prefix which fakes a URI, if it exists
            uuid = stripPrefix(uuid, "extension:///");

            if (!this._loaded)
                this._startupUuid = uuid;
            else if (!this._showPrefs(uuid))
                this._skipMainWindow = false;
        }
        return 0;
    }
});

var DescriptionLabel = new Lang.Class({
    Name: 'DescriptionLabel',
    Extends: Gtk.Label,

    vfunc_get_preferred_height_for_width(width) {
        // Hack: Request the maximum height allowed by the line limit
        if (this.lines > 0)
            return this.parent(0);
        return this.parent(width);
    }
});

var ExtensionRow = new Lang.Class({
    Name: 'ExtensionRow',
    Extends: Gtk.ListBoxRow,

    _init(extension) {
        this.parent();

        this._extension = extension;

        this._extensionStatusChangedId = app.shellProxy.connectSignal('ExtensionStatusChanged',
            (proxy, senderName, [uuid, state, error]) => {
                if (this.uuid !== uuid) {
                    return;
                }

                this._extension.state = state;
                this._toggleSwitch(state === ExtensionUtils.ExtensionState.ENABLED);
        });

        this._settings = new Gio.Settings({ schema_id: 'org.gnome.shell' });

        this._settings.connect('changed::disable-extension-version-validation',
            () => {
                this._switch.sensitive = this._canEnable();
            });
        this._settings.connect('changed::disable-user-extensions',
            () => {
                this._switch.sensitive = this._canEnable();
            });

        this._buildUI();
    },

    get uuid() {
        return this._extension.uuid;
    },

    get name() {
        return this._extension.metadata.name;
    },

    get hasPrefs() {
        return this._extension.hasPrefs;
    },

    disconnectSignals() {
        if (!app.shellProxy)
            return;

        if (this._extensionStatusChangedId)
            app.shellProxy.disconnectSignal(this._extensionStatusChangedId);
        this._extensionStatusChangedId = null;
    },

    _buildUI() {
        let hbox = new Gtk.Box({ orientation: Gtk.Orientation.HORIZONTAL,
                                 hexpand: true, margin_end: 24, spacing: 24,
                                 margin: 12 });
        this.add(hbox);

        let vbox = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL,
                                 spacing: 6, hexpand: true });
        hbox.add(vbox);

        let name = GLib.markup_escape_text(this.name, -1);
        let label = new Gtk.Label({ label: '<b>' + name + '</b>',
                                    use_markup: true,
                                    halign: Gtk.Align.START });
        vbox.add(label);

        let desc = this._extension.metadata.description.split('\n')[0];
        label = new DescriptionLabel({ label: desc, wrap: true, lines: 2,
                                       ellipsize: Pango.EllipsizeMode.END,
                                       xalign: 0, yalign: 0 });
        vbox.add(label);

        let button = new Gtk.Button({ valign: Gtk.Align.CENTER,
                                      no_show_all: true });
        button.add(new Gtk.Image({ icon_name: 'emblem-system-symbolic',
                                   icon_size: Gtk.IconSize.BUTTON,
                                   visible: true }));
        button.get_style_context().add_class('circular');
        button.visible = this.hasPrefs;
        hbox.add(button);

        this.prefsButton = button;

        this._switch = new Gtk.Switch({ valign: Gtk.Align.CENTER,
                                        sensitive: this._canToggle(),
                                        state: this._extension.state === ExtensionUtils.ExtensionState.ENABLED });
        this._switch.connect('notify::active', () => { this._switchToggled() });
        hbox.add(this._switch);
    },

    _toggleSwitch(enabled) {
        if (enabled !== this._switch.active)
            this._switch.active = enabled;
    },

    _switchToggled() {
        let enabled = this._switch.active;
        if (enabled && (this._extension.state !== ExtensionUtils.ExtensionState.ENABLED))
            app.shellProxy.EnableExtensionRemote(this.uuid);
        else if (!enabled && (this._extension.state === ExtensionUtils.ExtensionState.ENABLED))
            app.shellProxy.DisableExtensionRemote(this.uuid);
    },

    _canToggle() {
        let checkVersion = !this._settings.get_boolean('disable-extension-version-validation');

        return !this._settings.get_boolean('disable-user-extensions') &&
               !(checkVersion && this._extension.state === ExtensionUtils.ExtensionState.OUT_OF_DATE);
    },

    get prefsModule() {
        ExtensionUtils.installImporter(this._extension);

        // give extension prefs access to their own extension object
        ExtensionUtils.getCurrentExtension = () => this._extension;

        let prefsModule = this._extension.imports.prefs;
        prefsModule.init(this._extension.metadata);

        return prefsModule;
    },

});

function initEnvironment() {
    // Monkey-patch in a "global" object that fakes some Shell utilities
    // that ExtensionUtils depends on.
    window.global = {
        log() {
            print([].join.call(arguments, ', '));
        },

        logError(s) {
            log('ERROR: ' + s);
        },

        userdatadir: GLib.build_filenamev([GLib.get_user_data_dir(), 'gnome-shell'])
    };

    String.prototype.format = Format.format;
}

function main(argv) {
    initEnvironment();

    Gettext.bindtextdomain(Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
    Gettext.textdomain(Config.GETTEXT_PACKAGE);

    app = new Application();
    app.application.run(argv);
}
