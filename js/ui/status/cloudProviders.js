// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const { CloudProviders, Clutter, Gio, GObject, St } = imports.gi;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

function _iconNameForStatus(status) {
    switch (status) {
    case CloudProviders.AccountStatus.IDLE:
        return 'emblem-ok-symbolic';
    case CloudProviders.AccountStatus.SYNCING:
        return 'emblem-synchronizing-symbolic';
    case CloudProviders.AccountStatus.ERROR:
        return 'dialog-error-symbolic';
    default:
        return '';
    }
}

var Indicator = GObject.registerClass(
class Indicator extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        this._indicator = this._addIndicator();

        this._collector = CloudProviders.Collector.dup_singleton();
        this._collector.connect('providers-changed', this._rebuild.bind(this));

        this._providers = new Map();
        this._rebuild();
    }

    _rebuild() {
        this.menu.removeAll();

        for (let [provider, { accountsChangedId }] of this._providers)
            provider.disconnect(accountsChangedId);
        this._providers.clear();

        let providers = this._collector.get_providers();
        let useName = providers.length > 1;
        providers.forEach(provider => {
            let item = new PopupMenu.PopupSubMenuMenuItem(
                useName ? provider.name : _('File synchronization'), true);
            item.icon.icon_name = 'emblem-synchronizing-symbolic';
            this.menu.addMenuItem(item);

            this._providers.set(provider, {
                item,
                accountsChangedId: provider.connect('accounts-changed', () => {
                    this._updateAccounts(provider);
                    this._sync();
                }),
            });
            this._updateAccounts(provider);
        });
        this._sync();
    }

    _updateAccounts(provider) {
        let item = this._providers.get(provider).item;

        item.menu.removeAll();

        let accounts = provider.get_accounts();
        accounts.forEach(a => {
            let accountItem = new AccountItem(a);
            accountItem.connect('status-changed', this._sync.bind(this));
            item.menu.addMenuItem(accountItem);
        });
        item.visible = accounts.length > 0;
    }

    _sync() {
        let accounts = [];

        for (let provider of this._providers.keys())
            accounts.push(...provider.get_accounts());

        let bestStatus = accounts.reduce(
            (acc, cur) => Math.max(acc, cur.get_status()),
            CloudProviders.AccountStatus.INVALID);

        this._indicator.set({
            icon_name: _iconNameForStatus(bestStatus),
            visible: bestStatus >= CloudProviders.AccountStatus.SYNCING,
        });
    }
});

const AccountItem = GObject.registerClass({
    Signals: {
        'status-changed': {},
    },
}, class AccountItem extends PopupMenu.PopupMenuItem {
    _init(account) {
        this._account = account;

        super._init(account.name);

        this._status = new St.Icon({
            icon_name: _iconNameForStatus(account.get_status()),
            style_class: 'popup-menu-icon',
            x_expand: true,
            x_align: Clutter.ActorAlign.END,
        });
        this.add_child(this._status);

        this._accountSignals = [
            this._account.connect('notify::name', () => {
                this.label.text = this._account.name;
            }),
            this._account.connect('notify::status', () => {
                this._status.icon_name =
                    _iconNameForStatus(account.get_status());
                this.emit('status-changed');
            }),
        ];

        this.connect('destroy', () => {
            this._accountSignals.forEach(id => this._account.disconnect(id));
        });
    }

    activate(event) {
        super.activate(event);

        let file = Gio.File.new_for_path(this._account.path);
        let context = global.create_app_launch_context(event.get_time(), -1);
        Gio.AppInfo.launch_default_for_uri_async(file.get_uri(), context, null,
            (o, res) => {
                try {
                    Gio.AppInfo.launch_default_for_uri_finish(res);
                } catch (e) {
                    Main.notifyError(
                        _('Failed to open %s').format(this._account.path),
                        e.message);
                }
            });
    }
});
