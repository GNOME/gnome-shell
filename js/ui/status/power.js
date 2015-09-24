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

        this._desktopSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.interface' });
        this._desktopSettings.connect('changed::%s'.format(SHOW_BATTERY_PERCENTAGE),
                                      this._sync.bind(this));

        this._indicator = this._addIndicator();
        this._percentageLabel = new St.Label({ y_expand: true,
                                               y_align: Clutter.ActorAlign.CENTER });
        this.add_child(this._percentageLabel);
        this.add_style_class_name('power-status');

        this._proxy = new PowerManagerProxy(Gio.DBus.system, BUS_NAME, OBJECT_PATH,
                                            (proxy, error) => {
                                                if (error) {
                                                    log(error.message);
                                                    return;
                                                }
                                                this._proxy.connect('g-properties-changed',
                                                                    this._sync.bind(this));
                                                this._sync();
                                            });

        this._item = new PopupMenu.PopupSubMenuMenuItem("", true);
        this._item.menu.addSettingsAction(_("Power Settings"), 'gnome-power-panel.desktop');
        this.menu.addMenuItem(this._item);

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
    }

    _sessionUpdated() {
        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        this.menu.setSensitive(sensitive);
    }

    _getStatus() {
        if (this._proxy.State === UPower.DeviceState.FULLY_CHARGED)
            return _('Fully Charged');
        else if (this._proxy.State === UPower.DeviceState.PENDING_CHARGE)
            return _('Not Charging');
        else
            return _('%d%% Charged'.format(this._proxy.Percentage));
    }

    _sync() {
        // Do we have batteries or a UPS?
        let visible = this._proxy.IsPresent;
        if (visible) {
            this._item.show();
            this._percentageLabel.visible = this._desktopSettings.get_boolean(SHOW_BATTERY_PERCENTAGE);
        } else {
            // If there's no battery, then we use the power icon.
            this._item.hide();
            this._indicator.icon_name = 'system-shutdown-symbolic';
            this._percentageLabel.hide();
            return;
        }

        // The icons
        let chargingState = this._proxy.State == UPower.DeviceState.CHARGING
            ? '-charging' : '';
        let fillLevel = 10 * Math.floor(this._proxy.Percentage / 10);
        let icon;
        if (this._proxy.State == UPower.DeviceState.FULLY_CHARGED ||
            fillLevel === 100)
            icon = 'battery-level-100-charged-symbolic';
        else
            icon = 'battery-level-%d%s-symbolic'.format(fillLevel, chargingState);

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
        let label;
        if (this._proxy.State == UPower.DeviceState.FULLY_CHARGED)
            label = _("%d\u2009%%").format(100);
        else
            label = _("%d\u2009%%").format(this._proxy.Percentage);
        this._percentageLabel.clutter_text.set_markup('<span size="smaller">' + label + '</span>');

        // The status label
        this._item.label.text = this._getStatus();
    }
});
