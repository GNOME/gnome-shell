// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

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
const GrabHelper = new Lang.Class({
    Name: 'GrabHelper',

    _init: function(owner, params) {
        this._owner = owner;
        this._modalParams = params;

        this._grabStack = [];

        this._actors = [];
        this._capturedEventId = 0;
        this._ignoreRelease = false;

        this._modalCount = 0;
    },

    // addActor:
    // @actor: an actor
    //
    // Adds @actor to the set of actors that are allowed to process events
    // during a grab.
    addActor: function(actor) {
        actor.__grabHelperDestroyId = actor.connect('destroy', Lang.bind(this, function() { this.removeActor(actor); }));
        this._actors.push(actor);
    },

    // removeActor:
    // @actor: an actor
    //
    // Removes @actor from the set of actors that are allowed to
    // process events during a grab.
    removeActor: function(actor) {
        let index = this._actors.indexOf(actor);
        if (index != -1)
            this._actors.splice(index, 1);
        if (actor.__grabHelperDestroyId) {
            actor.disconnect(actor.__grabHelperDestroyId);
            delete actor.__grabHelperDestroyId;
        }
    },

    _isWithinGrabbedActor: function(actor) {
        let currentActor = this.currentGrab.actor;
        while (actor) {
            if (this._actors.indexOf(actor) != -1)
                return true;
            if (actor == currentActor)
                return true;
            actor = actor.get_parent();
        }
        return false;
    },

    get currentGrab() {
        return this._grabStack[this._grabStack.length - 1] || {};
    },

    get grabbed() {
        return this._grabStack.length > 0;
    },

    get grabStack() {
        return this._grabStack;
    },

    _findStackIndex: function(actor) {
        if (!actor)
            return -1;

        for (let i = 0; i < this._grabStack.length; i++) {
            if (this._grabStack[i].actor === actor)
                return i;
        }
        return -1;
    },

    _actorInGrabStack: function(actor) {
        while (actor) {
            let idx = this._findStackIndex(actor);
            if (idx >= 0)
                return idx;
            actor = actor.get_parent();
        }
        return -1;
    },

    isActorGrabbed: function(actor) {
        return this._findStackIndex(actor) >= 0;
    },

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
    grab: function(params) {
        params = Params.parse(params, { actor: null,
                                        focus: null,
                                        onUngrab: null });

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
            if (!newFocus.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false))
                newFocus.grab_key_focus();
        }

        return true;
    },

    _takeModalGrab: function() {
        let firstGrab = (this._modalCount == 0);
        if (firstGrab) {
            if (!Main.pushModal(this._owner, this._modalParams))
                return false;

            this._capturedEventId = global.stage.connect('captured-event', Lang.bind(this, this._onCapturedEvent));
        }

        this._modalCount++;
        return true;
    },

    _releaseModalGrab: function() {
        this._modalCount--;
        if (this._modalCount > 0)
            return;

        global.stage.disconnect(this._capturedEventId);
        this._capturedEventId = 0;

        this._ignoreRelease = false;

        Main.popModal(this._owner);
        global.sync_pointer();
    },

    // ignoreRelease:
    //
    // Make sure that the next button release event evaluated by the
    // capture event handler returns false. This is designed for things
    // like the ComboBoxMenu that go away on press, but need to eat
    // the next release event.
    ignoreRelease: function() {
        this._ignoreRelease = true;
    },

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
    ungrab: function(params) {
        params = Params.parse(params, { actor: this.currentGrab.actor,
                                        isUser: false });

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
    },

    _onCapturedEvent: function(actor, event) {
        let type = event.type();

        if (type == Clutter.EventType.KEY_PRESS &&
            event.get_key_symbol() == Clutter.KEY_Escape) {
            this.ungrab({ isUser: true });
            return true;
        }

        let press = type == Clutter.EventType.BUTTON_PRESS;
        let release = type == Clutter.EventType.BUTTON_RELEASE;
        let button = press || release;

        if (release && this._ignoreRelease) {
            this._ignoreRelease = false;
            return true;
        }

        if (this._isWithinGrabbedActor(event.get_source()))
            return false;

        if (Main.keyboard.shouldTakeEvent(event))
            return false;

        if (button) {
            // If we have a press event, ignore the next event,
            // which should be a release event.
            if (press)
                this._ignoreRelease = true;
            let i = this._actorInGrabStack(event.get_source()) + 1;
            this.ungrab({ actor: this._grabStack[i].actor, isUser: true });
            return true;
        }

        return true;
    },
});
