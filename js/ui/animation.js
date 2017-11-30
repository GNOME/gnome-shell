// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const St = imports.gi.St;
const Signals = imports.signals;
const Atk = imports.gi.Atk;

var ANIMATED_ICON_UPDATE_TIMEOUT = 16;

var Animation = new Lang.Class({
    Name: 'Animation',

    _init: function(file, width, height, speed) {
        this.actor = new St.Bin();
        this.actor.set_size(width, height);
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));
        this.actor.connect('notify::size', () => { this._syncAnimationSize(); });
        this.actor.connect('resource-scale-changed', (actor) => {
            this._loadFile(file, width, height);
        });
        this._speed = speed;

        this._isLoaded = false;
        this._isPlaying = false;
        this._timeoutId = 0;
        this._frame = 0;

        this._loadFile(file, width, height);
    },

    play: function() {
        if (this._isLoaded && this._timeoutId == 0) {
            if (this._frame == 0)
                this._showFrame(0);

            this._timeoutId = GLib.timeout_add(GLib.PRIORITY_LOW, this._speed, Lang.bind(this, this._update));
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

    _loadFile: function(file, width, height) {
        this._isLoaded = false;
        this.actor.destroy_all_children();

        if (this.actor.resource_scale <= 0)
            return;

        let texture_cache = St.TextureCache.get_default();
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        this._animations = texture_cache.load_sliced_image(file, width, height, scaleFactor,
                                                           this.actor.resource_scale,
                                                           Lang.bind(this, this._animationsLoaded));
        this.actor.set_child(this._animations);
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

    _syncAnimationSize: function() {
        if (!this._isLoaded)
            return;

        let [width, height] = this.actor.get_size();

        for (let i = 0; i < this._animations.get_n_children(); ++i)
            this._animations.get_child_at_index(i).set_size(width, height);
    },

    _animationsLoaded: function() {
        this._isLoaded = this._animations.get_n_children() > 0;

        this._syncAnimationSize();

        if (this._isPlaying)
            this.play();
    },

    _onDestroy: function() {
        this.stop();
    }
});

var AnimatedIcon = new Lang.Class({
    Name: 'AnimatedIcon',
    Extends: Animation,

    _init: function(file, size) {
        this.parent(file, size, size, ANIMATED_ICON_UPDATE_TIMEOUT);
    }
});
