// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Gio, St, UPowerGlib: UPower } = imports.gi;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const { loadInterfaceXML } = imports.misc.fileUtils;

const BUS_NAME = 'org.freedesktop.UPower';
const OBJECT_PATH = '/org/freedesktop/UPower/devices/DisplayDevice';

const DisplayDeviceInterface = loadInterfaceXML('org.freedesktop.UPower.Device');
const PowerManagerProxy = Gio.DBusProxy.makeProxyWrapper(DisplayDeviceInterface);

const SHOW_BATTERY_PERCENTAGE       = 'show-battery-percentage';

var Indicator = class extends PanelMenu.SystemIndicator {
    constructor() {
        super();

        this._desktopSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.interface' });
        this._desktopSettings.connect('changed::' + SHOW_BATTERY_PERCENTAGE,
                                      this._sync.bind(this));

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
                                                                    this._sync.bind(this));
                                                this._sync();
                                            });

        this.menu.connect('open-state-changed', (menu, isOpen) => {
            this._parentMenuOpen = isOpen;
            this._sync();
        });
    }

    _sync() {
        // Do we have batteries or a UPS?
        if (!this._proxy.IsPresent) {
            // If there's no battery, then we use the power icon.
            this._indicator.icon_name = 'system-shutdown-symbolic';
            this._percentageLabel.hide();
            return;
        }

        this._indicator.icon_name = this._proxy.IconName;

        let label
        if (this._proxy.State == UPower.DeviceState.FULLY_CHARGED)
          label = _("%d\u2009%%").format(100);
        else
          label = _("%d\u2009%%").format(this._proxy.Percentage);
        this._percentageLabel.clutter_text.set_markup('<span size="smaller">' + label + '</span>');
        this._percentageLabel.visible = this._parentMenuOpen ||
                                        this._desktopSettings.get_boolean(SHOW_BATTERY_PERCENTAGE);
    }
};
