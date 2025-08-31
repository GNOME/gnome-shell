// Load all required dependencies with the correct versions
import '../misc/dependencies.js';

import {setConsoleLogDomain} from 'console';
import * as Gettext from 'gettext';

import Cairo from 'cairo';
import Clutter from 'gi://Clutter';
import Gdk from 'gi://Gdk';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Polkit from 'gi://Polkit';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as SignalTracker from '../misc/signalTracker.js';
import {adjustAnimationTime} from '../misc/animationUtils.js';

setConsoleLogDomain('GNOME Shell');

Gio._promisify(Gio.DataInputStream.prototype, 'fill_async');
Gio._promisify(Gio.DataInputStream.prototype, 'read_line_async');
Gio._promisify(Gio.DBus, 'get');
Gio._promisify(Gio.DBusConnection.prototype, 'call');
Gio._promisify(Gio.DBusProxy, 'new');
Gio._promisify(Gio.DBusProxy.prototype, 'init_async');
Gio._promisify(Gio.DBusProxy.prototype, 'call_with_unix_fd_list');
Gio._promisify(Gio.File.prototype, 'query_info_async');
Gio._promisify(Polkit.Permission, 'new');
Gio._promisify(Shell.App.prototype, 'activate_action');
Gio._promisify(Meta.Backend.prototype, 'set_keymap_async');
Gio._promisify(Meta.Backend.prototype, 'set_keymap_layout_group_async');

// We can't import shell JS modules yet, because they may have
// variable initializations, etc, that depend on this file's
// changes

function _patchLayoutClass(layoutClass, styleProps) {
    if (styleProps) {
        layoutClass.prototype.hookup_style = function (container) {
            container.connect('style-changed', () => {
                let node = container.get_theme_node();
                for (let prop in styleProps) {
                    let [found, length] = node.lookup_length(styleProps[prop], false);
                    if (found)
                        this[prop] = length;
                }
            });
        };
    }
}

function _makeEaseCallback(params, cleanup) {
    let onComplete = params.onComplete;
    delete params.onComplete;

    let onStopped = params.onStopped;
    delete params.onStopped;

    return isFinished => {
        cleanup();

        if (onStopped)
            onStopped(isFinished);
        if (onComplete && isFinished)
            onComplete();
    };
}

function _getPropertyTarget(actor, propName) {
    if (!propName.startsWith('@'))
        return [actor, propName];

    let [type, name, prop] = propName.split('.');
    switch (type) {
    case '@layout':
        return [actor.layout_manager, name];
    case '@actions':
        return [actor.get_action(name), prop];
    case '@constraints':
        return [actor.get_constraint(name), prop];
    case '@content':
        return [actor.content, name];
    case '@effects':
        return [actor.get_effect(name), prop];
    }

    throw new Error(`Invalid property name ${propName}`);
}

function _easeActor(actor, params) {
    params = {
        repeatCount: 0,
        autoReverse: false,
        animationRequired: false,
        ...params,
    };

    actor.save_easing_state();

    const animationRequired = params.animationRequired;
    delete params.animationRequired;

    if (params.duration !== undefined)
        actor.set_easing_duration(params.duration, {animationRequired});
    delete params.duration;

    if (params.delay !== undefined)
        actor.set_easing_delay(params.delay, {animationRequired});
    delete params.delay;

    const repeatCount = params.repeatCount;
    delete params.repeatCount;

    const autoReverse = params.autoReverse;
    delete params.autoReverse;

    // repeatCount doesn't include the initial iteration
    const numIterations = repeatCount + 1;
    // whether the transition should finish where it started
    const isReversed = autoReverse && numIterations % 2 === 0;

    if (params.mode !== undefined)
        actor.set_easing_mode(params.mode);
    delete params.mode;

    const prepare = () => {
        global.compositor.disable_unredirect();
        global.begin_work();
    };
    const cleanup = () => {
        global.compositor.enable_unredirect();
        global.end_work();
    };
    let callback = _makeEaseCallback(params, cleanup);

    // cancel overwritten transitions
    let animatedProps = Object.keys(params).map(p => p.replace('_', '-', 'g'));
    animatedProps.forEach(p => actor.remove_transition(p));

    if (actor.get_easing_duration() > 0 || !isReversed)
        actor.set(params);
    actor.restore_easing_state();

    const transitions = animatedProps
        .map(p => actor.get_transition(p))
        .filter(t => t !== null);

    transitions.forEach(t => t.set({repeatCount, autoReverse}));

    const [transition] = transitions;

    if (transition && transition.delay)
        transition.connect('started', () => prepare());
    else
        prepare();

    if (transition)
        transition.connect('stopped', (t, finished) => callback(finished));
    else
        callback(true);
}

function _easeActorProperty(actor, propName, target, params) {
    params = {
        repeatCount: 0,
        autoReverse: false,
        animationRequired: false,
        ...params,
    };

    // Avoid pointless difference with ease()
    if (params.mode)
        params.progress_mode = params.mode;
    delete params.mode;

    const animationRequired = params.animationRequired;
    delete params.animationRequired;

    if (params.duration)
        params.duration = adjustAnimationTime(params.duration, {animationRequired});
    let duration = Math.floor(params.duration || 0);

    if (params.delay)
        params.delay = adjustAnimationTime(params.delay, {animationRequired});

    const repeatCount = params.repeatCount;
    delete params.repeatCount;

    const autoReverse = params.autoReverse;
    delete params.autoReverse;

    // repeatCount doesn't include the initial iteration
    const numIterations = repeatCount + 1;
    // whether the transition should finish where it started
    const isReversed = autoReverse && numIterations % 2 === 0;

    // Copy Clutter's behavior for implicit animations, see
    // should_skip_implicit_transition()
    if (actor instanceof Clutter.Actor && !actor.mapped)
        duration = 0;

    const prepare = () => {
        global.compositor.disable_unredirect();
        global.begin_work();
    };
    const cleanup = () => {
        global.compositor.enable_unredirect();
        global.end_work();
    };
    let callback = _makeEaseCallback(params, cleanup);

    // cancel overwritten transition
    actor.remove_transition(propName);

    if (duration === 0) {
        let [obj, prop] = _getPropertyTarget(actor, propName);

        if (!isReversed)
            obj[prop] = target;

        prepare();
        callback(true);

        return;
    }

    let pspec = actor.find_property(propName);
    let transition = new Clutter.PropertyTransition({
        property_name: propName,
        interval: new Clutter.Interval({value_type: pspec.value_type}),
        remove_on_complete: true,
        repeat_count: repeatCount,
        auto_reverse: autoReverse,
        ...params,
    });
    actor.add_transition(propName, transition);

    transition.set_to(target);

    if (transition.delay)
        transition.connect('started', () => prepare());
    else
        prepare();

    transition.connect('stopped', (t, finished) => callback(finished));
}

// Add some bindings to the global JS namespace
globalThis.global = Shell.Global.get();

globalThis._ = Gettext.gettext;
globalThis.C_ = Gettext.pgettext;
globalThis.ngettext = Gettext.ngettext;
globalThis.N_ = s => s;

GObject.gtypeNameBasedOnJSPath = true;

GObject.Object.prototype.connectObject = function (...args) {
    SignalTracker.connectObject(this, ...args);
};
GObject.Object.prototype.connect_object = function (...args) {
    SignalTracker.connectObject(this, ...args);
};
GObject.Object.prototype.disconnectObject = function (...args) {
    SignalTracker.disconnectObject(this, ...args);
};
GObject.Object.prototype.disconnect_object = function (...args) {
    SignalTracker.disconnectObject(this, ...args);
};

SignalTracker.registerDestroyableType(Clutter.Actor);

Cairo.Context.prototype.setSourceColor = function (color) {
    const {red, green, blue, alpha} = color;
    const rgb = [red, green, blue].map(v => v / 255.0);

    if (alpha !== 0xff)
        this.setSourceRGBA(...rgb, alpha / 255.0);
    else
        this.setSourceRGB(...rgb);
};

// Miscellaneous monkeypatching
_patchLayoutClass(Clutter.GridLayout, {
    row_spacing: 'spacing-rows',
    column_spacing: 'spacing-columns',
});
_patchLayoutClass(Clutter.BoxLayout, {spacing: 'spacing'});

const origSetEasingDuration = Clutter.Actor.prototype.set_easing_duration;
Clutter.Actor.prototype.set_easing_duration = function (msecs, params = {}) {
    origSetEasingDuration.call(this, adjustAnimationTime(msecs, params));
};
const origSetEasingDelay = Clutter.Actor.prototype.set_easing_delay;
Clutter.Actor.prototype.set_easing_delay = function (msecs, params = {}) {
    origSetEasingDelay.call(this, adjustAnimationTime(msecs, params));
};

Clutter.Actor.prototype.ease = function (props) {
    _easeActor(this, props);
};
Clutter.Actor.prototype.ease_property = function (propName, target, params) {
    _easeActorProperty(this, propName, target, params);
};
St.Adjustment.prototype.ease = function (target, params) {
    // we're not an actor of course, but we implement the same
    // transition API as Clutter.Actor, so this works anyway
    _easeActorProperty(this, 'value', target, params);
};

Clutter.Actor.prototype[Symbol.iterator] = function* () {
    for (let c = this.get_first_child(); c; c = c.get_next_sibling())
        yield c;
};

Clutter.Actor.prototype.toString = function () {
    return St.describe_actor(this);
};

Gio.File.prototype.touch_async = function (callback) {
    Shell.util_touch_file_async(this, callback);
};
Gio.File.prototype.touch_finish = function (result) {
    return Shell.util_touch_file_finish(this, result);
};

const origToString = Object.prototype.toString;
Object.prototype.toString = function () {
    let base = origToString.call(this);
    try {
        if ('actor' in this && this.actor instanceof Clutter.Actor)
            return base.replace(/\]$/, ` delegate for ${this.actor.toString().substring(1)}`);
        else
            return base;
    } catch {
        return base;
    }
};

const slowdownEnv = GLib.getenv('GNOME_SHELL_SLOWDOWN_FACTOR');
if (slowdownEnv) {
    let factor = parseFloat(slowdownEnv);
    if (!isNaN(factor) && factor > 0.0)
        St.Settings.get().slow_down_factor = factor;
}

function wrapSpawnFunction(func) {
    const originalFunc = GLib[func];
    return function (workingDirectory, argv, envp, flags, childSetup, ...args) {
        const commonArgs = [workingDirectory, argv, envp, flags];
        if (childSetup) {
            logError(new Error(`Using child GLib.${func} with a GLib.SpawnChildSetupFunc ` +
                'is unsafe and may dead-lock, thus it should never be used from JavaScript. ' +
                `Shell.${func} can be used to perform default actions or an ` +
                'async-signal-safe alternative should be used instead'));
            return originalFunc(...commonArgs, childSetup, ...args);
        }

        const retValue = Shell[`util_${func}`](...commonArgs, ...args);
        return [true, ...Array.isArray(retValue) ? retValue : [retValue]];
    };
}
GLib.spawn_async = wrapSpawnFunction('spawn_async');
GLib.spawn_async_with_pipes = wrapSpawnFunction('spawn_async_with_pipes');
GLib.spawn_async_with_fds = wrapSpawnFunction('spawn_async_with_fds');
GLib.spawn_async_with_pipes_and_fds = wrapSpawnFunction('spawn_async_with_pipes_and_fds');

// OK, now things are initialized enough that we can import shell JS
const Format = imports.format;

String.prototype.format = Format.format;

Math.clamp = function (x, lower, upper) {
    return Math.min(Math.max(x, lower), upper);
};

// Prevent extensions from opening a display connection to ourselves
Gdk.set_allowed_backends('');
