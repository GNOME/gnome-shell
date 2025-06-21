import Gio from 'gi://Gio';
import GObject from 'gi://GObject';

import * as Main from '../ui/main.js';

const SCALE_VALUE_CHANGE_EPSILON = 0.001;

const POWER_SCHEMA = 'org.gnome.settings-daemon.plugins.power';

export const BrightnessManager = GObject.registerClass({
    Signals: {
        'changed': {},
    },
}, class BrightnessManager extends GObject.Object {
    constructor() {
        super();

        this._globalScale = null;
        this._monitorScales = new Map();

        // This is still being used in the power plugin for the keyboard backlight
        // so we just use that setting here
        const powerSettings = new Gio.Settings({schema_id: POWER_SCHEMA});
        this._dimmingTarget = powerSettings.get_int('idle-brightness') / 100;
        this._dimmingEnabled = false;

        this._abTarget = -1.0;

        const monitorManager = global.backend.get_monitor_manager();
        monitorManager.connectObject('monitors-changed',
            this._monitorsChanged.bind(this), this);
        this._monitorsChanged();
    }

    get dimming() {
        return this._dimmingEnabled;
    }

    set dimming(enable) {
        this._dimmingEnabled = enable;
        this._sync();
    }

    get autoBrightnessTarget() {
        return this._abTarget;
    }

    set autoBrightnessTarget(target) {
        this._abTarget = target;
        this._sync();
    }

    get globalScale() {
        return this._globalScale;
    }

    get scales() {
        return [...this._monitorScales.values()];
    }

    _monitorsChanged() {
        const monitors = global.backend
            .get_monitor_manager()
            .get_logical_monitors()
            .filter(lm => {
                return lm.get_monitors()
                    .some(m => m.get_backlight() && m.is_active());
            });

        if (monitors.length === 0) {
            this._globalScale = null;
        } else if (!this._globalScale) {
            this._globalScale = new BrightnessScale(_('Brightness'), 1.0);
            this._globalScale.connect('notify::value', () => {
                if (this._inhibitUpdates)
                    return;
                this._globalScaleChanged = true;
                this._sync();
            });
        }

        this._monitorScales.clear();

        for (const monitor of monitors) {
            const scale = new MonitorBrightnessScale(monitor, 1.0);
            scale._scaleChanged = true;

            scale.connectObject(
                'backlights-changed', () => this._sync(),
                'notify::value',  () => {
                    if (this._inhibitUpdates)
                        return;
                    scale._scaleChanged = true;
                    this._sync();
                }, this);

            this._monitorScales.set(monitor, scale);
        }

        if (this._globalScale)
            this._sync({showOSD: false});

        this.emit('changed');
    }

    _sync({showOSD = true} = {}) {
        if (this._inhibitUpdates)
            return;
        this._inhibitUpdates = true;

        // Handle changed backlights
        for (const scale of this._monitorScales.values()) {
            if (scale.syncWithBacklight()) {
                // disable dimming for all if we have a single system initiated
                // backlight change
                this._dimmingEnabled = false;
            }
        }

        // Find scales which have been changed (and reset _scaleChanged)
        const changedScales = [...this._monitorScales.values()].filter(s => {
            const c = s._scaleChanged;
            s._scaleChanged = false;
            return c;
        });

        if (changedScales.length > 0) {
            // update the factors of all the scales when a scale changes

            // normalize everything to the maximum of all scales
            const max = Math.max(...[...this._monitorScales.values()]
                .map(s => s.value));

            // if max is 0 we can't deduce any ratios, so don't try
            if (max > 0.01) {
                for (const scale of this._monitorScales.values())
                    scale.updateScaleFactor(max);
            }

            // the global scale always follows the maximum, because one monitor
            // scale is at the maximum and we want the global scale to be a
            // factor we apply on the ratio of the monitor scales.
            this._globalScale.value = max;

            if (showOSD)
                this._showOSD(changedScales);
        } else if (this._globalScaleChanged) {
            // if the global scale changed, update the monitor scales according
            // to their scaleFactor and the global scale.

            this._globalScaleChanged = false;

            for (const scale of this._monitorScales.values())
                scale.syncWithScale(this._globalScale);

            if (showOSD)
                this._showOSD(this._monitorScales.values());
        }

        // ensure we lock the global scale when auto brightness is in control
        this._globalScale.locked = this._abTarget >= 0.0;

        // Update the actual backlight according to the new monitor brightnesses
        // and other factors, such as dimming.
        for (const scale of this._monitorScales.values()) {
            const max = this._dimmingEnabled ? this._dimmingTarget : 1.0;
            const target = this._abTarget >= 0.0 ? this._abTarget : scale.value;
            const brightness = Math.min(max, target);

            scale.setBacklight(brightness);
        }

        this._inhibitUpdates = false;
    }

    _showOSD(monitorScales) {
        const osdMonitors = {};
        for (const scale of monitorScales) {
            const level = scale.value;
            osdMonitors[scale.monitor.get_number()] = {level};
        }

        Main.osdWindowManager.show(
            Gio.Icon.new_for_string('display-brightness-symbolic'),
            null,
            osdMonitors
        );
    }
});

export const BrightnessScale = GObject.registerClass({
    Properties: {
        'value': GObject.ParamSpec.float(
            'value', null, null,
            GObject.ParamFlags.READWRITE,
            0, 1.0, 1.0),
        'locked': GObject.ParamSpec.boolean(
            'locked', null, null,
            GObject.ParamFlags.READWRITE,
            false),
    },
}, class BrightnessScale extends GObject.Object {
    constructor(name, value) {
        super({
            value,
            locked: false,
        });

        this._name = name;
    }

    get name() {
        return this._name;
    }

    get value() {
        return this._value;
    }

    set value(value) {
        this._setValue(value);
    }

    get locked() {
        return this._locked;
    }

    set locked(locked) {
        if (this._locked === locked)
            return;
        this._locked = locked;
        this.notify('locked');
    }

    _setValue(value) {
        value = Math.clamp(value, 0.0, 1.0);
        if (Math.abs(value - this._value) < SCALE_VALUE_CHANGE_EPSILON)
            return;
        this._value = value;
        this.notify('value');
    }
});

const MonitorBrightnessScale = GObject.registerClass({
    Signals: {
        'backlights-changed': {},
    },
}, class MonitorBrightnessScale extends BrightnessScale {
    constructor(monitor, value) {
        const name = monitor.get_monitors()[0].get_display_name();

        super(name, value);

        this._monitor = monitor;
        this._currentBacklightBrightness = -1;
        this._scaleFactor = 1.0;

        for (const backlight of this._getBacklights()) {
            backlight.connectObject('notify::brightness', () => {
                this.emit('backlights-changed');
            }, this);
        }
    }

    get monitor() {
        return this._monitor;
    }

    _getBacklights() {
        return this._monitor.get_monitors()
            .filter(m => m.get_backlight() && m.is_active())
            .map(m => m.get_backlight());
    }

    _getRelativeBrightness(backlight) {
        const {brightness, brightnessMin: min, brightnessMax: max} = backlight;
        return (brightness - min) / (max - min);
    }

    _setRelativeBrightness(backlight, brightness) {
        const {brightnessMin: min, brightnessMax: max} = backlight;
        backlight.brightness = min + ((max - min) * brightness);
    }

    syncWithBacklight() {
        const [backlight] = this._getBacklights();

        if (backlight.brightness === this._currentBacklightBrightness)
            return false;
        this._currentBacklightBrightness = backlight.brightness;

        this.value = this._getRelativeBrightness(backlight);
        return true;
    }

    syncWithScale(globalScale) {
        this.value = globalScale.value * this._scaleFactor;
    }

    setBacklight(brightness) {
        const backlights = this._getBacklights();
        for (const backlight of backlights)
            this._setRelativeBrightness(backlight, brightness);

        this._currentBacklightBrightness = backlights[0].brightness;
    }

    updateScaleFactor(max) {
        this._scaleFactor = this.value / max;
    }
});
