// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;

function clampAbs(v, cap) {
    if (v > cap)
        v = cap;
    if (v < -cap)
        v = -cap;
    return v;
}

const ActorWobbler = new Lang.Class({
    Name: 'ActorWobbler',

    _init: function(actor) {
        this._actor = actor;
        this._effect = new Shell.WobblyEffect();
        this._actor.add_effect(this._effect);
        this._actor._wobbler = this;

        this._currentBend = 0;
        this._currentHeightOffset = 0;
        this._running = false;

        this._allocationChangedId = this._actor.connect('allocation-changed', Lang.bind(this, this._allocationChanged));

        this._timeline = new Clutter.Timeline({ duration: 100, repeat_count: -1 });
        this._timeline.connect('new-frame', Lang.bind(this, this._newFrame));
        this._timeline.start();
    },

    start: function() {
        this._running = true;
    },

    stop: function() {
        this._running = false;
    },

    _destroy: function() {
        this._timeline.run_dispose();
        this._timeline = null;

        this._actor.disconnect(this._allocationChangedId);
        this._actor.scale_y = 1.0;
        this._actor.remove_effect(this._effect);
        this._actor._wobbler = null;
    },

    _newFrame: function() {
        this._step();
    },

    _step: function() {
        const DAMPEN = 0.8;
        this._currentBend *= DAMPEN;
        if (Math.abs(this._currentBend) < 1)
            this._currentBend = 0;
        this._currentHeightOffset *= DAMPEN;
        if (Math.abs(this._currentHeightOffset) < 1)
            this._currentHeightOffset = 0;

        // Cap the bend to a 100px shift.
        const BEND_CAP = 50;
        this._currentBend = clampAbs(this._currentBend, BEND_CAP);
        this._effect.set_bend_x(this._currentBend);

        // Cap the height change to 25px in either direction.
        const HEIGHT_OFFSET_CAP = 25;
        this._currentHeightOffset = clampAbs(this._currentHeightOffset, HEIGHT_OFFSET_CAP);
        let [minHeight, natHeight] = this._actor.get_preferred_height(-1);
        let scale = (natHeight + this._currentHeightOffset) / natHeight;
        this._actor.scale_y = scale;

        if (!this._running && this._currentBend == 0 && this._currentHeightOffset == 0)
            this._destroy();
    },

    _allocationChanged: function(actor, box, flags) {
        if (!this._running)
            return;

        if (this._oldX) {
            let deltaX = box.x1 - this._oldX;
            // Every 2px the user moves the window, we bend it by 1px.
            this._currentBend -= deltaX / 2;
        }

        if (this._oldY) {
            let deltaY = box.y1 - this._oldY;
            // Every 2px the user moves the window, we scale it by 1px.
            this._currentHeightOffset -= deltaY / 2;
        }

        this._oldX = box.x1;
        this._oldY = box.y1;
    },
});

const WobblyWindowManager = new Lang.Class({
    Name: 'WobblyWindowManager',

    _init: function() {
        global.display.connect('grab-op-begin', Lang.bind(this, this._grabOpBegin));
        global.display.connect('grab-op-end', Lang.bind(this, this._grabOpEnd));
    },

    _grabOpBegin: function(display, screen, window, op) {
        if (op != Meta.GrabOp.MOVING)
            return;

        let actor = window.get_compositor_private();
        if (!actor._wobbler)
            new ActorWobbler(actor);
        actor._wobbler.start();
    },

    _grabOpEnd: function(display, screen, window, op) {
        if (!window)
            return;

        let actor = window.get_compositor_private();
        if (actor._wobbler)
            actor._wobbler.stop();
    },
});
