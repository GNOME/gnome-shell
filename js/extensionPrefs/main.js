/* exported main */
imports.gi.versions.Gdk = '3.0';
imports.gi.versions.Gtk = '3.0';

const Gettext = imports.gettext;
const { Gdk, GLib, Gio, GObject, Gtk, Pango } = imports.gi;
const Format = imports.format;

const _ = Gettext.gettext;

const Config = imports.misc.config;
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

var Application = GObject.registerClass({
    GTypeName: 'ExtensionPrefs_Application'
}, class Application extends Gtk.Application {
    _init() {
        GLib.set_prgname('gnome-shell-extension-prefs');
        super._init({
            application_id: 'org.gnome.shell.ExtensionPrefs',
            flags: Gio.ApplicationFlags.HANDLES_COMMAND_LINE
        });

        this._startupUuid = null;
        this._loaded = false;
        this._skipMainWindow = false;
        this._shellProxy = null;
    }

    get shellProxy() {
        return this._shellProxy;
    }

    _showPrefs(uuid) {
        let row = this._extensionSelector.get_children().find(c => {
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
            this.add_window(dialog);
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
    }

    _buildErrorUI(row, exc) {
        let scroll = new Gtk.ScrolledWindow({
            hscrollbar_policy: Gtk.PolicyType.NEVER,
            propagate_natural_height: true
        });

        let box = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL,
            spacing: 12,
            margin: 100,
            margin_bottom: 60
        });
        scroll.add(box);

        let label = new Gtk.Label({
            label: '<span size="x-large">%s</span>'.format(_("Something’s gone wrong")),
            use_markup: true
        });
        label.get_style_context().add_class(Gtk.STYLE_CLASS_DIM_LABEL);
        box.add(label);

        label = new Gtk.Label({
            label: _("We’re very sorry, but there’s been a problem: the settings for this extension can’t be displayed. We recommend that you report the issue to the extension authors."),
            justify: Gtk.Justification.CENTER,
            wrap: true
        });
        box.add(label);

        let expander = new Expander({
            label: _("Technical Details"),
            margin_top: 12
        });
        box.add(expander);

        let errortext = `${exc}\n\nStack trace:\n${
            // Indent stack trace.
            exc.stack.split('\n').map(line => `  ${line}`).join('\n')
        }`;

        let buffer = new Gtk.TextBuffer({ text: errortext });
        let textview = new Gtk.TextView({
            buffer: buffer,
            wrap_mode: Gtk.WrapMode.WORD,
            monospace: true,
            editable: false,
            top_margin: 12,
            bottom_margin: 12,
            left_margin: 12,
            right_margin: 12
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
            tooltip_text: _("Copy Error")
        });
        toolbar.add(copyButton);

        copyButton.connect('clicked', w => {
            let clipboard = Gtk.Clipboard.get_default(w.get_display());
            // markdown for pasting in gitlab issues
            let lines = [
                `The settings of extension ${row.uuid} had an error:`,
                '```',
                `${exc}`,
                '```',
                '',
                'Stack trace:',
                '```',
                exc.stack.replace(/\n$/, ''), // stack without trailing newline
                '```',
                ''
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
            visible: row.url != null
        });
        toolbar.add(urlButton);

        urlButton.connect('clicked', w => {
            let context = w.get_display().get_app_launch_context();
            Gio.AppInfo.launch_default_for_uri(row.url, context);
        });

        let expandedBox = new Gtk.Box({
            orientation: Gtk.Orientation.VERTICAL
        });
        expandedBox.add(textview);
        expandedBox.add(toolbar);

        expander.add(expandedBox);

        scroll.show_all();
        return scroll;
    }

    _buildUI() {
        this._window = new Gtk.ApplicationWindow({ application: this,
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

        this._mainStack = new Gtk.Stack({
            transition_type: Gtk.StackTransitionType.CROSSFADE
        });
        this._window.add(this._mainStack);

        let scroll = new Gtk.ScrolledWindow({ hscrollbar_policy: Gtk.PolicyType.NEVER });

        this._extensionSelector = new Gtk.ListBox({ selection_mode: Gtk.SelectionMode.NONE });
        this._extensionSelector.set_sort_func(this._sortList.bind(this));
        this._extensionSelector.set_header_func(this._updateHeader.bind(this));

        scroll.add(this._extensionSelector);

        this._mainStack.add_named(scroll, 'listing');
        this._mainStack.add_named(new EmptyPlaceholder(), 'placeholder');

        this._shellProxy = new GnomeShellProxy(Gio.DBus.session, 'org.gnome.Shell', '/org/gnome/Shell');
        this._shellProxy.connectSignal('ExtensionStateChanged',
            this._onExtensionStateChanged.bind(this));

        this._window.show_all();
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
        return this._extensionSelector.get_children().find(c => c.uuid === uuid);
    }

    _onExtensionStateChanged(proxy, senderName, [uuid, newState]) {
        let row = this._findExtensionRow(uuid);
        if (row) {
            let { state } = ExtensionUtils.deserializeExtension(newState);
            if (state == ExtensionState.UNINSTALLED)
                row.destroy();
            return; // we only deal with new and deleted extensions here
        }

        this._shellProxy.GetExtensionInfoRemote(uuid, ([serialized]) => {
            let extension = ExtensionUtils.deserializeExtension(serialized);
            if (!extension)
                return;
            // check the extension wasn't added in between
            if (this._findExtensionRow(uuid) != null)
                return;
            this._addExtensionRow(extension);
        });
    }

    _scanExtensions() {
        this._shellProxy.ListExtensionsRemote(([extensionsMap], e) => {
            if (e) {
                if (e instanceof Gio.DBusError) {
                    log(`Failed to connect to shell proxy: ${e}`);
                    this._mainStack.add_named(new NoShellPlaceholder(), 'noshell');
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
        this._extensionSelector.add(row);
    }

    _extensionsLoaded() {
        if (this._extensionSelector.get_children().length > 0)
            this._mainStack.visible_child_name = 'listing';
        else
            this._mainStack.visible_child_name = 'placeholder';

        if (this._startupUuid)
            this._showPrefs(this._startupUuid);
        this._startupUuid = null;
        this._skipMainWindow = false;
        this._loaded = true;
    }

    vfunc_activate() {
        this._window.present();
    }

    vfunc_startup() {
        super.vfunc_startup();

        this._buildUI();
        this._scanExtensions();
    }

    vfunc_command_line(commandLine) {
        this.activate();
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

var Expander = GObject.registerClass({
    Properties: {
        'label': GObject.ParamSpec.string(
            'label', 'label', 'label',
            GObject.ParamFlags.READWRITE,
            null
        )
    }
}, class Expander extends Gtk.Box {
    _init(params = {}) {
        this._labelText = null;

        super._init(Object.assign(params, {
            orientation: Gtk.Orientation.VERTICAL,
            spacing: 0
        }));

        this._frame = new Gtk.Frame({
            shadow_type: Gtk.ShadowType.IN,
            hexpand: true
        });

        let eventBox = new Gtk.EventBox();
        this._frame.add(eventBox);

        let hbox = new Gtk.Box({
            spacing: 6,
            margin: 12
        });
        eventBox.add(hbox);

        this._arrow = new Gtk.Image({
            icon_name: 'pan-end-symbolic'
        });
        hbox.add(this._arrow);

        this._label = new Gtk.Label({ label: this._labelText });
        hbox.add(this._label);

        this._revealer = new Gtk.Revealer();

        this._childBin = new Gtk.Frame({
            shadow_type: Gtk.ShadowType.IN
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
            exclusive: true
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

var EmptyPlaceholder = GObject.registerClass(
class EmptyPlaceholder extends Gtk.Box {
    _init() {
        super._init({
            orientation: Gtk.Orientation.VERTICAL,
            spacing: 6,
            margin: 32
        });

        let image = new Gtk.Image({
            icon_name: 'application-x-addon-symbolic',
            pixel_size: 96,
            visible: true,
            vexpand: true,
            valign: Gtk.Align.END
        });
        image.get_style_context().add_class(Gtk.STYLE_CLASS_DIM_LABEL);
        this.add(image);

        let label = new Gtk.Label({
            label: `<b><span size="x-large">${_("No Extensions Installed" )}</span></b>`,
            use_markup: true,
            visible: true
        });
        label.get_style_context().add_class(Gtk.STYLE_CLASS_DIM_LABEL);
        this.add(label);

        let appInfo = Gio.DesktopAppInfo.new('org.gnome.Software.desktop');

        let desc = new Gtk.Label({
            label: _("Extensions can be installed through Software or <a href=\"https://extensions.gnome.org\">extensions.gnome.org</a>."),
            use_markup: true,
            wrap: true,
            justify: Gtk.Justification.CENTER,
            visible: true,
            max_width_chars: 50,
            hexpand: true,
            vexpand: (appInfo == null),
            halign: Gtk.Align.CENTER,
            valign: Gtk.Align.START
        });
        this.add(desc);

        if (appInfo) {
            let button = new Gtk.Button({
                label: _("Browse in Software"),
                image: new Gtk.Image({
                    icon_name: "org.gnome.Software-symbolic"
                }),
                always_show_image: true,
                margin_top: 12,
                visible: true,
                halign: Gtk.Align.CENTER,
                valign: Gtk.Align.START,
                vexpand: true
            });
            this.add(button);

            button.connect('clicked', w => {
                let context = w.get_display().get_app_launch_context();
                appInfo.launch([], context);
            });
        }
    }
});

var NoShellPlaceholder = GObject.registerClass(
class NoShellPlaceholder extends Gtk.Box {
    _init() {
        super._init({
            orientation: Gtk.Orientation.VERTICAL,
            spacing: 12,
            margin: 100,
            margin_bottom: 60
        });

        let label = new Gtk.Label({
            label: '<span size="x-large">%s</span>'.format(
                _("Something’s gone wrong")),
            use_markup: true
        });
        label.get_style_context().add_class(Gtk.STYLE_CLASS_DIM_LABEL);
        this.add(label);

        label = new Gtk.Label({
            label: _("We’re very sorry, but it was not possible to get the list of installed extensions. Make sure you are logged into GNOME and try again."),
            justify: Gtk.Justification.CENTER,
            wrap: true
        });
        this.add(label);

        this.show_all();
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
                let state = (this._extension.state == ExtensionState.ENABLED);

                GObject.signal_handler_block(this._switch, this._notifyActiveId);
                this._switch.state = state;
                GObject.signal_handler_unblock(this._switch, this._notifyActiveId);

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
            state: this._extension.state === ExtensionState.ENABLED
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
        if (!this._prefsModule) {
            ExtensionUtils.installImporter(this._extension);

            // give extension prefs access to their own extension object
            ExtensionUtils.getCurrentExtension = () => this._extension;

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

        userdatadir: GLib.build_filenamev([GLib.get_user_data_dir(), 'gnome-shell'])
    };

    String.prototype.format = Format.format;
}

function main(argv) {
    initEnvironment();

    Gettext.bindtextdomain(Config.GETTEXT_PACKAGE, Config.LOCALEDIR);
    Gettext.textdomain(Config.GETTEXT_PACKAGE);

    new Application().run(argv);
}
