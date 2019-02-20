// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Gio, GLib, St } = imports.gi;
const Layout = imports.ui.layout;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

// Steal the key from g-s-d for now.
// TODO: Move the key to "org.gnome.desktop.peripherals.mouse" and remove
// the X11 centric implementation from g-s-d.
const LOCATE_POINTER_KEY = "locate-pointer";
const MOUSE_SCHEMA = "org.gnome.settings-daemon.peripherals.mouse"

// LocationRipples:
// Shamelessly copied from the layout "hotcorner" ripples implementation,
// see in ui/layout.js for the logic.
var LocationRipples = class LocationRipples {
    constructor() {
        this._pointerX = null;
        this._pointerY = null;

        this._ripple1 = new St.BoxLayout({ style_class: 'ripple-pointer-location',
                                           opacity: 0,
                                           visible: false });
        this._ripple2 = new St.BoxLayout({ style_class: 'ripple-pointer-location',
                                           opacity: 0,
                                           visible: false });
        this._ripple3 = new St.BoxLayout({ style_class: 'ripple-pointer-location',
                                           opacity: 0,
                                           visible: false });

        Main.layoutManager.uiGroup.add_actor(this._ripple1);
        Main.layoutManager.uiGroup.add_actor(this._ripple2);
        Main.layoutManager.uiGroup.add_actor(this._ripple3);
    }

    _animRipple(ripple, delay, time, startScale, startOpacity, finalScale) {
        ripple.x = this._pointerX;
        ripple.y = this._pointerY;
        ripple._opacity = startOpacity;
        ripple.set_anchor_point_from_gravity(Clutter.Gravity.CENTER);
        ripple.visible = true;
        ripple.opacity = 255 * Math.sqrt(startOpacity);
        ripple.scale_x = ripple.scale_y = startScale;

        Tweener.addTween(ripple, { _opacity: 0,
                                   scale_x: finalScale,
                                   scale_y: finalScale,
                                   delay: delay,
                                   time: time,
                                   transition: 'linear',
                                   onUpdate() { ripple.opacity = 255 * Math.sqrt(ripple._opacity); },
                                   onComplete() { ripple.visible = false; } });
    }

    playAnimation() {
        let [x, y, mods] = global.get_pointer();
        this._pointerX = x;
        this._pointerY = y;

        //                              delay  time  scale opacity => scale
        this._animRipple(this._ripple1, 0.0,   0.83,  0.25,  1.0,     1.5);
        this._animRipple(this._ripple2, 0.05,  1.0,   0.0,   0.7,     1.25);
        this._animRipple(this._ripple3, 0.35,  1.0,   0.0,   0.3,     1);
    }
};

class locatePointer {
    constructor() {
        this._mouseSettings = new Gio.Settings({schema_id: MOUSE_SCHEMA});
    }

    locatePointer() {
        if (this._mouseSettings.get_boolean(LOCATE_POINTER_KEY)) {
            let ripples = new LocationRipples();
            ripples.playAnimation();
        }
    }
};
