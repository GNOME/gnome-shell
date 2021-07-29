// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported ExtensionPrefsDialog */

const { Gdk, Gio, GObject, Gtk } = imports.gi;

const ExtensionUtils = imports.misc.extensionUtils;

var ExtensionPrefsDialog = GObject.registerClass({
    GTypeName: 'ExtensionPrefsDialog',
    Template: 'resource:///org/gnome/Shell/Extensions/ui/extension-prefs-dialog.ui',
    InternalChildren: [
        'stack',
        'expander',
        'expanderArrow',
        'revealer',
        'errorView',
    ],
}, class ExtensionPrefsDialog extends Gtk.Window {
    _init(extension) {
        super._init({
            title: extension.metadata.name,
        });

        this._uuid = extension.uuid;
        this._url = extension.metadata.url || '';

        this._actionGroup = new Gio.SimpleActionGroup();
        this.insert_action_group('win', this._actionGroup);

        this._initActions();
        this._addCustomStylesheet();

        this._gesture = new Gtk.GestureClick({
            button: 0,
            exclusive: true,
        });
        this._expander.add_controller(this._gesture);

        this._gesture.connect('released', (gesture, nPress) => {
            if (nPress === 1)
                this._revealer.reveal_child = !this._revealer.reveal_child;
        });

        this._revealer.connect('notify::reveal-child', () => {
            this._expanderArrow.icon_name = this._revealer.reveal_child
                ? 'pan-down-symbolic'
                : 'pan-end-symbolic';
            this._syncExpandedStyle();
        });
        this._revealer.connect('notify::child-revealed',
            () => this._syncExpandedStyle());

        try {
            ExtensionUtils.installImporter(extension);

            // give extension prefs access to their own extension object
            ExtensionUtils.getCurrentExtension = () => extension;

            const prefsModule = extension.imports.prefs;
            prefsModule.init(extension.metadata);

            const widget = prefsModule.buildPrefsWidget();
            this._stack.add_named(widget, 'prefs');
            this._stack.visible_child = widget;
        } catch (e) {
            this._setError(e);
            logError(e, 'Failed to open preferences');
        }
    }

    _syncExpandedStyle() {
        if (this._revealer.reveal_child)
            this._expander.add_css_class('expanded');
        else if (!this._revealer.child_revealed)
            this._expander.remove_css_class('expanded');
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
            const clipboard = this.get_display().get_clipboard();
            clipboard.set(this._errorMarkdown);
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
        Gtk.StyleContext.add_provider_for_display(Gdk.Display.get_default(),
            provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
});

