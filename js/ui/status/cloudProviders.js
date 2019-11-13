// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const { CloudProviders, Clutter, GObject, St } = imports.gi;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

function _iconNameForStatus(status) {
    switch (status) {
    case CloudProviders.AccountStatus.IDLE:
        return 'emblem-ok-symbolic';
    case CloudProviders.AccountStatus.SYNCING:
        return 'emblem-synchronizing-symbolic';
    case CloudProviders.AccountStatus.ERROR:
        return 'error-symbolic';
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
        this._collector.connect('providers-changed', this._sync.bind(this));

        this._accountsSection = new PopupMenu.PopupMenuSection();

        this._item = new PopupMenu.PopupSubMenuMenuItem(
            _('File synchronization'), true);
        this._item.icon.icon_name = 'emblem-synchronizing-symbolic';

        this._item.menu.addMenuItem(this._accountsSection);
        this._item.menu.addSettingsAction(_('Online Accounts'),
            'gnome-online-accounts-panel.desktop');

        this.menu.addMenuItem(this._item);

        this._sync();
    }

    _sync() {
        this._accountsSection.removeAll();

        let accounts = this._collector.get_providers();
        accounts.forEach(a => {
            this._accountsSection.addMenuItem(new AccountItem(a));
        });

        let bestStatus = accounts.reduce(
            (acc, cur) => Math.max(acc, cur.status),
            CloudProviders.AccountStatus.INVALID);

        this._indicator.set({
            icon_name: _iconNameForStatus(bestStatus),
            visible: bestStatus >= CloudProviders.AccountStatus.SYNCING,
        });
        this._item.visible = accounts.length > 0;
    }
});

const AccountItem = GObject.registerClass(
class AccountItem extends PopupMenu.PopupImageMenuItem {
    _init(account) {
        this._account = account;

        super._init(account.name, account.icon);

        this._status = new St.Icon({
            icon_name: _iconNameForStatus(account.status),
            x_expand: true,
            x_align: Clutter.ActorAlign.END,
        });
        this.add_child(this._status);

        this._accountSignals = [
            this._account.connect('notify::name', () => {
                this.label.text = this._account.name;
            }),
            this._account.connect('notify::icon', () => {
                this.setIcon(this._account.icon);
            }),
            this._account.connect('notify::status', () => {
                this._status.icon_name = _iconNameForStatus(account.status);
            }),
        ];

        this.connect('destroy', () => {
            this._accountSignals.forEach(id => this._account.disconnect(id));
        });
    }
});
