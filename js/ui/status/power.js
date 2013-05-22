// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const Lang = imports.lang;
const St = imports.gi.St;
const UPower = imports.gi.UPowerGlib;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const BUS_NAME = 'org.gnome.SettingsDaemon.Power';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Power';

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

        this.item = new PopupMenu.PopupMenuItem('', { reactive: false });
        this._primaryPercentage = new St.Label({ style_class: 'popup-battery-percentage' });
        this.item.addActor(this._primaryPercentage, { align: St.Align.END });
        this.menu.addMenuItem(this.item);
    },

    _readPrimaryDevice: function() {
        this._proxy.GetPrimaryDeviceRemote(Lang.bind(this, function(result, error) {
            if (error) {
                this.item.actor.hide();
                return;
            }

            let [[device_id, device_type, icon, percentage, state, seconds]] = result;
            if (device_type == UPower.DeviceKind.BATTERY) {
                let time = Math.round(seconds / 60);
                if (time == 0) {
                    // 0 is reported when UPower does not have enough data
                    // to estimate battery life
                    this.item.label.text = _("Estimating…");
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
                    this.item.label.text = timestring;
                }
                this._primaryPercentage.text = C_("percent of battery remaining", "%d%%").format(Math.round(percentage));
                this.item.actor.show();
            } else {
                this.item.actor.hide();
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
    }
});
