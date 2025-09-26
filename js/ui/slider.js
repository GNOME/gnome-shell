import Atk from 'gi://Atk';
import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';

import * as BarLevel from './barLevel.js';

const SLIDER_SCROLL_STEP = 0.02; /* Slider scrolling step in % */

export const Slider = GObject.registerClass({
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
            track_hover: true,
            hover: false,
            accessible_role: Atk.Role.SLIDER,
            x_expand: true,
        });

        this._releaseId = 0;
        this._handleRadius = 0;

        this._panGesture = new Clutter.PanGesture();
        this._panGesture.set_begin_threshold(0);
        this._panGesture.connect('recognize', this._onPanBegin.bind(this));
        this._panGesture.connect('pan-update', this._onPanUpdate.bind(this));
        this._panGesture.connect('end', this._onPanEnd.bind(this));
        this.add_action(this._panGesture);

        this._customAccessible.connect('get-minimum-increment', this._getMinimumIncrement.bind(this));
    }

    vfunc_style_changed() {
        super.vfunc_style_changed();

        const themeNode = this.get_theme_node();
        this._handleRadius =
            Math.round(2 * themeNode.get_length('-slider-handle-radius')) / 2;
    }

    vfunc_repaint() {
        super.vfunc_repaint();

        // Add handle
        const cr = this.get_context();
        const themeNode = this.get_theme_node();
        const [width, height] = this.get_surface_size();
        const rtl = this.get_text_direction() === Clutter.TextDirection.RTL;

        const handleY = height / 2;

        let handleX = this._handleRadius +
            (width - 2 * this._handleRadius) * this._value / this._maxValue;
        if (rtl)
            handleX = width - handleX;

        const color = themeNode.get_foreground_color();
        cr.setSourceColor(color);
        cr.arc(handleX, handleY, this._handleRadius, 0, 2 * Math.PI);
        cr.fill();
        cr.$dispose();
    }

    _getPreferredHeight() {
        const barHeight = super._getPreferredHeight();
        const handleHeight = 2 * this._handleRadius;
        return Math.max(barHeight, handleHeight);
    }

    _getPreferredWidth() {
        const barWidth = super._getPreferredWidth();
        const handleWidth = 2 * this._handleRadius;
        return Math.max(barWidth, handleWidth);
    }

    _onPanBegin() {
        this._grab = global.stage.grab(this);

        // We need to emit 'drag-begin' before moving the handle to make
        // sure that no 'notify::value' signal is emitted before this one.
        this.emit('drag-begin');

        const coords = this._panGesture.get_centroid_abs();
        this._moveHandle(coords.x, coords.y);
        return Clutter.EVENT_STOP;
    }

    _onPanEnd() {
        if (this._releaseId) {
            this.disconnect(this._releaseId);
            this._releaseId = 0;
        }

        if (this._grab) {
            this._grab.dismiss();
            this._grab = null;
        }

        this.emit('drag-end');
    }

    _onPanUpdate() {
        const coords = this._panGesture.get_centroid_abs();
        this._moveHandle(coords.x, coords.y);
    }

    step(nSteps) {
        const delta = nSteps * SLIDER_SCROLL_STEP;
        const value = Math.min(Math.max(0, this._value + delta), this._maxValue);

        if (value !== this.value) {
            this.value = value;
            return true;
        }

        return false;
    }

    vfunc_scroll_event(event) {
        const direction = event.get_scroll_direction();
        let nSteps = 0;

        if (event.get_flags() & Clutter.EventFlags.FLAG_POINTER_EMULATED)
            return Clutter.EVENT_PROPAGATE;

        if (direction === Clutter.ScrollDirection.DOWN) {
            nSteps = -1;
        } else if (direction === Clutter.ScrollDirection.UP) {
            nSteps = 1;
        } else if (direction === Clutter.ScrollDirection.SMOOTH) {
            const [dx] = event.get_scroll_delta();
            nSteps = dx;
            // Match physical direction
            if (event.get_scroll_flags() & Clutter.ScrollFlags.INVERTED)
                nSteps *= -1;
            if (this.get_text_direction() === Clutter.TextDirection.RTL)
                nSteps *= -1;
        }

        this.step(nSteps);

        return Clutter.EVENT_STOP;
    }

    vfunc_key_press_event(event) {
        const key = event.get_key_symbol();
        if (key === Clutter.KEY_Right || key === Clutter.KEY_Left) {
            const rtl = this.get_text_direction() === Clutter.TextDirection.RTL;
            const increaseKey = rtl ? Clutter.KEY_Left : Clutter.KEY_Right;
            const delta = key === increaseKey ? 0.1 : -0.1;
            this.value = Math.max(0, Math.min(this._value + delta, this._maxValue));
            return Clutter.EVENT_STOP;
        }
        return super.vfunc_key_press_event(event);
    }

    _moveHandle(absX, _absY) {
        let relX;
        const [sliderX] = this.get_transformed_position();
        const rtl = this.get_text_direction() === Clutter.TextDirection.RTL;
        const width = this._barLevelWidth;

        relX = absX - sliderX;
        if (rtl)
            relX = width - relX;

        let newvalue;
        if (relX < this._handleRadius)
            newvalue = 0;
        else if (relX > width - this._handleRadius)
            newvalue = 1;
        else
            newvalue = (relX - this._handleRadius) / (width - 2 * this._handleRadius);
        this.value = newvalue * this._maxValue;
    }

    _getMinimumIncrement() {
        return 0.1;
    }
});
