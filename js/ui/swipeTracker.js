// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported SwipeTracker */

const { Clutter, Gio, GObject, Meta } = imports.gi;

const Signals = imports.signals;

const Main = imports.ui.main;

const TOUCHPAD_BASE_DISTANCE = 400;
const SCROLL_MULTIPLIER = 10;
const SWIPE_MULTIPLIER = 0.5;

const MIN_ANIMATION_DURATION = 100;
const MAX_ANIMATION_DURATION = 400;
const VELOCITY_THRESHOLD = 0.001;
const DURATION_MULTIPLIER = 3;
const ANIMATION_BASE_VELOCITY = 0.002;

var State = {
    NONE: 0,
    SCROLLING: 1,
};

function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
}

var TouchpadSwipeGesture = class TouchpadSwipeGesture {
    constructor(shouldSkip) {
        this._shouldSkip = shouldSkip;
        this._touchpadSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.peripherals.touchpad' });
        this._orientation = Clutter.Orientation.VERTICAL;

        global.stage.connect('captured-event', this._handleEvent.bind(this));
    }

    _handleEvent(_actor, event) {
        if (event.type() != Clutter.EventType.TOUCHPAD_SWIPE)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_touchpad_gesture_finger_count() != 4)
            return Clutter.EVENT_PROPAGATE;

        if (this._shouldSkip())
            return Clutter.EVENT_PROPAGATE;

        let time = event.get_time();

        let [x, y] = event.get_coords();
        let [dx, dy] = event.get_gesture_motion_delta();
        let delta = (this._orientation == Clutter.Orientation.VERTICAL) ? dy : dx;

        switch (event.get_gesture_phase()) {
        case Clutter.TouchpadGesturePhase.BEGIN:
            this.emit('begin', time, x, y);
            break;

        case Clutter.TouchpadGesturePhase.UPDATE:
            if (!(this._touchpadSettings.get_boolean('natural-scroll')))
                delta = -delta;

            this.emit('update', time, -delta / TOUCHPAD_BASE_DISTANCE * SWIPE_MULTIPLIER);
            break;

        case Clutter.TouchpadGesturePhase.END:
        case Clutter.TouchpadGesturePhase.CANCEL:
            this.emit('end', time);
            break;
        }

        return Clutter.EVENT_STOP;
    }

    setOrientation(orientation) {
        this._orientation = orientation;
    }
};
Signals.addSignalMethods(TouchpadSwipeGesture.prototype);

var TouchSwipeGesture = GObject.registerClass({
    Signals: { 'begin':  { param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE, GObject.TYPE_DOUBLE] },
               'update': { param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE] },
               'end':    { param_types: [GObject.TYPE_UINT] },
               'cancel': { param_types: [GObject.TYPE_UINT] } },
}, class TouchSwipeGesture extends Clutter.TriggerAction {
    _init(shouldSkip, nTouchPoints, triggerEdge = Clutter.TriggerEdge.NONE) {
        super._init();
        this.set_n_touch_points(nTouchPoints);
        this.set_trigger_edge(triggerEdge);

        this._shouldSkip = shouldSkip;
        this._distance = global.screen_height;
        this._orientation = Clutter.Orientation.VERTICAL;
    }

    vfunc_gesture_begin(actor, point) {
        if (!super.vfunc_gesture_begin(actor, point))
            return false;

        if (this._shouldSkip())
            return false;

        let time = this.get_last_event(point).get_time();
        let [x, y] = this.get_press_coords(point);

        this.emit('begin', time, x, y);
        return true;
    }

    // TODO: track center of the fingers instead of the first one
    vfunc_gesture_progress(_actor, point) {
        if (point != 0)
            return;

        let [distance_, dx, dy] = this.get_motion_delta(0);
        let time = this.get_last_event(point).get_time();
        let delta = (this._orientation == Clutter.Orientation.VERTICAL) ? dy : dx;

        this.emit('update', time, -delta / this._distance);
    }

    vfunc_gesture_end(_actor, point) {
        let time = this.get_last_event(point).get_time();

        this.emit('end', time);
    }

    vfunc_gesture_cancel(_actor) {
        let time = Clutter.get_current_event_time();

        this.emit('cancel', time);
    }

    setDistance(distance) {
        this._distance = distance;
    }

    setOrientation(orientation) {
        this._orientation = orientation;
    }
});

var ScrollGesture = class ScrollGesture {
    constructor(actor, shouldSkip) {
        this._shouldSkip = shouldSkip;
        this._began = false;
        this._orientation = Clutter.Orientation.VERTICAL;

        actor.connect('scroll-event', this._handleEvent.bind(this));
    }

    _handleEvent(_actor, event) {
        if (event.type() != Clutter.EventType.SCROLL)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_scroll_direction() != Clutter.ScrollDirection.SMOOTH)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_scroll_source() != Clutter.ScrollSource.FINGER && event.get_source_device().get_device_type() != Clutter.InputDeviceType.TOUCHPAD_DEVICE)
            return Clutter.EVENT_PROPAGATE;

        if (this._shouldSkip())
            return Clutter.EVENT_PROPAGATE;

        let time = event.get_time();
        let [dx, dy] = event.get_scroll_delta();
        if (dx == 0 && dy == 0) {
            this.emit('end', time);
            this._began = false;
            return Clutter.EVENT_STOP;
        }

        if (!this._began) {
            let [x, y] = event.get_coords();
            this.emit('begin', time, x, y);
            this._began = true;
        }

        let delta = (this._orientation == Clutter.Orientation.VERTICAL) ? dy : dx;
        this.emit('update', time, delta / TOUCHPAD_BASE_DISTANCE * SCROLL_MULTIPLIER);

        return Clutter.EVENT_STOP;
    }

    setOrientation(orientation) {
        this._orientation = orientation;
    }
};
Signals.addSignalMethods(ScrollGesture.prototype);

// USAGE:
//
// To correctly implement the gesture, the implementer must implement handlers for the
// following three signals with the following behavior:
//
// begin(tracker, monitor)
//   The handler should check whether a deceleration animation is currently
//   running. If it is, it should stop the animation (without resetting progress).
//   Then it should call tracker.confirmSwipe(distance, snapPoints, currentProgress, cancelProgress).
//   If it's not called, the swipe would be ignored.
//   The parameters are:
//    * distance: the page size;
//    * snapPoints: an (sorted with ascending order) array of snap points;
//    * currentProgress: the current progress;
//    * cancelprogress: a non-transient value that would be used if the gesture is cancelled.
//   If no animation was running, currentProgress and cancelProgress should be same.
//   The handler may set 'orientation' property here.
//
// update(tracker, progress)
//   The handler should set the progress to the given value.
//
// end(tracker, duration, endProgress)
//   The handler should animate the progress to endProgress. If endProgress is 0, it
//   should do nothing after the animation, otherwise it should change the state,
//   e.g. change the current page or switch workspace.
//   NOTE: duration can be 0 in some cases, in this case it should finish instantly.
//
// ======================================================================================
//
// 'enabled'
//   This property can be used to enable or disable the swipe tracker temporarily.

var SwipeTracker = class {
    constructor(actor, allowedModes, allowDrag = true, allowScroll = true) {
        this._allowedModes = allowedModes;
        this._enabled = true;
        this._orientation = Clutter.Orientation.VERTICAL;

        this._reset();

        let shouldSkip = () =>
            ((this._allowedModes & Main.actionMode) == 0 || !this._enabled);

        this._touchpadGesture = new TouchpadSwipeGesture(shouldSkip);
        this._touchpadGesture.connect('begin', this._beginGesture.bind(this));
        this._touchpadGesture.connect('update', this._updateGesture.bind(this));
        this._touchpadGesture.connect('end', this._endGesture.bind(this));
        // this._touchpadGesture.connect('cancel', this._cancelGesture.bind(this)); // End the gesture normally for touchpads

        this._touchGesture = new TouchSwipeGesture(shouldSkip, 4, Clutter.TriggerEdge.NONE);
        this._touchGesture.connect('begin', this._beginGesture.bind(this));
        this._touchGesture.connect('update', this._updateGesture.bind(this));
        this._touchGesture.connect('end', this._endGesture.bind(this));
        this._touchGesture.connect('cancel', this._cancelGesture.bind(this));
        global.stage.add_action(this._touchGesture);

        if (allowDrag) {
            this._dragGesture = new TouchSwipeGesture(shouldSkip, 1, Clutter.TriggerEdge.AFTER);
            this._dragGesture.connect('begin', this._beginGesture.bind(this));
            this._dragGesture.connect('update', this._updateGesture.bind(this));
            this._dragGesture.connect('end', this._endGesture.bind(this));
            this._dragGesture.connect('cancel', this._cancelGesture.bind(this));
            actor.add_action(this._dragGesture);
        } else
            this._dragGesture = null;

        if (allowScroll) {
            this._scrollGesture = new ScrollGesture(actor, shouldSkip);
            this._scrollGesture.connect('begin', this._beginGesture.bind(this));
            this._scrollGesture.connect('update', this._updateGesture.bind(this));
            this._scrollGesture.connect('end', this._endGesture.bind(this));
        } else
            this._scrollGesture = null;

        this._prevUpdate = 0;
    }

    get enabled() {
        return this._enabled;
    }

    set enabled(enabled) {
        if (this._enabled == enabled)
            return;

        this._enabled = enabled;
        if (!enabled && this._state == State.SCROLLING)
            this._cancel();
    }

    get orientation() {
        return this._orientation;
    }

    set orientation(orientation) {
        if (this._orientation == orientation)
            return;

        this._orientation = orientation;

        this._touchpadGesture.setOrientation(orientation);
        this._touchGesture.setOrientation(orientation);
        if (this._dragGesture)
            this._dragGesture.setOrientation(orientation);
        if (this._scrollGesture)
            this._scrollGesture.setOrientation(orientation);
    }

    _reset() {
        this._state = State.NONE;

        this._snapPoints = [];
        this._distance = 0;
        this._initialProgress = 0;
        this._cancelProgress = 0;

        this._prevOffset = 0;
        this._progress = 0;

        this._prevTime = 0;
        this._velocity = 0;

        this._cancelled = false;
    }

    _cancel() {
        this.emit('end', 0, this._ancelProgress);
        this._reset();
    }

    _beginGesture(_gesture, time, x, y) {
        if (this._state == State.SCROLLING)
            return;

        this._prevTime = time;

        let rect = new Meta.Rectangle({ x: x, y: y, width: 1, height: 1 });
        let monitor = global.display.get_monitor_index_for_rect(rect);

        this.emit('begin', monitor);
    }

    _updateGesture(_gesture, time, delta) {
        if ((this._allowedModes & Main.actionMode) == 0 || !this._enabled)
            return;

        if (this._state != State.SCROLLING)
            return;

        if (this.orientation == Clutter.Orientation.HORIZONTAL &&
            Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
            delta = -delta;

        this._progress += delta;

        if (time != this._prevTime)
            this._velocity = delta / (time - this._prevTime);

        let firstPoint = this._snapPoints[0];
        let lastPoint = this._snapPoints[this._snapPoints.length - 1];
        this._progress = clamp(this._progress, firstPoint, lastPoint);
        this._progress = clamp(this._progress, this._initialProgress - 1, this._initialProgress + 1);

        if (this._prevUpdate != 0)
            Meta.later_remove(this._prevUpdate);
        this._prevUpdate = Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
            this.emit('update', this._progress);
            this._prevUpdate = 0;
        });

        this._prevTime = time;
    }

    _getClosestSnapPoints() {
        let upper, lower;

        for (let i = 0; i < this._snapPoints.length; i++) {
            if (this._snapPoints[i] >= this._progress) {
                upper = this._snapPoints[i];
                break;
            }
        }

        for (let i = this._snapPoints.length - 1; i >= 0; i--) {
            if (this._snapPoints[i] <= this._progress) {
                lower = this._snapPoints[i];
                break;
            }
        }

        return [upper, lower];
    }

    _getEndProgress() {
        if (this._cancelled)
            return this._cancelProgress;

        let [upper, lower] = this._getClosestSnapPoints();
        let middle = (upper + lower) / 2;

        if (this._progress > middle)
            return (this._velocity * this._distance > -VELOCITY_THRESHOLD ||
                    this._initialProgress > upper) ? upper : lower;

        return (this._velocity * this._distance < VELOCITY_THRESHOLD ||
                this._initialProgress < lower) ? lower : upper;
    }

    _endGesture(_gesture, _time) {
        if ((this._allowedModes & Main.actionMode) == 0 || !this._enabled) {
            // this._cancel();
            return;
        }

        if (this._state != State.SCROLLING)
            return;

        let endProgress = this._getEndProgress();

        let velocity = ANIMATION_BASE_VELOCITY;
        if ((endProgress - this._progress) * this._velocity > 0)
            velocity = this._velocity;

        let duration = Math.abs((this._progress - endProgress) / velocity * DURATION_MULTIPLIER);
        duration = clamp(duration, MIN_ANIMATION_DURATION, MAX_ANIMATION_DURATION);

        this.emit('end', duration, endProgress);
        this._reset();
    }

    _cancelGesture(gesture, time) {
        if (this._state != State.SCROLLING)
            return;

        this._cancelled = true;
        this._endGesture(gesture, time);
    }

    confirmSwipe(distance, snapPoints, currentProgress, cancelProgress) {
        this._distance = distance;
        this._snapPoints = snapPoints;
        this._initialProgress = currentProgress;
        this._progress = currentProgress;
        this._cancelProgress = cancelProgress;

        this._touchGesture.setDistance(distance);
        if (this._dragGesture)
            this._dragGesture.setDistance(distance);

        this._velocity = 0;
        this._state = State.SCROLLING;
    }
};
Signals.addSignalMethods(SwipeTracker.prototype);
