// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported GrabHelper */

const { Clutter, St } = imports.gi;

const Main = imports.ui.main;
const Params = imports.misc.params;

// GrabHelper:
// @owner: the actor that owns the GrabHelper
// @params: optional parameters to pass to Main.pushModal()
//
// Creates a new GrabHelper object, for dealing with keyboard and pointer grabs
// associated with a set of actors.
//
// Note that the grab can be automatically dropped at any time by the user, and
// your code just needs to deal with it; you shouldn't adjust behavior directly
// after you call ungrab(), but instead pass an 'onUngrab' callback when you
// call grab().
var GrabHelper = class GrabHelper {
    constructor(owner, params) {
        if (!(owner instanceof Clutter.Actor))
            throw new Error('GrabHelper owner must be a Clutter.Actor');

        this._owner = owner;
        this._modalParams = params;

        this._grabStack = [];

        this._ignoreUntilRelease = false;

        this._modalCount = 0;
    }

    _isWithinGrabbedActor(actor) {
        let currentActor = this.currentGrab.actor;
        while (actor) {
            if (actor == currentActor)
                return true;
            actor = actor.get_parent();
        }
        return false;
    }

    get currentGrab() {
        return this._grabStack[this._grabStack.length - 1] || {};
    }

    get grabbed() {
        return this._grabStack.length > 0;
    }

    get grabStack() {
        return this._grabStack;
    }

    _findStackIndex(actor) {
        if (!actor)
            return -1;

        for (let i = 0; i < this._grabStack.length; i++) {
            if (this._grabStack[i].actor === actor)
                return i;
        }
        return -1;
    }

    _actorInGrabStack(actor) {
        while (actor) {
            let idx = this._findStackIndex(actor);
            if (idx >= 0)
                return idx;
            actor = actor.get_parent();
        }
        return -1;
    }

    isActorGrabbed(actor) {
        return this._findStackIndex(actor) >= 0;
    }

    // grab:
    // @params: A bunch of parameters, see below
    //
    // The general effect of a "grab" is to ensure that the passed in actor
    // and all actors inside the grab get exclusive control of the mouse and
    // keyboard, with the grab automatically being dropped if the user tries
    // to dismiss it. The actor is passed in through @params.actor.
    //
    // grab() can be called multiple times, with the scope of the grab being
    // changed to a different actor every time. A nested grab does not have
    // to have its grabbed actor inside the parent grab actors.
    //
    // Grabs can be automatically dropped if the user tries to dismiss it
    // in one of two ways: the user clicking outside the currently grabbed
    // actor, or the user typing the Escape key.
    //
    // If the user clicks outside the grabbed actors, and the clicked on
    // actor is part of a previous grab in the stack, grabs will be popped
    // until that grab is active. However, the click event will not be
    // replayed to the actor.
    //
    // If the user types the Escape key, one grab from the grab stack will
    // be popped.
    //
    // When a grab is popped by user interacting as described above, if you
    // pass a callback as @params.onUngrab, it will be called with %true.
    //
    // If @params.focus is not null, we'll set the key focus directly
    // to that actor instead of navigating in @params.actor. This is for
    // use cases like menus, where we want to grab the menu actor, but keep
    // focus on the clicked on menu item.
    grab(params) {
        params = Params.parse(params, {
            actor: null,
            focus: null,
            onUngrab: null,
        });

        let focus = global.stage.key_focus;
        let hadFocus = focus && this._isWithinGrabbedActor(focus);
        let newFocus = params.actor;

        if (this.isActorGrabbed(params.actor))
            return true;

        params.savedFocus = focus;

        if (!this._takeModalGrab())
            return false;

        this._grabStack.push(params);

        if (params.focus) {
            params.focus.grab_key_focus();
        } else if (newFocus && hadFocus) {
            if (!newFocus.navigate_focus(null, St.DirectionType.TAB_FORWARD, false))
                newFocus.grab_key_focus();
        }

        return true;
    }

    grabAsync(params) {
        return new Promise((resolve, reject) => {
            params.onUngrab = resolve;

            if (!this.grab(params))
                reject(new Error('Grab failed'));
        });
    }

    _takeModalGrab() {
        let firstGrab = this._modalCount == 0;
        if (firstGrab) {
            let grab = Main.pushModal(this._owner, this._modalParams);
            if (grab.get_seat_state() !== Clutter.GrabState.ALL) {
                Main.popModal(grab);
                return false;
            }

            this._grab = grab;
            this._capturedEventId = this._owner.connect('captured-event',
                (actor, event) => {
                    return this.onCapturedEvent(event);
                });
        }

        this._modalCount++;
        return true;
    }

    _releaseModalGrab() {
        this._modalCount--;
        if (this._modalCount > 0)
            return;

        this._owner.disconnect(this._capturedEventId);
        this._ignoreUntilRelease = false;

        Main.popModal(this._grab);
        this._grab = null;
    }

    // ignoreRelease:
    //
    // Make sure that the next button release event evaluated by the
    // capture event handler returns false. This is designed for things
    // like the ComboBoxMenu that go away on press, but need to eat
    // the next release event.
    ignoreRelease() {
        this._ignoreUntilRelease = true;
    }

    // ungrab:
    // @params: The parameters for the grab; see below.
    //
    // Pops @params.actor from the grab stack, potentially dropping
    // the grab. If the actor is not on the grab stack, this call is
    // ignored with no ill effects.
    //
    // If the actor is not at the top of the grab stack, grabs are
    // popped until the grabbed actor is at the top of the grab stack.
    // The onUngrab callback for every grab is called for every popped
    // grab with the parameter %false.
    ungrab(params) {
        params = Params.parse(params, {
            actor: this.currentGrab.actor,
            isUser: false,
        });

        let grabStackIndex = this._findStackIndex(params.actor);
        if (grabStackIndex < 0)
            return;

        let focus = global.stage.key_focus;
        let hadFocus = focus && this._isWithinGrabbedActor(focus);

        let poppedGrabs = this._grabStack.slice(grabStackIndex);
        // "Pop" all newly ungrabbed actors off the grab stack
        // by truncating the array.
        this._grabStack.length = grabStackIndex;

        for (let i = poppedGrabs.length - 1; i >= 0; i--) {
            let poppedGrab = poppedGrabs[i];

            if (poppedGrab.onUngrab)
                poppedGrab.onUngrab(params.isUser);

            this._releaseModalGrab();
        }

        if (hadFocus) {
            let poppedGrab = poppedGrabs[0];
            if (poppedGrab.savedFocus)
                poppedGrab.savedFocus.grab_key_focus();
        }
    }

    onCapturedEvent(event) {
        let type = event.type();

        if (type == Clutter.EventType.KEY_PRESS &&
            event.get_key_symbol() == Clutter.KEY_Escape) {
            this.ungrab({ isUser: true });
            return Clutter.EVENT_STOP;
        }

        let motion = type == Clutter.EventType.MOTION;
        let press = type == Clutter.EventType.BUTTON_PRESS;
        let release = type == Clutter.EventType.BUTTON_RELEASE;
        let button = press || release;

        let touchUpdate = type == Clutter.EventType.TOUCH_UPDATE;
        let touchBegin = type == Clutter.EventType.TOUCH_BEGIN;
        let touchEnd = type == Clutter.EventType.TOUCH_END;
        let touch = touchUpdate || touchBegin || touchEnd;

        if (touch && !global.display.is_pointer_emulating_sequence(event.get_event_sequence()))
            return Clutter.EVENT_PROPAGATE;

        if (this._ignoreUntilRelease && (motion || release || touch)) {
            if (release || touchEnd)
                this._ignoreUntilRelease = false;
            return Clutter.EVENT_PROPAGATE;
        }

        const targetActor = global.stage.get_event_actor(event);

        if (type === Clutter.EventType.ENTER ||
            type === Clutter.EventType.LEAVE ||
            this.currentGrab.actor.contains(targetActor))
            return Clutter.EVENT_PROPAGATE;

        if (Main.keyboard.maybeHandleEvent(event))
            return Clutter.EVENT_PROPAGATE;

        if (button || touchBegin) {
            // If we have a press event, ignore the next
            // motion/release events.
            if (press || touchBegin)
                this._ignoreUntilRelease = true;

            let i = this._actorInGrabStack(targetActor) + 1;
            this.ungrab({ actor: this._grabStack[i].actor, isUser: true });
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_STOP;
    }
};
