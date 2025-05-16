import GObject from 'gi://GObject';

import {QuickSlider, SystemIndicator} from '../quickSettings.js';

const BrightnessItem = GObject.registerClass(
class BrightnessItem extends QuickSlider {
    _init() {
        super._init({
            iconName: 'display-brightness-symbolic',
        });

        this.slider.accessible_name = _('Brightness');

        const monitorManager = global.backend.get_monitor_manager();
        monitorManager.connectObject('monitors-changed',
            this._monitorsChanged.bind(this), this);
        this._monitorsChanged();

        this._sliderChangedId = this.slider.connect('notify::value',
            this._sliderChanged.bind(this));
    }

    _sliderChanged() {
        this._setBrightness(this.slider.value);
    }

    _changeSlider(value) {
        this.slider.block_signal_handler(this._sliderChangedId);
        this.slider.value = value;
        this.slider.unblock_signal_handler(this._sliderChangedId);
    }

    _setBrightness(brightness) {
        this._primaryBacklight?.block_signal_handler(this._brightnessChangedId);
        this._monitors.forEach(monitor => {
            const backlight = monitor.get_backlight();
            const {brightnessMin: min, brightnessMax: max} = backlight;
            backlight.brightness = min + ((max - min) * brightness);
        });
        this._primaryBacklight?.unblock_signal_handler(this._brightnessChangedId);

        this._changeSlider(brightness);
    }

    _primaryBacklightChanged() {
        const {brightness, brightnessMin: min, brightnessMax: max} =
            this._primaryBacklight;
        const target = (brightness - min) / (max - min);
        this._setBrightness(target);
    }

    _setPrimaryBacklight(backlight) {
        if (this._primaryBacklight) {
            this._primaryBacklight.disconnect(this._brightnessChangedId);
            this._brightnessChangedId = 0;
        }

        this._primaryBacklight = backlight;
        this.visible = !!backlight;

        if (this._primaryBacklight) {
            this._brightnessChangedId =
                this._primaryBacklight.connect('notify::brightness',
                    this._primaryBacklightChanged.bind(this));
            this._primaryBacklightChanged();
        }
    }

    _monitorsChanged() {
        this._monitors = global.backend.get_monitor_manager()
            .get_monitors()
            .filter(m => m.get_backlight() && m.is_active());
        const primary = this._monitors.find(m => m.is_primary()) ||
                        this._monitors[0];
        this._setPrimaryBacklight(primary?.get_backlight());
    }
});

export const Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this.quickSettingsItems.push(new BrightnessItem());
    }
});
