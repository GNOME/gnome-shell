/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;

const Main = imports.ui.main;

// This manages the shell "chrome"; the UI that's visible in the
// normal mode (ie, outside the overlay), that surrounds the main
// workspace content.

function Chrome() {
    this._init();
}

Chrome.prototype = {
    _init: function() {
        let global = Shell.Global.get();

        // The group itself has zero size so it doesn't interfere with DND
        this.actor = new Clutter.Group({ width: 0, height: 0 });
        global.stage.add_actor(this.actor);
        this.nonOverlayActor = new Clutter.Group();
        this.actor.add_actor(this.nonOverlayActor);

        this._obscuredByFullscreen = false;

        this._trackedActors = [];

        global.screen.connect('restacked',
                              Lang.bind(this, this._windowsRestacked));

        // Need to update struts on new workspaces when they are added
        global.screen.connect('notify::n-workspaces',
                              Lang.bind(this, this._queueUpdateRegions));

        Main.overlay.connect('showing',
                             Lang.bind(this, this._overlayShowing));
        Main.overlay.connect('hidden',
                             Lang.bind(this, this._overlayHidden));

        this._queueUpdateRegions();
    },

    _verifyAncestry: function(actor, ancestor) {
        while (actor) {
            if (actor == ancestor)
                return true;
            actor = actor.get_parent();
        }
        return false;
    },

    // addActor:
    // @actor: an actor to add to the chrome layer
    // @shapeActor: optional "shape actor".
    //
    // Adds @actor to the chrome layer and extends the input region
    // and window manager struts to include it. (Window manager struts
    // will only be affected if @actor is touching a screen edge.)
    // Changes in @actor's size and position will automatically result
    // in appropriate changes to the input region and struts. Changes
    // in its visibility will affect the input region, but NOT the
    // struts.
    //
    // If @shapeActor is provided, it will be used instead of @actor
    // for the input region/strut shape. (This lets you have things like
    // drop shadows in @actor that don't affect the struts.) It must
    // be a child of @actor. Alternatively, you can pass %null for
    // @shapeActor to indicate that @actor should not affect the input
    // region or struts at all.
    addActor: function(actor, shapeActor) {
        if (shapeActor === undefined)
            shapeActor = actor;
        else if (shapeActor && !this._verifyAncestry(shapeActor, actor))
            throw new Error('shapeActor is not a descendent of actor');

        this.nonOverlayActor.add_actor(actor);

        if (shapeActor)
            this._trackActor(shapeActor, true, true);
    },

    // setVisibleInOverlay:
    // @actor: an actor in the chrome layer
    // @visible: overlay visibility
    //
    // By default, actors in the chrome layer are automatically hidden
    // when the overlay is shown. This can be used to override that
    // behavior
    setVisibleInOverlay: function(actor, visible) {
        if (!this._verifyAncestry(actor, this.actor))
            throw new Error('actor is not a descendent of the chrome layer');

        if (visible)
            actor.reparent(this.actor);
        else
            actor.reparent(this.nonOverlayActor);
    },

    // addInputRegionActor:
    // @actor: an actor to add to the stage input region
    //
    // Adds @actor to the stage input region, as with addActor(), but
    // for actors that are already descendants of the chrome layer.
    addInputRegionActor: function(actor) {
        if (!this._verifyAncestry(actor, this.actor))
            throw new Error('actor is not a descendent of the chrome layer');

        this._trackActor(actor, true, false);
    },

    // removeInputRegionActor:
    // @actor: an actor previously added to the stage input region
    //
    // Undoes the effect of addInputRegionActor()
    removeInputRegionActor: function(actor) {
        this._untrackActor(actor, true, false);
    },

    // removeActor:
    // @actor: a child of the chrome layer
    //
    // Removes @actor from the chrome layer
    removeActor: function(actor) {
        this.actor.remove_actor(actor);
        // We don't have to do anything else; the parent-set handlers
        // will do the rest.
    },

    _findActor: function(actor) {
        for (let i = 0; i < this._trackedActors.length; i++) {
            let actorData = this._trackedActors[i];
            if (actorData.actor == actor)
                return i;
        }
        return -1;
    },

    _trackActor: function(actor, inputRegion, strut) {
        let actorData;
        let i = this._findActor(actor);

        if (i != -1) {
            actorData = this._trackedActors[i];
            if (inputRegion)
                actorData.inputRegion++;
            if (strut)
                actorData.strut++;
            if (!inputRegion && !strut)
                actorData.children++;
            return;
        }

        actorData = { actor: actor,
                      inputRegion: inputRegion ? 1 : 0,
                      strut: strut ? 1 : 0,
                      children: 0 };

        actorData.visibleId = actor.connect('notify::visible',
                                            Lang.bind(this, this._queueUpdateRegions));
        actorData.allocationId = actor.connect('notify::allocation',
                                               Lang.bind(this, this._queueUpdateRegions));
        actorData.parentSetId = actor.connect('parent-set',
                                              Lang.bind(this, this._actorReparented));

        this._trackedActors.push(actorData);

        actor = actor.get_parent();
        if (actor != this.actor)
            this._trackActor(actor, false, false);

        if (inputRegion || strut)
            this._queueUpdateRegions();
    },

    _untrackActor: function(actor, inputRegion, strut) {
        let i = this._findActor(actor);

        if (i == -1)
            return;
        let actorData = this._trackedActors[i];

        if (inputRegion)
            actorData.inputRegion--;
        if (strut)
            actorData.strut--;
        if (!inputRegion && !strut)
            actorData.children--;

        if (actorData.inputRegion <= 0 && actorData.strut <= 0 && actorData.children <= 0) {
            this._trackedActors.splice(i, 1);
            actor.disconnect(actorData.visibleId);
            actor.disconnect(actorData.allocationId);
            actor.disconnect(actorData.parentSetId);

            actor = actor.get_parent();
            if (actor && actor != this.actor)
                this._untrackActor(actor, false, false);
        }

        if (inputRegion || strut)
            this._queueUpdateRegions();
    },

    _actorReparented: function(actor, oldParent) {
        if (this._verifyAncestry(actor, this.actor)) {
            let newParent = actor.get_parent();
            if (newParent != this.actor)
                this._trackActor(newParent, false, false);
        }
        if (oldParent != this.actor)
            this._untrackActor(oldParent, false, false);
    },

    _overlayShowing: function() {
        this.actor.show();
        this.nonOverlayActor.hide();
        this._queueUpdateRegions();
    },

    _overlayHidden: function() {
        if (this._obscuredByFullscreen)
            this.actor.hide();
        this.nonOverlayActor.show();
        this._queueUpdateRegions();
    },

    _queueUpdateRegions: function() {
        if (!this._updateRegionIdle)
            this._updateRegionIdle = Mainloop.idle_add(Lang.bind(this, this._updateRegions));
    },

    _windowsRestacked: function() {
        let global = Shell.Global.get();
        let windows = global.get_windows();

        // The chrome layer should be visible unless there is a window
        // with layer FULLSCREEN, or a window with layer
        // OVERRIDE_REDIRECT that covers the whole screen.
        // ("override_redirect" is not actually a layer above all
        // other windows, but this seems to be how mutter treats it
        // currently...) If we wanted to be extra clever, we could
        // figure out when an OVERRIDE_REDIRECT window was trying to
        // partially overlap us, and then adjust the input region and
        // our clip region accordingly...

        // @windows is sorted bottom to top.

        this._obscuredByFullscreen = false;
        for (let i = windows.length - 1; i > -1; i--) {
            let layer = windows[i].get_meta_window().get_layer();

            if (layer == Meta.StackLayer.OVERRIDE_REDIRECT) {
                if (windows[i].x <= 0 &&
                    windows[i].x + windows[i].width >= global.screen_width &&
                    windows[i].y <= 0 &&
                    windows[i].y + windows[i].height >= global.screen_height) {
                    this._obscuredByFullscreen = true;
                    break;
                }
            } else if (layer == Meta.StackLayer.FULLSCREEN) {
                this._obscuredByFullscreen = true;
                break;
            } else
                break;
        }

        let shouldBeVisible = !this._obscuredByFullscreen || Main.overlay.visible;
        if (this.actor.visible != shouldBeVisible) {
            this.actor.visible = shouldBeVisible;
            this._queueUpdateRegions();
        }
    },

    _updateRegions: function() {
        let global = Shell.Global.get();
        let rects = [], struts = [], i;

        delete this._updateRegionIdle;

        for (i = 0; i < this._trackedActors.length; i++) {
            let actorData = this._trackedActors[i];
            if (!actorData.inputRegion && !actorData.strut)
                continue;

            let [x, y] = actorData.actor.get_transformed_position();
            let [w, h] = actorData.actor.get_transformed_size();
            x = Math.round(x);
            y = Math.round(y);
            w = Math.round(w);
            h = Math.round(h);
            let rect = new Meta.Rectangle({ x: x, y: y, width: w, height: h});

            if (actorData.inputRegion && actorData.actor.get_paint_visibility())
                rects.push(rect);

            if (!actorData.strut)
                continue;

            // Metacity wants to know what side of the screen the
            // strut is considered to be attached to. If the actor is
            // only touching one edge, or is touching the entire
            // width/height of one edge, then it's obvious which side
            // to call it. If it's in a corner, we pick a side
            // arbitrarily. If it doesn't touch any edges, or it spans
            // the width/height across the middle of the screen, then
            // we don't create a strut for it at all.
            let side;
            if (w >= global.screen_width) {
                if (y <= 0)
                    side = Meta.Side.TOP;
                else if (y + h >= global.screen_height)
                    side = Meta.Side.BOTTOM;
                else
                    continue;
            } else if (h >= global.screen_height) {
                if (x <= 0)
                    side = Meta.Side.LEFT;
                else if (x + w >= global.screen_width)
                    side = Meta.Side.RIGHT;
                else
                    continue;
            } else if (x <= 0)
                side = Meta.Side.LEFT;
            else if (y <= 0)
                side = Meta.Side.TOP;
            else if (x + w >= global.screen_width)
                side = Meta.Side.RIGHT;
            else if (y + h >= global.screen_height)
                side = Meta.Side.BOTTOM;
            else
                continue;

            let strut = new Meta.Strut({ rect: rect, side: side });
            struts.push(strut);
        }

        global.set_stage_input_region(rects);

        let screen = global.screen;
        for (let w = 0; w < screen.n_workspaces; w++) {
            let workspace = screen.get_workspace_by_index(w);
            workspace.set_builtin_struts(struts);
        }

        return false;
    }
};
