// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Gio, GLib, St } = imports.gi;
const Main = imports.ui.main;
const Ripples = imports.ui.ripples;

// Steal the key from g-s-d for now.
// TODO: Move the key to "org.gnome.desktop.peripherals.mouse" and remove
// the X11 centric implementation from g-s-d.
const LOCATE_POINTER_KEY = "locate-pointer";
const MOUSE_SCHEMA = "org.gnome.settings-daemon.peripherals.mouse"

class locatePointer {
    constructor() {
        this._mouseSettings = new Gio.Settings({schema_id: MOUSE_SCHEMA});
    }

    show() {
        if (this._mouseSettings.get_boolean(LOCATE_POINTER_KEY)) {
            let [x, y, mods] = global.get_pointer();
            let ripples = new Ripples.Ripples(Main.layoutManager,
                                              x, y,
                                              Clutter.Gravity.CENTER,
                                              'ripple-pointer-location');
            ripples.playAnimation();
        }
    }
};
