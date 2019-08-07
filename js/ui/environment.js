// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported init */

const Config = imports.misc.config;

imports.gi.versions.Clutter = Config.LIBMUTTER_API_VERSION;
imports.gi.versions.Gio = '2.0';
imports.gi.versions.GdkPixbuf = '2.0';
imports.gi.versions.Gtk = '3.0';
imports.gi.versions.TelepathyGLib = '0.12';
imports.gi.versions.TelepathyLogger = '0.2';

const { Clutter, GLib, Shell, St } = imports.gi;
const Gettext = imports.gettext;

// We can't import shell JS modules yet, because they may have
// variable initializations, etc, that depend on init() already having
// been run.


// "monkey patch" in some varargs ClutterContainer methods; we need
// to do this per-container class since there is no representation
// of interfaces in Javascript
function _patchContainerClass(containerClass) {
    // This one is a straightforward mapping of the C method
    containerClass.prototype.child_set = function(actor, props) {
        let meta = this.get_child_meta(actor);
        for (let prop in props)
            meta[prop] = props[prop];
    };

    // clutter_container_add() actually is a an add-many-actors
    // method. We conveniently, but somewhat dubiously, take the
    // this opportunity to make it do something more useful.
    containerClass.prototype.add = function(actor, props) {
        this.add_actor(actor);
        if (props)
            this.child_set(actor, props);
    };
}

function _patchLayoutClass(layoutClass, styleProps) {
    if (styleProps)
        layoutClass.prototype.hookup_style = function(container) {
            container.connect('style-changed', () => {
                let node = container.get_theme_node();
                for (let prop in styleProps) {
                    let [found, length] = node.lookup_length(styleProps[prop], false);
                    if (found)
                        this[prop] = length;
                }
            });
        };
    layoutClass.prototype.child_set = function(actor, props) {
        let meta = this.get_child_meta(actor.get_parent(), actor);
        for (let prop in props)
            meta[prop] = props[prop];
    };
}

let _easingTransitions = new Map();

function _trackTransition(transition, callback) {
    if (_easingTransitions.has(transition))
        transition.disconnect(_easingTransitions.get(transition));

    let id = transition.connect('stopped', (t, isFinished) => {
        _easingTransitions.delete(transition);
        callback(isFinished);
    });

    _easingTransitions.set(transition, id);
}

function _makeEaseCallback(params) {
    let onComplete = params.onComplete;
    delete params.onComplete;

    let onStopped = params.onStopped;
    delete params.onStopped;

    if (!onComplete && !onStopped)
        return null;

    return isFinished => {
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
    case '@effects':
        return [actor.get_effect(name), prop];
    }

    throw new Error(`Invalid property name ${propName}`);
}

function _easeActor(actor, params) {
    actor.save_easing_state();

    if (params.duration != undefined)
        actor.set_easing_duration(params.duration);
    delete params.duration;

    if (params.delay != undefined)
        actor.set_easing_delay(params.delay);
    delete params.delay;

    if (params.mode != undefined)
        actor.set_easing_mode(params.mode);
    delete params.mode;

    let callback = _makeEaseCallback(params);

    // cancel overwritten transitions
    let animatedProps = Object.keys(params).map(p => p.replace('_', '-', 'g'));
    animatedProps.forEach(p => actor.remove_transition(p));

    actor.set(params);

    if (callback) {
        let transition = actor.get_transition(animatedProps[0]);

        if (transition)
            _trackTransition(transition, callback);
        else
            callback(true);
    }

    actor.restore_easing_state();
}

function _easeActorProperty(actor, propName, target, params) {
    // Avoid pointless difference with ease()
    if (params.mode)
        params.progress_mode = params.mode;
    delete params.mode;

    if (params.duration)
        params.duration = adjustAnimationTime(params.duration);
    let duration = Math.floor(params.duration || 0);

    let callback = _makeEaseCallback(params);

    // cancel overwritten transition
    actor.remove_transition(propName);

    if (duration == 0) {
        let [obj, prop] = _getPropertyTarget(actor, propName);
        obj[prop] = target;

        if (callback)
            callback(true);

        return;
    }

    let pspec = actor.find_property(propName);
    let transition = new Clutter.PropertyTransition(Object.assign({
        property_name: propName,
        interval: new Clutter.Interval({ value_type: pspec.value_type }),
        remove_on_complete: true
    }, params));
    actor.add_transition(propName, transition);

    transition.set_to(target);

    if (callback)
        _trackTransition(transition, callback);
}

function _loggingFunc(...args) {
    let fields = { 'MESSAGE': args.join(', ') };
    let domain = "GNOME Shell";

    // If the caller is an extension, add it as metadata
    let extension = imports.misc.extensionUtils.getCurrentExtension();
    if (extension != null) {
        domain = extension.metadata.name;
        fields['GNOME_SHELL_EXTENSION_UUID'] = extension.uuid;
        fields['GNOME_SHELL_EXTENSION_NAME'] = extension.metadata.name;
    }

    GLib.log_structured(domain, GLib.LogLevelFlags.LEVEL_MESSAGE, fields);
}

function init() {
    // Add some bindings to the global JS namespace; (gjs keeps the web
    // browser convention of having that namespace be called 'window'.)
    window.global = Shell.Global.get();

    window.log = _loggingFunc;

    window._ = Gettext.gettext;
    window.C_ = Gettext.pgettext;
    window.ngettext = Gettext.ngettext;
    window.N_ = s => s;

    // Miscellaneous monkeypatching
    _patchContainerClass(St.BoxLayout);

    _patchLayoutClass(Clutter.TableLayout, { row_spacing: 'spacing-rows',
                                             column_spacing: 'spacing-columns' });
    _patchLayoutClass(Clutter.GridLayout, { row_spacing: 'spacing-rows',
                                            column_spacing: 'spacing-columns' });
    _patchLayoutClass(Clutter.BoxLayout, { spacing: 'spacing' });

    let origSetEasingDuration = Clutter.Actor.prototype.set_easing_duration;
    Clutter.Actor.prototype.set_easing_duration = function(msecs) {
        origSetEasingDuration.call(this, adjustAnimationTime(msecs));
    };
    let origSetEasingDelay = Clutter.Actor.prototype.set_easing_delay;
    Clutter.Actor.prototype.set_easing_delay = function(msecs) {
        origSetEasingDelay.call(this, adjustAnimationTime(msecs));
    };

    Clutter.Actor.prototype.ease = function(props, easingParams) {
        _easeActor(this, props, easingParams);
    };
    Clutter.Actor.prototype.ease_property = function(propName, target, params) {
        _easeActorProperty(this, propName, target, params);
    };

    Clutter.Actor.prototype.toString = function() {
        return St.describe_actor(this);
    };
    // Deprecation warning for former JS classes turned into an actor subclass
    Object.defineProperty(Clutter.Actor.prototype, 'actor', {
        get() {
            let klass = this.constructor.name;
            let { stack } = new Error();
            log(`Usage of object.actor is deprecated for ${klass}\n${stack}`);
            return this;
        }
    });

    St.set_slow_down_factor = function(factor) {
        let { stack } = new Error();
        log(`St.set_slow_down_factor() is deprecated, use St.Settings.slow_down_factor\n${stack}`);
        St.Settings.get().slow_down_factor = factor;
    };

    let origToString = Object.prototype.toString;
    Object.prototype.toString = function() {
        let base = origToString.call(this);
        try {
            if ('actor' in this && this.actor instanceof Clutter.Actor)
                return base.replace(/\]$/, ` delegate for ${this.actor.toString().substring(1)}`);
            else
                return base;
        } catch (e) {
            return base;
        }
    };

    // Work around https://bugzilla.mozilla.org/show_bug.cgi?id=508783
    Date.prototype.toLocaleFormat = function(format) {
        return Shell.util_format_date(format, this.getTime());
    };

    let slowdownEnv = GLib.getenv('GNOME_SHELL_SLOWDOWN_FACTOR');
    if (slowdownEnv) {
        let factor = parseFloat(slowdownEnv);
        if (!isNaN(factor) && factor > 0.0)
            St.Settings.get().slow_down_factor = factor;
    }

    // OK, now things are initialized enough that we can import shell JS
    const Format = imports.format;
    const Tweener = imports.ui.tweener;

    Tweener.init();
    String.prototype.format = Format.format;
}

// adjustAnimationTime:
// @msecs: time in milliseconds
//
// Adjust @msecs to account for St's enable-animations
// and slow-down-factor settings
function adjustAnimationTime(msecs) {
    let settings = St.Settings.get();

    if (!settings.enable_animations)
        return 1;
    return settings.slow_down_factor * msecs;
}

