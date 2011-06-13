/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const Main = imports.ui.main;
const Params = imports.misc.params;

// This manages the shell "chrome"; the UI that's visible in the
// normal mode (ie, outside the Overview), that surrounds the main
// workspace content.

const defaultParams = {
    visibleInFullscreen: false,
    affectsStruts: true,
    affectsInputRegion: true
};

function Chrome() {
    this._init();
}

Chrome.prototype = {
    _init: function() {
        // The group itself has zero size so it doesn't interfere with DND
        this.actor = new Shell.GenericContainer({ width: 0, height: 0 });
        Main.uiGroup.add_actor(this.actor);
        this.actor.connect('allocate', Lang.bind(this, this._allocated));

        this._monitors = [];
        this._inOverview = false;

        this._trackedActors = [];

        Main.layoutManager.connect('monitors-changed',
                                   Lang.bind(this, this._relayout));
        global.screen.connect('restacked',
                              Lang.bind(this, this._windowsRestacked));

        // Need to update struts on new workspaces when they are added
        global.screen.connect('notify::n-workspaces',
                              Lang.bind(this, this._queueUpdateRegions));

        Main.overview.connect('showing',
                             Lang.bind(this, this._overviewShowing));
        Main.overview.connect('hidden',
                             Lang.bind(this, this._overviewHidden));

        this._relayout();
    },

    _allocated: function(actor, box, flags) {
        let children = this.actor.get_children();
        for (let i = 0; i < children.length; i++)
            children[i].allocate_preferred_size(flags);
    },

    // addActor:
    // @actor: an actor to add to the chrome layer
    // @params: (optional) additional params
    //
    // Adds @actor to the chrome layer and extends the input region
    // and window manager struts to include it. (Window manager struts
    // will only be affected if @actor is touching a screen edge.)
    // Changes in @actor's size and position will automatically result
    // in appropriate changes to the input region and struts. Changes
    // in its visibility will affect the input region, but NOT the
    // struts.
    //
    // If %visibleInFullscreen is %true, the actor will be visible
    // even when a fullscreen window should be covering it.
    //
    // If %affectsStruts or %affectsInputRegion is %false, the actor
    // will not have the indicated effect.
    addActor: function(actor, params) {
        this.actor.add_actor(actor);
        this._trackActor(actor, params);
    },

    // trackActor:
    // @actor: a descendant of the chrome to begin tracking
    // @params: parameters describing how to track @actor
    //
    // Tells the chrome to track @actor, which must be a descendant
    // of an actor added via addActor(). This can be used to extend the
    // struts or input region to cover specific children.
    //
    // @params can have any of the same values as in addActor(), though
    // some possibilities don't make sense (eg, trying to have a
    // %visibleInFullscreen child of a non-%visibleInFullscreen parent).
    // By default, @actor has the same params as its chrome ancestor.
    trackActor: function(actor, params) {
        let ancestor = actor.get_parent();
        let index = this._findActor(ancestor);
        while (ancestor && index == -1) {
            ancestor = ancestor.get_parent();
            index = this._findActor(ancestor);
        }
        if (!ancestor)
            throw new Error('actor is not a descendent of the chrome layer');

        let ancestorData = this._trackedActors[index];
        if (!params)
            params = {};
        // We can't use Params.parse here because we want to drop
        // the extra values like ancestorData.actor
        for (let prop in defaultParams) {
            if (!params[prop])
                params[prop] = ancestorData[prop];
        }

        this._trackActor(actor, params);
    },

    // untrackActor:
    // @actor: an actor previously tracked via trackActor()
    //
    // Undoes the effect of trackActor()
    untrackActor: function(actor) {
        this._untrackActor(actor);
    },

    // removeActor:
    // @actor: a child of the chrome layer
    //
    // Removes @actor from the chrome layer
    removeActor: function(actor) {
        this.actor.remove_actor(actor);
        this._untrackActor(actor);
    },

    _findActor: function(actor) {
        for (let i = 0; i < this._trackedActors.length; i++) {
            let actorData = this._trackedActors[i];
            if (actorData.actor == actor)
                return i;
        }
        return -1;
    },

    _trackActor: function(actor, params) {
        if (this._findActor(actor) != -1)
            throw new Error('trying to re-track existing chrome actor');

        let actorData = Params.parse(params, defaultParams);
        actorData.actor = actor;
        actorData.visibleId = actor.connect('notify::visible',
                                            Lang.bind(this, this._queueUpdateRegions));
        actorData.allocationId = actor.connect('notify::allocation',
                                               Lang.bind(this, this._queueUpdateRegions));
        actorData.parentSetId = actor.connect('parent-set',
                                              Lang.bind(this, this._actorReparented));
        // Note that destroying actor will unset its parent, so we don't
        // need to connect to 'destroy' too.

        this._trackedActors.push(actorData);
        this._queueUpdateRegions();
    },

    _untrackActor: function(actor) {
        let i = this._findActor(actor);

        if (i == -1)
            return;
        let actorData = this._trackedActors[i];

        this._trackedActors.splice(i, 1);
        actor.disconnect(actorData.visibleId);
        actor.disconnect(actorData.allocationId);
        actor.disconnect(actorData.parentSetId);

        this._queueUpdateRegions();
    },

    _actorReparented: function(actor, oldParent) {
        if (!this.actor.contains(actor))
            this._untrackActor(actor);
    },

    _updateVisibility: function() {
        for (let i = 0; i < this._trackedActors.length; i++) {
            let actorData = this._trackedActors[i];
            if (!this._inOverview && !actorData.visibleInFullscreen &&
                this._findMonitorForActor(actorData.actor).inFullscreen)
                this.actor.set_skip_paint(actorData.actor, true);
            else
                this.actor.set_skip_paint(actorData.actor, false);
        }
    },

    _overviewShowing: function() {
        this._inOverview = true;
        this._updateVisibility();
        this._queueUpdateRegions();
    },

    _overviewHidden: function() {
        this._inOverview = false;
        this._updateVisibility();
        this._queueUpdateRegions();
    },

    _relayout: function() {
        this._monitors = Main.layoutManager.monitors;
        this._primaryMonitor = Main.layoutManager.primaryMonitor;

        this._updateFullscreen();
        this._updateVisibility();
        this._queueUpdateRegions();
    },

    _findMonitorForRect: function(x, y, w, h) {
        // First look at what monitor the center of the rectangle is at
        let cx = x + w/2;
        let cy = y + h/2;
        for (let i = 0; i < this._monitors.length; i++) {
            let monitor = this._monitors[i];
            if (cx >= monitor.x && cx < monitor.x + monitor.width &&
                cy >= monitor.y && cy < monitor.y + monitor.height)
                return monitor;
        }
        // If the center is not on a monitor, return the first overlapping monitor
        for (let i = 0; i < this._monitors.length; i++) {
            let monitor = this._monitors[i];
            if (x + w > monitor.x && x < monitor.x + monitor.width &&
                y + h > monitor.y && y < monitor.y + monitor.height)
                return monitor;
        }
        // otherwise on no monitor
        return null;
    },

    _findMonitorForWindow: function(window) {
        return this._findMonitorForRect(window.x, window.y, window.width, window.height);
    },

    // This call guarantees that we return some monitor to simplify usage of it
    // In practice all tracked actors should be visible on some monitor anyway
    _findMonitorForActor: function(actor) {
        let [x, y] = actor.get_transformed_position();
        let [w, h] = actor.get_transformed_size();
        let monitor = this._findMonitorForRect(x, y, w, h);
        if (monitor)
            return monitor;
        return this._primaryMonitor; // Not on any monitor, pretend its on the primary
    },

    _queueUpdateRegions: function() {
        if (!this._updateRegionIdle)
            this._updateRegionIdle = Mainloop.idle_add(Lang.bind(this, this._updateRegions),
                                                       Meta.PRIORITY_BEFORE_REDRAW);
    },

    _updateFullscreen: function() {
        let windows = Main.getWindowActorsForWorkspace(global.screen.get_active_workspace_index());

        // Reset all monitors to not fullscreen
        for (let i = 0; i < this._monitors.length; i++)
            this._monitors[i].inFullscreen = false;

        // The chrome layer should be visible unless there is a window
        // with layer FULLSCREEN, or a window with layer
        // OVERRIDE_REDIRECT that covers the whole screen.
        // ('override_redirect' is not actually a layer above all
        // other windows, but this seems to be how mutter treats it
        // currently...) If we wanted to be extra clever, we could
        // figure out when an OVERRIDE_REDIRECT window was trying to
        // partially overlap us, and then adjust the input region and
        // our clip region accordingly...

        // @windows is sorted bottom to top.

        for (let i = windows.length - 1; i > -1; i--) {
            let window = windows[i];
            let layer = window.get_meta_window().get_layer();

            if (layer == Meta.StackLayer.FULLSCREEN) {
                let monitor = this._findMonitorForWindow(window);
                if (monitor)
                    monitor.inFullscreen = true;
            }
            if (layer == Meta.StackLayer.OVERRIDE_REDIRECT) {
                let monitor = this._findMonitorForWindow(window);
                if (monitor &&
                    window.x <= monitor.x &&
                    window.x + window.width >= monitor.x + monitor.width &&
                    window.y <= monitor.y &&
                    window.y + window.height >= monitor.y + monitor.height)
                    monitor.inFullscreen = true;
            } else
                break;
        }
    },

    _windowsRestacked: function() {
        let wasInFullscreen = [];
        for (let i = 0; i < this._monitors.length; i++)
            wasInFullscreen[i] = this._monitors[i].inFullscreen;

        this._updateFullscreen();

        let changed = false;
        for (let i = 0; i < wasInFullscreen.length; i++) {
            if (wasInFullscreen[i] != this._monitors[i].inFullscreen) {
                changed = true;
                break;
            }
        }
        if (changed) {
            this._updateVisibility();
            this._queueUpdateRegions();
        }
    },

    _updateRegions: function() {
        let rects = [], struts = [], i;

        delete this._updateRegionIdle;

        for (i = 0; i < this._trackedActors.length; i++) {
            let actorData = this._trackedActors[i];
            if (!actorData.affectsInputRegion && !actorData.affectsStruts)
                continue;

            let [x, y] = actorData.actor.get_transformed_position();
            let [w, h] = actorData.actor.get_transformed_size();
            x = Math.round(x);
            y = Math.round(y);
            w = Math.round(w);
            h = Math.round(h);
            let rect = new Meta.Rectangle({ x: x, y: y, width: w, height: h});

            if (actorData.affectsInputRegion &&
                actorData.actor.get_paint_visibility() &&
                !this.actor.get_skip_paint(actorData.actor))
                rects.push(rect);

            if (!actorData.affectsStruts)
                continue;

            // Limit struts to the size of the screen
            let x1 = Math.max(x, 0);
            let x2 = Math.min(x + w, global.screen_width);
            let y1 = Math.max(y, 0);
            let y2 = Math.min(y + h, global.screen_height);

            // NetWM struts are not really powerful enought to handle
            // a multi-monitor scenario, they only describe what happens
            // around the outer sides of the full display region. However
            // it can describe a partial region along each side, so
            // we can support having the struts only affect the
            // primary monitor. This should be enough as we only have
            // chrome affecting the struts on the primary monitor so
            // far.
            //
            // Metacity wants to know what side of the screen the
            // strut is considered to be attached to. If the actor is
            // only touching one edge, or is touching the entire
            // border of the primary monitor, then it's obvious which
            // side to call it. If it's in a corner, we pick a side
            // arbitrarily. If it doesn't touch any edges, or it spans
            // the width/height across the middle of the screen, then
            // we don't create a strut for it at all.
            let side;
            let primary = this._primaryMonitor;
            if (x1 <= primary.x && x2 >= primary.x + primary.width) {
                if (y1 <= primary.y)
                    side = Meta.Side.TOP;
                else if (y2 >= primary.y + primary.height)
                    side = Meta.Side.BOTTOM;
                else
                    continue;
            } else if (y1 <= primary.y && y2 >= primary.y + primary.height) {
                if (x1 <= 0)
                    side = Meta.Side.LEFT;
                else if (x2 >= global.screen_width)
                    side = Meta.Side.RIGHT;
                else
                    continue;
            } else if (x1 <= 0)
                side = Meta.Side.LEFT;
            else if (y1 <= 0)
                side = Meta.Side.TOP;
            else if (x2 >= global.screen_width)
                side = Meta.Side.RIGHT;
            else if (y2 >= global.screen_height)
                side = Meta.Side.BOTTOM;
            else
                continue;

            // Ensure that the strut rects goes all the way to the screen edge,
            // as this really what mutter expects.
            switch (side) {
            case Meta.Side.TOP:
                y1 = 0;
                break;
            case Meta.Side.BOTTOM:
                y2 = global.screen_height;
                break;
            case Meta.Side.LEFT:
                x1 = 0;
                break;
            case Meta.Side.RIGHT:
                x2 = global.screen_width;
                break;
            }

            let strutRect = new Meta.Rectangle({ x: x1, y: y1, width: x2 - x1, height: y2 - y1});
            let strut = new Meta.Strut({ rect: strutRect, side: side });
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
Signals.addSignalMethods(Chrome.prototype);
