import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import Mtk from 'gi://Mtk';

import * as Main from './main.js';
import * as Params from '../misc/params.js';

// FIXME: ideally these values matches physical touchpad size. We can get the
// correct values for gnome-shell specifically, since mutter uses libinput
// directly, but GTK apps cannot get it, so use an arbitrary value so that
// it's consistent with apps.
const TOUCHPAD_BASE_HEIGHT = 300;
const TOUCHPAD_BASE_WIDTH = 400;

const EVENT_HISTORY_THRESHOLD_MS = 150;

const SCROLL_MULTIPLIER = 10;

const MIN_ANIMATION_DURATION = 100;
const MAX_ANIMATION_DURATION = 400;
const VELOCITY_THRESHOLD_TOUCH = 0.3;
const VELOCITY_THRESHOLD_TOUCHPAD = 0.6;
const DECELERATION_TOUCH = 0.998;
const DECELERATION_TOUCHPAD = 0.997;
const VELOCITY_CURVE_THRESHOLD = 2;
const DECELERATION_PARABOLA_MULTIPLIER = 0.35;
const DRAG_THRESHOLD_DISTANCE = 16;

// Derivative of easeOutCubic at t=0
const DURATION_MULTIPLIER = 3;
const ANIMATION_BASE_VELOCITY = 0.002;
const EPSILON = 0.005;

const GESTURE_FINGER_COUNT = 3;

/** @enum {number} */
const State = {
    NONE: 0,
    SCROLLING: 1,
};

const TouchpadState = {
    NONE: 0,
    PENDING: 1,
    HANDLING: 2,
    IGNORED: 3,
};

const EventHistory = class {
    constructor() {
        this.reset();
    }

    reset() {
        this._data = [];
    }

    trim(time) {
        const thresholdTime = time - EVENT_HISTORY_THRESHOLD_MS;
        const index = this._data.findIndex(r => r.time >= thresholdTime);

        this._data.splice(0, index);
    }

    append(time, delta) {
        this.trim(time);

        this._data.push({time, delta});
    }

    calculateVelocity() {
        if (this._data.length < 2)
            return 0;

        const firstTime = this._data[0].time;
        const lastTime = this._data[this._data.length - 1].time;

        if (firstTime === lastTime)
            return 0;

        const totalDelta = this._data.slice(1).map(a => a.delta).reduce((a, b) => a + b);
        const period = lastTime - firstTime;

        return totalDelta / period;
    }
};

const TouchpadSwipeGesture = GObject.registerClass({
    Properties: {
        'enabled': GObject.ParamSpec.boolean(
            'enabled', null, null,
            GObject.ParamFlags.READWRITE,
            true),
        'orientation': GObject.ParamSpec.enum(
            'orientation', null, null,
            GObject.ParamFlags.READWRITE,
            Clutter.Orientation, Clutter.Orientation.HORIZONTAL),
    },
    Signals: {
        'begin':  {param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE, GObject.TYPE_DOUBLE]},
        'update': {param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE, GObject.TYPE_DOUBLE]},
        'end':    {param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE]},
    },
}, class TouchpadSwipeGesture extends GObject.Object {
    _init(allowedModes) {
        super._init();
        this._allowedModes = allowedModes;
        this._state = TouchpadState.NONE;
        this._cumulativeX = 0;
        this._cumulativeY = 0;
        this._touchpadSettings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.peripherals.touchpad',
        });

        global.stage.connectObject(
            'captured-event::touchpad', this._handleEvent.bind(this), this);
    }

    _handleEvent(actor, event) {
        if (event.type() !== Clutter.EventType.TOUCHPAD_SWIPE)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_gesture_phase() === Clutter.TouchpadGesturePhase.BEGIN)
            this._state = TouchpadState.NONE;

        if (event.get_touchpad_gesture_finger_count() < GESTURE_FINGER_COUNT)
            return Clutter.EVENT_PROPAGATE;

        if ((this._allowedModes & Main.actionMode) === 0)
            return Clutter.EVENT_PROPAGATE;

        if (!this.enabled)
            return Clutter.EVENT_PROPAGATE;

        if (this._state === TouchpadState.IGNORED)
            return Clutter.EVENT_PROPAGATE;

        let time = event.get_time();

        const [x, y] = event.get_coords();
        const [dx, dy] = event.get_gesture_motion_delta_unaccelerated();

        if (this._state === TouchpadState.NONE) {
            if (dx === 0 && dy === 0)
                return Clutter.EVENT_PROPAGATE;

            this._cumulativeX = 0;
            this._cumulativeY = 0;
            this._state = TouchpadState.PENDING;
        }

        if (this._state === TouchpadState.PENDING) {
            this._cumulativeX += dx;
            this._cumulativeY += dy;

            const cdx = this._cumulativeX;
            const cdy = this._cumulativeY;
            const distance = Math.sqrt(cdx * cdx + cdy * cdy);

            if (distance >= DRAG_THRESHOLD_DISTANCE) {
                const gestureOrientation = Math.abs(cdx) > Math.abs(cdy)
                    ? Clutter.Orientation.HORIZONTAL
                    : Clutter.Orientation.VERTICAL;

                this._cumulativeX = 0;
                this._cumulativeY = 0;

                if (gestureOrientation === this.orientation) {
                    this._state = TouchpadState.HANDLING;
                    this.emit('begin', time, x, y);
                } else {
                    this._state = TouchpadState.IGNORED;
                    return Clutter.EVENT_PROPAGATE;
                }
            } else {
                return Clutter.EVENT_PROPAGATE;
            }
        }

        const vertical = this.orientation === Clutter.Orientation.VERTICAL;
        let delta = vertical ? dy : dx;
        const distance = vertical ? TOUCHPAD_BASE_HEIGHT : TOUCHPAD_BASE_WIDTH;

        switch (event.get_gesture_phase()) {
        case Clutter.TouchpadGesturePhase.BEGIN:
        case Clutter.TouchpadGesturePhase.UPDATE:
            if (this._touchpadSettings.get_boolean('natural-scroll'))
                delta = -delta;

            this.emit('update', time, delta, distance);
            break;

        case Clutter.TouchpadGesturePhase.END:
        case Clutter.TouchpadGesturePhase.CANCEL:
            this.emit('end', time, distance);
            this._state = TouchpadState.NONE;
            break;
        }

        return this._state === TouchpadState.HANDLING
            ? Clutter.EVENT_STOP
            : Clutter.EVENT_PROPAGATE;
    }

    destroy() {
        global.stage.disconnectObject(this);
    }
});

const ScrollGesture = GObject.registerClass({
    Properties: {
        'enabled': GObject.ParamSpec.boolean(
            'enabled', null, null,
            GObject.ParamFlags.READWRITE,
            true),
        'orientation': GObject.ParamSpec.enum(
            'orientation', null, null,
            GObject.ParamFlags.READWRITE,
            Clutter.Orientation, Clutter.Orientation.HORIZONTAL),
        'scroll-modifiers': GObject.ParamSpec.flags(
            'scroll-modifiers', null, null,
            GObject.ParamFlags.READWRITE,
            Clutter.ModifierType, 0),
    },
    Signals: {
        'begin':  {param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE, GObject.TYPE_DOUBLE]},
        'update': {param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE, GObject.TYPE_DOUBLE]},
        'end':    {param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE]},
    },
}, class ScrollGesture extends GObject.Object {
    _init(actor, allowedModes) {
        super._init();
        this._allowedModes = allowedModes;
        this._began = false;
        this._enabled = true;

        actor.connect('scroll-event', this._handleEvent.bind(this));
    }

    get enabled() {
        return this._enabled;
    }

    set enabled(enabled) {
        if (this._enabled === enabled)
            return;

        this._enabled = enabled;
        this._began = false;

        this.notify('enabled');
    }

    canHandleEvent(event) {
        if (event.type() !== Clutter.EventType.SCROLL)
            return false;

        if (event.get_scroll_source() !== Clutter.ScrollSource.FINGER &&
            event.get_source_device().get_device_type() !== Clutter.InputDeviceType.TOUCHPAD_DEVICE)
            return false;

        if (!this.enabled)
            return false;

        if ((this._allowedModes & Main.actionMode) === 0)
            return false;

        if (!this._began && this.scrollModifiers !== 0 &&
            (event.get_state() & this.scrollModifiers) !== this.scrollModifiers)
            return false;

        return true;
    }

    _handleEvent(actor, event) {
        if (!this.canHandleEvent(event))
            return Clutter.EVENT_PROPAGATE;

        if (event.get_scroll_direction() !== Clutter.ScrollDirection.SMOOTH)
            return Clutter.EVENT_PROPAGATE;

        const vertical = this.orientation === Clutter.Orientation.VERTICAL;
        const distance = vertical ? TOUCHPAD_BASE_HEIGHT : TOUCHPAD_BASE_WIDTH;

        let time = event.get_time();
        let [dx, dy] = event.get_scroll_delta();
        if (dx === 0 && dy === 0) {
            this.emit('end', time, distance);
            this._began = false;
            return Clutter.EVENT_STOP;
        }

        if (!this._began) {
            let [x, y] = event.get_coords();
            this.emit('begin', time, x, y);
            this._began = true;
        }

        const delta = (vertical ? dy : dx) * SCROLL_MULTIPLIER;

        this.emit('update', time, delta, distance);

        return Clutter.EVENT_STOP;
    }
});

// USAGE:
//
// To correctly implement the gesture, there must be handlers for the following
// signals:
//
// begin(tracker, monitor)
//   The handler should check whether a deceleration animation is currently
//   running. If it is, it should stop the animation (without resetting
//   progress). Then it should call:
//   tracker.confirmSwipe(distance, snapPoints, currentProgress, cancelProgress)
//   If it's not called, the swipe would be ignored.
//   The parameters are:
//    * distance: the page size;
//    * snapPoints: an (sorted with ascending order) array of snap points;
//    * currentProgress: the current progress;
//    * cancelprogress: a non-transient value that would be used if the gesture
//      is cancelled.
//   If no animation was running, currentProgress and cancelProgress should be
//   same. The handler may set 'orientation' property here.
//
// update(tracker, progress)
//   The handler should set the progress to the given value.
//
// end(tracker, duration, endProgress)
//   The handler should animate the progress to endProgress. If endProgress is
//   0, it should do nothing after the animation, otherwise it should change the
//   state, e.g. change the current page or switch workspace.
//   NOTE: duration can be 0 in some cases, in this case it should finish
//   instantly.

/** A class for handling swipe gestures */
export const SwipeTracker = GObject.registerClass({
    Properties: {
        'enabled': GObject.ParamSpec.boolean(
            'enabled', null, null,
            GObject.ParamFlags.READWRITE,
            true),
        'orientation': GObject.ParamSpec.enum(
            'orientation', null, null,
            GObject.ParamFlags.READWRITE,
            Clutter.Orientation, Clutter.Orientation.HORIZONTAL),
        'distance': GObject.ParamSpec.double(
            'distance', null, null,
            GObject.ParamFlags.READWRITE,
            0, Infinity, 0),
        'allow-long-swipes': GObject.ParamSpec.boolean(
            'allow-long-swipes', null, null,
            GObject.ParamFlags.READWRITE,
            false),
        'scroll-modifiers': GObject.ParamSpec.flags(
            'scroll-modifiers', null, null,
            GObject.ParamFlags.READWRITE,
            Clutter.ModifierType, 0),
    },
    Signals: {
        'begin':  {param_types: [GObject.TYPE_UINT]},
        'update': {param_types: [GObject.TYPE_DOUBLE]},
        'end':    {param_types: [GObject.TYPE_UINT64, GObject.TYPE_DOUBLE]},
    },
}, class SwipeTracker extends GObject.Object {
    _init(actor, orientation, allowedModes, params) {
        params = Params.parse(params, {
            allowDrag: true,
            allowScroll: true,
            phase: Clutter.EventPhase.BUBBLE,
            name: 'SwipeTracker',
        });

        super._init();

        this.orientation = orientation;
        this._allowedModes = allowedModes;
        this._enabled = true;
        this._distance = global.screen_height;
        this._history = new EventHistory();
        this._reset();

        this._panGesture = new Clutter.PanGesture({
            min_n_points: params.allowDrag ? 1 : GESTURE_FINGER_COUNT,
            max_n_points: params.allowDrag ? 0 : GESTURE_FINGER_COUNT,
        });
        this._panGesture.connect('may-recognize', () => {
            return (this._allowedModes & Main.actionMode) !== 0;
        });
        this._panGesture.connect('recognize', this._beginPanGesture.bind(this));
        this._panGesture.connect('pan-update', this._updatePanGesture.bind(this));
        this._panGesture.connect('end', this._endPanGesture.bind(this));
        this._panGesture.connect('cancel', this._cancelPanGesture.bind(this));
        this.bind_property('enabled', this._panGesture, 'enabled', 0);
        this.bind_property_full('orientation',
            this._panGesture, 'pan-axis',
            GObject.BindingFlags.SYNC_CREATE,
            (bind, source) => [
                true,
                source === Clutter.Orientation.HORIZONTAL
                    ? Clutter.PanAxis.X : Clutter.PanAxis.Y,
            ],
            null);
        actor.add_action_full(params.name, params.phase, this._panGesture);

        this._touchpadGesture = new TouchpadSwipeGesture(allowedModes);
        this._touchpadGesture.connect('begin', this._beginTouchpadGesture.bind(this));
        this._touchpadGesture.connect('update', this._updateTouchpadGesture.bind(this));
        this._touchpadGesture.connect('end', this._endTouchpadGesture.bind(this));
        this.bind_property('enabled', this._touchpadGesture, 'enabled', 0);
        this.bind_property('orientation', this._touchpadGesture, 'orientation',
            GObject.BindingFlags.SYNC_CREATE);

        if (params.allowScroll) {
            this._scrollGesture = new ScrollGesture(actor, allowedModes);
            this._scrollGesture.connect('begin', this._beginTouchpadGesture.bind(this));
            this._scrollGesture.connect('update', this._updateTouchpadGesture.bind(this));
            this._scrollGesture.connect('end', this._endTouchpadGesture.bind(this));
            this.bind_property('enabled', this._scrollGesture, 'enabled', 0);
            this.bind_property('orientation', this._scrollGesture, 'orientation',
                GObject.BindingFlags.SYNC_CREATE);
            this.bind_property('scroll-modifiers',
                this._scrollGesture, 'scroll-modifiers', 0);
        } else {
            this._scrollGesture = null;
        }
    }

    /**
     * canHandleScrollEvent:
     * This function can be used to combine swipe gesture and mouse
     * scrolling.
     *
     * @param {Clutter.Event} scrollEvent an event to check
     * @returns {boolean} whether the event can be handled by the tracker
     */
    canHandleScrollEvent(scrollEvent) {
        if (!this.enabled || this._scrollGesture === null)
            return false;

        return this._scrollGesture.canHandleEvent(scrollEvent);
    }

    get enabled() {
        return this._enabled;
    }

    set enabled(enabled) {
        if (this._enabled === enabled)
            return;

        this._enabled = enabled;
        if (!enabled && this._state === State.SCROLLING)
            this._interrupt();
        this.notify('enabled');
    }

    get distance() {
        return this._distance;
    }

    set distance(distance) {
        if (this._distance === distance)
            return;

        this._distance = distance;
        this.notify('distance');
    }

    _reset() {
        this._state = State.NONE;

        this._snapPoints = [];
        this._initialProgress = 0;
        this._cancelProgress = 0;

        this._prevOffset = 0;
        this._progress = 0;

        this._cancelled = false;

        this._history.reset();
    }

    _interrupt() {
        this.emit('end', 0, this._cancelProgress);
        this._panGesture.cancel();
        this._reset();
    }

    _beginGesture(x, y) {
        if (this._state === State.SCROLLING)
            return;

        const rect = new Mtk.Rectangle({x, y, width: 1, height: 1});
        let monitor = global.display.get_monitor_index_for_rect(rect);

        this.emit('begin', monitor);
    }

    _beginPanGesture(panGesture) {
        const coords = panGesture.get_begin_centroid();

        this._beginGesture(coords.x, coords.y);
    }

    _beginTouchpadGesture(gesture, time, x, y) {
        this._history.append(time, 0);

        this._beginGesture(x, y);
    }

    _findClosestPoint(pos) {
        const distances = this._snapPoints.map(x => Math.abs(x - pos));
        const min = Math.min(...distances);
        return distances.indexOf(min);
    }

    _findNextPoint(pos) {
        return this._snapPoints.findIndex(p => p >= pos);
    }

    _findPreviousPoint(pos) {
        const reversedIndex = this._snapPoints.slice().reverse().findIndex(p => p <= pos);
        return this._snapPoints.length - 1 - reversedIndex;
    }

    _findPointForProjection(pos, velocity) {
        const initial = this._findClosestPoint(this._initialProgress);
        const prev = this._findPreviousPoint(pos);
        const next = this._findNextPoint(pos);

        if ((velocity > 0 ? prev : next) === initial)
            return velocity > 0 ? next : prev;

        return this._findClosestPoint(pos);
    }

    _getBounds(pos) {
        if (this.allowLongSwipes)
            return [this._snapPoints[0], this._snapPoints[this._snapPoints.length - 1]];

        const closest = this._findClosestPoint(pos);

        let prev, next;
        if (Math.abs(this._snapPoints[closest] - pos) < EPSILON) {
            prev = next = closest;
        } else {
            prev = this._findPreviousPoint(pos);
            next = this._findNextPoint(pos);
        }

        const lowerIndex = Math.max(prev - 1, 0);
        const upperIndex = Math.min(next + 1, this._snapPoints.length - 1);

        return [this._snapPoints[lowerIndex], this._snapPoints[upperIndex]];
    }

    _updateGesture(delta, distance) {
        if (this._state !== State.SCROLLING)
            return;

        if (this.orientation === Clutter.Orientation.HORIZONTAL &&
            Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
            delta = -delta;

        this._progress += delta / distance;

        this._progress = Math.clamp(this._progress, ...this._getBounds(this._initialProgress));

        this.emit('update', this._progress);
    }

    _updatePanGesture(panGesture) {
        const deltaVec = panGesture.get_delta_abs();
        const delta = this.orientation === Clutter.Orientation.HORIZONTAL
            ? -deltaVec.get_x()
            : -deltaVec.get_y();

        this._updateGesture(delta, this._distance);
    }

    _updateTouchpadGesture(gesture, time, delta, distance) {
        if ((this._allowedModes & Main.actionMode) === 0 || !this.enabled) {
            this._interrupt();
            return;
        }

        this._history.append(time, delta);
        this._updateGesture(delta, distance);
    }

    _getEndProgress(velocity, distance, isTouchpad) {
        if (this._cancelled)
            return this._cancelProgress;

        const threshold = isTouchpad ? VELOCITY_THRESHOLD_TOUCHPAD : VELOCITY_THRESHOLD_TOUCH;

        if (Math.abs(velocity) < threshold)
            return this._snapPoints[this._findClosestPoint(this._progress)];

        const decel = isTouchpad ? DECELERATION_TOUCHPAD : DECELERATION_TOUCH;
        const slope = decel / (1.0 - decel) / 1000.0;

        let pos;
        if (Math.abs(velocity) > VELOCITY_CURVE_THRESHOLD) {
            const c = slope / 2 / DECELERATION_PARABOLA_MULTIPLIER;
            const x = Math.abs(velocity) - VELOCITY_CURVE_THRESHOLD + c;

            pos = slope * VELOCITY_CURVE_THRESHOLD +
                DECELERATION_PARABOLA_MULTIPLIER * x * x -
                DECELERATION_PARABOLA_MULTIPLIER * c * c;
        } else {
            pos = Math.abs(velocity) * slope;
        }

        pos = pos * Math.sign(velocity) + this._progress;
        pos = Math.clamp(pos, ...this._getBounds(this._initialProgress));

        const index = this._findPointForProjection(pos, velocity);

        return this._snapPoints[index];
    }

    _endGesture(velocity, distance, isTouchpad) {
        if (this._state !== State.SCROLLING)
            return;

        if ((this._allowedModes & Main.actionMode) === 0 || !this.enabled) {
            this._interrupt();
            return;
        }

        const endProgress = this._getEndProgress(velocity, distance, isTouchpad);

        velocity /= distance;

        if ((endProgress - this._progress) * velocity <= 0)
            velocity = ANIMATION_BASE_VELOCITY;

        const nPoints = Math.max(1, Math.ceil(Math.abs(this._progress - endProgress)));
        const maxDuration = MAX_ANIMATION_DURATION * Math.log2(1 + nPoints);

        let duration = Math.abs((this._progress - endProgress) / velocity * DURATION_MULTIPLIER);
        if (duration > 0)
            duration = Math.clamp(duration, MIN_ANIMATION_DURATION, maxDuration);

        this._reset();
        this.emit('end', duration, endProgress);
    }

    _endPanGesture(panGesture) {
        const velocity = panGesture.get_velocity();
        const v = this.orientation === Clutter.Orientation.HORIZONTAL
            ? -velocity.get_x()
            : -velocity.get_y();

        this._endGesture(v, this._distance, false);
    }

    _endTouchpadGesture(gesture, time, distance) {
        if ((this._allowedModes & Main.actionMode) === 0 || !this.enabled) {
            this._interrupt();
            return;
        }

        this._history.trim(time);
        const velocity = this._history.calculateVelocity();

        this._endGesture(velocity, distance, true);
    }

    _cancelPanGesture() {
        if (this._state !== State.SCROLLING)
            return;

        this._cancelled = true;
        this._endGesture(0, this._distance, false);
    }

    /**
     * Confirms a swipe. User has to call this in 'begin' signal handler,
     * otherwise the swipe wouldn't start. If there's an animation running,
     * it should be stopped first.
     *
     * `cancelProgress` must always be a snap point, or a value matching
     * some other non-transient state.
     *
     * @param {number} distance - swipe distance in pixels
     * @param {number[]} snapPoints - An array of snap points, sorted in ascending order
     * @param {number} currentProgress - initial progress value
     * @param {number} cancelProgress - the value to be used on cancelling
     */
    confirmSwipe(distance, snapPoints, currentProgress, cancelProgress) {
        this.distance = distance;
        this._snapPoints = snapPoints;
        this._initialProgress = currentProgress;
        this._progress = currentProgress;
        this._cancelProgress = cancelProgress;

        this._state = State.SCROLLING;
    }

    destroy() {
        if (this._touchpadGesture) {
            this._touchpadGesture.destroy();
            delete this._touchpadGesture;
        }

        if (this._panGesture) {
            this._panGesture.actor?.remove_action(this._panGesture);
            delete this._panGesture;
        }
    }
});
