
const Lang = imports.lang;
const Gettext = imports.gettext;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
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

const Application = new Lang.Class({
    Name: 'Application',
    _init: function() {
        GLib.set_prgname('gnome-shell-extension-prefs');
        this.application = new Gtk.Application({
            application_id: 'org.gnome.shell.ExtensionPrefs',
            flags: Gio.ApplicationFlags.HANDLES_COMMAND_LINE
        });

        this.application.connect('activate', Lang.bind(this, this._onActivate));
        this.application.connect('command-line', Lang.bind(this, this._onCommandLine));
        this.application.connect('startup', Lang.bind(this, this._onStartup));

        this._extensionPrefsModules = {};

        this._extensionIters = {};
        this._startupUuid = null;
    },

    _buildModel: function() {
        this._model = new Gtk.ListStore();
        this._model.set_column_types([GObject.TYPE_STRING, GObject.TYPE_STRING]);
    },

    _extensionAvailable: function(uuid) {
        let extension = ExtensionUtils.extensions[uuid];

        if (!extension)
            return false;

        if (ExtensionUtils.isOutOfDate(extension))
            return false;

        if (!extension.dir.get_child('prefs.js').query_exists(null))
            return false;

        return true;
    },

    _setExtensionInsensitive: function(layout, cell, model, iter, data) {
        let uuid = model.get_value(iter, 0);
        cell.set_sensitive(this._extensionAvailable(uuid));
    },

    _getExtensionPrefsModule: function(extension) {
        let uuid = extension.metadata.uuid;

        if (this._extensionPrefsModules.hasOwnProperty(uuid))
            return this._extensionPrefsModules[uuid];

        ExtensionUtils.installImporter(extension);

        let prefsModule = extension.imports.prefs;
        prefsModule.init(extension.metadata);

        this._extensionPrefsModules[uuid] = prefsModule;
        return prefsModule;
    },

    _selectExtension: function(uuid) {
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

        // Destroy the current prefs widget, if it exists
        if (this._extensionPrefsBin.get_child())
            this._extensionPrefsBin.get_child().destroy();

        this._extensionPrefsBin.add(widget);
        this._extensionSelector.set_active_iter(this._extensionIters[uuid]);
    },

    _extensionSelected: function() {
        let [success, iter] = this._extensionSelector.get_active_iter();
        if (!success)
            return;

        let uuid = this._model.get_value(iter, 0);
        this._selectExtension(uuid);
    },

    _buildErrorUI: function(extension, exc) {
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

    _buildUI: function(app) {
        this._window = new Gtk.ApplicationWindow({ application: app,
                                                   window_position: Gtk.WindowPosition.CENTER,
                                                   title: _("GNOME Shell Extension Preferences") });

        this._window.set_size_request(600, 400);

        let vbox = new Gtk.Box({ orientation: Gtk.Orientation.VERTICAL });
        this._window.add(vbox);

        let toolbar = new Gtk.Toolbar();
        toolbar.get_style_context().add_class(Gtk.STYLE_CLASS_PRIMARY_TOOLBAR);
        vbox.add(toolbar);
        let toolitem;

        let label = new Gtk.Label({ label: '<b>' + _("Extension") + '</b>',
                                    use_markup: true });
        toolitem = new Gtk.ToolItem({ child: label });
        toolbar.add(toolitem);

        this._extensionSelector = new Gtk.ComboBox({ model: this._model,
                                                     margin_left: 8,
                                                     hexpand: true });
        this._extensionSelector.get_style_context().add_class(Gtk.STYLE_CLASS_RAISED);

        let renderer = new Gtk.CellRendererText();
        this._extensionSelector.pack_start(renderer, true);
        this._extensionSelector.add_attribute(renderer, 'text', 1);
        this._extensionSelector.set_cell_data_func(renderer, Lang.bind(this, this._setExtensionInsensitive));
        this._extensionSelector.connect('changed', Lang.bind(this, this._extensionSelected));

        toolitem = new Gtk.ToolItem({ child: this._extensionSelector });
        toolitem.set_expand(true);
        toolbar.add(toolitem);

        this._extensionPrefsBin = new Gtk.Frame();
        vbox.add(this._extensionPrefsBin);

        let label = new Gtk.Label({
            label: _("Select an extension to configure using the combobox above."),
            vexpand: true
        });

        this._extensionPrefsBin.add(label);

        this._shellProxy = new GnomeShellProxy(Gio.DBus.session, 'org.gnome.Shell', '/org/gnome/Shell');
        this._shellProxy.connectSignal('ExtensionStatusChanged', Lang.bind(this, function(proxy, senderName, [uuid, state, error]) {
            if (ExtensionUtils.extensions[uuid] !== undefined)
                this._scanExtensions();
        }));

        this._window.show_all();
    },

    _scanExtensions: function() {
        let finder = new ExtensionUtils.ExtensionFinder();
        finder.connect('extension-found', Lang.bind(this, this._extensionFound));
        finder.connect('extensions-loaded', Lang.bind(this, this._extensionsLoaded));
        finder.scanExtensions();
    },

    _extensionFound: function(signals, extension) {
        let iter = this._model.append();
        this._model.set(iter, [0, 1], [extension.uuid, extension.metadata.name]);
        this._extensionIters[extension.uuid] = iter;
    },

    _extensionsLoaded: function() {
        if (this._startupUuid && this._extensionAvailable(this._startupUuid))
            this._selectExtension(this._startupUuid);
        this._startupUuid = null;
    },

    _onActivate: function() {
        this._window.present();
    },

    _onStartup: function(app) {
        this._buildModel();
        this._buildUI(app);
        this._scanExtensions();
    },

    _onCommandLine: function(app, commandLine) {
        app.activate();
        let args = commandLine.get_arguments();
        if (args.length) {
            let uuid = args[0];

            // Strip off "extension:///" prefix which fakes a URI, if it exists
            uuid = stripPrefix(uuid, "extension:///");

            if (this._extensionAvailable(uuid))
                this._selectExtension(uuid);
            else
                this._startupUuid = uuid;
        }
        return 0;
    }
});

function initEnvironment() {
    // Monkey-patch in a "global" object that fakes some Shell utilities
    // that ExtensionUtils depends on.
    window.global = {
        log: function() {
            print([].join.call(arguments, ', '));
        },

        logError: function(s) {
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
