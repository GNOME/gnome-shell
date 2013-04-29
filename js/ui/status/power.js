// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;
const St = imports.gi.St;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const BUS_NAME = 'org.gnome.SettingsDaemon.Power';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Power';

const UPDeviceType = {
    UNKNOWN: 0,
    AC_POWER: 1,
    BATTERY: 2,
    UPS: 3,
    MONITOR: 4,
    MOUSE: 5,
    KEYBOARD: 6,
    PDA: 7,
    PHONE: 8,
    MEDIA_PLAYER: 9,
    TABLET: 10,
    COMPUTER: 11
};

const UPDeviceState = {
    UNKNOWN: 0,
    CHARGING: 1,
    DISCHARGING: 2,
    EMPTY: 3,
    FULLY_CHARGED: 4,
    PENDING_CHARGE: 5,
    PENDING_DISCHARGE: 6
};

const PowerManagerInterface = <interface name="org.gnome.SettingsDaemon.Power">
<method name="GetDevices">
    <arg type="a(susdut)" direction="out" />
</method>
<method name="GetPrimaryDevice">
    <arg type="(susdut)" direction="out" />
</method>
<property name="Icon" type="s" access="read" />
</interface>;

const PowerManagerProxy = Gio.DBusProxy.makeProxyWrapper(PowerManagerInterface);

const Indicator = new Lang.Class({
    Name: 'PowerIndicator',
    Extends: PanelMenu.SystemStatusButton,

    _init: function() {
        this.parent('battery-missing-symbolic', _("Battery"));

        this._proxy = new PowerManagerProxy(Gio.DBus.session, BUS_NAME, OBJECT_PATH,
                                           Lang.bind(this, function(proxy, error) {
                                               if (error) {
                                                   log(error.message);
                                                   return;
                                               }
                                               this._proxy.connect('g-properties-changed',
                                                                   Lang.bind(this, this._devicesChanged));
                                               this._devicesChanged();
                                           }));

        this._deviceItems = [ ];
        this._hasPrimary = false;
        this._primaryDeviceId = null;

        this._batteryItem = new PopupMenu.PopupMenuItem('', { reactive: false });
        this._primaryPercentage = new St.Label({ style_class: 'popup-battery-percentage' });
        this._batteryItem.addActor(this._primaryPercentage, { align: St.Align.END });
        this.menu.addMenuItem(this._batteryItem);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this._otherDevicePosition = 2;
    },

    _readPrimaryDevice: function() {
        this._proxy.GetPrimaryDeviceRemote(Lang.bind(this, function(result, error) {
            if (error) {
                this._hasPrimary = false;
                this._primaryDeviceId = null;
                this._batteryItem.actor.hide();
                return;
            }
            let [[device_id, device_type, icon, percentage, state, seconds]] = result;
            if (device_type == UPDeviceType.BATTERY) {
                this._hasPrimary = true;
                let time = Math.round(seconds / 60);
                if (time == 0) {
                    // 0 is reported when UPower does not have enough data
                    // to estimate battery life
                    this._batteryItem.label.text = _("Estimating…");
                } else {
                    let minutes = time % 60;
                    let hours = Math.floor(time / 60);
                    let timestring;
                    if (time >= 60) {
                        if (minutes == 0) {
                            timestring = ngettext("%d hour remaining", "%d hours remaining", hours).format(hours);
                        } else {
                            /* TRANSLATORS: this is a time string, as in "%d hours %d minutes remaining" */
                            let template = _("%d %s %d %s remaining");

                            timestring = template.format (hours, ngettext("hour", "hours", hours), minutes, ngettext("minute", "minutes", minutes));
                        }
                    } else
                        timestring = ngettext("%d minute remaining", "%d minutes remaining", minutes).format(minutes);
                    this._batteryItem.label.text = timestring;
                }
                this._primaryPercentage.text = C_("percent of battery remaining", "%d%%").format(Math.round(percentage));
                this._batteryItem.actor.show();
            } else {
                this._hasPrimary = false;
                this._batteryItem.actor.hide();
            }

            this._primaryDeviceId = device_id;
        }));
    },

    _readOtherDevices: function() {
        this._proxy.GetDevicesRemote(Lang.bind(this, function(result, error) {
            this._deviceItems.forEach(function(i) { i.destroy(); });
            this._deviceItems = [];

            if (error) {
                return;
            }

            let position = 0;
            let [devices] = result;
            for (let i = 0; i < devices.length; i++) {
                let [device_id, device_type] = devices[i];
                if (device_type == UPDeviceType.AC_POWER || device_id == this._primaryDeviceId)
                    continue;

                let item = new DeviceItem (devices[i]);
                this._deviceItems.push(item);
                this.menu.addMenuItem(item, this._otherDevicePosition + position);
                position++;
            }
        }));
    },

    _syncIcon: function() {
        let icon = this._proxy.Icon;
        let hasIcon = false;

        if (icon) {
            let gicon = Gio.icon_new_for_string(icon);
            this.setGIcon(gicon);
            hasIcon = true;
        }
        this.mainIcon.visible = hasIcon;
        this.actor.visible = hasIcon;
    },

    _devicesChanged: function() {
        this._syncIcon();
        this._readPrimaryDevice();
        this._readOtherDevices();
    }
});

const DeviceItem = new Lang.Class({
    Name: 'DeviceItem',
    Extends: PopupMenu.PopupBaseMenuItem,

    _init: function(device) {
        this.parent({ reactive: false });

        let [device_id, device_type, icon, percentage, state, time] = device;

        this._label = new St.Label({ text: this._deviceTypeToString(device_type) });
        this.addActor(this._label);

        let percentLabel = new St.Label({ text: C_("percent of battery remaining", "%d%%").format(Math.round(percentage)),
                                          style_class: 'popup-status-menu-item popup-battery-percentage' });
        this.addActor(percentLabel, { align: St.Align.END });
        //FIXME: ideally we would like to expose this._label and percentLabel
        this.actor.label_actor = percentLabel;
    },

    _deviceTypeToString: function(type) {
	switch (type) {
	case UPDeviceType.AC_POWER:
            return _("AC Adapter");
        case UPDeviceType.BATTERY:
            return _("Laptop Battery");
        case UPDeviceType.UPS:
            return _("UPS");
        case UPDeviceType.MONITOR:
            return _("Monitor");
        case UPDeviceType.MOUSE:
            return _("Mouse");
        case UPDeviceType.KEYBOARD:
            return _("Keyboard");
        case UPDeviceType.PDA:
            return _("PDA");
        case UPDeviceType.PHONE:
            return _("Cell Phone");
        case UPDeviceType.MEDIA_PLAYER:
            return _("Media Player");
        case UPDeviceType.TABLET:
            return _("Tablet");
        case UPDeviceType.COMPUTER:
            return _("Computer");
        default:
            return C_("device", "Unknown");
        }
    }
});
