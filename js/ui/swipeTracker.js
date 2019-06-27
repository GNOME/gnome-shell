// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Gio, GObject, Shell } = imports.gi;

const Signals = imports.signals;

const Main = imports.ui.main;

const TOUCHPAD_BASE_DISTANCE = 400;
const SCROLL_MULTIPLIER = 10;

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

// TODO: support scrolling
// TODO: support dragging
// TODO: support horizontal

var TouchpadSwipeGesture = class TouchpadSwipeGesture {
    constructor(actor, shouldSkip) {
        this._touchpadSettings = new Gio.Settings({schema_id: 'org.gnome.desktop.peripherals.touchpad'});

        actor.connect('captured-event', this._handleEvent.bind(this));
        this._shouldSkip = shouldSkip;
    }

    _handleEvent(actor, event) {
        if (event.type() != Clutter.EventType.TOUCHPAD_SWIPE)
            return Clutter.EVENT_PROPAGATE;

        if (event.get_touchpad_gesture_finger_count() != 4)
            return Clutter.EVENT_PROPAGATE;

        if (this._shouldSkip())
            return Clutter.EVENT_PROPAGATE;

        let time = event.get_time();
        let [dx, dy] = event.get_gesture_motion_delta();

        if (event.get_gesture_phase() == Clutter.TouchpadGesturePhase.UPDATE) {
            if(!(this._touchpadSettings.get_boolean('natural-scroll'))) {
                dx = -dx;
                dy = -dy;
            }
            this.emit('update', time, -dy / TOUCHPAD_BASE_DISTANCE);
        } else if (event.get_gesture_phase() == Clutter.TouchpadGesturePhase.END ||
                   event.get_gesture_phase() == Clutter.TouchpadGesturePhase.CANCEL)
            this.emit('end', time);

        return Clutter.EVENT_STOP;
    }
};
Signals.addSignalMethods(TouchpadSwipeGesture.prototype);

var TouchSwipeGesture = GObject.registerClass({
    Signals: { 'update': { param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE] },
               'end':    { param_types: [GObject.TYPE_UINT] },
               'cancel': { param_types: [GObject.TYPE_UINT] }},
}, class TouchSwipeGesture extends Clutter.GestureAction {
    _init(actor, shouldSkip) {
        super._init();
        this.set_n_touch_points(4);

        this._actor = actor;
        this._shouldSkip = shouldSkip;
    }

    vfunc_gesture_begin(actor, point) {
        if (!super.vfunc_gesture_begin(actor, point))
            return false;

        return !this._shouldSkip();
    }

    // TODO: track center of the fingers instead of the first one
    vfunc_gesture_progress(actor, point) {
        if (point != 0)
            return;

        let [distance, dx, dy] = this.get_motion_delta(0);
        let time = this.get_last_event(point).get_time();

        this.emit('update', time, -dy / (global.screen_height - Main.panel.height)); // TODO: the height isn't always equal to the actor height
    }

    vfunc_gesture_end(actor, point) {
        let time = this.get_last_event(point).get_time();

        this.emit('end', time);
    }

    vfunc_gesture_cancel(actor) {
        let time = Clutter.get_current_event_time();

        this.emit('cancel', time);
    }
});

/*var DragGesture = GObject.registerClass({
    Signals: { 'update': { param_types: [GObject.TYPE_UINT, GObject.TYPE_DOUBLE] },
               'end':    { param_types: [GObject.TYPE_UINT] },
               'cancel': { param_types: [GObject.TYPE_UINT] }},
}, class PanGesture extends Clutter.DragAction {
    _init(actor, shouldSkip) {
        super._init();

        this._actor = actor;
        this._shouldSkip = shouldSkip;
    }

    vfunc_drag_begin(actor, x, y, modifiers) {
        
    }

    vfunc_drag_motion(actor, x, y, modifiers) {
        
    }

});*/
/*
let panAction = new Clutter.PanAction({ trigger_edge: Clutter.TriggerEdge.AFTER });
        panAction.connect('pan', this._onPan.bind(this));
        panAction.connect('gesture-begin', () => {
            if (this._workspacesOnlyOnPrimary) {
                let event = Clutter.get_current_event();
                if (this._getMonitorIndexForEvent(event) != this._primaryIndex)
                    return false;
            }

            this._startSwipeScroll();
            return true;
        });
        panAction.connect('gesture-cancel', () => {
            clickAction.release();
            this._endSwipeScroll();
        });
        panAction.connect('gesture-end', () => {
            clickAction.release();
            this._endSwipeScroll();
        });
        Main.overview.addAction(panAction);
*/
// USAGE:
//
// To correctly implement the gesture, the implementer must implement handlers for the
// following four signals with the following behavior:
//
// begin(tracker)
//   The handler should check whether a deceleration animation is currently
//   running. If it is, it should stop the animation (without resetting progress)
//   and call tracker.continueFrom(progress). Otherwise it should initialize the gesture.
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
// 'can_swipe_back' and 'can_swipe_forward'
//   These properties can be used to disable swiping back from the first page or forward from the last page.
//
// 'enabled'
//   This property can be used to enable or disable the swipe tracker temporarily.

var SwipeTracker = class {
    constructor(actor, allowedModes) {
        this.actor = actor;
        this._allowedModes = allowedModes;
        this._enabled = true;

        this._reset();

        this._can_swipe_back = true;
        this._can_swipe_forward = true;

        let shouldSkip = () =>
            ((this._allowedModes & Main.actionMode) == 0 || !this._enabled);

        let gesture = new TouchpadSwipeGesture(actor, shouldSkip);
        gesture.connect('update', this._updateGesture.bind(this));
        gesture.connect('end', this._endGesture.bind(this));
//        gesture.connect('cancel', this._cancelGesture.bind(this)); // End the gesture normally for touchpads

        gesture = new TouchSwipeGesture(actor, shouldSkip);
        gesture.connect('update', this._updateGesture.bind(this));
        gesture.connect('end', this._endGesture.bind(this));
        gesture.connect('cancel', this._cancelGesture.bind(this));
        actor.add_action(gesture);
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

    get can_swipe_back() {
        return this._can_swipe_back;
    }

    set can_swipe_back(can_swipe_back) {
        this._can_swipe_back = can_swipe_back;
        if (!can_swipe_back && this._progress > 0)
            this._cancel();
    }

    get can_swipe_forward() {
        return this._can_swipe_forward;
    }

    set can_swipe_forward(can_swipe_forward) {
        this._can_swipe_forward = can_swipe_forward;
        if (!can_swipe_forward && this._progress < 0)
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

    _beginGesture(gesture, time) {
        if (this._state == State.SCROLLING)
            return;

        this._prevTime = time;
        this.emit('begin');
        this._state = State.SCROLLING;
    }

    _updateGesture(gesture, time, delta) {
        if ((this._allowedModes & Main.actionMode) == 0 || !this._enabled)
            return;

        if (this._state != State.SCROLLING)
            this._beginGesture(gesture, time);

        this._progress += delta;

        if (time != this._prevTime)
            this._velocity = delta / (time - this._prevTime);

        if (this._progress > 0 && !this.can_swipe_back)
            this._progress = 0;
        if (this._progress < 0 && !this.can_swipe_forward)
            this._progress = 0;

        let maxProgress = (this._progress > 0) ? 1 : 0;
        let minProgress = (this._progress < 0) ? -1 : 0;
        this._progress = clamp(this._progress, minProgress, maxProgress);

        this.emit('update', this._progress);

        this._prevTime = time;
    }

    _shouldCancel() {
        if (this._cancelled)
            return true;

        if (this._progress == 0)
            return true;

        if (this._progress > 0 && !this.can_swipe_back)
            return true;

        if (this._progress < 0 && !this.can_swipe_forward)
            return true;

        if (Math.abs(this._velocity) < VELOCITY_THRESHOLD)
            return Math.abs(this._progress) < CANCEL_AREA;

        return (this._velocity * this._progress < 0);
    }

    _endGesture(gesture, time) {
        if ((this._allowedModes & Main.actionMode) == 0 || !this._enabled)
            return;

        if (this._state != State.SCROLLING)
            return;

        let cancelled = this._shouldCancel();

        let endProgress = 0;
        if (!cancelled)
            endProgress = (this._progress > 0) ? 1 : -1;

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

    continueFrom(progress) {
        this._progress = progress;
        this._velocity = 0;
        this._state = State.SCROLLING;
    }

};
Signals.addSignalMethods(SwipeTracker.prototype);
