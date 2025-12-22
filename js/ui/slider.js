import Atk from 'gi://Atk';
import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';

import * as BarLevel from './barLevel.js';

const SLIDER_SCROLL_STEP = 0.02; /* Slider scrolling step in % */
const SNAP_THRESHOLD = 0.04; /* Snap to marks within 4% */

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

        this._marks = new Set();
        this._unsnappedValue = null;
    }

    addMark(value) {
        this._marks.add(value);
    }

    clearMarks() {
        this._marks.clear();
    }

    _snapToMark(value) {
        for (const mark of this._marks) {
            if (Math.abs(value - mark) < SNAP_THRESHOLD)
                return mark;
        }
        return value;
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

        const coords = this._panGesture.get_centroid();
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
        const coords = this._panGesture.get_centroid();
        this._moveHandle(coords.x, coords.y);
    }

    _applyDelta(delta) {
        // Track unsnapped value to allow escaping snap zones when scrolling/using arrow keys.
        // Without this, if the scroll step is smaller than the snap threshold,
        // the slider gets stuck at the mark because each scroll snaps back.
        const base = this._unsnappedValue ?? this._value;
        const oldValue = this._value;
        this._unsnappedValue = Math.clamp(base + delta, 0, this._maxValue);
        this.value = this._snapToMark(this._unsnappedValue);
        return this._value !== oldValue;
    }

    step(nSteps) {
        return this._applyDelta(nSteps * SLIDER_SCROLL_STEP);
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
            this._applyDelta(delta);
            return Clutter.EVENT_STOP;
        }
        return super.vfunc_key_press_event(event);
    }

    _moveHandle(x, _y) {
        const rtl = this.get_text_direction() === Clutter.TextDirection.RTL;
        const width = this._barLevelWidth;

        let relX = x;
        if (rtl)
            relX = width - relX;

        let newvalue;
        if (relX < this._handleRadius)
            newvalue = 0;
        else if (relX > width - this._handleRadius)
            newvalue = 1;
        else
            newvalue = (relX - this._handleRadius) / (width - 2 * this._handleRadius);
        this._unsnappedValue = newvalue * this._maxValue;
        this.value = this._snapToMark(this._unsnappedValue);
    }

    _getMinimumIncrement() {
        return 0.1;
    }
});
