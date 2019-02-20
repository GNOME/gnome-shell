// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Gio, GLib, St } = imports.gi;
const Ripples = imports.ui.ripples;
const Main = imports.ui.main;

class locatePointer {
    constructor() {
        this._ripples = new Ripples.Ripples(0.5, 0.5, 'ripple-pointer-location');
        this._ripples.addTo(Main.layoutManager.uiGroup);
    }

    show() {
        if (!global.settings.get_boolean("locate-pointer"))
            return;

        let [x, y, mods] = global.get_pointer();
        this._ripples.playAnimation(x, y);
    }
};
