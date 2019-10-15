// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const { Clutter, GLib, GObject, Shell, St } = imports.gi;

const BoxPointer = imports.ui.boxpointer;
const SystemActions = imports.misc.systemActions;
const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;


var Indicator = class extends PanelMenu.SystemIndicator {
    constructor() {
        super();

        this._systemActions = new SystemActions.getDefault();

        this._createSubMenu();

        this._loginScreenItem.connect('notify::visible',
                                      () => this._updateMultiUser());
        this._logoutItem.connect('notify::visible',
                                 () => this._updateMultiUser());
        // Whether shutdown is available or not depends on both lockdown
        // settings (disable-log-out) and Polkit policy - the latter doesn't
        // notify, so we update the menu item each time the menu opens or
        // the lockdown setting changes, which should be close enough.
        this.menu.connect('open-state-changed', (menu, open) => {
            if (!open)
                return;

            this._systemActions.forceUpdate();
        });
        this._updateMultiUser();

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
    }

    _sessionUpdated() {
        this._settingsItem.visible = Main.sessionMode.allowSettings;
    }

    _updateMultiUser() {
        let hasSwitchUser = this._loginScreenItem.visible;
        let hasLogout = this._logoutItem.visible;

        this._switchUserSubMenu.visible = hasSwitchUser || hasLogout;
    }

    _createSubMenu() {
        let bindFlags = GObject.BindingFlags.DEFAULT | GObject.BindingFlags.SYNC_CREATE;
        let item;

        item = new PopupMenu.PopupImageMenuItem(_('Lock Screen Rotation'),
            this._systemActions.orientation_lock_icon);
        item.connect('activate', () => {
            this.menu.itemActivated(BoxPointer.PopupAnimation.NONE);
            this._systemActions.activateLockOrientation();
        });
        this.menu.addMenuItem(item);
        this._orientationLockItem = item;
        this._systemActions.bind_property('can-lock-orientation',
                                          this._orientationLockItem,
                                          'visible',
                                          bindFlags);
        this._systemActions.connect('notify::orientation-lock-icon', () => {
            let iconName = this._systemActions.orientation_lock_icon;
            this._orientationLockItem.setIcon(iconName);
        });

        let app = this._settingsApp = Shell.AppSystem.get_default().lookup_app(
            'gnome-control-center.desktop'
        );
        if (app) {
            let [icon, name] = [app.app_info.get_icon().names[0],
                                app.get_name()];
            item = new PopupMenu.PopupImageMenuItem(name, icon);
            item.connect('activate', () => {
                this.menu.itemActivated(BoxPointer.PopupAnimation.NONE);
                Main.overview.hide();
                this._settingsApp.activate();
            });
            this.menu.addMenuItem(item);
            this._settingsItem = item;
        } else {
            log('Missing required core component Settings, expect trouble…');
            this._settingsItem = new St.Widget();
        }

        item = new PopupMenu.PopupImageMenuItem(_('Lock'), 'changes-prevent-symbolic');
        item.connect('activate', () => {
            this.menu.itemActivated(BoxPointer.PopupAnimation.NONE);
            this._systemActions.activateLockScreen();
        });
        this.menu.addMenuItem(item);
        this._lockScreenItem = item;
        this._systemActions.bind_property('can-lock-screen',
                                          this._lockScreenItem,
                                          'visible',
                                          bindFlags);

        this._switchUserSubMenu = new PopupMenu.PopupSubMenuMenuItem(
            _('Power Off / Log Out'), true);
        this._switchUserSubMenu.icon.icon_name = 'system-shutdown-symbolic';

        item = new PopupMenu.PopupMenuItem(_("Log Out"));
        item.connect('activate', () => {
            this.menu.itemActivated(BoxPointer.PopupAnimation.NONE);
            this._systemActions.activateLogout();
        });
        this._switchUserSubMenu.menu.addMenuItem(item);
        this._logoutItem = item;
        this._systemActions.bind_property('can-logout',
                                          this._logoutItem,
                                          'visible',
                                          bindFlags);

        item = new PopupMenu.PopupMenuItem(_("Switch User…"));
        item.connect('activate', () => {
            this.menu.itemActivated(BoxPointer.PopupAnimation.NONE);
            this._systemActions.activateSwitchUser();
        });
        this._switchUserSubMenu.menu.addMenuItem(item);
        this._loginScreenItem = item;
        this._systemActions.bind_property('can-switch-user',
                                          this._loginScreenItem,
                                          'visible',
                                          bindFlags);

        this._switchUserSubMenu.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        item = new PopupMenu.PopupMenuItem(_("Suspend"));
        item.connect('activate', () => {
            this.menu.itemActivated(BoxPointer.PopupAnimation.NONE);
            this._systemActions.activateSuspend();
        });
        this._switchUserSubMenu.menu.addMenuItem(item);
        this._suspendItem = item;
        this._systemActions.bind_property('can-suspend',
                                          this._suspendItem,
                                          'visible',
                                          bindFlags);

        item = new PopupMenu.PopupMenuItem(_("Power Off…"));
        item.connect('activate', () => {
            this.menu.itemActivated(BoxPointer.PopupAnimation.NONE);
            this._systemActions.activatePowerOff();
        });
        this._switchUserSubMenu.menu.addMenuItem(item);
        this._powerOffItem = item;
        this._systemActions.bind_property('can-power-off',
                                          this._powerOffItem,
                                          'visible',
                                          bindFlags);

        this.menu.addMenuItem(this._switchUserSubMenu);
    }

    _onSettingsClicked() {
        this.menu.itemActivated();
    }
};
