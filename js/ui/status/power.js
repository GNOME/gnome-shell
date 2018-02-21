// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Clutter = imports.gi.Clutter;
const St = imports.gi.St;
const Lang = imports.lang;
const UPower = imports.gi.UPowerGlib;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const BUS_NAME = 'org.freedesktop.UPower';
const OBJECT_PATH = '/org/freedesktop/UPower/devices/DisplayDevice';

const DisplayDeviceInterface = '<node> \
<interface name="org.freedesktop.UPower.Device"> \
  <property name="Type" type="u" access="read"/> \
  <property name="State" type="u" access="read"/> \
  <property name="Percentage" type="d" access="read"/> \
  <property name="TimeToEmpty" type="x" access="read"/> \
  <property name="TimeToFull" type="x" access="read"/> \
  <property name="IsPresent" type="b" access="read"/> \
  <property name="IconName" type="s" access="read"/> \
</interface> \
</node>';

const PowerManagerProxy = Gio.DBusProxy.makeProxyWrapper(DisplayDeviceInterface);

const SHOW_BATTERY_PERCENTAGE       = 'show-battery-percentage';

var Indicator = new Lang.Class({
    Name: 'PowerIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init() {
        this.parent();

        this._desktopSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.interface' });
        this._desktopSettings.connect('changed::' + SHOW_BATTERY_PERCENTAGE,
                                      Lang.bind(this, this._sync));

        this._indicator = this._addIndicator();
        this._percentageLabel = new St.Label({ y_expand: true,
                                               y_align: Clutter.ActorAlign.CENTER });
        this.indicators.add(this._percentageLabel, { expand: true, y_fill: true });
        this.indicators.add_style_class_name('power-status');

        this._proxy = new PowerManagerProxy(Gio.DBus.system, BUS_NAME, OBJECT_PATH,
                                            (proxy, error) => {
                                                if (error) {
                                                    log(error.message);
                                                    return;
                                                }
                                                this._proxy.connect('g-properties-changed',
                                                                    Lang.bind(this, this._sync));
                                                this._sync();
                                            });

        this._item = new PopupMenu.PopupSubMenuMenuItem("", true);
        this._item.menu.addSettingsAction(_("Power Settings"), 'gnome-power-panel.desktop');
        this.menu.addMenuItem(this._item);

        Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
        this._sessionUpdated();
    },

    _sessionUpdated() {
        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        this.menu.setSensitive(sensitive);
    },

    _getStatus() {
        let seconds = 0;

        if (this._proxy.State == UPower.DeviceState.FULLY_CHARGED)
            return _("Fully Charged");
        else if (this._proxy.State == UPower.DeviceState.CHARGING)
            seconds = this._proxy.TimeToFull;
        else if (this._proxy.State == UPower.DeviceState.DISCHARGING)
            seconds = this._proxy.TimeToEmpty;
        // state is one of PENDING_CHARGING, PENDING_DISCHARGING
        else
            return _("Estimating…");

        let time = Math.round(seconds / 60);
        if (time == 0) {
            // 0 is reported when UPower does not have enough data
            // to estimate battery life
            return _("Estimating…");
        }

        let minutes = time % 60;
        let hours = Math.floor(time / 60);

        if (this._proxy.State == UPower.DeviceState.DISCHARGING) {
            // Translators: this is <hours>:<minutes> Remaining (<percentage>)
            return _("%d\u2236%02d Remaining (%d\u2009%%)").format(hours, minutes, this._proxy.Percentage);
        }

        if (this._proxy.State == UPower.DeviceState.CHARGING) {
            // Translators: this is <hours>:<minutes> Until Full (<percentage>)
            return _("%d\u2236%02d Until Full (%d\u2009%%)").format(hours, minutes, this._proxy.Percentage);
        }

        return null;
    },

    _sync() {
        // Do we have batteries or a UPS?
        let visible = this._proxy.IsPresent;
        if (visible) {
            this._item.actor.show();
            this._percentageLabel.visible = this._desktopSettings.get_boolean(SHOW_BATTERY_PERCENTAGE);
        } else {
            // If there's no battery, then we use the power icon.
            this._item.actor.hide();
            this._indicator.icon_name = 'system-shutdown-symbolic';
            this._percentageLabel.hide();
            return;
        }

        // The icons
        let icon = this._proxy.IconName;
        this._indicator.icon_name = icon;
        this._item.icon.icon_name = icon;

        // The icon label
        let label
        if (this._proxy.State == UPower.DeviceState.FULLY_CHARGED)
          label = _("%d\u2009%%").format(100);
        else
          label = _("%d\u2009%%").format(this._proxy.Percentage);
        this._percentageLabel.clutter_text.set_markup('<span size="smaller">' + label + '</span>');

        // The status label
        this._item.label.text = this._getStatus();
    },
});
