// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const {Atk, Clutter, Gio, GLib, GObject, Meta, Shell, St, UPowerGlib: UPower} = imports.gi;

const SystemActions = imports.misc.systemActions;
const Main = imports.ui.main;
const PopupMenu = imports.ui.popupMenu;
const {PopupAnimation} = imports.ui.boxpointer;

const {QuickSettingsItem, QuickToggle, SystemIndicator} = imports.ui.quickSettings;
const {loadInterfaceXML} = imports.misc.fileUtils;

const BUS_NAME = 'org.freedesktop.UPower';
const OBJECT_PATH = '/org/freedesktop/UPower/devices/DisplayDevice';

const DisplayDeviceInterface = loadInterfaceXML('org.freedesktop.UPower.Device');
const PowerManagerProxy = Gio.DBusProxy.makeProxyWrapper(DisplayDeviceInterface);

const SHOW_BATTERY_PERCENTAGE = 'show-battery-percentage';

const PowerToggle = GObject.registerClass({
    Properties: {
        'fallback-icon-name': GObject.ParamSpec.string('fallback-icon-name', '', '',
            GObject.ParamFlags.READWRITE,
            ''),
    },
}, class PowerToggle extends QuickToggle {
    _init() {
        super._init({
            accessible_role: Atk.Role.PUSH_BUTTON,
        });

        this.add_style_class_name('power-item');

        this._proxy = new PowerManagerProxy(Gio.DBus.system, BUS_NAME, OBJECT_PATH,
            (proxy, error) => {
                if (error)
                    console.error(error.message);
                else
                    this._proxy.connect('g-properties-changed', () => this._sync());
                this._sync();
            });

        this.bind_property('fallback-icon-name',
            this._icon, 'fallback-icon-name',
            GObject.BindingFlags.SYNC_CREATE);

        this.connect('clicked', () => {
            const app = Shell.AppSystem.get_default().lookup_app('gnome-power-panel.desktop');
            Main.overview.hide();
            Main.panel.closeQuickSettings();
            app.activate();
        });

        Main.sessionMode.connect('updated', () => this._sessionUpdated());
        this._sessionUpdated();
        this._sync();
    }

    _sessionUpdated() {
        this.reactive = Main.sessionMode.allowSettings;
    }

    _sync() {
        // Do we have batteries or a UPS?
        this.visible = this._proxy.IsPresent;
        if (!this.visible)
            return;

        // The icons
        let chargingState = this._proxy.State === UPower.DeviceState.CHARGING
            ? '-charging' : '';
        let fillLevel = 10 * Math.floor(this._proxy.Percentage / 10);
        const charged =
            this._proxy.State === UPower.DeviceState.FULLY_CHARGED ||
            (this._proxy.State === UPower.DeviceState.CHARGING && fillLevel === 100);
        const icon = charged
            ? 'battery-level-100-charged-symbolic'
            : `battery-level-${fillLevel}${chargingState}-symbolic`;

        // Make sure we fall back to fallback-icon-name and not GThemedIcon's
        // default fallbacks
        const gicon = new Gio.ThemedIcon({
            name: icon,
            use_default_fallbacks: false,
        });

        this.set({
            title: _('%d\u2009%%').format(this._proxy.Percentage),
            fallback_icon_name: this._proxy.IconName,
            gicon,
        });
    }
});

const ScreenshotItem = GObject.registerClass(
class ScreenshotItem extends QuickSettingsItem {
    _init() {
        super._init({
            style_class: 'icon-button',
            can_focus: true,
            icon_name: 'screenshooter-symbolic',
            visible: !Main.sessionMode.isGreeter,
            accessible_name: _('Take Screenshot'),
        });

        this.connect('clicked', () => {
            const topMenu = Main.panel.statusArea.quickSettings.menu;
            const laters = global.compositor.get_laters();
            laters.add(Meta.LaterType.BEFORE_REDRAW, () => {
                Main.screenshotUI.open().catch(logError);
                return GLib.SOURCE_REMOVE;
            });
            topMenu.close(PopupAnimation.NONE);
        });
    }
});

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
            icon_name: 'system-shutdown-symbolic',
            accessible_name: _('Power Off Menu'),
        });

        this._systemActions = new SystemActions.getDefault();
        this._items = [];

        this.menu.setHeader('system-shutdown-symbolic', C_('title', 'Power Off'));

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

        this._addSystemAction(_('Log Out…'), 'can-logout', () => {
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
            icon_name: 'system-lock-screen-symbolic',
            accessible_name: C_('action', 'Lock Screen'),
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

        this._powerToggle = new PowerToggle();
        this.child.add_child(this._powerToggle);

        this._laptopSpacer = new Clutter.Actor({x_expand: true});
        this._powerToggle.bind_property('visible',
            this._laptopSpacer, 'visible',
            GObject.BindingFlags.SYNC_CREATE);
        this.child.add_child(this._laptopSpacer);

        const screenshotItem = new ScreenshotItem();
        this.child.add_child(screenshotItem);

        const settingsItem = new SettingsItem();
        this.child.add_child(settingsItem);

        this._desktopSpacer = new Clutter.Actor({x_expand: true});
        this._powerToggle.bind_property('visible',
            this._desktopSpacer, 'visible',
            GObject.BindingFlags.INVERT_BOOLEAN |
            GObject.BindingFlags.SYNC_CREATE);
        this.child.add_child(this._desktopSpacer);

        const lockItem = new LockItem();
        this.child.add_child(lockItem);

        const shutdownItem = new ShutdownItem();
        this.child.add_child(shutdownItem);

        this.menu = shutdownItem.menu;
    }

    get powerToggle() {
        return this._powerToggle;
    }
});

var Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this._desktopSettings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.interface',
        });
        this._desktopSettings.connectObject(
            `changed::${SHOW_BATTERY_PERCENTAGE}`, () => this._sync(), this);

        this._indicator = this._addIndicator();
        this._percentageLabel = new St.Label({
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(this._percentageLabel);
        this.add_style_class_name('power-status');

        this._systemItem = new SystemItem();

        const {powerToggle} = this._systemItem;

        powerToggle.bind_property('title',
            this._percentageLabel, 'text',
            GObject.BindingFlags.SYNC_CREATE);

        powerToggle.connectObject(
            'notify::visible', () => this._sync(),
            'notify::gicon', () => this._sync(),
            'notify::fallback-icon-name', () => this._sync(),
            this);

        this.quickSettingsItems.push(this._systemItem);

        this._sync();
    }

    _sync() {
        const {powerToggle} = this._systemItem;
        if (powerToggle.visible) {
            this._indicator.set({
                gicon: powerToggle.gicon,
                fallback_icon_name: powerToggle.fallback_icon_name,
            });
            this._percentageLabel.visible =
                this._desktopSettings.get_boolean(SHOW_BATTERY_PERCENTAGE);
        } else {
            // If there's no battery, then we use the power icon.
            this._indicator.icon_name = 'system-shutdown-symbolic';
            this._percentageLabel.hide();
        }
    }
});
