/* exported main */
imports.gi.versions.Gdk = '3.0';
imports.gi.versions.Gtk = '3.0';

const Gettext = imports.gettext;
const { Gdk, GLib, Gio, GObject, Gtk, Pango } = imports.gi;
const Format = imports.format;

const _ = Gettext.gettext;

const ExtensionUtils = imports.misc.extensionUtils;
const { loadInterfaceXML } = imports.misc.fileUtils;

const { ExtensionState } = ExtensionUtils;

const GnomeShellIface = loadInterfaceXML('org.gnome.Shell.Extensions');
const GnomeShellProxy = Gio.DBusProxy.makeProxyWrapper(GnomeShellIface);

function stripPrefix(string, prefix) {
    if (string.slice(0, prefix.length) == prefix)
        return string.slice(prefix.length);
    return string;
}

var Application = GObject.registerClass(
class Application extends Gtk.Application {
    _init() {
        GLib.set_prgname('gnome-shell-extension-prefs');
        super._init({
            application_id: 'org.gnome.shell.ExtensionPrefs',
            flags: Gio.ApplicationFlags.HANDLES_COMMAND_LINE,
        });
    }

    get shellProxy() {
        return this._shellProxy;
    }

    vfunc_activate() {
        this._window.present();
    }

    vfunc_startup() {
        super.vfunc_startup();

        this._shellProxy = new GnomeShellProxy(Gio.DBus.session, 'org.gnome.Shell', '/org/gnome/Shell');
        this._window = new ExtensionsWindow({ application: this });
    }

    vfunc_command_line(commandLine) {
        let args = commandLine.get_arguments();

        if (args.length) {
            let uuid = args[0];

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
    Template: 'resource:///org/gnome/shell/ui/extensions-window.ui',
    InternalChildren: [
        'extensionsList',
        'killSwitch',
        'mainStack',
    ],
}, class ExtensionsWindow extends Gtk.ApplicationWindow {
    _init(params) {
        super._init(params);

        this._startupUuid = null;
        this._loaded = false;
        this._prefsDialog = null;

        this._settings = new Gio.Settings({ schema_id: 'org.gnome.shell' });
        this._settings.bind('disable-user-extensions',
            this._killSwitch, 'active',
            Gio.SettingsBindFlags.DEFAULT | Gio.SettingsBindFlags.INVERT_BOOLEAN);

        this._extensionsList.set_sort_func(this._sortList.bind(this));
        this._extensionsList.set_header_func(this._updateHeader.bind(this));

        this._shellProxy.connectSignal('ExtensionStateChanged',
            this._onExtensionStateChanged.bind(this));

        this._scanExtensions();
    }

    get _shellProxy() {
        return this.application.shellProxy;
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

        let row = this._extensionsList.get_children().find(c => {
            return c.uuid === uuid && c.hasPrefs;
        });

        if (!row)
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

        let errortext = `${exc}\n\nStack trace:\n${
            // Indent stack trace.
            exc.stack.split('\n').map(line => `  ${line}`).join('\n')
        }`;

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
                `The settings of extension ${row.uuid} had an error:`,
                '```', // '`' (xgettext throws up on odd number of backticks)
                `${exc}`,
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
            visible: row.url != null,
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
        return this._extensionsList.get_children().find(c => c.uuid === uuid);
    }

    _onExtensionStateChanged(proxy, senderName, [uuid, newState]) {
        let extension = ExtensionUtils.deserializeExtension(newState);
        let row = this._findExtensionRow(uuid);

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
                    log(`Failed to connect to shell proxy: ${e}`);
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

        row.prefsButton.connect('clicked', () => {
            this._showPrefs(row.uuid);
        });

        row.show_all();
        this._extensionsList.add(row);
    }

    _extensionsLoaded() {
        if (this._extensionsList.get_children().length > 0)
            this._mainStack.visible_child_name = 'main';
        else
            this._mainStack.visible_child_name = 'placeholder';

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

var DescriptionLabel = GObject.registerClass(
class DescriptionLabel extends Gtk.Label {
    vfunc_get_preferred_height_for_width(width) {
        // Hack: Request the maximum height allowed by the line limit
        if (this.lines > 0)
            return super.vfunc_get_preferred_height_for_width(0);
        return super.vfunc_get_preferred_height_for_width(width);
    }
});

var ExtensionRow = GObject.registerClass(
class ExtensionRow extends Gtk.ListBoxRow {
    _init(extension) {
        super._init();

        this._app = Gio.Application.get_default();
        this._extension = extension;
        this._prefsModule = null;

        this.connect('destroy', this._onDestroy.bind(this));

        this._buildUI();

        this._extensionStateChangedId = this._app.shellProxy.connectSignal(
            'ExtensionStateChanged', (p, sender, [uuid, newState]) => {
                if (this.uuid !== uuid)
                    return;

                this._extension = ExtensionUtils.deserializeExtension(newState);
                let state = this._extension.state == ExtensionState.ENABLED;

                this._switch.block_signal_handler(this._notifyActiveId);
                this._switch.state = state;
                this._switch.unblock_signal_handler(this._notifyActiveId);

                this._switch.sensitive = this._canToggle();
            });
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

    get url() {
        return this._extension.metadata.url;
    }

    _onDestroy() {
        if (!this._app.shellProxy)
            return;

        if (this._extensionStateChangedId)
            this._app.shellProxy.disconnectSignal(this._extensionStateChangedId);
        this._extensionStateChangedId = 0;
    }

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
                                      visible: this.hasPrefs,
                                      no_show_all: true });
        button.set_image(new Gtk.Image({ icon_name: 'emblem-system-symbolic',
                                         icon_size: Gtk.IconSize.BUTTON,
                                         visible: true }));
        button.get_style_context().add_class('circular');
        hbox.add(button);

        this.prefsButton = button;

        this._switch = new Gtk.Switch({
            valign: Gtk.Align.CENTER,
            sensitive: this._canToggle(),
            state: this._extension.state === ExtensionState.ENABLED,
        });
        this._notifyActiveId = this._switch.connect('notify::active', () => {
            if (this._switch.active)
                this._app.shellProxy.EnableExtensionRemote(this.uuid);
            else
                this._app.shellProxy.DisableExtensionRemote(this.uuid);
        });
        this._switch.connect('state-set', () => true);
        hbox.add(this._switch);
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
            log(`ERROR: ${s}`);
        },

        userdatadir: GLib.build_filenamev([GLib.get_user_data_dir(), 'gnome-shell']),
    };

    String.prototype.format = Format.format;
}

function main(argv) {
    initEnvironment();

    new Application().run(argv);
}
