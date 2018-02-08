/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Atk = imports.gi.Atk;
const Cairo = imports.cairo;
const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const St = imports.gi.St;
const Signals = imports.signals;

var SLIDER_SCROLL_STEP = 0.02; /* Slider scrolling step in % */

var Slider = new Lang.Class({
    Name: "Slider",

    _init(value) {
        if (isNaN(value))
            // Avoid spreading NaNs around
            throw TypeError('The slider value must be a number');
        this._value = Math.max(Math.min(value, 1), 0);
        this._sliderWidth = 0;

        this.actor = new St.DrawingArea({ style_class: 'slider',
                                          can_focus: true,
                                          reactive: true,
                                          accessible_role: Atk.Role.SLIDER });
        this.actor.connect('repaint', Lang.bind(this, this._sliderRepaint));
        this.actor.connect('button-press-event', Lang.bind(this, this._startDragging));
        this.actor.connect('touch-event', Lang.bind(this, this._touchDragging));
        this.actor.connect('scroll-event', Lang.bind(this, this._onScrollEvent));
        this.actor.connect('key-press-event', Lang.bind(this, this.onKeyPressEvent));
        this.actor.connect('allocation-changed', (actor, box) => {
            this._sliderWidth = box.get_width();
        });

        this._releaseId = this._motionId = 0;
        this._dragging = false;

        this._customAccessible = St.GenericAccessible.new_for_actor(this.actor);
        this.actor.set_accessible(this._customAccessible);

        this._customAccessible.connect('get-current-value', Lang.bind(this, this._getCurrentValue));
        this._customAccessible.connect('get-minimum-value', Lang.bind(this, this._getMinimumValue));
        this._customAccessible.connect('get-maximum-value', Lang.bind(this, this._getMaximumValue));
        this._customAccessible.connect('get-minimum-increment', Lang.bind(this, this._getMinimumIncrement));
        this._customAccessible.connect('set-current-value', Lang.bind(this, this._setCurrentValue));

        this.connect('value-changed', Lang.bind(this, this._valueChanged));
    },

    setValue(value) {
        if (isNaN(value))
            throw TypeError('The slider value must be a number');

        this._value = Math.max(Math.min(value, 1), 0);
        this.actor.queue_repaint();
    },

    _sliderRepaint(area) {
        let cr = area.get_context();
        let themeNode = area.get_theme_node();
        let [width, height] = area.get_surface_size();

        let handleRadius = themeNode.get_length('-slider-handle-radius');

        let handleBorderWidth = themeNode.get_length('-slider-handle-border-width');
        let [hasHandleColor, handleBorderColor] =
            themeNode.lookup_color('-slider-handle-border-color', false);

        let sliderHeight = themeNode.get_length('-slider-height');

        let sliderBorderWidth = themeNode.get_length('-slider-border-width');
        let sliderBorderRadius = Math.min(width, sliderHeight) / 2;

        let sliderBorderColor = themeNode.get_color('-slider-border-color');
        let sliderColor = themeNode.get_color('-slider-background-color');

        let sliderActiveBorderColor = themeNode.get_color('-slider-active-border-color');
        let sliderActiveColor = themeNode.get_color('-slider-active-background-color');

        const TAU = Math.PI * 2;

        let handleX = handleRadius + (width - 2 * handleRadius) * this._value;

        cr.arc(sliderBorderRadius + sliderBorderWidth, height / 2, sliderBorderRadius, TAU * 1/4, TAU * 3/4);
        cr.lineTo(handleX, (height - sliderHeight) / 2);
        cr.lineTo(handleX, (height + sliderHeight) / 2);
        cr.lineTo(sliderBorderRadius + sliderBorderWidth, (height + sliderHeight) / 2);
        Clutter.cairo_set_source_color(cr, sliderActiveColor);
        cr.fillPreserve();
        Clutter.cairo_set_source_color(cr, sliderActiveBorderColor);
        cr.setLineWidth(sliderBorderWidth);
        cr.stroke();

        cr.arc(width - sliderBorderRadius - sliderBorderWidth, height / 2, sliderBorderRadius, TAU * 3/4, TAU * 1/4);
        cr.lineTo(handleX, (height + sliderHeight) / 2);
        cr.lineTo(handleX, (height - sliderHeight) / 2);
        cr.lineTo(width - sliderBorderRadius - sliderBorderWidth, (height - sliderHeight) / 2);
        Clutter.cairo_set_source_color(cr, sliderColor);
        cr.fillPreserve();
        Clutter.cairo_set_source_color(cr, sliderBorderColor);
        cr.setLineWidth(sliderBorderWidth);
        cr.stroke();

        let handleY = height / 2;

        let color = themeNode.get_foreground_color();
        Clutter.cairo_set_source_color(cr, color);
        cr.arc(handleX, handleY, handleRadius, 0, 2 * Math.PI);
        cr.fillPreserve();
        if (hasHandleColor && handleBorderWidth) {
            Clutter.cairo_set_source_color(cr, handleBorderColor);
            cr.setLineWidth(handleBorderWidth);
            cr.stroke();
        }
        cr.$dispose();
    },

    _startDragging(actor, event) {
        return this.startDragging(event);
    },

    startDragging(event) {
        if (this._dragging)
            return Clutter.EVENT_PROPAGATE;

        this._dragging = true;

        let device = event.get_device();
        let sequence = event.get_event_sequence();

        if (sequence != null)
            device.sequence_grab(sequence, this.actor);
        else
            device.grab(this.actor);

        this._grabbedDevice = device;
        this._grabbedSequence = sequence;

        if (sequence == null) {
            this._releaseId = this.actor.connect('button-release-event', Lang.bind(this, this._endDragging));
            this._motionId = this.actor.connect('motion-event', Lang.bind(this, this._motionEvent));
        }

        // We need to emit 'drag-begin' before moving the handle to make
        // sure that no 'value-changed' signal is emitted before this one.
        this.emit('drag-begin');

        let absX, absY;
        [absX, absY] = event.get_coords();
        this._moveHandle(absX, absY);
        return Clutter.EVENT_STOP;
    },

    _endDragging() {
        if (this._dragging) {
            if (this._releaseId)
                this.actor.disconnect(this._releaseId);
            if (this._motionId)
                this.actor.disconnect(this._motionId);

            if (this._grabbedSequence != null)
                this._grabbedDevice.sequence_ungrab(this._grabbedSequence);
            else
                this._grabbedDevice.ungrab();

            this._grabbedSequence = null;
            this._grabbedDevice = null;
            this._dragging = false;

            this.emit('drag-end');
        }
        return Clutter.EVENT_STOP;
    },

    _touchDragging(actor, event) {
        let device = event.get_device();
        let sequence = event.get_event_sequence();

        if (!this._dragging &&
            event.type() == Clutter.EventType.TOUCH_BEGIN) {
            this.startDragging(event);
            return Clutter.EVENT_STOP;
        } else if (device.sequence_get_grabbed_actor(sequence) == actor) {
            if (event.type() == Clutter.EventType.TOUCH_UPDATE)
                return this._motionEvent(actor, event);
            else if (event.type() == Clutter.EventType.TOUCH_END)
                return this._endDragging();
        }

        return Clutter.EVENT_PROPAGATE;
    },

    scroll(event) {
        let direction = event.get_scroll_direction();
        let delta;

        if (event.is_pointer_emulated())
            return Clutter.EVENT_PROPAGATE;

        if (direction == Clutter.ScrollDirection.DOWN) {
            delta = -SLIDER_SCROLL_STEP;
        } else if (direction == Clutter.ScrollDirection.UP) {
            delta = +SLIDER_SCROLL_STEP;
        } else if (direction == Clutter.ScrollDirection.SMOOTH) {
            let [dx, dy] = event.get_scroll_delta();
            // Even though the slider is horizontal, use dy to match
            // the UP/DOWN above.
            delta = -dy * SLIDER_SCROLL_STEP;
        }

        this._value = Math.min(Math.max(0, this._value + delta), 1);

        this.actor.queue_repaint();
        this.emit('value-changed', this._value);
        return Clutter.EVENT_STOP;
    },

    _onScrollEvent(actor, event) {
        return this.scroll(event);
    },

    _motionEvent(actor, event) {
        let absX, absY;
        [absX, absY] = event.get_coords();
        this._moveHandle(absX, absY);
        return Clutter.EVENT_STOP;
    },

    onKeyPressEvent (actor, event) {
        let key = event.get_key_symbol();
        if (key == Clutter.KEY_Right || key == Clutter.KEY_Left) {
            let delta = key == Clutter.KEY_Right ? 0.1 : -0.1;
            this._value = Math.max(0, Math.min(this._value + delta, 1));
            this.actor.queue_repaint();
            this.emit('drag-begin');
            this.emit('value-changed', this._value);
            this.emit('drag-end');
            return Clutter.EVENT_STOP;
        }
        return Clutter.EVENT_PROPAGATE;
    },

    _moveHandle(absX, absY) {
        let relX, relY, sliderX, sliderY;
        [sliderX, sliderY] = this.actor.get_transformed_position();
        relX = absX - sliderX;
        relY = absY - sliderY;

        let width = this._sliderWidth;
        let handleRadius = this.actor.get_theme_node().get_length('-slider-handle-radius');

        let newvalue;
        if (relX < handleRadius)
            newvalue = 0;
        else if (relX > width - handleRadius)
            newvalue = 1;
        else
            newvalue = (relX - handleRadius) / (width - 2 * handleRadius);
        this._value = newvalue;
        this.actor.queue_repaint();
        this.emit('value-changed', this._value);
    },

    _getCurrentValue (actor) {
        return this._value;
    },

    _getMinimumValue (actor) {
        return 0;
    },

    _getMaximumValue (actor) {
        return 1;
    },

    _getMinimumIncrement (actor) {
        return 0.1;
    },

    _setCurrentValue (actor, value) {
        this._value = value;
    },

    _valueChanged (slider, value, property) {
        this._customAccessible.notify ("accessible-value");
    },

    get value() {
        return this._value;
    }
});

Signals.addSignalMethods(Slider.prototype);
