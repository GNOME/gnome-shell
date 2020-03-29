/* exported DwellClickIndicator */
const { Clutter, Gio, GLib, GObject, St } = imports.gi;

const PanelMenu = imports.ui.panelMenu;

const MOUSE_A11Y_SCHEMA       = 'org.gnome.desktop.a11y.mouse';
const KEY_DWELL_CLICK_ENABLED = 'dwell-click-enabled';
const KEY_DWELL_MODE          = 'dwell-mode';
const DWELL_MODE_WINDOW       = 'window';
const DWELL_CLICK_MODES = {
    primary: {
        name: _("Single Click"),
        icon: 'pointer-primary-click-symbolic',
        type: Clutter.PointerA11yDwellClickType.PRIMARY,
    },
    double: {
        name: _("Double Click"),
        icon: 'pointer-double-click-symbolic',
        type: Clutter.PointerA11yDwellClickType.DOUBLE,
    },
    drag: {
        name: _("Drag"),
        icon: 'pointer-drag-symbolic',
        type: Clutter.PointerA11yDwellClickType.DRAG,
    },
    secondary: {
        name: _("Secondary Click"),
        icon: 'pointer-secondary-click-symbolic',
        type: Clutter.PointerA11yDwellClickType.SECONDARY,
    },
};

var DwellClickIndicator = GObject.registerClass(
class DwellClickIndicator extends PanelMenu.Button {
    _init() {
        super._init(0.5, _("Dwell Click"));

        this._icon = new St.Icon({
            style_class: 'system-status-icon',
            icon_name: 'pointer-primary-click-symbolic',
        });
        this.add_child(this._icon);

        this._a11ySettings = new Gio.Settings({ schema_id: MOUSE_A11Y_SCHEMA });
        this._a11ySettings.connect(`changed::${KEY_DWELL_CLICK_ENABLED}`, this._syncMenuVisibility.bind(this));
        this._a11ySettings.connect(`changed::${KEY_DWELL_MODE}`, this._syncMenuVisibility.bind(this));

        this._seat = Clutter.get_default_backend().get_default_seat();
        this._seat.connect('ptr-a11y-dwell-click-type-changed', this._updateClickType.bind(this));

        this._addDwellAction(DWELL_CLICK_MODES.primary);
        this._addDwellAction(DWELL_CLICK_MODES.double);
        this._addDwellAction(DWELL_CLICK_MODES.drag);
        this._addDwellAction(DWELL_CLICK_MODES.secondary);

        this._setClickType(DWELL_CLICK_MODES.primary);
        this._syncMenuVisibility();
    }

    _syncMenuVisibility() {
        this.visible =
          this._a11ySettings.get_boolean(KEY_DWELL_CLICK_ENABLED) &&
           this._a11ySettings.get_string(KEY_DWELL_MODE) == DWELL_MODE_WINDOW;

        return GLib.SOURCE_REMOVE;
    }

    _addDwellAction(mode) {
        this.menu.addAction(mode.name, this._setClickType.bind(this, mode), mode.icon);
    }

    _updateClickType(manager, clickType) {
        for (let mode in DWELL_CLICK_MODES) {
            if (DWELL_CLICK_MODES[mode].type == clickType)
                this._icon.icon_name = DWELL_CLICK_MODES[mode].icon;
        }
    }

    _setClickType(mode) {
        this._seat.set_pointer_a11y_dwell_click_type(mode.type);
        this._icon.icon_name = mode.icon;
    }
});
