import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import St from 'gi://St';

import * as Params from '../misc/params.js';

const SPINNER_ANIMATION_TIME = 300;
const SPINNER_ANIMATION_DELAY = 1000;

export const Spinner = GObject.registerClass(
class Spinner extends St.Widget {
    constructor(size, params) {
        params = Params.parse(params, {
            animate: false,
            hideOnStop: false,
        });
        super({
            opacity: 0,
        });

        this._size = size;
        this._animate = params.animate;
        this._hideOnStop = params.hideOnStop;
        this.visible = !this._hideOnStop;
    }

    vfunc_map() {
        const {scaleFactor} = St.ThemeContext.get_for_stage(global.stage);
        const logicalSize = this._size * scaleFactor;
        this.set_size(logicalSize, logicalSize);
        super.vfunc_map();
    }

    play() {
        this.remove_all_transitions();
        this.set_content(new St.SpinnerContent());
        this.show();

        if (this._animate) {
            this.ease({
                opacity: 255,
                delay: SPINNER_ANIMATION_DELAY,
                duration: SPINNER_ANIMATION_TIME,
                mode: Clutter.AnimationMode.LINEAR,
            });
        } else {
            this.opacity = 255;
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
                    this.set_content(null);
                    if (this._hideOnStop)
                        this.hide();
                },
            });
        } else {
            this.opacity = 0;
            this.set_content(null);

            if (this._hideOnStop)
                this.hide();
        }
    }
});
