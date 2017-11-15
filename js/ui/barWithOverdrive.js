/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Atk = imports.gi.Atk;
const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const St = imports.gi.St;
const Signals = imports.signals;

var BarWithOverdrive = new Lang.Class({
    Name: "BarWithOverdrive",

    _init: function(value, params) {
        if (isNaN(value))
            // Avoid spreading NaNs around
            throw TypeError('The bar value must be a number');
        this._max_value = 1;
        this._overdrive_value = 1;
        this._value = Math.max(Math.min(value, this._max_value), 0);
        this._barWidth = 0;

        if (params == undefined)
            params = {}

        this.actor = new St.DrawingArea({ style_class: params['style-class'] || 'bar',
                                          can_focus: params['can-focus'] || false,
                                          reactive: params['reactive'] || false,
                                          accessible_role: params['accessible-role'] || Atk.Role.LEVEL_BAR });
        this.actor.connect('repaint', Lang.bind(this, this._barRepaint));
        this.actor.connect('allocation-changed', (actor, box) => {
            this._barWidth = box.get_width();
        });

        this._customAccessible = St.GenericAccessible.new_for_actor(this.actor);
        this.actor.set_accessible(this._customAccessible);

        this._customAccessible.connect('get-current-value', Lang.bind(this, this._getCurrentValue));
        this._customAccessible.connect('get-minimum-value', Lang.bind(this, this._getMinimumValue));
        this._customAccessible.connect('get-maximum-value', Lang.bind(this, this._getMaximumValue));
        this._customAccessible.connect('get-overdrive-value', Lang.bind(this, this._getOverdriveValue));
        this._customAccessible.connect('set-current-value', Lang.bind(this, this._setCurrentValue));

        this.connect('value-changed', Lang.bind(this, this._valueChanged));
    },

    setValue: function(value) {
        if (isNaN(value))
            throw TypeError('The bar value must be a number');

        this._value = Math.max(Math.min(value, this._max_value), 0);
        this.actor.queue_repaint();
    },

    setMaximumValue: function (value) {
        if (isNaN(value))
            throw TypeError('The bar max value must be a number');

        this._max_value = Math.max(value, this._overdrive_value);
        this.actor.queue_repaint();
    },

    setOverdriveValue: function (value) {
        if (isNaN(value))
            throw TypeError('The overdrive limit value must be a number');
        if (value > this._max_value)
            throw new Error(`You should set a maximum value higer than overdrive value ` +
                            `before calling setOverdriveValue(): max value is ${this._max_value} ` +
                            `where overdrive value was going to be set to ${value}`);
        this._overdrive_value = value;
        this.actor.queue_repaint();
    },

    _barRepaint: function(area) {
        let cr = area.get_context();
        let themeNode = area.get_theme_node();
        let [width, height] = area.get_surface_size();

        let barHeight = themeNode.get_length('-bar-height');
        let barBorderRadius = Math.min(width, barHeight) / 2;
        let fgColor = themeNode.get_foreground_color();

        let barColor = themeNode.get_color('-bar-background-color');
        let barActiveColor = themeNode.get_color('-bar-active-background-color');
        let barOverdriveColor = themeNode.get_color('-bar-overdrive-color');

        let barBorderColor = barColor;
        let barActiveBorderColor = barActiveColor;
        let barOverdriveBorderColor = barOverdriveColor;
        let [hasCustomBorder, barBorderWidth] = themeNode.lookup_length('-bar-border-width', false);
        /* we want to have the bar lines itself drawn */
        barBorderWidth = Math.min(barBorderWidth, 1);
        if (hasCustomBorder) {
            barBorderColor = themeNode.get_color('-bar-border-color');
            barActiveBorderColor = themeNode.get_color('-bar-active-border-color');
            barOverdriveBorderColor = themeNode.get_color('-bar-overdrive-border-color');
        }

        const TAU = Math.PI * 2;

        let endX = barBorderRadius + (width - 2 * barBorderRadius) * this._value / this._max_value;
        let overdriveSeparatorX = barBorderRadius + (width - 2 * barBorderRadius) * this._overdrive_value / this._max_value;
        let overdriveActive = this._max_value > this._overdrive_value;
        let overdriveSeparatorWidth = 0;
        if (overdriveActive)
            overdriveSeparatorWidth = themeNode.get_length('-bar-overdrive-separator-width');

        /* background bar */
        cr.arc(width - barBorderRadius - barBorderWidth, height / 2, barBorderRadius, TAU * 3 / 4, TAU * 1 / 4);
        cr.lineTo(endX, (height + barHeight) / 2);
        cr.lineTo(endX, (height - barHeight) / 2);
        cr.lineTo(width - barBorderRadius - barBorderWidth, (height - barHeight) / 2);
        Clutter.cairo_set_source_color(cr, barColor);
        cr.fillPreserve();
        Clutter.cairo_set_source_color(cr, barBorderColor);
        cr.setLineWidth(barBorderWidth);
        cr.stroke();

        /* normal bar progress bar */
        let x = Math.min(endX, overdriveSeparatorX - overdriveSeparatorWidth / 2);
        cr.arc(barBorderRadius + barBorderWidth, height / 2, barBorderRadius, TAU * 1/4, TAU * 3/4);
        cr.lineTo(x, (height - barHeight) / 2);
        cr.lineTo(x, (height + barHeight) / 2);
        cr.lineTo(barBorderRadius + barBorderWidth, (height + barHeight) / 2);
        Clutter.cairo_set_source_color(cr, barActiveColor);
        cr.fillPreserve();
        Clutter.cairo_set_source_color(cr, barActiveBorderColor);
        cr.setLineWidth(barBorderWidth);
        cr.stroke();

        /* overdrive progress bar */
        x = Math.min(endX, overdriveSeparatorX) + overdriveSeparatorWidth / 2;
        if (this._value > this._overdrive_value) {
            cr.moveTo(x, (height - barHeight) / 2);
            cr.lineTo(endX, (height - barHeight) / 2);
            cr.lineTo(endX, (height + barHeight) / 2);
            cr.lineTo(x, (height + barHeight) / 2);
            cr.lineTo(x, (height - barHeight) / 2);
            Clutter.cairo_set_source_color(cr, barOverdriveColor);
            cr.fillPreserve();
            Clutter.cairo_set_source_color(cr, barOverdriveBorderColor);
            cr.setLineWidth(barBorderWidth);
            cr.stroke();
        }

        /* end progress bar arc */
        if (this._value <= this._overdrive_value)
            Clutter.cairo_set_source_color(cr, barActiveColor);
        else
            Clutter.cairo_set_source_color(cr, barOverdriveColor);
        cr.arc(endX, height / 2, barBorderRadius, TAU * 3 / 4, TAU * 1 / 4);
        cr.lineTo(Math.floor(endX), (height + barHeight) / 2);
        cr.lineTo(Math.floor(endX), (height - barHeight) / 2);
        cr.lineTo(endX, (height - barHeight) / 2);
        cr.fillPreserve();
        cr.setLineWidth(barBorderWidth);
        cr.stroke();

        /* draw overdrive separator */
        if (overdriveActive) {
            cr.moveTo(overdriveSeparatorX - overdriveSeparatorWidth / 2, (height - barHeight) / 2);
            cr.lineTo(overdriveSeparatorX + overdriveSeparatorWidth / 2, (height - barHeight) / 2);
            cr.lineTo(overdriveSeparatorX + overdriveSeparatorWidth / 2, (height + barHeight) / 2);
            cr.lineTo(overdriveSeparatorX - overdriveSeparatorWidth / 2, (height + barHeight) / 2);
            cr.lineTo(overdriveSeparatorX - overdriveSeparatorWidth / 2, (height - barHeight) / 2);
            if (this._value <= this._overdrive_value)
                Clutter.cairo_set_source_color(cr, fgColor);
            else
                Clutter.cairo_set_source_color(cr, barColor);
            cr.fill();
        }

        cr.$dispose();
    },

    _getCurrentValue: function (actor) {
        return this._value;
    },

    _getOverdriveValue: function (actor) {
        return this._overdrive_value;
    },

    _getMinimumValue: function (actor) {
        return 0;
    },

    _getMaximumValue: function (actor) {
        return this._max_value;
    },

    _setCurrentValue: function (actor, value) {
        this._value = value;
    },

    _valueChanged: function (bar, value, property) {
        this._customAccessible.notify ("accessible-value");
    },

    get value() {
        return this._value;
    }
});

Signals.addSignalMethods(BarWithOverdrive.prototype);
