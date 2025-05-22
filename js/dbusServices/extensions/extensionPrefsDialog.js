import Adw from 'gi://Adw?version=1';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Gtk from 'gi://Gtk?version=4.0';

import {gettext as _} from 'gettext';

import {formatError} from './misc/errorUtils.js';

export class ExtensionPrefsDialog extends Adw.PreferencesWindow {
    static [GObject.GTypeName] = 'ExtensionPrefsDialog';
    static [GObject.signals] = {
        'loaded': {},
    };

    static {
        GObject.registerClass(this);
    }

    constructor(extension) {
        super({
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

        await prefsObj.fillPreferencesWindow(this);

        if (!this.visible_page)
            throw new Error('Extension did not provide any UI');
    }

    set titlebar(w) {
        this.set_titlebar(w);
    }

    set_titlebar() {
        // intercept fatal libadwaita error, show error page instead
        GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            this._showErrorPage(
                new Error('set_titlebar() is not supported for Adw.Window'));
            return GLib.SOURCE_REMOVE;
        });
    }

    destroy() {
        this._showErrorPage(
            new Error('destroy() breaks tracking open dialogs, use close() if you must'));
    }

    _showErrorPage(e) {
        while (this.visible_page)
            this.remove(this.visible_page);

        this.set_title(_('Extension Error'));
        this.add(new ExtensionPrefsErrorPage(this._extension, e));
    }
}

class ExtensionPrefsErrorPage extends Adw.PreferencesPage {
    static [GObject.GTypeName] = 'ExtensionPrefsErrorPage';
    static [Gtk.template] =
        'resource:///org/gnome/Shell/Extensions/ui/extension-error-page.ui';

    static [Gtk.internalChildren] = [
        'descriptionLabel',
        'errorView',
    ];

    static {
        GObject.registerClass(this);

        this.install_action('page.copy-error',
            null,
            self => {
                const clipboard = self.get_display().get_clipboard();
                clipboard.set(self._errorMarkdown);
            });
    }

    constructor(extension, error) {
        super();

        const {uuid, name, url} = extension.metadata;

        let label =
            /* Translators: %s is an extension name */
            _('Unable to display the settings for “%s”.')
                .format(GLib.markup_escape_text(name, -1));

        if (url) {
            /* Translators: Link label in the phrase "Information about this
               problem may be available on the extension website" */
            const linkLabel = _('extension website');

            label += ' ';
            /* Translators: %s is "extension website" */
            label += _('Information about this problem may be available on the %s.')
                .format(`<a href="${url}">${linkLabel}</a>`);
        }

        this._descriptionLabel.set({label});

        const formattedError = formatError(error);
        this._errorView.buffer.text = formattedError;

        // markdown for pasting in gitlab issues
        let lines = [
            `The settings of extension ${uuid} had an error:`,
            '```',
            formattedError.replace(/\n$/, ''),  // remove trailing newline
            '```',
            '',
        ];
        this._errorMarkdown = lines.join('\n');
    }
}
