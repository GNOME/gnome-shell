
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

const GnomeShellIface = '<node> \
<interface name="org.gnome.Shell.Extensions"> \
<signal name="ExtensionStatusChanged"> \
    <arg type="s" name="uuid"/> \
    <arg type="i" name="state"/> \
    <arg type="s" name="error"/> \
</signal> \
</interface> \
</node>';

const GnomeShellProxy = Gio.DBusProxy.makeProxyWrapper(GnomeShellIface);

function stripPrefix(string, prefix) {
    if (string.slice(0, prefix.length) == prefix)
        return string.slice(prefix.length);
    return string;
}

var Application = new Lang.Class({
    Name: 'Application',
    _init() {
        GLib.set_prgname('gnome-shell-extension-prefs');
        this.application = new Gtk.Application({
            application_id: 'org.gnome.shell.ExtensionPrefs',
            flags: Gio.ApplicationFlags.HANDLES_COMMAND_LINE
        });

        this.application.connect('activate', Lang.bind(this, this._onActivate));
        this.application.connect('command-line', Lang.bind(this, this._onCommandLine));
        this.application.connect('startup', Lang.bind(this, this._onStartup));

        this._extensionPrefsModules = {};

        this._startupUuid = null;
        this._loaded = false;
        this._skipMainWindow = false;
    },

    _extensionAvailable(uuid) {
        let extension = ExtensionUtils.extensions[uuid];

        if (!extension)
            return false;

        if (!extension.dir.get_child('prefs.js').query_exists(null))
            return false;

        return true;
    },

    _getExtensionPrefsModule(extension) {
        let uuid = extension.metadata.uuid;

        if (this._extensionPrefsModules.hasOwnProperty(uuid))
            return this._extensionPrefsModules[uuid];

        ExtensionUtils.installImporter(extension);

        let prefsModule = extension.imports.prefs;
        prefsModule.init(extension.metadata);

        this._extensionPrefsModules[uuid] = prefsModule;
        return prefsModule;
    },

    _selectExtension(uuid) {
        if (!this._extensionAvailable(uuid))
            return;

        let extension = ExtensionUtils.extensions[uuid];
        let widget;

        try {
            let prefsModule = this._getExtensionPrefsModule(extension);
            widget = prefsModule.buildPrefsWidget();
        } catch (e) {
            widget = this._buildErrorUI(extension, e);
        }

        let dialog = new Gtk.Window({ modal: !this._skipMainWindow,
                                      type_hint: Gdk.WindowTypeHint.DIALOG });
        dialog.set_titlebar(new Gtk.HeaderBar({ show_close_button: true,
                                                title: extension.metadata.name,
                                                visible: true }));

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
    },

    _buildErrorUI(extension, exc) {
        let box = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL });
        let label = new Gtk.Label({
            label: _("There was an error loading the preferences dialog for %s:").format(extension.metadata.name)
        });
        box.add(label);

        let errortext = '';
        errortext += exc;
        errortext += '\n\n';
        errortext += 'Stack trace:\n';

        // Indent stack trace.
        errortext += exc.stack.split('\n').map(function(line) {
            return '  ' + line;
        }).join('\n');

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

        this._settings = new Gio.Settings({ schema_id: 'org.gnome.shell' });
        this._settings.bind('disable-user-extensions', killSwitch, 'active',
                            Gio.SettingsBindFlags.DEFAULT |
                            Gio.SettingsBindFlags.INVERT_BOOLEAN);

        let scroll = new Gtk.ScrolledWindow({ hscrollbar_policy: Gtk.PolicyType.NEVER });
        this._window.add(scroll);

        this._extensionSelector = new Gtk.ListBox({ selection_mode: Gtk.SelectionMode.NONE });
        this._extensionSelector.set_sort_func(Lang.bind(this, this._sortList));
        this._extensionSelector.set_header_func(Lang.bind(this, this._updateHeader));

        scroll.add(this._extensionSelector);


        this._shellProxy = new GnomeShellProxy(Gio.DBus.session, 'org.gnome.Shell', '/org/gnome/Shell');
        this._shellProxy.connectSignal('ExtensionStatusChanged', Lang.bind(this, function(proxy, senderName, [uuid, state, error]) {
            if (ExtensionUtils.extensions[uuid] !== undefined)
                this._scanExtensions();
        }));

        this._window.show_all();
    },

    _sortList(row1, row2) {
        let name1 = ExtensionUtils.extensions[row1.uuid].metadata.name;
        let name2 = ExtensionUtils.extensions[row2.uuid].metadata.name;
        return name1.localeCompare(name2);
    },

    _updateHeader(row, before) {
        if (!before || row.get_header())
            return;

        let sep = new Gtk.Separator({ orientation: Gtk.Orientation.HORIZONTAL });
        row.set_header(sep);
    },

    _scanExtensions() {
        let finder = new ExtensionUtils.ExtensionFinder();
        finder.connect('extension-found', Lang.bind(this, this._extensionFound));
        finder.scanExtensions();
        this._extensionsLoaded();
    },

    _extensionFound(finder, extension) {
        let row = new ExtensionRow(extension.uuid);

        row.prefsButton.visible = this._extensionAvailable(row.uuid);
        row.prefsButton.connect('clicked', Lang.bind(this,
            function() {
                this._selectExtension(row.uuid);
            }));

        row.show_all();
        this._extensionSelector.add(row);
    },

    _extensionsLoaded() {
        if (this._startupUuid && this._extensionAvailable(this._startupUuid))
            this._selectExtension(this._startupUuid);
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

            if (this._extensionAvailable(uuid))
                this._selectExtension(uuid);
            else if (!this._loaded)
                this._startupUuid = uuid;
            else
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

    _init(uuid) {
        this.parent();

        this.uuid = uuid;

        this._settings = new Gio.Settings({ schema_id: 'org.gnome.shell' });
        this._settings.connect('changed::enabled-extensions', Lang.bind(this,
            function() {
                this._switch.state = this._isEnabled();
            }));
        this._settings.connect('changed::disable-extension-version-validation',
            Lang.bind(this, function() {
                this._switch.sensitive = this._canEnable();
            }));
        this._settings.connect('changed::disable-user-extensions',
            Lang.bind(this, function() {
                this._switch.sensitive = this._canEnable();
            }));

        this._buildUI();
    },

    _buildUI() {
        let extension = ExtensionUtils.extensions[this.uuid];

        let hbox = new Gtk.Box({ orientation: Gtk.Orientation.HORIZONTAL,
                                 hexpand: true, margin_end: 24, spacing: 24,
                                 margin: 12 });
        this.add(hbox);

        let vbox = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL,
                                 spacing: 6, hexpand: true });
        hbox.add(vbox);

        let name = GLib.markup_escape_text(extension.metadata.name, -1);
        let label = new Gtk.Label({ label: '<b>' + name + '</b>',
                                    use_markup: true,
                                    halign: Gtk.Align.START });
        vbox.add(label);

        let desc = extension.metadata.description.split('\n')[0];
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
        hbox.add(button);

        this.prefsButton = button;

        this._switch = new Gtk.Switch({ valign: Gtk.Align.CENTER,
                                        sensitive: this._canEnable(),
                                        state: this._isEnabled() });
        this._switch.connect('notify::active', Lang.bind(this,
            function() {
                if (this._switch.active)
                    this._enable();
                else
                    this._disable();
            }));
        this._switch.connect('state-set', function() { return true; });
        hbox.add(this._switch);
    },

    _canEnable() {
        let extension = ExtensionUtils.extensions[this.uuid];
        let checkVersion = !this._settings.get_boolean('disable-extension-version-validation');

        return !this._settings.get_boolean('disable-user-extensions') &&
               !(checkVersion && ExtensionUtils.isOutOfDate(extension));
    },

    _isEnabled() {
        let extensions = this._settings.get_strv('enabled-extensions');
        return extensions.indexOf(this.uuid) != -1;
    },

    _enable() {
        let extensions = this._settings.get_strv('enabled-extensions');
        if (extensions.indexOf(this.uuid) != -1)
            return;

        extensions.push(this.uuid);
        this._settings.set_strv('enabled-extensions', extensions);
    },

    _disable() {
        let extensions = this._settings.get_strv('enabled-extensions');
        let pos = extensions.indexOf(this.uuid);
        if (pos == -1)
            return;
        do {
            extensions.splice(pos, 1);
            pos = extensions.indexOf(this.uuid);
        } while (pos != -1);
        this._settings.set_strv('enabled-extensions', extensions);
    }
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

    let app = new Application();
    app.application.run(argv);
}
