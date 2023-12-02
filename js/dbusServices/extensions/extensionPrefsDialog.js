// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

import Adw from 'gi://Adw?version=1';
import Gdk from 'gi://Gdk?version=4.0';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Gtk from 'gi://Gtk?version=4.0';

import {formatError} from './misc/errorUtils.js';

export const ExtensionPrefsDialog = GObject.registerClass({
    GTypeName: 'ExtensionPrefsDialog',
    Signals: {
        'loaded': {},
    },
}, class ExtensionPrefsDialog extends Adw.PreferencesWindow {
    _init(extension) {
        super._init({
            title: extension.metadata.name,
            search_enabled: false,
        });

        this._extension = extension;

        this._loadPrefs().catch(e => {
            this._showErrorPage(e);
            logError(e, 'Failed to open preferences');
        }).finally(() => this.emit('loaded'));
    }

    async _loadPrefs() {
        const {dir, path, metadata} = this._extension;

        const prefsJs = dir.get_child('prefs.js');
        const prefsModule = await import(prefsJs.get_uri());

        const prefsObj = new prefsModule.default({...metadata, dir, path});
        this._extension.stateObj = prefsObj;

        prefsObj.fillPreferencesWindow(this);

        if (!this.visible_page)
            throw new Error('Extension did not provide any UI');
    }

    set titlebar(w) {
        this.set_titlebar(w);
    }

    // eslint-disable-next-line camelcase
    set_titlebar() {
        // intercept fatal libadwaita error, show error page instead
        GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            this._showErrorPage(
                new Error('set_titlebar() is not supported for Adw.Window'));
            return GLib.SOURCE_REMOVE;
        });
    }

    _showErrorPage(e) {
        while (this.visible_page)
            this.remove(this.visible_page);

        this.add(new ExtensionPrefsErrorPage(this._extension, e));
    }
});

const ExtensionPrefsErrorPage = GObject.registerClass({
    GTypeName: 'ExtensionPrefsErrorPage',
    Template: 'resource:///org/gnome/Shell/Extensions/ui/extension-error-page.ui',
    InternalChildren: [
        'expander',
        'expanderArrow',
        'revealer',
        'errorView',
    ],
}, class ExtensionPrefsErrorPage extends Adw.PreferencesPage {
    static _classInit(klass) {
        super._classInit(klass);

        klass.install_action('page.copy-error',
            null,
            self => {
                const clipboard = self.get_display().get_clipboard();
                clipboard.set(self._errorMarkdown);
            });
        klass.install_action('page.show-url',
            null,
            self => Gtk.show_uri(self.get_root(), self._url, Gdk.CURRENT_TIME));

        return klass;
    }

    _init(extension, error) {
        super._init();

        this._addCustomStylesheet();

        this._uuid = extension.uuid;
        this._url = extension.metadata.url || '';

        this.action_set_enabled('page.show-url', this._url !== '');

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

        const formattedError = formatError(error);
        this._errorView.buffer.text = formattedError;

        // markdown for pasting in gitlab issues
        let lines = [
            `The settings of extension ${this._uuid} had an error:`,
            '```',
            formattedError.replace(/\n$/, ''),  // remove trailing newline
            '```',
            '',
        ];
        this._errorMarkdown = lines.join('\n');
    }

    _syncExpandedStyle() {
        if (this._revealer.reveal_child)
            this._expander.add_css_class('expanded');
        else if (!this._revealer.child_revealed)
            this._expander.remove_css_class('expanded');
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
