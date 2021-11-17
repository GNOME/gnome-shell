/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */
/* exported Slider */

const { Atk, Clutter, GObject } = imports.gi;

const BarLevel = imports.ui.barLevel;

var SLIDER_SCROLL_STEP = 0.02; /* Slider scrolling step in % */

var Slider = GObject.registerClass({
    Signals: {
        'drag-begin': {},
        'drag-end': {},
    },
}, class Slider extends BarLevel.BarLevel {
    _init(value) {
        super._init({
            value,
            style_class: 'slider',
            can_focus: true,
            reactive: true,
            accessible_role: Atk.Role.SLIDER,
            x_expand: true,
        });

        this._releaseId = 0;
        this._dragging = false;

        this._customAccessible.connect('get-minimum-increment', this._getMinimumIncrement.bind(this));
    }

    vfunc_repaint() {
        super.vfunc_repaint();

        // Add handle
        let cr = this.get_context();
        let themeNode = this.get_theme_node();
        let [width, height] = this.get_surface_size();

        let handleRadius = themeNode.get_length('-slider-handle-radius');

        let handleBorderWidth = themeNode.get_length('-slider-handle-border-width');
        let [hasHandleColor, handleBorderColor] =
            themeNode.lookup_color('-slider-handle-border-color', false);

        const ceiledHandleRadius = Math.ceil(handleRadius + handleBorderWidth);
        const handleX = ceiledHandleRadius +
            (width - 2 * ceiledHandleRadius) * this._value / this._maxValue;
        const handleY = height / 2;

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
    }

    vfunc_button_press_event() {
        return this.startDragging(Clutter.get_current_event());
    }

    startDragging(event) {
        if (this._dragging)
            return Clutter.EVENT_PROPAGATE;

        this._dragging = true;

        let device = event.get_device();
        let sequence = event.get_event_sequence();

        this._grab = global.stage.grab(this);

        this._grabbedDevice = device;
        this._grabbedSequence = sequence;

        // We need to emit 'drag-begin' before moving the handle to make
        // sure that no 'notify::value' signal is emitted before this one.
        this.emit('drag-begin');

        let absX, absY;
        [absX, absY] = event.get_coords();
        this._moveHandle(absX, absY);
        return Clutter.EVENT_STOP;
    }

    _endDragging() {
        if (this._dragging) {
            if (this._releaseId) {
                this.disconnect(this._releaseId);
                this._releaseId = 0;
            }

            if (this._grab) {
                this._grab.dismiss();
                this._grab = null;
            }

            this._grabbedSequence = null;
            this._grabbedDevice = null;
            this._dragging = false;

            this.emit('drag-end');
        }
        return Clutter.EVENT_STOP;
    }

    vfunc_button_release_event() {
        if (this._dragging && !this._grabbedSequence)
            return this._endDragging();

        return Clutter.EVENT_PROPAGATE;
    }

    vfunc_touch_event() {
        let event = Clutter.get_current_event();
        let sequence = event.get_event_sequence();

        if (!this._dragging &&
            event.type() == Clutter.EventType.TOUCH_BEGIN) {
            this.startDragging(event);
            return Clutter.EVENT_STOP;
        } else if (this._grabbedSequence &&
                   sequence.get_slot() === this._grabbedSequence.get_slot()) {
            if (event.type() == Clutter.EventType.TOUCH_UPDATE)
                return this._motionEvent(this, event);
            else if (event.type() == Clutter.EventType.TOUCH_END)
                return this._endDragging();
        }

        return Clutter.EVENT_PROPAGATE;
    }

    scroll(event) {
        let direction = event.get_scroll_direction();
        let delta;

        if (event.is_pointer_emulated())
            return Clutter.EVENT_PROPAGATE;

        if (direction == Clutter.ScrollDirection.DOWN) {
            delta = -SLIDER_SCROLL_STEP;
        } else if (direction == Clutter.ScrollDirection.UP) {
            delta = SLIDER_SCROLL_STEP;
        } else if (direction == Clutter.ScrollDirection.SMOOTH) {
            let [, dy] = event.get_scroll_delta();
            // Even though the slider is horizontal, use dy to match
            // the UP/DOWN above.
            delta = -dy * SLIDER_SCROLL_STEP;
        }

        this.value = Math.min(Math.max(0, this._value + delta), this._maxValue);

        return Clutter.EVENT_STOP;
    }

    vfunc_scroll_event() {
        return this.scroll(Clutter.get_current_event());
    }

    vfunc_motion_event() {
        if (this._dragging && !this._grabbedSequence)
            return this._motionEvent(this, Clutter.get_current_event());

        return Clutter.EVENT_PROPAGATE;
    }

    _motionEvent(actor, event) {
        let absX, absY;
        [absX, absY] = event.get_coords();
        this._moveHandle(absX, absY);
        return Clutter.EVENT_STOP;
    }

    vfunc_key_press_event(keyPressEvent) {
        let key = keyPressEvent.keyval;
        if (key == Clutter.KEY_Right || key == Clutter.KEY_Left) {
            let delta = key == Clutter.KEY_Right ? 0.1 : -0.1;
            this.value = Math.max(0, Math.min(this._value + delta, this._maxValue));
            return Clutter.EVENT_STOP;
        }
        return super.vfunc_key_press_event(keyPressEvent);
    }

    _moveHandle(absX, _absY) {
        let relX, sliderX;
        [sliderX] = this.get_transformed_position();
        relX = absX - sliderX;

        let width = this._barLevelWidth;
        let handleRadius = this.get_theme_node().get_length('-slider-handle-radius');

        let newvalue;
        if (relX < handleRadius)
            newvalue = 0;
        else if (relX > width - handleRadius)
            newvalue = 1;
        else
            newvalue = (relX - handleRadius) / (width - 2 * handleRadius);
        this.value = newvalue * this._maxValue;
    }

    _getMinimumIncrement() {
        return 0.1;
    }
});
