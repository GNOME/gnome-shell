// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const {Atk, Clutter, Gio, GObject, Shell, St, UPowerGlib: UPower} = imports.gi;

const Main = imports.ui.main;
const {QuickToggle, SystemIndicator} = imports.ui.quickSettings;

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
            label: _('%d\u2009%%').format(this._proxy.Percentage),
            fallback_icon_name: this._proxy.IconName,
            gicon,
        });
    }
});

var Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this._desktopSettings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.interface',
        });
        this._desktopSettings.connect(
            `changed::${SHOW_BATTERY_PERCENTAGE}`, () => this._sync());

        this._indicator = this._addIndicator();
        this._percentageLabel = new St.Label({
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(this._percentageLabel);
        this.add_style_class_name('power-status');

        this._powerToggle = new PowerToggle();

        this._powerToggle.bind_property('label',
            this._percentageLabel, 'text',
            GObject.BindingFlags.SYNC_CREATE);

        this._powerToggle.connectObject(
            'notify::visible', () => this._sync(),
            'notify::gicon', () => this._sync(),
            'notify::fallback-icon-name', () => this._sync(),
            this);

        this.quickSettingsItems.push(this._powerToggle);

        this._sync();
    }

    _sync() {
        if (this._powerToggle.visible) {
            this._indicator.set({
                gicon: this._powerToggle.gicon,
                fallback_icon_name: this._powerToggle.fallback_icon_name,
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
