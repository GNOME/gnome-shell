// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Animation, AnimatedIcon, Spinner */

const { Clutter, GLib, Gio, St } = imports.gi;

const Params = imports.misc.params;

var ANIMATED_ICON_UPDATE_TIMEOUT = 16;
var SPINNER_ANIMATION_TIME = 300;
var SPINNER_ANIMATION_DELAY = 1000;

var Animation = class {
    constructor(file, width, height, speed) {
        this.actor = new St.Bin();
        this.actor.set_size(width, height);
        this.actor.connect('destroy', this._onDestroy.bind(this));
        this.actor.connect('notify::size', this._syncAnimationSize.bind(this));
        this.actor.connect('resource-scale-changed',
            this._loadFile.bind(this, file, width, height));

        let themeContext = St.ThemeContext.get_for_stage(global.stage);
        this._scaleChangedId = themeContext.connect('notify::scale-factor',
            this._loadFile.bind(this, file, width, height));

        this._speed = speed;

        this._isLoaded = false;
        this._isPlaying = false;
        this._timeoutId = 0;
        this._frame = 0;

        this._loadFile(file, width, height);
    }

    play() {
        if (this._isLoaded && this._timeoutId == 0) {
            if (this._frame == 0)
                this._showFrame(0);

            this._timeoutId = GLib.timeout_add(GLib.PRIORITY_LOW, this._speed, this._update.bind(this));
            GLib.Source.set_name_by_id(this._timeoutId, '[gnome-shell] this._update');
        }

        this._isPlaying = true;
    }

    stop() {
        if (this._timeoutId > 0) {
            GLib.source_remove(this._timeoutId);
            this._timeoutId = 0;
        }

        this._isPlaying = false;
    }

    _loadFile(file, width, height) {
        let [validResourceScale, resourceScale] = this.actor.get_resource_scale();
        let wasPlaying = this._isPlaying;

        if (this._isPlaying)
            this.stop();

        this._isLoaded = false;
        this.actor.destroy_all_children();

        if (!validResourceScale) {
            if (wasPlaying)
                this.play();
            return;
        }

        let textureCache = St.TextureCache.get_default();
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        this._animations = textureCache.load_sliced_image(file, width, height,
                                                          scaleFactor, resourceScale,
                                                          this._animationsLoaded.bind(this));
        this.actor.set_child(this._animations);

        if (wasPlaying)
            this.play();
    }

    _showFrame(frame) {
        let oldFrameActor = this._animations.get_child_at_index(this._frame);
        if (oldFrameActor)
            oldFrameActor.hide();

        this._frame = (frame % this._animations.get_n_children());

        let newFrameActor = this._animations.get_child_at_index(this._frame);
        if (newFrameActor)
            newFrameActor.show();
    }

    _update() {
        this._showFrame(this._frame + 1);
        return GLib.SOURCE_CONTINUE;
    }

    _syncAnimationSize() {
        if (!this._isLoaded)
            return;

        let [width, height] = this.actor.get_size();

        for (let i = 0; i < this._animations.get_n_children(); ++i)
            this._animations.get_child_at_index(i).set_size(width, height);
    }

    _animationsLoaded() {
        this._isLoaded = this._animations.get_n_children() > 0;

        this._syncAnimationSize();

        if (this._isPlaying)
            this.play();
    }

    _onDestroy() {
        this.stop();

        let themeContext = St.ThemeContext.get_for_stage(global.stage);
        if (this._scaleChangedId)
            themeContext.disconnect(this._scaleChangedId);
        this._scaleChangedId = 0;
    }
};

var AnimatedIcon = class extends Animation {
    constructor(file, size) {
        super(file, size, size, ANIMATED_ICON_UPDATE_TIMEOUT);
    }
};

var Spinner = class extends AnimatedIcon {
    constructor(size, params) {
        // Compatibility with older callers
        if (params === true || params === false)
            params = { animate: params };

        params = Params.parse(params, {
            animate: false,
            hideOnStop: false,
        });
        let file = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/process-working.svg');
        super(file, size);

        this.actor.opacity = 0;
        this._animate = params.animate;
        this._hideOnStop = params.hideOnStop;
        this.actor.visible = !this._hideOnStop;
    }

    _onDestroy() {
        this._animate = false;
        super._onDestroy();
    }

    play() {
        this.actor.remove_all_transitions();
        this.actor.show();

        if (this._animate) {
            super.play();
            this.actor.ease({
                opacity: 255,
                delay: SPINNER_ANIMATION_DELAY,
                duration: SPINNER_ANIMATION_TIME,
                mode: Clutter.AnimationMode.LINEAR
            });
        } else {
            this.actor.opacity = 255;
            super.play();
        }
    }

    stop() {
        this.actor.remove_all_transitions();

        if (this._animate) {
            this.actor.ease({
                opacity: 0,
                duration: SPINNER_ANIMATION_TIME,
                mode: Clutter.AnimationMode.LINEAR,
                onComplete: () => {
                    super.stop();
                    if (this._hideOnStop)
                        this.actor.hide();
                },
            });
        } else {
            this.actor.opacity = 0;
            super.stop();

            if (this._hideOnStop)
                this.actor.hide();
        }
    }
};
