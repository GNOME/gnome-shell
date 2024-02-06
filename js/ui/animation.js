// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Gio from 'gi://Gio';
import St from 'gi://St';

import * as Params from '../misc/params.js';

const ANIMATED_ICON_UPDATE_TIMEOUT = 16;
const SPINNER_ANIMATION_TIME = 300;
const SPINNER_ANIMATION_DELAY = 1000;
const SPINNER_ANIMATION_CELL_SIZE = 16;

export const Animation = GObject.registerClass(
class Animation extends St.Bin {
    _init(params) {
        const themeContext = St.ThemeContext.get_for_stage(global.stage);
        params = Params.parse(params, {
            file: null,
            cellWidth: null,
            cellHeight: null,
            frameWidth: null,
            frameHeight: null,
            speed: 0,
        });

        const {file, speed} = params;
        let { frameWidth, frameHeight, cellHeight, cellWidth } = params;

        if (!cellWidth)
            throw new Error('Animation has unspecified cell width');

        if (!frameHeight) {
            if (frameWidth)
                frameHeight = frameWidth;
            else
                frameHeight = cellHeight;
        }

        if (!frameWidth)
            frameWidth = cellWidth;

        if (!cellHeight)
            cellHeight = cellWidth;

        if (!frameHeight)
            frameHeight = cellWidth;

        super._init({
            style: `width: ${frameWidth}px; height: ${frameHeight}px;`,
        });

        this._file = file;
        this._cellWidth = cellWidth;
        this._cellHeight = cellHeight;
        this._frameWidth = frameWidth;
        this._frameHeight = frameHeight;

        this.connect('destroy', this._onDestroy.bind(this));
        this.connect('resource-scale-changed',
            () => this._loadFile());

        themeContext.connectObject('notify::scale-factor',
            () => {
                this._loadFile();
                this.set_size(
                    this._frameWidth * themeContext.scale_factor,
                    this._frameHeight * themeContext.scale_factor);
            }, this);

        this._speed = speed;

        this._isLoaded = false;
        this._isPlaying = false;
        this._timeoutId = 0;
        this._frame = 0;

        this._loadFile();
    }

    play() {
        if (this._isLoaded && this._timeoutId === 0) {
            if (this._frame === 0)
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

    _loadFile() {
        const resourceScale = this.get_resource_scale();
        let wasPlaying = this._isPlaying;

        if (this._isPlaying)
            this.stop();

        this._isLoaded = false;
        this.destroy_all_children();

        let textureCache = St.TextureCache.get_default();
        let scaleFactor = St.ThemeContext.get_for_stage(global.stage).scale_factor;
        this._animations = textureCache.load_sliced_image(this._file,
            this._cellWidth, this._cellHeight,
            this._frameWidth, this._frameHeight,
            scaleFactor, resourceScale,
            () => this._loadFinished());
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

    _loadFinished() {
        this._isLoaded = this._animations.get_n_children() > 0;

        if (this._isLoaded && this._isPlaying)
            this.play();
    }

    _onDestroy() {
        this.stop();
    }
});

export const AnimatedIcon = GObject.registerClass(
class AnimatedIcon extends Animation {
    _init(params) {
        const { file, cellSize, frameSize } = params;

        super._init({file,
            cellWidth: cellSize,
            frameWidth: frameSize,
            speed: ANIMATED_ICON_UPDATE_TIMEOUT,
        });
    }
});

export const Spinner = GObject.registerClass(
class Spinner extends AnimatedIcon {
    _init(size, params) {
        params = Params.parse(params, {
            animate: false,
            hideOnStop: false,
        });
        this._fileDark = Gio.File.new_for_uri(
            'resource:///org/gnome/shell/theme/process-working-dark.svg');
        this._fileLight = Gio.File.new_for_uri(
            'resource:///org/gnome/shell/theme/process-working-light.svg');
        super._init({
            file: this._fileDark,
            cellSize: SPINNER_ANIMATION_CELL_SIZE,
            frameSize: size
        });

        this.connect('style-changed', () => {
            const themeNode = this.get_theme_node();
            const textColor = themeNode.get_foreground_color();
            const [, luminance] = textColor.to_hls();
            const file = luminance > 0.5
                ? this._fileDark
                : this._fileLight;
            if (file !== this._file) {
                this._file = file;
                this._loadFile();
            }
        });

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
