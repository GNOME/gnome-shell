/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Main = imports.ui.main;
const Params = imports.misc.params;

function _navigateActor(actor) {
    if (!actor)
        return;

    let needsGrab = true;
    if (actor instanceof St.Widget)
        needsGrab = !actor.navigate_focus(null, Gtk.DirectionType.TAB_FORWARD, false);
    if (needsGrab)
        actor.grab_key_focus();
}

// GrabHelper:
// @owner: the actor that owns the GrabHelper
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

    _init: function(owner) {
        this._owner = owner;

        this._grabStack = [];

        this._actors = [];
        this._capturedEventId = 0;
        this._eventId = 0;
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
        while (actor) {
            if (this._actors.indexOf(actor) != -1)
                return true;
            actor = actor.get_parent();
        }
        return false;
    },

    get currentGrab() {
        let idx = this._grabStack.length - 1;
        while (idx >= 0 && this._grabStack[idx].untracked)
            idx--;

        return this._grabStack[idx] || {};
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
    //
    // If @params contains { untracked: true }, then it will be skipped
    // when the grab helper ungrabs for you, or when calculating
    // currentGrab.
    grab: function(params) {
        params = Params.parse(params, { actor: null,
                                        modal: false,
                                        untracked: false,
                                        grabFocus: false,
                                        onUngrab: null });

        let focus = global.stage.key_focus;
        let hadFocus = focus && this._isWithinGrabbedActor(focus);
        let newFocus = params.actor;

        if (this.isActorGrabbed(params.actor))
            return true;

        if (this._grabFocusCount == 0 && this._modalCount == 0) {
            if (!this._fullGrab(hadFocus, params.modal, params.grabFocus))
                return false;
        }

        params.savedFocus = focus;
        this._grabStack.push(params);

        if (params.modal)
            this._modalCount++;

        if (params.grabFocus)
            this._grabFocusCount++;

        if (hadFocus || params.grabFocus)
            _navigateActor(newFocus);

        return true;
    },

    _fullGrab: function(hadFocus, modal, grabFocus) {
        let metaDisplay = global.screen.get_display();

        if (modal) {
            if (!Main.pushModal(this._owner))
                return false;
        }

        this._grabbedFromKeynav = hadFocus;
        this._preGrabInputMode = global.stage_input_mode;
        this._prevFocusedWindow = null;

        if (grabFocus) {
            this._prevFocusedWindow = metaDisplay.focus_window;
            if (this._preGrabInputMode == Shell.StageInputMode.NONREACTIVE ||
                this._preGrabInputMode == Shell.StageInputMode.NORMAL) {
                global.set_stage_input_mode(Shell.StageInputMode.FOCUSED);
            }
        }

        if (modal || grabFocus) {
            this._capturedEventId = global.stage.connect('captured-event', Lang.bind(this, this._onCapturedEvent));
            this._eventId = global.stage.connect('event', Lang.bind(this, this._onEvent));
            this._keyFocusNotifyId = global.stage.connect('notify::key-focus', Lang.bind(this, this._onKeyFocusChanged));
            this._focusWindowChangedId = metaDisplay.connect('notify::focus-window', Lang.bind(this, this._focusWindowChanged));
        }

        return true;
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
        params = Params.parse(params, { actor: this.currentGrab.actor });

        let grabStackIndex = this._findStackIndex(params.actor);
        if (grabStackIndex < 0)
            return;

        let poppedGrabs = this._grabStack.slice(grabStackIndex);
        // "Pop" all newly ungrabbed actors off the grab stack
        // by truncating the array.
        this._grabStack.length = grabStackIndex;

        let wasModal = this._modalCount > 0;
        for (let i = poppedGrabs.length - 1; i >= 0; i--) {
            let poppedGrab = poppedGrabs[i];

            if (poppedGrab.onUngrab)
                poppedGrab.onUngrab();

            if (poppedGrab.modal)
                this._modalCount--;

            if (poppedGrab.grabFocus)
                this._grabFocusCount--;
        }

        let focus = global.stage.key_focus;
        let hadFocus = focus && this._isWithinGrabbedActor(focus);

        if (this._grabFocusCount == 0 && this._modalCount == 0)
            this._fullUngrab(wasModal);

        let poppedGrab = poppedGrabs[0];

        if (hadFocus)
            _navigateActor(poppedGrab.savedFocus);
    },

    _fullUngrab: function(wasModal) {
        if (this._capturedEventId > 0) {
            global.stage.disconnect(this._capturedEventId);
            this._capturedEventId = 0;
        }

        if (this._eventId > 0) {
            global.stage.disconnect(this._eventId);
            this._eventId = 0;
        }

        if (this._keyFocusNotifyId > 0) {
            global.stage.disconnect(this._keyFocusNotifyId);
            this._keyFocusNotifyId = 0;
        }

        if (!this._focusWindowChanged > 0) {
            let metaDisplay = global.screen.get_display();
            metaDisplay.disconnect(this._focusWindowChangedId);
            this._focusWindowChangedId = 0;
        }

        let prePopInputMode = global.stage_input_mode;

        if (wasModal) {
            Main.popModal(this._owner);
            global.sync_pointer();
        }

        if (this._grabbedFromKeynav) {
            if (this._preGrabInputMode == Shell.StageInputMode.FOCUSED &&
                prePopInputMode != Shell.StageInputMode.FULLSCREEN)
                global.set_stage_input_mode(Shell.StageInputMode.FOCUSED);
        }

        if (this._prevFocusedWindow) {
            let metaDisplay = global.screen.get_display();
            if (!metaDisplay.focus_window) {
                metaDisplay.set_input_focus_window(this._prevFocusedWindow,
                                                   false, global.get_current_time());
            }
        }
    },

    _onCapturedEvent: function(actor, event) {
        let type = event.type();
        let press = type == Clutter.EventType.BUTTON_PRESS;
        let release = type == Clutter.EventType.BUTTON_RELEASE;
        let button = press || release;

        if (release && this._ignoreRelease) {
            this._ignoreRelease = false;
            return false;
        }

        if (!button && this._modalCount == 0)
            return false;

        if (this._isWithinGrabbedActor(event.get_source()))
            return false;

        if (button) {
            // If we have a press event, ignore the next event,
            // which should be a release event.
            if (press)
                this._ignoreRelease = true;
            this.ungrab();
        }

        return this._modalCount > 0;
    },

    // We catch 'event' rather than 'key-press-event' so that we get
    // a chance to run before the overview's own Escape check
    _onEvent: function(actor, event) {
        if (event.type() == Clutter.EventType.KEY_PRESS &&
            event.get_key_symbol() == Clutter.KEY_Escape) {
            this.ungrab();
            return true;
        }

        return false;
    },

    _onKeyFocusChanged: function() {
        let focus = global.stage.key_focus;
        if (!focus || !this._isWithinGrabbedActor(focus))
            this.ungrab();
    },

    _focusWindowChanged: function() {
        let metaDisplay = global.screen.get_display();
        if (metaDisplay.focus_window != null)
            this.ungrab();
    }
});
