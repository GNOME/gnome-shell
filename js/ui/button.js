/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;

const DEFAULT_BUTTON_COLOR = new Clutter.Color();
DEFAULT_BUTTON_COLOR.from_pixel(0xeeddcc66);

const DEFAULT_PRESSED_BUTTON_COLOR = new Clutter.Color();
DEFAULT_PRESSED_BUTTON_COLOR.from_pixel(0xccbbaa66);

function Button(widget, buttonColor, pressedButtonColor, staysPressed, minWidth, minHeight) {
    this._init(widget, buttonColor, pressedButtonColor, staysPressed, minWidth, minHeight);
}

Button.prototype = {
    _init : function(widgetOrText, buttonColor, pressedButtonColor, staysPressed, minWidth, minHeight) {
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

        this.button = new Big.Box({ reactive: true,
                                    corner_radius: 5,
                                    padding_left: 4,
                                    padding_right: 4,
                                    orientation: Big.BoxOrientation.HORIZONTAL,
                                    y_align: Big.BoxAlignment.CENTER
                                  });
        if (typeof widgetOrText == 'string') {
            this._widget = new Clutter.Text({ font_name: "Sans Bold 16px",
                                              text: widgetOrText });
        } else {
            this._widget = widgetOrText;
        }

        this.button.append(this._widget, Big.BoxPackFlags.EXPAND);

        this._minWidth = minWidth;
        this._minHeight = minHeight;

        this.button.connect('button-press-event',
            function(o, event) {
                me._isBetweenPressAndRelease = true;
                me.button.backgroundColor = me._pressedButtonColor;
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
                    me.button.backgroundColor = me._buttonColor;
                }
                return false;
            });
        this.button.connect('leave-event',
            function(o, event) {
                me._isBetweenPressAndRelease = false;
                me._mouseIsOverButton = false;
                if (!me._active) {
                    me.button.backgroundColor = null;
                }
                return false;
            });
    },

    release : function() {
        if (!this._isBetweenPressAndRelease) {
            this._active = false;
            if (this._mouseIsOverButton) {
                this.button.backgroundColor = this._buttonColor;
            } else {
                this.button.backgroundColor = null;
            }
        }
    }
};
