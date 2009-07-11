/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;

const Shell = imports.gi.Shell;
const Tweener = imports.ui.tweener;

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

        this._staysPressed = staysPressed
        if (staysPressed == null)
            this._staysPressed = false;

        if (minWidth == null)
            minWidth = 0;
        if (minHeight == null)
            minHeight = 0;

        // if this._staysPressed is true, this._active will be true past the first release of a button, until a subsequent one (the button
        // is unpressed) or until release() is called explicitly
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
                if (!me._staysPressed || me._active) {
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

    pressIn : function() {
        if (!this._isBetweenPressAndRelease && this._staysPressed) {
            this._active = true;
            this.button.backgroundColor = this._pressedButtonColor;
        }
    },

    release : function() {
        if (!this._isBetweenPressAndRelease && this._staysPressed) {
            this._active = false;
            if (this._mouseIsOverButton) {
                this.button.backgroundColor = this._buttonColor;
            } else {
                this.button.backgroundColor = null;
            }
        }
    }
};

/* Delay before the icon should appear, in seconds after the pointer has entered the parent */
const ANIMATION_TIME = 0.25;

/* This is an icon button that fades in/out when mouse enters/leaves the parent.
 * A delay is used before the fading starts. You can force it to be shown if needed.
 *
 * parent -- used to show/hide the button depending on mouse entering/leaving it
 * size -- size in pixels of  both the button and the icon it contains
 * texture -- optional, must be used if the texture for the icon is already created (else, use setIconFromName)
 */
function iconButton(parent, size, texture) {
    this._init(parent, size, texture);
}

iconButton.prototype = {
    _init : function(parent, size, texture) {
        this._size = size;
        if (texture)
            this.actor = texture;
        else
            this.actor = new Clutter.Texture({ width: this._size, height: this._size });
        this.actor.set_reactive(true);
        this.actor.set_opacity(0);
        parent.connect("enter-event", Lang.bind(this, function(actor, event) {
            this._shouldHide = false;

            // Nothing to do if the cursor has come back from a child of the parent actor
            if (actor.get_children().indexOf(Shell.get_event_related(event)) != -1)
                return;

            this._fadeIn();
        }));
        parent.connect("leave-event", Lang.bind(this, function(actor, event) {
            // Nothing to do if the cursor has merely entered a child of the parent actor
            if (actor.get_children().indexOf(Shell.get_event_related(event)) != -1)
                return;

            // Remember that we should not be visible to hide the button if forceShow is unset
            if (this._forceShow) {
                this._shouldHide = true;
                return;
            }

            this._fadeOut();
        }));
    },

    /// Private methods ///

    setIconFromName : function(iconName) {
        let iconTheme = Gtk.IconTheme.get_default();
        let iconInfo = iconTheme.lookup_icon(iconName, this._size, 0);
        if (!iconInfo)
            return;

        let iconPath = iconInfo.get_filename();
        this.actor.set_from_file(iconPath);
    },

    // Useful if we want to show the button immediately,
    // e.g. in case the mouse is already in the parent when the button is created
    show : function() {
        this.actor.set_opacity(255);
    },

    // If show is true, prevents the button from fading out
    forceShow : function(show) {
        this._forceShow = show;
        // Hide the button if it should have been hidden under normal conditions
        if (!this._forceShow && this._shouldHide) {
           this._fadeOut();
        }
    },

    /// Private methods ///

    _fadeIn : function() {
        Tweener.removeTweens(this.actor);
        Tweener.addTween(this.actor, { opacity: 255,
                                       time: ANIMATION_TIME,
                                       transition :"easeInQuad" });
    },

    _fadeOut : function() {
        Tweener.removeTweens(this.actor);
        Tweener.addTween(this.actor, { opacity: 0,
                                       time: ANIMATION_TIME,
                                       transition :"easeOutQuad" });
    }
};

