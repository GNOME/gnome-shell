// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Gio, Shell } = imports.gi;

const Signals = imports.signals;

const Main = imports.ui.main;

const TOUCHPAD_BASE_DISTANCE = 400;
const SCROLL_MULTIPLIER = 10;

const MIN_ANIMATION_DURATION = 0.1;
const MAX_ANIMATION_DURATION = 0.4;
const CANCEL_AREA = 0.5;
const VELOCITY_THRESHOLD = 0.001;
const DURATION_MULTIPLIER = 3;

/*var Direction = {
    NEGATIVE: -1,
    NONE: 0,
    POSITIVE: 1
};*/

var State = {
    NONE: 0,
    SCROLLING: 1,
};

function clamp(value, min, max) {
    return Math.max(min, Math.min(max, value));
}

var SwipeTracker = class {
    constructor(actor, allowedModes) {
        this.actor = actor;
        this._allowedModes = allowedModes;
        this._enabled = true;

        this._reset();

        this._can_swipe_back = true;
        this._can_swipe_forward = true;

//        actor.connect('event', this._handleEvent.bind(this));
        actor.connect('captured-event', this._handleEvent.bind(this));
        this._touchpadSettings = new Gio.Settings({schema_id: 'org.gnome.desktop.peripherals.touchpad'});
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
        if (this._can_swipe_back == can_swipe_back)
            return;

        this._can_swipe_back = can_swipe_back;
        if (!can_swipe_back && this._progress > 0)
            this._cancel();
    }

    get can_swipe_forward() {
        return this._can_swipe_forward;
    }

    set can_swipe_forward(can_swipe_forward) {
        if (this._can_swipe_forward == can_swipe_forward)
            return;

        this._can_swipe_forward = can_swipe_forward;
        if (!can_swipe_forward && this._progress < 0)
            this._cancel();
    }

    _reset() {
        this._state = State.NONE;
//        this._direction = Direction.NONE;

        this._prevOffset = 0;

        this._progress = 0;

        this._prevTime = 0;
        this._velocity = 0;
    }

    _cancel() {
        this.emit('end', true, 0);
        this.reset();
    }

    _handleEvent(actor, event) {
        if (event.type() != Clutter.EventType.TOUCHPAD_SWIPE) // SCROLL
            return Clutter.EVENT_PROPAGATE;

        if (event.get_touchpad_gesture_finger_count() != 4)
            return Clutter.EVENT_PROPAGATE;

//        if (event.get_scroll_direction() != Clutter.ScrollDirection.SMOOTH)
//            return Clutter.EVENT_PROPAGATE;

        if ((this._allowedModes & Main.actionMode) == 0)
            return Clutter.EVENT_PROPAGATE;

        if (!this._enabled)
            return Clutter.EVENT_PROPAGATE;

        let time = event.get_time();
        let [dx, dy] = event.get_scroll_delta();

        if (event.get_gesture_phase() == Clutter.TouchpadGesturePhase.UPDATE) {
//        if ((event.get_scroll_finish_flags() & Clutter.ScrollFinishFlags.VERTICAL == 0) || (dx == 0 && dy == 0))
            if(!(this._touchpadSettings.get_boolean('natural-scroll'))).
                dy = -dy;
            this._updateGesture(time, -dy / TOUCHPAD_BASE_DISTANCE * SCROLL_MULTIPLIER); // TODO: multiply on actor diman
        } else if (event.get_gesture_phase() == Clutter.TouchpadGesturePhase.END)
            this._endGesture(time);
        else if (event.get_gesture_phase() == Clutter.TouchpadGesturePhase.CANCEL)
            this._endGesture(time); // TODO: maybe cancel it?

        return Clutter.EVENT_STOP;
    }

    _beginGesture(time) {
        if (this._state == State.SCROLLING)
            return;

        this._prevTime = time;
        this.emit('begin');
        this._state = State.SCROLLING;
    }

    _updateGesture(time, delta) {
        if (this._state != State.SCROLLING) {
            _beginGesture(time);
        }

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

        this.emit('progress', this._progress);

        this._prevTime = time;
    }

    _shouldCancel() {
        if (this._progress == 0)
            return true;

        if (this._progress > 0 && !this.can_swipe_back)
            return true;

        if (this._progress < 0 && !this.can_swipe_forward)
            return true;

        if (this._velocity * this._progress < 0)
            return true;

        return Math.abs(this._progress) < CANCEL_AREA && Math.abs(this._velocity) < VELOCITY_THRESHOLD;
    }

    _endGesture(time) {
        if (this._state != State.SCROLLING)
            return;

        let cancelled = this._shouldCancel();

        let endProgress = 0;
        if (!cancelled)
            endProgress = (this._progress > 0) ? 1 : -1;

        let duration = MAX_ANIMATION_DURATION;
        if ((endProgress - this._progress) * this._velocity > 0) {
            duration = Math.abs((this._progress - endProgress) / this._velocity * DURATION_MULTIPLIER) / 1000;
            if (duration != 0)
                duration = clamp(duration, MIN_ANIMATION_DURATION, MAX_ANIMATION_DURATION);
        }

        this.emit('end', cancelled, duration);
        this.reset();
    }
};
Signals.addSignalMethods(SwipeTracker.prototype);
