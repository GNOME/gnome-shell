import Gio from 'gi://Gio';
import * as Ripples from './ripples.js';
import * as Main from './main.js';

const LOCATE_POINTER_KEY = 'locate-pointer';
const LOCATE_POINTER_SCHEMA = 'org.gnome.desktop.interface';

export class LocatePointer {
    constructor() {
        this._settings = new Gio.Settings({schema_id: LOCATE_POINTER_SCHEMA});
        this._settings.connect(`changed::${LOCATE_POINTER_KEY}`, () => this._syncEnabled());
        this._syncEnabled();
    }

    _syncEnabled() {
        let enabled = this._settings.get_boolean(LOCATE_POINTER_KEY);
        if (enabled === !!this._ripples)
            return;

        if (enabled) {
            this._ripples = new Ripples.Ripples(0.5, 0.5, 'ripple-pointer-location', true);
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
}
