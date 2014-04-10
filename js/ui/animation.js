// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const St = imports.gi.St;
const Signals = imports.signals;
const Atk = imports.gi.Atk;

const ANIMATED_ICON_UPDATE_TIMEOUT = 100;

const Animation = new Lang.Class({
    Name: 'Animation',

    _init: function(filename, width, height, speed) {
        this.actor = new St.Bin();
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));
        this._speed = speed;

        this._isLoaded = false;
        this._isPlaying = false;
        this._timeoutId = 0;
        this._frame = 0;

        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        this._animations = St.TextureCache.get_default().load_sliced_image (filename, width, height, scaleFactor,
                                                                            Lang.bind(this, this._animationsLoaded));
        this.actor.set_child(this._animations);
    },

    play: function() {
        if (this._isLoaded && this._timeoutId == 0) {
            if (this._frame == 0)
                this._showFrame(0);

            this._timeoutId = Mainloop.timeout_add(this._speed, Lang.bind(this, this._update));
            GLib.Source.set_name_by_id(this._timeoutId, '[gnome-shell] this._update');
        }

        this._isPlaying = true;
    },

    stop: function() {
        if (this._timeoutId > 0) {
            Mainloop.source_remove(this._timeoutId);
            this._timeoutId = 0;
        }

        this._isPlaying = false;
    },

    _showFrame: function(frame) {
        let oldFrameActor = this._animations.get_child_at_index(this._frame);
        if (oldFrameActor)
            oldFrameActor.hide();

        this._frame = (frame % this._animations.get_n_children());

        let newFrameActor = this._animations.get_child_at_index(this._frame);
        if (newFrameActor)
            newFrameActor.show();
    },

    _update: function() {
        this._showFrame(this._frame + 1);
        return GLib.SOURCE_CONTINUE;
    },

    _animationsLoaded: function() {
        this._isLoaded = true;

        if (this._isPlaying)
            this.play();
    },

    _onDestroy: function() {
        this.stop();
    }
});

const AnimatedIcon = new Lang.Class({
    Name: 'AnimatedIcon',
    Extends: Animation,

    _init: function(filename, size) {
        this.parent(filename, size, size, ANIMATED_ICON_UPDATE_TIMEOUT);
    }
});
