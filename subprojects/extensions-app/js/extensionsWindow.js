import Adw from 'gi://Adw?version=1';
import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import Shew from 'gi://Shew';

const Package = imports.package;
import * as Gettext from 'gettext';

import * as Config from './misc/config.js';
import {ExtensionRow} from './extensionRow.js';

Gio._promisify(Adw.AlertDialog.prototype, 'choose');
Gio._promisify(Gio.DBusConnection.prototype, 'call');
Gio._promisify(Shew.WindowExporter.prototype, 'export');

export const ExtensionsWindow = GObject.registerClass({
    GTypeName: 'ExtensionsWindow',
    Template: 'resource:///org/gnome/Extensions/ui/extensions-window.ui',
    InternalChildren: [
        'sortModel',
        'searchFilter',
        'userListModel',
        'systemListModel',
        'searchListModel',
        'userGroup',
        'systemGroup',
        'searchGroup',
        'mainStack',
        'searchBar',
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

        const settings = new Gio.Settings({
            schema_id: 'org.gnome.Extensions',
        });

        settings.bind('window-width',
            this, 'default-width',
            Gio.SettingsBindFlags.DEFAULT);
        settings.bind('window-height',
            this, 'default-height',
            Gio.SettingsBindFlags.DEFAULT);
        settings.bind('window-maximized',
            this, 'maximized',
            Gio.SettingsBindFlags.DEFAULT);

        this._searchEntry.connect('search-changed',
            () => (this._searchFilter.search = this._searchEntry.text));
        this._searchBar.connect('notify::search-mode-enabled',
            () => this._syncVisiblePage());
        this._searchListModel.connect('notify::n-items',
            () => this._syncVisiblePage());

        this._userGroup.connect('notify::visible', () => this._syncVisiblePage());
        this._systemGroup.connect('notify::visible', () => this._syncVisiblePage());

        const {extensionManager} = this.application;
        extensionManager.connect('notify::failed',
            () => this._syncVisiblePage());
        extensionManager.connect('notify::n-updates',
            () => this._checkUpdates());
        extensionManager.connect('notify::user-extensions-enabled',
            this._onUserExtensionsEnabledChanged.bind(this));
        this._onUserExtensionsEnabledChanged();

        this._sortModel.model = extensionManager.extensions;

        this._userGroup.bind_model(this._userListModel,
            extension => new ExtensionRow(extension));
        this._systemGroup.bind_model(this._systemListModel,
            extension => new ExtensionRow(extension));
        this._searchGroup.bind_model(this._searchListModel,
            extension => new ExtensionRow(extension));

        extensionManager.connect('extensions-loaded',
            () => this._extensionsLoaded());
    }

    async uninstall(extension) {
        const dialog = new Adw.AlertDialog({
            heading: _('Remove “%s”?').format(extension.name),
            body: _('If you remove the extension, you need to return to download it if you want to enable it again'),
        });

        dialog.add_response('cancel', _('_Cancel'));
        dialog.add_response('remove', _('_Remove'));

        dialog.set_response_appearance('remove',
            Adw.ResponseAppearance.DESTRUCTIVE);

        const {extensionManager} = this.application;
        const response = await dialog.choose(this, null);
        if (response === 'remove')
            extensionManager.uninstallExtension(extension.uuid);
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
        const [version] = Package.version.split(' ');
        const aboutDialog = Adw.AboutDialog.new_from_appdata(
            '/org/gnome/Extensions/metainfo.xml', version);

        aboutDialog.set({
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
            version: Package.version,
        });
        aboutDialog.present(this);
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

    _onUserExtensionsEnabledChanged() {
        const {userExtensionsEnabled} = this.application.extensionManager;
        const action = this.lookup_action('user-extensions-enabled');
        action.set_state(new GLib.Variant('b', userExtensionsEnabled));
    }

    _syncVisiblePage() {
        const {extensionManager} = this.application;
        const {searchModeEnabled} = this._searchBar;

        if (extensionManager.failed)
            this._mainStack.visible_child_name = 'noshell';
        else if (searchModeEnabled && this._searchListModel.get_n_items() > 0)
            this._mainStack.visible_child_name = 'search';
        else if (searchModeEnabled)
            this._mainStack.visible_child_name = 'noresults';
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
        this._syncVisiblePage();
        this._checkUpdates();
    }
});
