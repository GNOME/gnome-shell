// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Gio, GObject, Meta, Shell } = imports.gi;

const Signals = imports.signals;

const Main = imports.ui.main;

const TOUCHPAD_BASE_DISTANCE = 400;
const SCROLL_MULTIPLIER = 20;

const MIN_ANIMATION_DURATION = 0.1;
const MAX_ANIMATION_DURATION = 0.4;
const CANCEL_AREA = 0.5;
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

// TODO: support horizontal
// TODO: swap back and forward, this doesn't make sense

var TouchpadSwipeGesture = class TouchpadSwipeGesture {
    constructor(shouldSkip) {
        this._shouldSkip = shouldSkip;
        this._touchpadSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.peripherals.touchpad' });

        global.stage.connect('captured-event', this._handleEvent.bind(this));
    }

    _handleEvent(actor, event) {
        if (event.type() != Clutter.EventType.TOUCHPAD_SWIPE)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_touchpad_gesture_finger_count() != 4)
            return Clutter.EVENT_PROPAGATE;

        if (this._shouldSkip())
            return Clutter.EVENT_PROPAGATE;

        let time = event.get_time();

        switch (event.get_gesture_phase()) {
        case Clutter.TouchpadGesturePhase.BEGIN:
            let [x, y] = event.get_coords();
            this.emit('begin', time, x, y);
            break;

        case Clutter.TouchpadGesturePhase.UPDATE:
            let [dx, dy] = event.get_gesture_motion_delta();

            if(!(this._touchpadSettings.get_boolean('natural-scroll'))) {
                dx = -dx;
                dy = -dy;
            }

            this.emit('update', time, -dy / TOUCHPAD_BASE_DISTANCE);
            break;

        case Clutter.TouchpadGesturePhase.END:
        case Clutter.TouchpadGesturePhase.CANCEL:
            this.emit('end', time);
            break;
        }

        return Clutter.EVENT_STOP;
    }
};
Signals.addSignalMethods(TouchpadSwipeGesture.prototype);

var TouchSwipeGesture = GObject.registerClass({
    Signals: { 'begin':  { param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE, GObject.TYPE_DOUBLE] },
               'update': { param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE] },
               'end':    { param_types: [GObject.TYPE_UINT] },
               'cancel': { param_types: [GObject.TYPE_UINT] } },
}, class TouchSwipeGesture extends Clutter.TriggerAction {
    _init(shouldSkip, n_touch_points, trigger_edge = Clutter.TriggerEdge.NONE) {
        super._init();
        this.set_n_touch_points(n_touch_points);
        this.set_trigger_edge(trigger_edge);

        this._shouldSkip = shouldSkip;
        this._distance = global.screen_height;
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
    vfunc_gesture_progress(actor, point) {
        if (point != 0)
            return;

        let [distance, dx, dy] = this.get_motion_delta(0);
        let time = this.get_last_event(point).get_time();

        this.emit('update', time, -dy / this._distance);
    }

    vfunc_gesture_end(actor, point) {
        let time = this.get_last_event(point).get_time();

        this.emit('end', time);
    }

    vfunc_gesture_cancel(actor) {
        let time = Clutter.get_current_event_time();

        this.emit('cancel', time);
    }

    setDistance(distance) {
        this._distance = distance;
    }
});

var ScrollGesture = class ScrollGesture {
    constructor(actor, shouldSkip) {
        this._shouldSkip = shouldSkip;
        this._began = false;

        actor.connect('scroll-event', this._handleEvent.bind(this));
    }

    _handleEvent(actor, event) {
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
            return;
        }

        if (!this._began) {
            let [x, y] = event.get_coords();
            this.emit('begin', time, x, y);
            this._began = true;
        }

        this.emit('update', time, dy * SCROLL_MULTIPLIER / TOUCHPAD_BASE_DISTANCE);

        return Clutter.EVENT_STOP;
    }
};
Signals.addSignalMethods(ScrollGesture.prototype);

// USAGE:
//
// To correctly implement the gesture, the implementer must implement handlers for the
// following four signals with the following behavior:
//
// begin(tracker, monitor)
//   The handler should check whether a deceleration animation is currently
//   running. If it is, it should stop the animation (without resetting progress)
//   and call tracker.continueSwipe(progress). Otherwise it should initialize the gesture
//   and call tracker.confirmSwipe(canSwipeBack, canSwipeForward, distance, backExtent, forwardExtent).
//   If nothing is called, the swipe would be ignored.
//   The parameters are:
//    * canSwipeBack: whether the tracker should allow to swipe back;
//    * canSwipeForward: whether the tracker should allow to swipe forward;
//    * distance: the page size;
//    * backExtent: can be used to make "back" page longer. Normally this is 0;
//    * forwardExtent: can be used to make "forward" page longer. Normally this is 0.
//   Extents should be used for extending one or both page in some cases (such as switching to a
//   workspace with a fullscreen window). Speed of touchpad swipe and scrolling only depend on
//   distance, so the speed is consistent with or without extents.
//   Internally it means progress range is not [-1, 1], but [-(1 + forwardExtent/distance), 1 + (backExtent/distance)],
//   but progress and duration in update() and end() will be normalized.
//
// update(tracker, progress)
//   The handler should set the progress to the given value.
//
// end(tracker, duration, isBack)
//   The handler should animate the progress to 1 if isBack is true, or to -1 otherwise.
//   When the animation ends, it should change the state, e.g. change the current page
//   or switch workspace.
//
// cancel(tracker, duration)
//   The tracker should animate the progress back to 0 and forget about the gesture
//   NOTE: duration can be 0 in some cases, in this case it should reset instantly.
//
// ======================================================================================
//
// 'enabled'
//   This property can be used to enable or disable the swipe tracker temporarily.

var SwipeTracker = class {
    constructor(actor, allowedModes, allowDrag = true, allowScroll = true) {
        this._allowedModes = allowedModes;
        this._enabled = true;

        this._reset();

        this._canSwipeBack = true;
        this._canSwipeForward = true;
        this._distance = 0;
        this._backExtent = 0;
        this._forwardExtent = 0;

        let shouldSkip = () =>
            ((this._allowedModes & Main.actionMode) == 0 || !this._enabled);

        let touchpadGesture = new TouchpadSwipeGesture(shouldSkip);
        touchpadGesture.connect('begin', this._beginGesture.bind(this));
        touchpadGesture.connect('update', this._updateGesture.bind(this));
        touchpadGesture.connect('end', this._endGesture.bind(this));
//        touchpadGesture.connect('cancel', this._cancelGesture.bind(this)); // End the gesture normally for touchpads

        let touchGesture = new TouchSwipeGesture(shouldSkip, 4, Clutter.TriggerEdge.NONE);
        touchGesture.connect('begin', this._beginGesture.bind(this));
        touchGesture.connect('update', this._updateGesture.bind(this));
        touchGesture.connect('end', this._endGesture.bind(this));
        touchGesture.connect('cancel', this._cancelGesture.bind(this));
        global.stage.add_action(touchGesture);
        this._touchGesture = touchGesture;

        if (allowDrag) {
            let dragGesture = new TouchSwipeGesture(shouldSkip, 1, Clutter.TriggerEdge.AFTER);
            dragGesture.connect('begin', this._beginGesture.bind(this));
            dragGesture.connect('update', this._updateGesture.bind(this));
            dragGesture.connect('end', this._endGesture.bind(this));
            dragGesture.connect('cancel', this._cancelGesture.bind(this));
            try {
                actor.add_action(dragGesture);
            } catch (e) {
                actor.addAction(dragGesture); // FIXME: wtf is this
            }
            this._dragGesture = dragGesture;
        } else
            this._dragGesture = null;

        if (allowScroll) {
            let scrollGesture = new ScrollGesture(actor, shouldSkip);
            scrollGesture.connect('begin', this._beginGesture.bind(this));
            scrollGesture.connect('update', this._updateGesture.bind(this));
            scrollGesture.connect('end', this._endGesture.bind(this));
        }
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

    _reset() {
        this._state = State.NONE;

        this._prevOffset = 0;

        this._progress = 0;

        this._prevTime = 0;
        this._velocity = 0;

        this._cancelled = false;
    }

    _cancel() {
        this.emit('cancel', 0);
        this._reset();
    }

    _beginGesture(gesture, time, x, y) {
        if (this._state == State.SCROLLING)
            return;

        this._prevTime = time;

        let rect = new Meta.Rectangle({ x: x, y: y, width: 1, height: 1 });
        let monitor = global.display.get_monitor_index_for_rect(rect);

        this.emit('begin', monitor);
    }

    _updateGesture(gesture, time, delta) {
        if ((this._allowedModes & Main.actionMode) == 0 || !this._enabled)
            return;

        if (this._state != State.SCROLLING)
            return;

        this._progress += delta;

        if (time != this._prevTime)
            this._velocity = delta / (time - this._prevTime);

        if (this._progress > 0 && !this._canSwipeBack)
            this._progress = 0;
        if (this._progress < 0 && !this._canSwipeForward)
            this._progress = 0;

        let maxProgress = (this._progress > 0) ? (1 + this._backExtent / this._distance) : 0;
        let minProgress = (this._progress < 0) ? -(1 + this._forwardExtent / this._distance) : 0;
        this._progress = clamp(this._progress, minProgress, maxProgress);

        // Clamp progress to [0,1]
        let progress = this._progress / (1 + (this._progress > 0 ? this._backExtent : this._forwardExtent) / this._distance);
        this.emit('update', progress);

        this._prevTime = time;
    }

    _shouldCancel() {
        if (this._cancelled)
            return true;

        if (this._progress == 0)
            return true;

        if (this._progress > 0 && !this._canSwipeBack)
            return true;

        if (this._progress < 0 && !this._canSwipeForward)
            return true;

        if (Math.abs(this._velocity) < VELOCITY_THRESHOLD)
            return Math.abs(this._progress) < CANCEL_AREA;

        return (this._velocity * this._progress < 0);
    }

    _endGesture(gesture, time) {
        if ((this._allowedModes & Main.actionMode) == 0 || !this._enabled) {
//            this._cancel();
            return;
        }

        if (this._state != State.SCROLLING)
            return;

        let cancelled = this._shouldCancel();

        let endProgress = 0;
        if (!cancelled)
            endProgress = (this._progress > 0) ? (1 + this._backExtent / this._distance) : -(1 + this._forwardExtent / this._distance);

        let velocity = ANIMATION_BASE_VELOCITY;
        if ((endProgress - this._progress) * this._velocity > 0)
            velocity = this._velocity;

        let duration = Math.abs((this._progress - endProgress) / velocity * DURATION_MULTIPLIER) / 1000;
        duration = clamp(duration, MIN_ANIMATION_DURATION, MAX_ANIMATION_DURATION);

        if (cancelled)
            this.emit('cancel', duration);
        else
            this.emit('end', duration, this._progress > 0);
        this._reset();
    }

    _cancelGesture(gesture, time) {
        if (this._state != State.SCROLLING)
            return;

        this._cancelled = true;
        this._endGesture(gesture, time);
    }

    confirmSwipe(canSwipeBack, canSwipeForward, distance, backExtent, forwardExtent) {
        this._canSwipeBack = canSwipeBack;
        this._canSwipeForward = canSwipeForward;
        this._distance = distance;
        this._backExtent = backExtent;
        this._forwardExtent = forwardExtent;

        this._touchGesture.setDistance(distance);
        if (this._dragGesture)
            this._dragGesture.setDistance(distance);

        this._state = State.SCROLLING;
    }

    continueSwipe(progress) {
        this._progress = progress;
        this._velocity = 0;
        this._state = State.SCROLLING;
    }

};
Signals.addSignalMethods(SwipeTracker.prototype);
