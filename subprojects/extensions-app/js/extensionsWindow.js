import Adw from 'gi://Adw?version=1';
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import Gtk from 'gi://Gtk?version=4.0';
import Shew from 'gi://Shew';

const Package = imports.package;
import * as Gettext from 'gettext';

import * as Config from './misc/config.js';
import {ExtensionRow} from './extensionRow.js';
import {ExtensionState, ExtensionType, deserializeExtension}  from './misc/extensionUtils.js';

Gio._promisify(Gio.DBusConnection.prototype, 'call');
Gio._promisify(Shew.WindowExporter.prototype, 'export');

export const ExtensionsWindow = GObject.registerClass({
    GTypeName: 'ExtensionsWindow',
    Template: 'resource:///org/gnome/Extensions/ui/extensions-window.ui',
    InternalChildren: [
        'userGroup',
        'userList',
        'systemGroup',
        'systemList',
        'mainStack',
        'searchBar',
        'searchButton',
        'searchEntry',
        'updatesBanner',
    ],
}, class ExtensionsWindow extends Adw.ApplicationWindow {
    _init(params) {
        super._init(params);

        if (Config.PROFILE === 'development')
            this.add_css_class('devel');

        this._updatesCheckId = 0;

        this._exporter = new Shew.WindowExporter({window: this});
        this._exportedHandle = '';

        this.add_action_entries(
            [{
                name: 'show-about',
                activate: () => this._showAbout(),
            }, {
                name: 'logout',
                activate: () => this._logout(),
            }, {
                name: 'user-extensions-enabled',
                state: 'false',
                change_state: (a, state) => {
                    this._shellProxy.UserExtensionsEnabled = state.get_boolean();
                },
            }]);

        this._searchTerms = [];
        this._searchEntry.connect('search-changed', () => {
            const {text} = this._searchEntry;
            if (text === '')
                this._searchTerms = [];
            else
                [this._searchTerms] = GLib.str_tokenize_and_fold(text, null);

            this._userList.invalidate_filter();
            this._systemList.invalidate_filter();
        });

        this._userList.set_sort_func(this._sortList.bind(this));
        this._userList.set_filter_func(this._filterList.bind(this));
        this._userList.set_placeholder(new Gtk.Label({
            label: _('No Matches'),
            margin_start: 12,
            margin_end: 12,
            margin_top: 12,
            margin_bottom: 12,
        }));
        this._userList.connect('row-activated', (_list, row) => row.activate());

        this._systemList.set_sort_func(this._sortList.bind(this));
        this._systemList.set_filter_func(this._filterList.bind(this));
        this._systemList.set_placeholder(new Gtk.Label({
            label: _('No Matches'),
            margin_start: 12,
            margin_end: 12,
            margin_top: 12,
            margin_bottom: 12,
        }));
        this._systemList.connect('row-activated', (_list, row) => row.activate());

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
        const row = this._findExtensionRow(uuid);

        const dialog = new Gtk.MessageDialog({
            transient_for: this,
            modal: true,
            text: _('Remove “%s”?').format(row.name),
            secondary_text: _('If you remove the extension, you need to return to download it if you want to enable it again'),
        });

        dialog.add_button(_('_Cancel'), Gtk.ResponseType.CANCEL);
        dialog.add_button(_('_Remove'), Gtk.ResponseType.ACCEPT)
            .get_style_context().add_class('destructive-action');

        dialog.connect('response', (dlg, response) => {
            if (response === Gtk.ResponseType.ACCEPT)
                this._shellProxy.UninstallExtensionAsync(uuid).catch(console.error);
            dialog.destroy();
        });
        dialog.present();
    }

    async openPrefs(uuid) {
        if (!this._exportedHandle) {
            try {
                this._exportedHandle = await this._exporter.export();
            } catch (e) {
                console.warn(`Failed to export window: ${e.message}`);
            }
        }

        this._shellProxy.OpenExtensionPrefsAsync(uuid,
            this._exportedHandle,
            {modal: new GLib.Variant('b', true)}).catch(console.error);
    }

    _showAbout() {
        const aboutWindow = new Adw.AboutWindow({
            developers: [
                'Florian Müllner <fmuellner@gnome.org>',
                'Jasper St. Pierre <jstpierre@mecheye.net>',
                'Didier Roche <didrocks@ubuntu.com>',
                'Romain Vigier <contact@romainvigier.fr>',
            ],
            designers: [
                'Allan Day <allanpday@gmail.com>',
                'Tobias Bernard <tbernard@gnome.org>',
            ],
            translator_credits: _('translator-credits'),
            application_name: _('Extensions'),
            license_type: Gtk.License.GPL_2_0,
            application_icon: Package.name,
            version: Package.version,
            developer_name: _('The GNOME Project'),
            website: 'https://apps.gnome.org/app/org.gnome.Extensions/',
            issue_url: 'https://gitlab.gnome.org/GNOME/gnome-shell/issues/new',

            transient_for: this,
        });
        aboutWindow.present();
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
            null);
    }

    _sortList(row1, row2) {
        return row1.name.localeCompare(row2.name);
    }

    _filterList(row) {
        return this._searchTerms.every(
            t => row.keywords.some(k => k.startsWith(t)));
    }

    _findExtensionRow(uuid) {
        return [
            ...this._userList,
            ...this._systemList,
        ].find(c => c.uuid === uuid);
    }

    _onUserExtensionsEnabledChanged() {
        const action = this.lookup_action('user-extensions-enabled');
        action.set_state(
            new GLib.Variant('b', this._shellProxy.UserExtensionsEnabled));
    }

    _onExtensionStateChanged(proxy, senderName, [uuid, newState]) {
        const extension = deserializeExtension(newState);
        let row = this._findExtensionRow(uuid);

        this._queueUpdatesCheck();

        // the extension's type changed; remove the corresponding row
        // and reset the variable to null so that we create a new row
        // below and add it to the appropriate list
        if (row && row.type !== extension.type) {
            row.get_parent().remove(row);
            row = null;
        }

        if (row) {
            if (extension.state === ExtensionState.UNINSTALLED)
                row.get_parent().remove(row);
        } else {
            this._addExtensionRow(extension);
        }

        this._syncListVisibility();
    }

    async _scanExtensions() {
        try {
            const [extensionsMap] = await this._shellProxy.ListExtensionsAsync();

            for (let uuid in extensionsMap) {
                const extension = deserializeExtension(extensionsMap[uuid]);
                this._addExtensionRow(extension);
            }
            this._extensionsLoaded();
        } catch (e) {
            if (e instanceof Gio.DBusError) {
                console.log(`Failed to connect to shell proxy: ${e}`);
                this._mainStack.visible_child_name = 'noshell';
            } else {
                throw e;
            }
        }
    }

    _addExtensionRow(extension) {
        const row = new ExtensionRow(extension);

        if (row.type === ExtensionType.PER_USER)
            this._userList.append(row);
        else
            this._systemList.append(row);
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
        this._userGroup.visible = [...this._userList].length > 1;
        this._systemGroup.visible = [...this._systemList].length > 1;

        if (this._userGroup.visible || this._systemGroup.visible)
            this._mainStack.visible_child_name = 'main';
        else
            this._mainStack.visible_child_name = 'placeholder';
    }

    _checkUpdates() {
        const nUpdates = [...this._userList].filter(c => c.hasUpdate).length;

        this._updatesBanner.title = Gettext.ngettext(
            '%d extension will be updated on next login.',
            '%d extensions will be updated on next login.',
            nUpdates).format(nUpdates);
        this._updatesBanner.revealed = nUpdates > 0;
    }

    _extensionsLoaded() {
        this._syncListVisibility();
        this._checkUpdates();
    }
});
