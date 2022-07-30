// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const {Clutter, GObject, Shell, St} = imports.gi;

const SystemActions = imports.misc.systemActions;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;

const {QuickSettingsItem, SystemIndicator} = imports.ui.quickSettings;

const SettingsItem = GObject.registerClass(
class SettingsItem extends QuickSettingsItem {
    _init() {
        super._init({
            style_class: 'icon-button',
            can_focus: true,
            child: new St.Icon(),
        });

        this._settingsApp = Shell.AppSystem.get_default().lookup_app(
            'org.gnome.Settings.desktop');

        if (!this._settingsApp)
            console.warn('Missing required core component Settings, expect trouble…');

        this.child.gicon = this._settingsApp?.get_icon() ?? null;
        this.accessible_name = this._settingsApp?.get_name() ?? null;

        this.connect('clicked', () => {
            Main.overview.hide();
            Main.panel.closeQuickSettings();
            this._settingsApp.activate();
        });

        Main.sessionMode.connectObject('updated', () => this._sync(), this);
        this._sync();
    }

    _sync() {
        this.visible =
            this._settingsApp != null && Main.sessionMode.allowSettings;
    }
});

const ShutdownItem = GObject.registerClass(
class ShutdownItem extends QuickSettingsItem {
    _init() {
        super._init({
            style_class: 'icon-button',
            hasMenu: true,
            canFocus: true,
            child: new St.Icon({
                icon_name: 'system-shutdown-symbolic',
            }),
            accessible_name: _('Power Off Menu'),
        });

        this._systemActions = new SystemActions.getDefault();
        this._items = [];

        this.menu.setHeader('system-shutdown-symbolic', 'Power Off');

        this._addSystemAction(_('Suspend'), 'can-suspend', () => {
            this._systemActions.activateSuspend();
            Main.panel.closeQuickSettings();
        });

        this._addSystemAction(_('Restart…'), 'can-restart', () => {
            this._systemActions.activateRestart();
            Main.panel.closeQuickSettings();
        });

        this._addSystemAction(_('Power Off…'), 'can-power-off', () => {
            this._systemActions.activatePowerOff();
            Main.panel.closeQuickSettings();
        });

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        this._addSystemAction(_('Log Out'), 'can-logout', () => {
            this._systemActions.activateLogout();
            Main.panel.closeQuickSettings();
        });

        this._addSystemAction(_('Switch User…'), 'can-switch-user', () => {
            this._systemActions.activateSwitchUser();
            Main.panel.closeQuickSettings();
        });

        // Whether shutdown is available or not depends on both lockdown
        // settings (disable-log-out) and Polkit policy - the latter doesn't
        // notify, so we update the item each time we become visible or
        // the lockdown setting changes, which should be close enough.
        this.connect('notify::mapped', () => {
            if (!this.mapped)
                return;

            this._systemActions.forceUpdate();
        });

        this.connect('clicked', () => this.menu.open());
        this.connect('popup-menu', () => this.menu.open());
    }

    _addSystemAction(label, propName, callback) {
        const item = this.menu.addAction(label, callback);
        this._items.push(item);

        this._systemActions.bind_property(propName,
            item, 'visible',
            GObject.BindingFlags.DEFAULT | GObject.BindingFlags.SYNC_CREATE);
        item.connect('notify::visible', () => this._sync());
    }

    _sync() {
        this.visible = this._items.some(i => i.visible);
    }
});

const LockItem = GObject.registerClass(
class LockItem extends QuickSettingsItem {
    _init() {
        this._systemActions = new SystemActions.getDefault();

        super._init({
            style_class: 'icon-button',
            can_focus: true,
            child: new St.Icon({
                icon_name: 'system-lock-screen-symbolic',
            }),
            accessible_name: _('Lock Screen'),
        });

        this._systemActions.bind_property('can-lock-screen',
            this, 'visible',
            GObject.BindingFlags.DEFAULT |
            GObject.BindingFlags.SYNC_CREATE);

        this.connect('clicked',
            () => this._systemActions.activateLockScreen());
    }
});


const SystemItem = GObject.registerClass(
class SystemItem extends QuickSettingsItem {
    _init() {
        super._init({
            style_class: 'quick-settings-system-item',
            reactive: false,
        });

        this.child = new St.BoxLayout();

        // spacer
        this.child.add_child(new Clutter.Actor({x_expand: true}));

        const settingsItem = new SettingsItem();
        this.child.add_child(settingsItem);

        const lockItem = new LockItem();
        this.child.add_child(lockItem);

        const shutdownItem = new ShutdownItem();
        this.child.add_child(shutdownItem);

        this.menu = shutdownItem.menu;
    }
});

var Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        const item = new SystemItem();

        this.quickSettingsItems.push(item);
    }
});
