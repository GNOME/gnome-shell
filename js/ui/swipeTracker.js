// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported SwipeTracker */

const { Clutter, Gio, GObject, Meta } = imports.gi;

const Main = imports.ui.main;
const Params = imports.misc.params;

// FIXME: ideally these values matches physical touchpad size. We can get the
// correct values for gnome-shell specifically, since mutter uses libinput
// directly, but GTK apps cannot get it, so use an arbitrary value so that
// it's consistent with apps.
const TOUCHPAD_BASE_HEIGHT = 300;
const TOUCHPAD_BASE_WIDTH = 400;

const SCROLL_MULTIPLIER = 10;
const SWIPE_MULTIPLIER = 0.5;

const MIN_ANIMATION_DURATION = 100;
const MAX_ANIMATION_DURATION = 400;
const VELOCITY_THRESHOLD = 0.4;
// Derivative of easeOutCubic at t=0
const DURATION_MULTIPLIER = 3;
const ANIMATION_BASE_VELOCITY = 0.002;

const State = {
    NONE: 0,
    SCROLLING: 1,
};

function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
}

const TouchpadSwipeGesture = GObject.registerClass({
    Properties: {
        'enabled': GObject.ParamSpec.boolean(
            'enabled', 'enabled', 'enabled',
            GObject.ParamFlags.READWRITE,
            true),
        'orientation': GObject.ParamSpec.enum(
            'orientation', 'orientation', 'orientation',
            GObject.ParamFlags.READWRITE,
            Clutter.Orientation, Clutter.Orientation.VERTICAL),
    },
    Signals: {
        'begin':  { param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE, GObject.TYPE_DOUBLE] },
        'update': { param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE] },
        'end':    { param_types: [GObject.TYPE_UINT] },
    },
}, class TouchpadSwipeGesture extends GObject.Object {
    _init(allowedModes) {
        super._init();
        this._allowedModes = allowedModes;
        this._touchpadSettings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.peripherals.touchpad',
        });
        this._orientation = Clutter.Orientation.VERTICAL;
        this._enabled = true;

        global.stage.connect('captured-event::touchpad', this._handleEvent.bind(this));
    }

    get enabled() {
        return this._enabled;
    }

    set enabled(enabled) {
        if (this._enabled === enabled)
            return;

        this._enabled = enabled;
        this.notify('enabled');
    }

    get orientation() {
        return this._orientation;
    }

    set orientation(orientation) {
        if (this._orientation === orientation)
            return;

        this._orientation = orientation;
        this.notify('orientation');
    }

    _handleEvent(actor, event) {
        if (event.type() !== Clutter.EventType.TOUCHPAD_SWIPE)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_touchpad_gesture_finger_count() !== 4)
            return Clutter.EVENT_PROPAGATE;

        if ((this._allowedModes & Main.actionMode) === 0)
            return Clutter.EVENT_PROPAGATE;

        if (!this.enabled)
            return Clutter.EVENT_PROPAGATE;

        let time = event.get_time();

        let [x, y] = event.get_coords();
        let [dx, dy] = event.get_gesture_motion_delta();

        let delta;
        if (this._orientation === Clutter.Orientation.VERTICAL)
            delta = dy / TOUCHPAD_BASE_HEIGHT;
        else
            delta = dx / TOUCHPAD_BASE_WIDTH;

        switch (event.get_gesture_phase()) {
        case Clutter.TouchpadGesturePhase.BEGIN:
            this.emit('begin', time, x, y);
            break;

        case Clutter.TouchpadGesturePhase.UPDATE:
            if (this._touchpadSettings.get_boolean('natural-scroll'))
                delta = -delta;

            this.emit('update', time, delta * SWIPE_MULTIPLIER);
            break;

        case Clutter.TouchpadGesturePhase.END:
        case Clutter.TouchpadGesturePhase.CANCEL:
            this.emit('end', time);
            break;
        }

        return Clutter.EVENT_STOP;
    }
});

const TouchSwipeGesture = GObject.registerClass({
    Properties: {
        'distance': GObject.ParamSpec.double(
            'distance', 'distance', 'distance',
            GObject.ParamFlags.READWRITE,
            0, Infinity, 0),
        'orientation': GObject.ParamSpec.enum(
            'orientation', 'orientation', 'orientation',
            GObject.ParamFlags.READWRITE,
            Clutter.Orientation, Clutter.Orientation.VERTICAL),
    },
    Signals: {
        'begin':  { param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE, GObject.TYPE_DOUBLE] },
        'update': { param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE] },
        'end':    { param_types: [GObject.TYPE_UINT] },
        'cancel': { param_types: [GObject.TYPE_UINT] },
    },
}, class TouchSwipeGesture extends Clutter.GestureAction {
    _init(allowedModes, nTouchPoints, thresholdTriggerEdge) {
        super._init();
        this.set_n_touch_points(nTouchPoints);
        this.set_threshold_trigger_edge(thresholdTriggerEdge);

        this._allowedModes = allowedModes;
        this._distance = global.screen_height;
        this._orientation = Clutter.Orientation.VERTICAL;

        global.display.connect('grab-op-begin', () => {
            this.cancel();
        });

        this._lastPosition = 0;
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

    get orientation() {
        return this._orientation;
    }

    set orientation(orientation) {
        if (this._orientation === orientation)
            return;

        this._orientation = orientation;
        this.notify('orientation');
    }

    vfunc_gesture_prepare(actor) {
        if (!super.vfunc_gesture_prepare(actor))
            return false;

        if ((this._allowedModes & Main.actionMode) === 0)
            return false;

        let time = this.get_last_event(0).get_time();
        let [xPress, yPress] = this.get_press_coords(0);
        let [x, y] = this.get_motion_coords(0);

        this._lastPosition =
            this._orientation === Clutter.Orientation.VERTICAL ? y : x;

        this.emit('begin', time, xPress, yPress);
        return true;
    }

    vfunc_gesture_progress(_actor) {
        let [x, y] = this.get_motion_coords(0);
        let pos = this._orientation === Clutter.Orientation.VERTICAL ? y : x;

        let delta = pos - this._lastPosition;
        this._lastPosition = pos;

        let time = this.get_last_event(0).get_time();

        this.emit('update', time, -delta / this._distance);

        return true;
    }

    vfunc_gesture_end(_actor) {
        let time = this.get_last_event(0).get_time();

        this.emit('end', time);
    }

    vfunc_gesture_cancel(_actor) {
        let time = Clutter.get_current_event_time();

        this.emit('cancel', time);
    }
});

const ScrollGesture = GObject.registerClass({
    Properties: {
        'enabled': GObject.ParamSpec.boolean(
            'enabled', 'enabled', 'enabled',
            GObject.ParamFlags.READWRITE,
            true),
        'orientation': GObject.ParamSpec.enum(
            'orientation', 'orientation', 'orientation',
            GObject.ParamFlags.READWRITE,
            Clutter.Orientation, Clutter.Orientation.VERTICAL),
    },
    Signals: {
        'begin':  { param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE, GObject.TYPE_DOUBLE] },
        'update': { param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE] },
        'end':    { param_types: [GObject.TYPE_UINT] },
    },
}, class ScrollGesture extends GObject.Object {
    _init(actor, allowedModes) {
        super._init();
        this._allowedModes = allowedModes;
        this._began = false;
        this._enabled = true;
        this._orientation = Clutter.Orientation.VERTICAL;

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

    get orientation() {
        return this._orientation;
    }

    set orientation(orientation) {
        if (this._orientation === orientation)
            return;

        this._orientation = orientation;
        this.notify('orientation');
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

        return true;
    }

    _handleEvent(actor, event) {
        if (!this.canHandleEvent(event))
            return Clutter.EVENT_PROPAGATE;

        if (event.get_scroll_direction() !== Clutter.ScrollDirection.SMOOTH)
            return Clutter.EVENT_PROPAGATE;

        let time = event.get_time();
        let [dx, dy] = event.get_scroll_delta();
        if (dx === 0 && dy === 0) {
            this.emit('end', time);
            this._began = false;
            return Clutter.EVENT_STOP;
        }

        if (!this._began) {
            let [x, y] = event.get_coords();
            this.emit('begin', time, x, y);
            this._began = true;
        }

        let delta;
        if (this._orientation === Clutter.Orientation.VERTICAL)
            delta = dy / TOUCHPAD_BASE_HEIGHT;
        else
            delta = dx / TOUCHPAD_BASE_WIDTH;

        this.emit('update', time, delta * SCROLL_MULTIPLIER);

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
var SwipeTracker = GObject.registerClass({
    Properties: {
        'enabled': GObject.ParamSpec.boolean(
            'enabled', 'enabled', 'enabled',
            GObject.ParamFlags.READWRITE,
            true),
        'orientation': GObject.ParamSpec.enum(
            'orientation', 'orientation', 'orientation',
            GObject.ParamFlags.READWRITE,
            Clutter.Orientation, Clutter.Orientation.VERTICAL),
        'distance': GObject.ParamSpec.double(
            'distance', 'distance', 'distance',
            GObject.ParamFlags.READWRITE,
            0, Infinity, 0),
    },
    Signals: {
        'begin':  { param_types: [GObject.TYPE_UINT] },
        'update': { param_types: [GObject.TYPE_DOUBLE] },
        'end':    { param_types: [GObject.TYPE_UINT64, GObject.TYPE_DOUBLE] },
    },
}, class SwipeTracker extends GObject.Object {
    _init(actor, allowedModes, params) {
        super._init();
        params = Params.parse(params, { allowDrag: true, allowScroll: true });

        this._allowedModes = allowedModes;
        this._enabled = true;
        this._orientation = Clutter.Orientation.VERTICAL;
        this._distance = global.screen_height;

        this._reset();

        this._touchpadGesture = new TouchpadSwipeGesture(allowedModes);
        this._touchpadGesture.connect('begin', this._beginGesture.bind(this));
        this._touchpadGesture.connect('update', this._updateGesture.bind(this));
        this._touchpadGesture.connect('end', this._endGesture.bind(this));
        this.bind_property('enabled', this._touchpadGesture, 'enabled', 0);
        this.bind_property('orientation', this._touchpadGesture, 'orientation', 0);

        this._touchGesture = new TouchSwipeGesture(allowedModes, 4,
            Clutter.GestureTriggerEdge.NONE);
        this._touchGesture.connect('begin', this._beginTouchSwipe.bind(this));
        this._touchGesture.connect('update', this._updateGesture.bind(this));
        this._touchGesture.connect('end', this._endGesture.bind(this));
        this._touchGesture.connect('cancel', this._cancelGesture.bind(this));
        this.bind_property('enabled', this._touchGesture, 'enabled', 0);
        this.bind_property('orientation', this._touchGesture, 'orientation', 0);
        this.bind_property('distance', this._touchGesture, 'distance', 0);
        global.stage.add_action(this._touchGesture);

        if (params.allowDrag) {
            this._dragGesture = new TouchSwipeGesture(allowedModes, 1,
                Clutter.GestureTriggerEdge.AFTER);
            this._dragGesture.connect('begin', this._beginGesture.bind(this));
            this._dragGesture.connect('update', this._updateGesture.bind(this));
            this._dragGesture.connect('end', this._endGesture.bind(this));
            this._dragGesture.connect('cancel', this._cancelGesture.bind(this));
            this.bind_property('enabled', this._dragGesture, 'enabled', 0);
            this.bind_property('orientation', this._dragGesture, 'orientation', 0);
            this.bind_property('distance', this._dragGesture, 'distance', 0);
            actor.add_action(this._dragGesture);
        } else {
            this._dragGesture = null;
        }

        if (params.allowScroll) {
            this._scrollGesture = new ScrollGesture(actor, allowedModes);
            this._scrollGesture.connect('begin', this._beginGesture.bind(this));
            this._scrollGesture.connect('update', this._updateGesture.bind(this));
            this._scrollGesture.connect('end', this._endGesture.bind(this));
            this.bind_property('enabled', this._scrollGesture, 'enabled', 0);
            this.bind_property('orientation', this._scrollGesture, 'orientation', 0);
        } else {
            this._scrollGesture = null;
        }
    }

    /**
     * canHandleScrollEvent:
     * @param {Clutter.Event} scrollEvent: an event to check
     * @returns {bool} whether the event can be handled by the tracker
     *
     * This function can be used to combine swipe gesture and mouse
     * scrolling.
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

    get orientation() {
        return this._orientation;
    }

    set orientation(orientation) {
        if (this._orientation === orientation)
            return;

        this._orientation = orientation;
        this.notify('orientation');
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

        this._prevTime = 0;
        this._velocity = 0;

        this._cancelled = false;
    }

    _interrupt() {
        this.emit('end', 0, this._cancelProgress);
        this._reset();
    }

    _beginTouchSwipe(gesture, time, x, y) {
        if (this._dragGesture)
            this._dragGesture.cancel();

        this._beginGesture(gesture, time, x, y);
    }

    _beginGesture(gesture, time, x, y) {
        if (this._state === State.SCROLLING)
            return;

        this._prevTime = time;

        let rect = new Meta.Rectangle({ x, y, width: 1, height: 1 });
        let monitor = global.display.get_monitor_index_for_rect(rect);

        this.emit('begin', monitor);
    }

    _updateGesture(gesture, time, delta) {
        if (this._state !== State.SCROLLING)
            return;

        if ((this._allowedModes & Main.actionMode) === 0 || !this.enabled) {
            this._interrupt();
            return;
        }

        if (this.orientation === Clutter.Orientation.HORIZONTAL &&
            Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
            delta = -delta;

        this._progress += delta;

        if (time !== this._prevTime)
            this._velocity = delta / (time - this._prevTime);

        let firstPoint = this._snapPoints[0];
        let lastPoint = this._snapPoints[this._snapPoints.length - 1];
        this._progress = clamp(this._progress, firstPoint, lastPoint);
        this._progress = clamp(this._progress,
            this._initialProgress - 1, this._initialProgress + 1);

        this.emit('update', this._progress);

        this._prevTime = time;
    }

    _getClosestSnapPoints() {
        let upper = this._snapPoints.find(p => p >= this._progress);
        let lower = this._snapPoints.slice().reverse().find(p => p <= this._progress);
        return [lower, upper];
    }

    _getEndProgress() {
        if (this._cancelled)
            return this._cancelProgress;

        let [lower, upper] = this._getClosestSnapPoints();
        let middle = (upper + lower) / 2;

        if (this._progress > middle) {
            let thresholdMet = this._velocity * this._distance > -VELOCITY_THRESHOLD;
            return thresholdMet || this._initialProgress > upper ? upper : lower;
        } else {
            let thresholdMet = this._velocity * this._distance < VELOCITY_THRESHOLD;
            return thresholdMet || this._initialProgress < lower ? lower : upper;
        }
    }

    _endGesture(_gesture, _time) {
        if (this._state !== State.SCROLLING)
            return;

        if ((this._allowedModes & Main.actionMode) === 0 || !this.enabled) {
            this._interrupt();
            return;
        }

        let endProgress = this._getEndProgress();

        let velocity = ANIMATION_BASE_VELOCITY;
        if ((endProgress - this._progress) * this._velocity > 0)
            velocity = this._velocity;

        let duration = Math.abs((this._progress - endProgress) / velocity * DURATION_MULTIPLIER);
        if (duration > 0) {
            duration = clamp(duration,
                MIN_ANIMATION_DURATION, MAX_ANIMATION_DURATION);
        }

        this.emit('end', duration, endProgress);
        this._reset();
    }

    _cancelGesture(gesture, time) {
        if (this._state !== State.SCROLLING)
            return;

        this._cancelled = true;
        this._endGesture(gesture, time);
    }

    /**
     * confirmSwipe:
     * @param {number} distance: swipe distance in pixels
     * @param {number[]} snapPoints:
     *     An array of snap points, sorted in ascending order
     * @param {number} currentProgress: initial progress value
     * @param {number} cancelProgress: the value to be used on cancelling
     *
     * Confirms a swipe. User has to call this in 'begin' signal handler,
     * otherwise the swipe wouldn't start. If there's an animation running,
     * it should be stopped first.
     *
     * @cancel_progress must always be a snap point, or a value matching
     * some other non-transient state.
     */
    confirmSwipe(distance, snapPoints, currentProgress, cancelProgress) {
        this.distance = distance;
        this._snapPoints = snapPoints;
        this._initialProgress = currentProgress;
        this._progress = currentProgress;
        this._cancelProgress = cancelProgress;

        this._velocity = 0;
        this._state = State.SCROLLING;
    }
});
