/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Atk = imports.gi.Atk;
const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const St = imports.gi.St;
const Signals = imports.signals;

var BarLevel = new Lang.Class({
    Name: "BarLevel",

    _init(value, params) {
        if (isNaN(value))
            // Avoid spreading NaNs around
            throw TypeError('The bar level value must be a number');
        this._maxValue = 1;
        this._value = Math.max(Math.min(value, this._maxValue), 0);
        this._barLevelWidth = 0;

        if (params == undefined)
            params = {}

        this.actor = new St.DrawingArea({ styleClass: params['styleClass'] || 'barlevel',
                                          can_focus: params['canFocus'] || false,
                                          reactive: params['reactive'] || false,
                                          accessible_role: params['accessibleRole'] || Atk.Role.LEVEL_BAR });
        this.actor.connect('repaint', this._barLevelRepaint.bind(this));
        this.actor.connect('allocation-changed', (actor, box) => {
            this._barLevelWidth = box.get_width();
        });

        this._customAccessible = St.GenericAccessible.new_for_actor(this.actor);
        this.actor.set_accessible(this._customAccessible);

        this._customAccessible.connect('get-current-value', this._getCurrentValue.bind(this));
        this._customAccessible.connect('get-minimum-value', this._getMinimumValue.bind(this));
        this._customAccessible.connect('get-maximum-value', this._getMaximumValue.bind(this));
        this._customAccessible.connect('set-current-value', this._setCurrentValue.bind(this));

        this.connect('value-changed', this._valueChanged.bind(this));
    },

    setValue(value) {
        if (isNaN(value))
            throw TypeError('The bar level value must be a number');

        this._value = Math.max(Math.min(value, this._maxValue), 0);
        this.actor.queue_repaint();
    },

    setMaximumValue(value) {
        if (isNaN(value))
            throw TypeError('The bar level max value must be a number');

        this._maxValue = Math.max(value, 1);
        this.actor.queue_repaint();
    },

    _barLevelRepaint(area) {
        let cr = area.get_context();
        let themeNode = area.get_theme_node();
        let [width, height] = area.get_surface_size();

        let barLevelHeight = themeNode.get_length('-barlevel-height');
        let barLevelBorderRadius = Math.min(width, barLevelHeight) / 2;
        let fgColor = themeNode.get_foreground_color();

        let barLevelColor = themeNode.get_color('-barlevel-background-color');
        let barLevelActiveColor = themeNode.get_color('-barlevel-active-background-color');

        let barLevelBorderWidth = Math.min(themeNode.get_length('-barlevel-border-width'), 1);
        let [hasBorderColor, barLevelBorderColor] =
            themeNode.lookup_color('-barlevel-border-color', false);
        if (!hasBorderColor)
            barLevelBorderColor = barLevelColor;
        let [hasActiveBorderColor, barLevelActiveBorderColor] =
            themeNode.lookup_color('-barlevel-active-border-color', false);
        if (!hasActiveBorderColor)
            barLevelActiveBorderColor = barLevelActiveColor;

        const TAU = Math.PI * 2;

        let endX = 0;
        if (this._maxValue > 0)
            endX = barLevelBorderRadius + (width - 2 * barLevelBorderRadius) * this._value / this._maxValue;

        /* background bar */
        cr.arc(width - barLevelBorderRadius - barLevelBorderWidth, height / 2, barLevelBorderRadius, TAU * 3 / 4, TAU * 1 / 4);
        cr.lineTo(endX, (height + barLevelHeight) / 2);
        cr.lineTo(endX, (height - barLevelHeight) / 2);
        cr.lineTo(width - barLevelBorderRadius - barLevelBorderWidth, (height - barLevelHeight) / 2);
        Clutter.cairo_set_source_color(cr, barLevelColor);
        cr.fillPreserve();
        Clutter.cairo_set_source_color(cr, barLevelBorderColor);
        cr.setLineWidth(barLevelBorderWidth);
        cr.stroke();

        /* normal progress bar */
        cr.arc(barLevelBorderRadius + barLevelBorderWidth, height / 2, barLevelBorderRadius, TAU * 1 / 4, TAU * 3 / 4);
        cr.lineTo(endX, (height - barLevelHeight) / 2);
        cr.lineTo(endX, (height + barLevelHeight) / 2);
        cr.lineTo(barLevelBorderRadius + barLevelBorderWidth, (height + barLevelHeight) / 2);
        Clutter.cairo_set_source_color(cr, barLevelActiveColor);
        cr.fillPreserve();
        Clutter.cairo_set_source_color(cr, barLevelActiveBorderColor);
        cr.setLineWidth(barLevelBorderWidth);
        cr.stroke();

        /* end progress bar arc */
        Clutter.cairo_set_source_color(cr, barLevelActiveColor);
        cr.arc(endX, height / 2, barLevelBorderRadius, TAU * 3 / 4, TAU * 1 / 4);
        cr.lineTo(Math.floor(endX), (height + barLevelHeight) / 2);
        cr.lineTo(Math.floor(endX), (height - barLevelHeight) / 2);
        cr.lineTo(endX, (height - barLevelHeight) / 2);
        cr.fillPreserve();
        cr.setLineWidth(barLevelBorderWidth);
        cr.stroke();

        cr.$dispose();
    },

    _getCurrentValue(actor) {
        return this._value;
    },

    _getMinimumValue(actor) {
        return 0;
    },

    _getMaximumValue(actor) {
        return this._maxValue;
    },

    _setCurrentValue(actor, value) {
        this._value = value;
    },

    _valueChanged(barLevel, value, property) {
        this._customAccessible.notify("accessible-value");
    },

    get value() {
        return this._value;
    }
});

Signals.addSignalMethods(BarLevel.prototype);
