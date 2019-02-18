// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Cogl, GLib, GObject, Gio, St } = imports.gi;
const Mainloop = imports.mainloop;

const Tweener = imports.ui.tweener;

var ANIMATED_ICON_UPDATE_TIMEOUT = 16;
var SPINNER_ANIMATION_TIME = 0.3;
var SPINNER_ANIMATION_DELAY = 1.0;

var Animation = GObject.registerClass(
class Animation extends St.Bin {
    _init(file, width, height, speed) {
        super._init({});
        this.set_size(width, height);
        this.actor = this;
        this.connect('destroy', this._onDestroy.bind(this));
        this.connect('notify::size', this._syncAnimationSize.bind(this));
        this.connect('resource-scale-changed',
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
            Mainloop.source_remove(this._timeoutId);
            this._timeoutId = 0;
        }

        this._isPlaying = false;
    }

    _loadFile(file, width, height) {
        let [validResourceScale, resourceScale] = this.get_resource_scale();

        this._isLoaded = false;
        this.destroy_all_children();

        if (!validResourceScale)
            return;

        let texture_cache = St.TextureCache.get_default();
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        this._animations = texture_cache.load_sliced_image(file, width, height,
                                                           scaleFactor, resourceScale,
                                                           this._animationsLoaded.bind(this));

        this._colorEffect = new Clutter.ColorizeEffect();
        this._animations.add_effect(this._colorEffect);

        this.set_child(this._animations);

        this._shadowHelper = null;
        this._shadowWidth = this._shadowHeight = 0;
    }

    vfunc_get_paint_volume(volume) {
        if (!super.vfunc_get_paint_volume(volume))
            return false;

        if (!this._shadow)
            return true;

        let shadow_box = new Clutter.ActorBox();
        this._shadow.get_box(this.get_allocation_box(), shadow_box);

        volume.set_width(Math.max(shadow_box.x2 - shadow_box.x1, volume.get_width()));
        volume.set_height(Math.max(shadow_box.y2 - shadow_box.y1, volume.get_height()));

        return true;
    }

    vfunc_style_changed() {
        let node = this.get_theme_node();
        this._shadow = node.get_shadow('icon-shadow');
        if (this._shadow)
            this._shadowHelper = St.ShadowHelper.new(this._shadow);
        else
            this._shadowHelper = null;

        super.vfunc_style_changed();

        if (this._animations) {
            let color = node.get_color('color');

            this._colorEffect.set_tint(color);

            // Clutter.ColorizeEffect does not affect opacity, so set it separately
            this._animations.opacity = color.alpha;
        }
    }

    vfunc_paint() {
        if (this._shadowHelper) {
            this._shadowHelper.update(this._animations);

            let allocation = this._animations.get_allocation_box();
            let paintOpacity = this._animations.get_paint_opacity();
            let framebuffer = Cogl.get_draw_framebuffer();

            this._shadowHelper.paint(framebuffer, allocation, paintOpacity);
        }

        this._animations.paint();
    }

    _showFrame(frame) {
        let oldFrameActor = this._animations.get_child_at_index(this._frame);
        if (oldFrameActor)
            oldFrameActor.hide();

        this._frame = (frame % this._animations.get_n_children());

        let newFrameActor = this._animations.get_child_at_index(this._frame);
        if (newFrameActor)
            newFrameActor.show();

        this.vfunc_style_changed();
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

        this.vfunc_style_changed();
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
});

var AnimatedIcon = class extends Animation {
    constructor(file, size) {
        super(file, size, size, ANIMATED_ICON_UPDATE_TIMEOUT);
    }
};

var Spinner = class extends AnimatedIcon {
    constructor(size, animate=false) {
        let file = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/process-working.svg');
        super(file, size);

        this.opacity = 0;
        this._animate = animate;
        this.add_style_class_name('spinner');
    }

    _onDestroy() {
        this._animate = false;
        super._onDestroy();
    }

    play() {
        Tweener.removeTweens(this);

        if (this._animate) {
            super.play();
            Tweener.addTween(this, {
                opacity: 255,
                delay: SPINNER_ANIMATION_DELAY,
                time: SPINNER_ANIMATION_TIME,
                transition: 'linear'
            });
        } else {
            this.opacity = 255;
            super.play();
        }
    }

    stop() {
        Tweener.removeTweens(this);

        if (this._animate) {
            Tweener.addTween(this, {
                opacity: 0,
                time: SPINNER_ANIMATION_TIME,
                transition: 'linear',
                onComplete: () => {
                    this.stop(false);
                }
            });
        } else {
            this.opacity = 0;
            super.stop();
        }
    }
};
