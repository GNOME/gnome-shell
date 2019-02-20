// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Gio, GLib, St } = imports.gi;
const Main = imports.ui.main;
const Ripples = imports.ui.ripples;

class locatePointer {
    show() {
        if (global.settings.get_boolean("locate-pointer")) {
            let [x, y, mods] = global.get_pointer();
            let ripples = new Ripples.Ripples(Main.layoutManager,
                                              x, y,
                                              Clutter.Gravity.CENTER,
                                              'ripple-pointer-location');
            ripples.playAnimation();
        }
    }
};
