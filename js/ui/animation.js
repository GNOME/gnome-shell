// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Animation, AnimatedIcon, Spinner */

const { Clutter, GLib, GObject, Gio, St } = imports.gi;

const Params = imports.misc.params;

var ANIMATED_ICON_UPDATE_TIMEOUT = 16;
var SPINNER_ANIMATION_TIME = 300;
var SPINNER_ANIMATION_DELAY = 1000;

var Animation = GObject.registerClass(
class Animation extends St.Bin {
    _init(file, width, height, speed) {
        const themeContext = St.ThemeContext.get_for_stage(global.stage);

        super._init({
            style: `width: ${width}px; height: ${height}px;`,
        });

        this.connect('destroy', this._onDestroy.bind(this));
        this.connect('resource-scale-changed',
            this._loadFile.bind(this, file, width, height));

        themeContext.connectObject('notify::scale-factor',
            () => {
                this._loadFile(file, width, height);
                this.set_size(width * themeContext.scale_factor, height * themeContext.scale_factor);
            }, this);

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
        const resourceScale = this.get_resource_scale();
        let wasPlaying = this._isPlaying;

        if (this._isPlaying)
            this.stop();

        this._isLoaded = false;
        this.destroy_all_children();

        let textureCache = St.TextureCache.get_default();
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        this._animations = textureCache.load_sliced_image(file, width, height,
                                                          scaleFactor, resourceScale,
                                                          this._animationsLoaded.bind(this));
        this._animations.set({
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });
        this.set_child(this._animations);

        if (wasPlaying)
            this.play();
    }

    _showFrame(frame) {
        let oldFrameActor = this._animations.get_child_at_index(this._frame);
        if (oldFrameActor)
            oldFrameActor.hide();

        this._frame = frame % this._animations.get_n_children();

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

        let [width, height] = this.get_size();

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
    }
});

var AnimatedIcon = GObject.registerClass(
class AnimatedIcon extends Animation {
    _init(file, size) {
        super._init(file, size, size, ANIMATED_ICON_UPDATE_TIMEOUT);
    }
});

var Spinner = GObject.registerClass(
class Spinner extends AnimatedIcon {
    _init(size, params) {
        params = Params.parse(params, {
            animate: false,
            hideOnStop: false,
        });
        let file = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/process-working.svg');
        super._init(file, size);

        this.opacity = 0;
        this._animate = params.animate;
        this._hideOnStop = params.hideOnStop;
        this.visible = !this._hideOnStop;
    }

    _onDestroy() {
        this._animate = false;
        super._onDestroy();
    }

    play() {
        this.remove_all_transitions();
        this.show();

        if (this._animate) {
            super.play();
            this.ease({
                opacity: 255,
                delay: SPINNER_ANIMATION_DELAY,
                duration: SPINNER_ANIMATION_TIME,
                mode: Clutter.AnimationMode.LINEAR,
            });
        } else {
            this.opacity = 255;
            super.play();
        }
    }

    stop() {
        this.remove_all_transitions();

        if (this._animate) {
            this.ease({
                opacity: 0,
                duration: SPINNER_ANIMATION_TIME,
                mode: Clutter.AnimationMode.LINEAR,
                onComplete: () => {
                    super.stop();
                    if (this._hideOnStop)
                        this.hide();
                },
            });
        } else {
            this.opacity = 0;
            super.stop();

            if (this._hideOnStop)
                this.hide();
        }
    }
});
