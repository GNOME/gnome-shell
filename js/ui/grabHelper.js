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
        this._keyFocusNotifyId = 0;
        this._focusWindowChangedId = 0;
        this._ignoreRelease = false;

        this._modalCount = 0;
        this._grabFocusCount = 0;
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
    // Grabs the mouse and keyboard, according to the GrabHelper's
    // parameters. If @newFocus is not %null, then the keyboard focus
    // is moved to the first #StWidget:can-focus widget inside it.
    //
    // The grab will automatically be dropped if:
    //   - The user clicks outside the grabbed actors
    //   - The user types Escape
    //   - The keyboard focus is moved outside the grabbed actors
    //   - A window is focused
    //
    // If @params.actor is not null, then it will be focused as the
    // new actor. If you attempt to grab an already focused actor, the
    // request to be focused will be ignored. The actor will not be
    // added to the grab stack, so do not call a paired ungrab().
    //
    // If @params contains { modal: true }, then grab() will push a modal
    // on the owner of the GrabHelper. As long as there is at least one
    // { modal: true } actor on the grab stack, the grab will be kept.
    // When the last { modal: true } actor is ungrabbed, then the modal
    // will be dropped. A modal grab can fail if there is already a grab
    // in effect from aother application; in this case the function returns
    // false and nothing happens. Non-modal grabs can never fail.
    //
    // If @params contains { grabFocus: true }, then if you call grab()
    // while the shell is outside the overview, it will set the stage
    // input mode to %Shell.StageInputMode.FOCUSED, and ungrab() will
    // revert it back, and re-focus the previously-focused window (if
    // another window hasn't been explicitly focused before then).
    grab: function(params) {
        params = Params.parse(params, { actor: null,
                                        modal: false,
                                        grabFocus: false,
                                        focus: null,
                                        onUngrab: null });

        let focus = global.stage.key_focus;
        let hadFocus = focus && this._isWithinGrabbedActor(focus);
        let newFocus = params.actor;

        if (this.isActorGrabbed(params.actor))
            return true;

        params.savedFocus = focus;

        if (params.modal && !this._takeModalGrab())
            return false;

        if (params.grabFocus && !this._takeFocusGrab(hadFocus))
            return false;

        if (params.focus) {
            params.focus.grab_key_focus();
        } else if (newFocus && (hadFocus || params.grabFocus)) {
            if (!newFocus.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false))
                newFocus.grab_key_focus();
        }

        if ((params.grabFocus || params.modal) && !this._capturedEventId)
            this._capturedEventId = global.stage.connect('captured-event', Lang.bind(this, this._onCapturedEvent));

        this._grabStack.push(params);
        return true;
    },

    _takeModalGrab: function() {
        let firstGrab = (this._modalCount == 0);
        if (firstGrab) {
            if (!Main.pushModal(this._owner, this._modalParams))
                return false;
        }

        this._modalCount++;
        return true;
    },

    _releaseModalGrab: function() {
        this._modalCount--;
        if (this._modalCount > 0)
            return;

        Main.popModal(this._owner);
        global.sync_pointer();
    },

    _takeFocusGrab: function(hadFocus) {
        let firstGrab = (this._grabFocusCount == 0);
        if (firstGrab) {
            let metaDisplay = global.screen.get_display();

            this._grabbedFromKeynav = hadFocus;
            this._preGrabInputMode = global.stage_input_mode;

            if (this._preGrabInputMode == Shell.StageInputMode.NONREACTIVE ||
                this._preGrabInputMode == Shell.StageInputMode.NORMAL) {
                global.set_stage_input_mode(Shell.StageInputMode.FOCUSED);
            }

            this._keyFocusNotifyId = global.stage.connect('notify::key-focus', Lang.bind(this, this._onKeyFocusChanged));
            this._focusWindowChangedId = metaDisplay.connect('notify::focus-window', Lang.bind(this, this._focusWindowChanged));
        }

        this._grabFocusCount++;
        return true;
    },

    _releaseFocusGrab: function() {
        this._grabFocusCount--;
        if (this._grabFocusCount > 0)
            return;

        if (this._keyFocusNotifyId > 0) {
            global.stage.disconnect(this._keyFocusNotifyId);
            this._keyFocusNotifyId = 0;
        }

        if (this._focusWindowChangedId > 0) {
            let metaDisplay = global.screen.get_display();
            metaDisplay.disconnect(this._focusWindowChangedId);
            this._focusWindowChangedId = 0;
        }

        let prePopInputMode = global.stage_input_mode;

        if (this._grabbedFromKeynav) {
            if (this._preGrabInputMode == Shell.StageInputMode.FOCUSED &&
                prePopInputMode != Shell.StageInputMode.FULLSCREEN)
                global.set_stage_input_mode(Shell.StageInputMode.FOCUSED);
        }

        global.screen.focus_default_window(global.get_current_time());
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
    // Pops an actor from the grab stack, potentially dropping the grab.
    //
    // If the actor that was popped from the grab stack was not the actor
    // That was passed in, this call is ignored.
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

            if (poppedGrab.modal)
                this._releaseModalGrab();

            if (poppedGrab.grabFocus)
                this._releaseFocusGrab();
        }

        if (!this.grabbed && this._capturedEventId > 0) {
            global.stage.disconnect(this._capturedEventId);
            this._capturedEventId = 0;
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

        if (!button && this._modalCount == 0)
            return false;

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
        }

        return this._modalCount > 0;
    },

    _onKeyFocusChanged: function() {
        let focus = global.stage.key_focus;
        if (!focus || !this._isWithinGrabbedActor(focus))
            this.ungrab({ isUser: true });
    },

    _focusWindowChanged: function() {
        let metaDisplay = global.screen.get_display();
        if (metaDisplay.focus_window != null)
            this.ungrab({ isUser: true });
    }
});
