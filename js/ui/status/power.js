// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const { Clutter, Gio, GObject, St, UPowerGlib: UPower } = imports.gi;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const { loadInterfaceXML } = imports.misc.fileUtils;

const BUS_NAME = 'org.freedesktop.UPower';
const OBJECT_PATH = '/org/freedesktop/UPower/devices/DisplayDevice';

const DisplayDeviceInterface = loadInterfaceXML('org.freedesktop.UPower.Device');
const PowerManagerProxy = Gio.DBusProxy.makeProxyWrapper(DisplayDeviceInterface);

const SHOW_BATTERY_PERCENTAGE       = 'show-battery-percentage';

var Indicator = GObject.registerClass(
class Indicator extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        this._desktopSettings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.interface',
        });
        this._desktopSettings.connect(
            'changed::%s'.format(SHOW_BATTERY_PERCENTAGE), this._sync.bind(this));

        this._indicator = this._addIndicator();
        this._percentageLabel = new St.Label({
            y_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.add_child(this._percentageLabel);
        this.add_style_class_name('power-status');

        this._proxy = new PowerManagerProxy(Gio.DBus.system, BUS_NAME, OBJECT_PATH,
            (proxy, error) => {
                if (error) {
                    log(error.message);
                } else {
                    this._proxy.connect('g-properties-changed',
                        this._sync.bind(this));
                }
                this._sync();
            });

        this._item = new PopupMenu.PopupSubMenuMenuItem('', true);
        this._item.menu.addSettingsAction(_('Power Settings'),
            'gnome-power-panel.desktop');
        this.menu.addMenuItem(this._item);

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
    }

    _sessionUpdated() {
        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        this.menu.setSensitive(sensitive);
    }

    _getStatus() {
        let seconds = 0;

        if (this._proxy.State === UPower.DeviceState.FULLY_CHARGED)
            return _('Fully Charged');
        else if (this._proxy.State === UPower.DeviceState.CHARGING)
            seconds = this._proxy.TimeToFull;
        else if (this._proxy.State === UPower.DeviceState.DISCHARGING)
            seconds = this._proxy.TimeToEmpty;
        else if (this._proxy.State === UPower.DeviceState.PENDING_CHARGE)
            return _('Not Charging');
        // state is PENDING_DISCHARGE
        else
            return _('Estimating…');

        let time = Math.round(seconds / 60);
        if (time === 0) {
            // 0 is reported when UPower does not have enough data
            // to estimate battery life
            return _('Estimating…');
        }

        let minutes = time % 60;
        let hours = Math.floor(time / 60);

        if (this._proxy.State === UPower.DeviceState.DISCHARGING) {
            // Translators: this is <hours>:<minutes> Remaining (<percentage>)
            return _('%d\u2236%02d Remaining (%d\u2009%%)').format(
                hours, minutes, this._proxy.Percentage);
        }

        if (this._proxy.State === UPower.DeviceState.CHARGING) {
            // Translators: this is <hours>:<minutes> Until Full (<percentage>)
            return _('%d\u2236%02d Until Full (%d\u2009%%)').format(
                hours, minutes, this._proxy.Percentage);
        }

        return null;
    }

    _sync() {
        // Do we have batteries or a UPS?
        let visible = this._proxy.IsPresent;
        if (visible) {
            this._item.show();
            this._percentageLabel.visible =
                this._desktopSettings.get_boolean(SHOW_BATTERY_PERCENTAGE);
        } else {
            // If there's no battery, then we use the power icon.
            this._item.hide();
            this._indicator.icon_name = 'system-shutdown-symbolic';
            this._percentageLabel.hide();
            return;
        }

        // The icons
        let chargingState = this._proxy.State === UPower.DeviceState.CHARGING
            ? '-charging' : '';
        let fillLevel = 10 * Math.floor(this._proxy.Percentage / 10);
        const charged =
            this._proxy.State === UPower.DeviceState.FULLY_CHARGED ||
            (this._proxy.State === UPower.DeviceState.CHARGING && fillLevel === 100);
        const icon = charged
            ? 'battery-level-100-charged-symbolic'
            : 'battery-level-%d%s-symbolic'.format(fillLevel, chargingState);

        // Make sure we fall back to fallback-icon-name and not GThemedIcon's
        // default fallbacks
        let gicon = new Gio.ThemedIcon({
            name: icon,
            use_default_fallbacks: false,
        });

        this._indicator.gicon = gicon;
        this._item.icon.gicon = gicon;

        let fallbackIcon = this._proxy.IconName;
        this._indicator.fallback_icon_name = fallbackIcon;
        this._item.icon.fallback_icon_name = fallbackIcon;

        // The icon label
        const label = _('%d\u2009%%').format(this._proxy.Percentage);
        this._percentageLabel.text = label;

        // The status label
        this._item.label.text = this._getStatus();
    }
});
