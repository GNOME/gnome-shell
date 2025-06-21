import GObject from 'gi://GObject';

import {QuickSlider, SystemIndicator} from '../quickSettings.js';
import * as Main from '../main.js';

const BRIGHTNESS_NAME = _('Brightness');

const BrightnessItem = GObject.registerClass(
class BrightnessItem extends QuickSlider {
    _init() {
        super._init({
            iconName: 'display-brightness-symbolic',
        });

        this.slider.accessible_name = BRIGHTNESS_NAME;

        this._manager = Main.brightnessManager;
        this._manager.connectObject('changed',
            this._sync.bind(this), this);
        this._sync();
    }

    _sync() {
        const {globalScale} = this._manager;
        this.set({
            visible: !!globalScale,
        });

        if (!this.visible)
            return;

        this._connectSlider(this.slider, globalScale);
    }

    _connectSlider(slider, scale) {
        slider.disconnectObject(scale);
        slider.connectObject('notify::value', () => {
            if (slider._blockBrightnessAdjust)
                return;
            scale.value = slider.value;
        }, scale);

        const changeBrightness = () => {
            slider._blockBrightnessAdjust = true;
            slider.value = scale.value;
            slider._blockBrightnessAdjust = false;
        };
        scale.connectObject('notify::value', changeBrightness, slider);
        changeBrightness();
    }
});

export const Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this.quickSettingsItems.push(new BrightnessItem());
    }
});
