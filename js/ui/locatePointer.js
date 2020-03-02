// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported LocatePointer */

const { Gio } = imports.gi;
const Ripples = imports.ui.ripples;
const Main = imports.ui.main;

const LOCATE_POINTER_KEY = "locate-pointer";
const LOCATE_POINTER_SCHEMA = "org.gnome.desktop.interface";

var LocatePointer = class {
    constructor() {
        this._settings = new Gio.Settings({ schema_id: LOCATE_POINTER_SCHEMA });
        this._settings.connect(`changed::${LOCATE_POINTER_KEY}`, () => this._syncEnabled());
        this._syncEnabled();
    }

    _syncEnabled() {
        let enabled = this._settings.get_boolean(LOCATE_POINTER_KEY);
        if (enabled == !!this._ripples)
            return;

        if (enabled) {
            this._ripples = new Ripples.Ripples(0.5, 0.5, 'ripple-pointer-location');
            this._ripples.addTo(Main.uiGroup);
        } else {
            this._ripples.destroy();
            this._ripples = null;
        }
    }

    show() {
        if (!this._ripples)
            return;

        let [x, y] = global.get_pointer();
        this._ripples.playAnimation(x, y);
    }
};
