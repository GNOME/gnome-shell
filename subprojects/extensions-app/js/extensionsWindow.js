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
                    const {extensionManager} = this.application;
                    extensionManager.userExtensionsEnabled = state.get_boolean();
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

        const {extensionManager} = this.application;
        extensionManager.connect('notify::failed',
            () => this._syncVisiblePage());
        extensionManager.connect('notify::n-updates',
            () => this._checkUpdates());
        extensionManager.connect('notify::user-extensions-enabled',
            this._onUserExtensionsEnabledChanged.bind(this));
        this._onUserExtensionsEnabledChanged();

        extensionManager.connect('extension-added',
            (mgr, extension) => this._addExtensionRow(extension));
        extensionManager.connect('extension-removed',
            (mgr, extension) => this._removeExtensionRow(extension));
        extensionManager.connect('extension-changed',
            (mgr, extension) => {
                const row = this._findExtensionRow(extension);
                const isUser = row?.get_parent() === this._userList;
                if (extension.isUser !== isUser) {
                    this._removeExtensionRow(extension);
                    this._addExtensionRow(extension);
                }
            });

        extensionManager.connect('extensions-loaded',
            () => this._extensionsLoaded());
    }

    uninstall(extension) {
        const dialog = new Gtk.MessageDialog({
            transient_for: this,
            modal: true,
            text: _('Remove “%s”?').format(extension.name),
            secondary_text: _('If you remove the extension, you need to return to download it if you want to enable it again'),
        });

        dialog.add_button(_('_Cancel'), Gtk.ResponseType.CANCEL);
        dialog.add_button(_('_Remove'), Gtk.ResponseType.ACCEPT)
            .get_style_context().add_class('destructive-action');

        dialog.connect('response', (dlg, response) => {
            const {extensionManager} = this.application;

            if (response === Gtk.ResponseType.ACCEPT)
                extensionManager.uninstallExtension(extension.uuid);
            dialog.destroy();
        });
        dialog.present();
    }

    async openPrefs(extension) {
        if (!this._exportedHandle) {
            try {
                this._exportedHandle = await this._exporter.export();
            } catch (e) {
                console.warn(`Failed to export window: ${e.message}`);
            }
        }

        const {extensionManager} = this.application;
        extensionManager.openExtensionPrefs(extension.uuid, this._exportedHandle);
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
        const {name: name1} = row1.extension;
        const {name: name2} = row2.extension;
        return name1.localeCompare(name2);
    }

    _filterList(row) {
        const {keywords} = row.extension;
        return this._searchTerms.every(
            t => keywords.some(k => k.startsWith(t)));
    }

    _findExtensionRow(extension) {
        return [
            ...this._userList,
            ...this._systemList,
        ].find(c => c.extension === extension);
    }

    _onUserExtensionsEnabledChanged() {
        const {userExtensionsEnabled} = this.application.extensionManager;
        const action = this.lookup_action('user-extensions-enabled');
        action.set_state(new GLib.Variant('b', userExtensionsEnabled));
    }

    _addExtensionRow(extension) {
        const row = new ExtensionRow(extension);

        if (extension.isUser)
            this._userList.append(row);
        else
            this._systemList.append(row);

        this._syncListVisibility();
    }

    _removeExtensionRow(extension) {
        const row = this._findExtensionRow(extension);
        if (row)
            row.get_parent().remove(row);
        this._syncListVisibility();
    }

    _syncListVisibility() {
        this._userGroup.visible = [...this._userList].length > 1;
        this._systemGroup.visible = [...this._systemList].length > 1;

        this._syncVisiblePage();
    }

    _syncVisiblePage() {
        const {extensionManager} = this.application;

        if (extensionManager.failed)
            this._mainStack.visible_child_name = 'noshell';
        else if (this._userGroup.visible || this._systemGroup.visible)
            this._mainStack.visible_child_name = 'main';
        else
            this._mainStack.visible_child_name = 'placeholder';
    }

    _checkUpdates() {
        const {nUpdates} = this.application.extensionManager;

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
