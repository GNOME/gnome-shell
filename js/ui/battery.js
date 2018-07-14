// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Gtk = imports.gi.Gtk;
const Gdk = imports.gi.Gdk;
const St = imports.gi.St;
const UPower = imports.gi.UPowerGlib;

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

// FIXME: This class is heavily based on ScreenShield.Arrow. Does it need to be general?
var DynamicIcon = new Lang.Class({
    Name: 'DynamicIcon',
    Extends: St.Bin,

    _init(params) {
        this.parent(params);
        this.x_fill = true;
        this.y_fill = true;

        this._drawingArea = new St.DrawingArea();
        this._drawingArea.connect('repaint', this._repaint.bind(this));
        this._drawingArea.connect('style-changed', this._updateSize.bind(this));
        this.child = this._drawingArea;

        this._shadowHelper = null;
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

        this.drawIcon(cr, iconSize, node);

        cr.$dispose();
    },

    drawIcon(cr, iconSize, themeNode) {},

    update() {
        // Explicitly update shadow
        this.emit('style-changed');
        this.child.queue_repaint();
    }
});

var BatteryIcon = new Lang.Class({
    Name: 'BatteryIcon',
    Extends: DynamicIcon,

    setProxy(proxy) {
        this._proxy = proxy;
    },

    drawIcon(cr, size, themeNode) {
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

        bgName = 'battery-bg' + iconName + '-symbolic';
        fgName = 'battery-fg' + iconName + '-symbolic';

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
