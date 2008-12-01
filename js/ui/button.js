/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Tweener = imports.tweener.tweener;

const DEFAULT_BUTTON_COLOR = new Clutter.Color();
DEFAULT_BUTTON_COLOR.from_pixel(0xeeddccff);

const DEFAULT_PRESSED_BUTTON_COLOR = new Clutter.Color();
DEFAULT_PRESSED_BUTTON_COLOR.from_pixel(0xccbbaaff);

// Time for animation making the button darker
const ANIMATION_TIME = 0.3;

const NO_OPACITY = 0;

const PARTIAL_OPACITY = 0.4 * 255;

const FULL_OPACITY = 255;

function Button(text, buttonColor, pressedButtonColor, staysPressed, minWidth, minHeight) {
    this._init(text, buttonColor, pressedButtonColor, staysPressed, minWidth, minHeight);
}

Button.prototype = {
    _init : function(text, buttonColor, pressedButtonColor, staysPressed, minWidth, minHeight) {
        let me = this;

        this._buttonColor = buttonColor
        if (buttonColor == null)
            this._buttonColor = DEFAULT_BUTTON_COLOR;

        this._pressedButtonColor = pressedButtonColor
        if (pressedButtonColor == null)
            this._pressedButtonColor = DEFAULT_PRESSED_BUTTON_COLOR;

        if (staysPressed == null)
            staysPressed = false;
        if (minWidth == null)
            minWidth = 0;
        if (minHeight == null)
            minHeight = 0;

        // if staysPressed is true, this.active will be true past the first release of a button, untill a subsequent one (the button
        // is unpressed) or untill release() is called explicitly
        this._active = false;
        this._isBetweenPressAndRelease = false;
        this._mouseIsOverButton = false;

        this.button = new Clutter.Group({reactive: true});
        this._background = new Clutter.Rectangle({ color: this._buttonColor});
        this._pressedBackground = new Clutter.Rectangle({ color: this._pressedButtonColor, opacity: NO_OPACITY});
        this._label = new Clutter.Label({ font_name: "Sans Bold 16px",
                                         text: text});
        this._label.set_position(5, 5);
        let backgroundWidth = Math.max(this._label.get_width()+10, minWidth);
        let backgroundHeight = Math.max(this._label.get_height()+10, minHeight);
        this._background.set_width(backgroundWidth)
        this._background.set_height(backgroundHeight)
        this._pressedBackground.set_width(backgroundWidth)
        this._pressedBackground.set_height(backgroundHeight)
        this.button.add_actor(this._background);
        this.button.add_actor(this._pressedBackground);
        this.button.add_actor(this._label);
        this.button.connect('button-press-event',
            function(o, event) {
                me._isBetweenPressAndRelease = true;
                Tweener.addTween(me._pressedBackground,
                                { time: ANIMATION_TIME,
                                  opacity: FULL_OPACITY,
                                  transition: "linear"
                                });
                return false;
            });
        this.button.connect('button-release-event',
            function(o, event) {
                me._isBetweenPressAndRelease = false;
                if (!staysPressed || me._active) {
                    me.release();
                } else {
                    me._active = true;
                }
                return false;
            });
        this.button.connect('enter-event',
            function(o, event) {
                me._mouseIsOverButton = true;
                if (!me._active) {
                    Tweener.removeTweens(me._pressedBackground);
                    me._pressedBackground.set_opacity(PARTIAL_OPACITY);
                }
                return false;
            });
        this.button.connect('leave-event',
            function(o, event) {
                me._isBetweenPressAndRelease = false;
                me._mouseIsOverButton = false;
                if (!me._active) {
                    Tweener.removeTweens(me._pressedBackground);
                    me._pressedBackground.set_opacity(NO_OPACITY);
                }
                return false;
            });
    },

    release : function() {
        if (!this._isBetweenPressAndRelease) {
            this._active = false;
            Tweener.removeTweens(this._pressedBackground);
            if (this._mouseIsOverButton) {
                this._pressedBackground.set_opacity(PARTIAL_OPACITY);
            } else {
                this._pressedBackground.set_opacity(NO_OPACITY);
            }
        }
    }
};
