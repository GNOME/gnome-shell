// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Signals = imports.signals;
const Tweener = imports.tweener.tweener;

// This is a wrapper around imports.tweener.tweener that adds a bit of
// Clutter integration. If the tweening target is a Clutter.Actor, then
// the tweenings will automatically be removed if the actor is destroyed.

// ActionScript Tweener methods that imports.tweener.tweener doesn't
// currently implement: getTweens, getVersion, registerTransition,
// setTimeScale, updateTime.

// imports.tweener.tweener methods that we don't re-export:
// pauseAllTweens, removeAllTweens, resumeAllTweens. (It would be hard
// to clean up properly after removeAllTweens, and also, any code that
// calls any of these is almost certainly wrong anyway, because they
// affect the entire application.)

// Called from Main.start
function init() {
    Tweener.setFrameTicker(new ClutterFrameTicker());
}

const transitionTranslations = {
    easeNone: Clutter.AnimationMode.LINEAR,
    linear: Clutter.AnimationMode.LINEAR,
    easeInQuad: Clutter.AnimationMode.EASE_IN_QUAD,
    easeOutQuad: Clutter.AnimationMode.EASE_OUT_QUAD,
    easeInOutQuad: Clutter.AnimationMode.EASE_IN_OUT_QUAD,
    easeOutInQuad: undefined,
    easeInCubic: Clutter.AnimationMode.EASE_IN_CUBIC,
    easeOutCubic: Clutter.AnimationMode.EASE_OUT_CUBIC,
    easeInOutCubic: Clutter.AnimationMode.EASE_IN_OUT_CUBIC,
    easeOutInCubic: undefined,
    easeInQuart: Clutter.AnimationMode.EASE_IN_QUART,
    easeOutQuart: Clutter.AnimationMode.EASE_OUT_QUART,
    easeInOutQuart: Clutter.AnimationMode.EASE_IN_OUT_QUART,
    easeOutInQuart: undefined,
    easeInQuint: Clutter.AnimationMode.EASE_IN_QUINT,
    easeOutQuint: Clutter.AnimationMode.EASE_OUT_QUINT,
    easeInOutQuint: Clutter.AnimationMode.EASE_IN_OUT_QUINT,
    easeOutInQuint: undefined,
    easeInSine: Clutter.AnimationMode.EASE_IN_SINE,
    easeOutSine: Clutter.AnimationMode.EASE_OUT_SINE,
    easeInOutSine: Clutter.AnimationMode.EASE_IN_OUT_SINE,
    easeOutInSine: undefined,
    easeInExpo: Clutter.AnimationMode.EASE_IN_EXPO,
    easeOutExpo: Clutter.AnimationMode.EASE_OUT_EXPO,
    easeInOutExpo: Clutter.AnimationMode.EASE_IN_OUT_EXPO,
    easeOutInExpo: undefined,
    easeInCirc: Clutter.AnimationMode.EASE_IN_CIRC,
    easeOutCirc: Clutter.AnimationMode.EASE_OUT_CIRC,
    easeInOutCirc: Clutter.AnimationMode.EASE_IN_OUT_CIRC,
    easeOutInCirc: undefined,
    easeInElastic: Clutter.AnimationMode.EASE_IN_ELASTIC,
    easeOutElastic: Clutter.AnimationMode.EASE_OUT_ELASTIC,
    easeInOutElastic: Clutter.AnimationMode.EASE_IN_OUT_ELASTIC,
    easeOutInElastic: undefined,
    easeInBack: Clutter.AnimationMode.EASE_IN_BACK,
    easeOutBack: Clutter.AnimationMode.EASE_OUT_BACK,
    easeInOutBack: Clutter.AnimationMode.EASE_IN_OUT_BACK,
    easeOutInBack: undefined,
    easeInBounce: Clutter.AnimationMode.EASE_IN_BOUNCE,
    easeOutBounce: Clutter.AnimationMode.EASE_OUT_BOUNCE,
    easeInOutBounce: Clutter.AnimationMode.EASE_IN_OUT_BOUNCE,
    easeOutInBounce: undefined,
};

function _easeTarget(target, tweeningParameters) {
    if (!(target instanceof Clutter.Actor))
        return false;

    let props = Object.assign({}, tweeningParameters);
    let easingParams = {};

    if (tweeningParameters.hasOwnProperty('time'))
        easingParams['duration'] = tweeningParameters['time'] * 1000;
    delete props['time'];

    if (tweeningParameters.hasOwnProperty('delay'))
        easingParams['delay'] = tweeningParameters['delay'] * 1000;
    delete props['delay'];

    let mode = Clutter.AnimationMode.EASE_OUT_EXPO;
    if (tweeningParameters.hasOwnProperty('transition'))
        mode = transitionTranslations[tweeningParameters['transition']];
    delete props['transition'];

    if (mode == undefined)
        return false;

    easingParams['mode'] = mode;

    let getHandler = function(name) {
        if (!tweeningParameters.hasOwnProperty(name))
            return null;

        let [handler, scope, params] = [
            tweeningParameters[name],
            tweeningParameters[name + 'Scope'] || target,
            tweeningParameters[name + 'Params'] || []
        ];

        delete props[name];
        delete props[name + 'Scope'];
        delete props[name + 'Params'];

        return () => { handler.apply(scope, params); };
    }

    let onUpdate = getHandler('onUpdate');
    if (onUpdate)
        easingParams['onUpdate'] = onUpdate;

    let onComplete = getHandler('onComplete');
    let onOverwrite = getHandler('onOverwrite');
    if (onComplete || onOverwrite)
        easingParams['onStopped'] = isFinished => {
            if (!isFinished && onOverwrite)
                onOverwrite();
            if (!target._tweensRemoved && onComplete)
                onComplete();
        }

    // Implicit animations only work with animatable properties
    if (Object.keys(props).some(p => !target.find_property(p)))
        return false;

    target._tweensRemoved = false;
    target.ease(props, easingParams);

    return true;
}


function addCaller(target, tweeningParameters) {
    _wrapTweening(target, tweeningParameters);
    Tweener.addCaller(target, tweeningParameters);
}

function addTween(target, tweeningParameters) {
    if (_easeTarget(target, tweeningParameters))
        return;

    _wrapTweening(target, tweeningParameters);
    Tweener.addTween(target, tweeningParameters);
}

function _wrapTweening(target, tweeningParameters) {
    let state = _getTweenState(target);

    if (!state.destroyedId) {
        if (target instanceof Clutter.Actor) {
            state.actor = target;
            state.destroyedId = target.connect('destroy', _actorDestroyed);
        } else if (target.actor && target.actor instanceof Clutter.Actor) {
            state.actor = target.actor;
            state.destroyedId = target.actor.connect('destroy', function() { _actorDestroyed(target); });
        }
    }

    if (!Gtk.Settings.get_default().gtk_enable_animations) {
        tweeningParameters['time'] = 0.000001;
        tweeningParameters['delay'] = 0.000001;
    }

    _addHandler(target, tweeningParameters, 'onComplete', _tweenCompleted);
}

function _getTweenState(target) {
    // If we were paranoid, we could keep a plist mapping targets to
    // states... but we're not that paranoid.
    if (!target.__ShellTweenerState)
        target.__ShellTweenerState = {};
    return target.__ShellTweenerState;
}

function _resetTweenState(target) {
    let state = target.__ShellTweenerState;

    if (state) {
        if (state.destroyedId)
            state.actor.disconnect(state.destroyedId);
    }

    target.__ShellTweenerState = {};
}

function _addHandler(target, params, name, handler) {
    if (params[name]) {
        let oldHandler = params[name];
        let oldScope = params[name + 'Scope'];
        let oldParams = params[name + 'Params'];
        let eventScope = oldScope ? oldScope : target;

        params[name] = function () {
            oldHandler.apply(eventScope, oldParams);
            handler(target);
        };
    } else
        params[name] = function () { handler(target); };
}

function _actorDestroyed(target) {
    _resetTweenState(target);
    Tweener.removeTweens(target);
}

function _tweenCompleted(target) {
    if (!isTweening(target))
        _resetTweenState(target);
}

function getTweenCount(scope) {
    return Tweener.getTweenCount(scope);
}

// imports.tweener.tweener doesn't provide this method (which exists
// in the ActionScript version) but it's easy to implement.
function isTweening(scope) {
    return Tweener.getTweenCount(scope) != 0;
}

function removeTweens(scope) {
    scope._tweensRemoved = true;
    if (scope instanceof Clutter.Actor)
        scope.remove_all_transitions();
    scope._tweensRemoved = false;

    if (Tweener.removeTweens.apply(null, arguments)) {
        // If we just removed the last active tween, clean up
        if (Tweener.getTweenCount(scope) == 0)
            _tweenCompleted(scope);
        return true;
    } else
        return false;
}

function pauseTweens() {
    return Tweener.pauseTweens.apply(null, arguments);
}

function resumeTweens() {
    return Tweener.resumeTweens.apply(null, arguments);
}


function registerSpecialProperty(name, getFunction, setFunction,
                                 parameters, preProcessFunction) {
    Tweener.registerSpecialProperty(name, getFunction, setFunction,
                                    parameters, preProcessFunction);
}

function registerSpecialPropertyModifier(name, modifyFunction, getFunction) {
    Tweener.registerSpecialPropertyModifier(name, modifyFunction, getFunction);
}

function registerSpecialPropertySplitter(name, splitFunction, parameters) {
    Tweener.registerSpecialPropertySplitter(name, splitFunction, parameters);
}


// The 'FrameTicker' object is an object used to feed new frames to
// Tweener so it can update values and redraw. The default frame
// ticker for Tweener just uses a simple timeout at a fixed frame rate
// and has no idea of "catching up" by dropping frames.
//
// We substitute it with custom frame ticker here that connects
// Tweener to a Clutter.TimeLine. Now, Clutter.Timeline itself isn't a
// whole lot more sophisticated than a simple timeout at a fixed frame
// rate, but at least it knows how to drop frames. (See
// HippoAnimationManager for a more sophisticated view of continous
// time updates; even better is to pay attention to the vertical
// vblank and sync to that when possible.)
//
var ClutterFrameTicker = new Lang.Class({
    Name: 'ClutterFrameTicker',

    FRAME_RATE : 60,

    _init : function() {
        // We don't have a finite duration; tweener will tell us to stop
        // when we need to stop, so use 1000 seconds as "infinity", and
        // set the timeline to loop. Doing this means we have to track
        // time ourselves, since clutter timeline's time will cycle
        // instead of strictly increase.
        this._timeline = new Clutter.Timeline({ duration: 1000*1000 });
        this._timeline.set_loop(true);
        this._startTime = -1;
        this._currentTime = -1;

        this._timeline.connect('new-frame', Lang.bind(this,
            function(timeline, frame) {
                this._onNewFrame(frame);
            }));

        let perf_log = Shell.PerfLog.get_default();
        perf_log.define_event("tweener.framePrepareStart",
                              "Start of a new animation frame",
                              "");
        perf_log.define_event("tweener.framePrepareDone",
                              "Finished preparing frame",
                              "");
    },

    _onNewFrame : function(frame) {
        // If there is a lot of setup to start the animation, then
        // first frame number we get from clutter might be a long ways
        // into the animation (or the animation might even be done).
        // That looks bad, so we always start at the first frame of the
        // animation then only do frame dropping from there.
        if (this._startTime < 0)
            this._startTime = GLib.get_monotonic_time() / 1000.0;

        // currentTime is in milliseconds
        let perf_log = Shell.PerfLog.get_default();
        this._currentTime = GLib.get_monotonic_time() / 1000.0 - this._startTime;
        perf_log.event("tweener.framePrepareStart");
        this.emit('prepare-frame');
        perf_log.event("tweener.framePrepareDone");
    },

    getTime : function() {
        return this._currentTime;
    },

    start : function() {
        if (St.get_slow_down_factor() > 0)
            Tweener.setTimeScale(1 / St.get_slow_down_factor());
        this._timeline.start();
        global.begin_work();
    },

    stop : function() {
        this._timeline.stop();
        this._startTime = -1;
        this._currentTime = -1;
        global.end_work();
    }
});

Signals.addSignalMethods(ClutterFrameTicker.prototype);
