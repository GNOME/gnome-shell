// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Gio, GLib, St } = imports.gi;
const Ripples = imports.ui.ripples;
const Main = imports.ui.main;

const LOCATE_POINTER_KEY = "locate-pointer";
const LOCATE_POINTER_SCHEMA = "org.gnome.desktop.interface"

var locatePointer = class {
    constructor() {
        this._settings = new Gio.Settings({schema_id: LOCATE_POINTER_SCHEMA});
        this._ripples = new Ripples.Ripples(0.5, 0.5, 'ripple-pointer-location');
        this._ripples.addTo(Main.uiGroup);
    }

    show() {
        if (!this._settings.get_boolean("locate-pointer"))
            return;

        let [x, y, mods] = global.get_pointer();
        this._ripples.playAnimation(x, y);
    }
};
