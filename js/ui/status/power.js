// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gdk = imports.gi.Gdk;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const St = imports.gi.St;
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

const CAUTION_PERCENTAGE = 10;

// FIXME: Should themes be able to override this and battery bar direction?
const BATTERY_TOP = 1 / 16;
const BATTERY_BOTTOM = 15 / 16;
const BATTERY_LEFT = 3 / 16;
const BATTERY_RIGHT = 13 / 16;

// FIXME: could this be in StTextureCache instead?
function loadSymbolicIcon(cr, name, size, themeNode) {
    let gicon = Gtk.IconTheme.get_default().lookup_icon(name, size, 0);

    let colors = themeNode.get_icon_colors();

    function _rgbaFromClutter (color) {
        return new Gdk.RGBA({
            red: color.red / 255.,
            green: color.green / 255.,
            blue: color.blue / 255.,
            alpha: color.alpha / 255.
        });
    }

    let foregroundColor = _rgbaFromClutter(colors.foreground);
    let successColor = _rgbaFromClutter(colors.success);
    let warningColor = _rgbaFromClutter(colors.warning);
    let errorColor = _rgbaFromClutter(colors.error);

    return gicon.load_symbolic(foregroundColor, successColor, warningColor, errorColor)[0];
}

var BatteryIcon = new Lang.Class({
    Name: 'BatteryIcon',
    Extends: St.Bin,

    _init(proxy, params) {
        this.parent(params);
        this.x_fill = true;
        this.y_fill = true;

        this._drawingArea = new St.DrawingArea();
        this._drawingArea.connect('repaint', this._repaint.bind(this));
        this._drawingArea.connect('style-changed', this._updateSize.bind(this));
        this.child = this._drawingArea;

        this._shadowHelper = null;

        this._proxy = proxy;
        this._proxy.connect('g-properties-changed', this._update.bind(this));
    },

    vfunc_get_paint_volume(volume) {
        if (!this.parent(volume))
            return false;

        if (!this._shadow)
            return true;

        let shadow_box = new Clutter.ActorBox();
        this._shadow.get_box(this._drawingArea.get_allocation_box(), shadow_box);

        volume.set_width(Math.max(shadow_box.x2 - shadow_box.x1, volume.get_width()));
        volume.set_height(Math.max(shadow_box.y2 - shadow_box.y1, volume.get_height()));

        return true;
    },

    vfunc_style_changed() {
        let node = this.get_theme_node();
        this._shadow = node.get_shadow('icon-shadow');
        if (this._shadow)
            this._shadowHelper = St.ShadowHelper.new(this._shadow);
        else
            this._shadowHelper = null;

        this.parent();
    },

    vfunc_paint() {
        if (this._shadowHelper) {
            this._shadowHelper.update(this._drawingArea);

            let allocation = this._drawingArea.get_allocation_box();
            let paintOpacity = this._drawingArea.get_paint_opacity();
            this._shadowHelper.paint(allocation, paintOpacity);
        }

        this._drawingArea.paint();
    },

    _updateSize() {
        let node = this.get_theme_node();
        let iconSize = Math.round(node.get_length('icon-size'));
        this._drawingArea.width = iconSize;
        this._drawingArea.height = iconSize;
    },

    _repaint(icon) {
        let cr = icon.get_context();
        let [w, h] = icon.get_surface_size();
        let node = this.get_theme_node();

        let iconSize = Math.round(node.get_length('icon-size'));
        cr.translate(Math.floor((w - iconSize) / 2), Math.floor((h - iconSize) / 2));

        this._drawIcon(cr, iconSize, node);

        cr.$dispose();
    },

    _update() {
        // Explicitly update shadow
        this.emit('style-changed');
        this.child.queue_repaint();
    },

    _drawIcon(cr, size, themeNode) {
        if (!this._proxy || !this._proxy.IsPresent) {
            this._drawStaticIcon(cr, size, themeNode, 'system-shutdown-symbolic');
        } else {
            switch(this._proxy.State) {
                case UPower.DeviceState.EMPTY:
                    this._drawStaticIcon(cr, size, themeNode, 'battery-empty-symbolic');
                    return;

                case UPower.DeviceState.FULLY_CHARGED:
                    this._drawStaticIcon(cr, size, themeNode, 'battery-full-charged-symbolic');
                    return;

                case UPower.DeviceState.CHARGING:
                case UPower.DeviceState.PENDING_CHARGE:
                    if (this._proxy.Percentage == 0) {
                        this._drawStaticIcon(cr, size, themeNode, 'battery-empty-charging-symbolic');
                        return;
                    }

                    this._drawDynamicBattery(cr, size, themeNode, true);
                    return;
                case UPower.DeviceState.DISCHARGING:
                case UPower.DeviceState.PENDING_DISCHARGE:
                    this._drawDynamicBattery(cr, size, themeNode, false);
                    return;
                default:
                    this._drawStaticIcon(cr, size, themeNode, 'battery-missing-symbolic');
                    return;
            }
        }
    },

    _drawStaticIcon(cr, size, themeNode, iconName) {
        let pixbuf = loadSymbolicIcon(cr, iconName, size, themeNode);
        Gdk.cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
        cr.rectangle(0, 0, size, size);
        cr.fill();
    },

    _drawDynamicBattery(cr, size, themeNode, charging) {
        let percentage = this._proxy.Percentage;
        let caution = percentage <= CAUTION_PERCENTAGE;
        let frac = 1 - percentage / 100;

        let iconName = '';
        if (caution)
            iconName += '-caution';
        if (charging)
            iconName += '-charging';

        let bgName = 'battery-bg' + iconName + '-symbolic';
        let fgName = 'battery-fg' + iconName + '-symbolic';

        this._drawStaticIcon(cr, size, themeNode, bgName);

        let baseSize = St.ThemeContext.get_for_stage(global.stage).scale_factor;

        let rectT = BATTERY_TOP / baseSize * size;
        let rectB = BATTERY_BOTTOM / baseSize * size;
        let rectL = BATTERY_LEFT / baseSize * size;
        let rectR = BATTERY_RIGHT / baseSize * size;

        rectT += (rectB - rectT) * frac;

        let pixbuf = loadSymbolicIcon(cr, fgName, size, themeNode);
        Gdk.cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
        cr.rectangle(rectL, rectT, rectR - rectL, rectB - rectT);
        cr.fill();
    }
});

var PowerMenuItem = new Lang.Class({
    Name: 'PowerMenuItem',
    Extends: PopupMenu.PopupSubMenuMenuItem,

    _init(proxy) {
        this._proxy = proxy;
        this.parent("", true);
    },

    _createIcon() {
        return new BatteryIcon(this._proxy, { style_class: 'popup-menu-icon' });
    },
});

var Indicator = new Lang.Class({
    Name: 'PowerIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init() {
        this.parent();

        this._desktopSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.interface' });
        this._desktopSettings.connect('changed::' + SHOW_BATTERY_PERCENTAGE,
                                      this._sync.bind(this));

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

        this._indicator = this._addIndicator();
        this._percentageLabel = new St.Label({ y_expand: true,
                                               y_align: Clutter.ActorAlign.CENTER });
        this.indicators.add(this._percentageLabel, { expand: true, y_fill: true });
        this.indicators.add_style_class_name('power-status');

        this._item = new PowerMenuItem(this._proxy);
        this._item.menu.addSettingsAction(_("Power Settings"), 'gnome-power-panel.desktop');
        this.menu.addMenuItem(this._item);

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();
    },

    _createIcon() {
        return new BatteryIcon(this._proxy, { style_class: 'system-status-icon' });
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
            this._percentageLabel.hide();
            return;
        }

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
