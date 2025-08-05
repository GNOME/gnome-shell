import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import St from 'gi://St';

import {QuickSlider, SystemIndicator} from '../quickSettings.js';
import * as Main from '../main.js';
import * as PopupMenu from '../popupMenu.js';
import {Slider} from '../slider.js';

const BRIGHTNESS_NAME = _('Brightness');

class BrightnessSliderMenu extends PopupMenu.PopupMenuSection {
    addSlider(scale) {
        const text = new PopupMenu.PopupMenuItem(scale.name, {reactive: false});
        this.addMenuItem(text);

        const slider = new Slider(0);
        slider.accessible_name = scale.name;

        const sliderBin = new St.Bin({
            style_class: 'slider-bin',
            child: slider,
            reactive: true,
            can_focus: true,
            x_expand: true,
            y_align: Clutter.ActorAlign.CENTER,
        });

        const sliderMenuItem = new PopupMenu.PopupBaseMenuItem({reactive: false});
        sliderMenuItem.add_child(sliderBin);
        this.addMenuItem(sliderMenuItem);

        return slider;
    }
}

const BrightnessItem = GObject.registerClass(
class BrightnessItem extends QuickSlider {
    _init() {
        super._init({
            iconName: 'display-brightness-symbolic',
            menuButtonAccessibleName: _('Open brightness menu'),
        });

        this.slider.accessible_name = BRIGHTNESS_NAME;

        this.menu.setHeader('display-brightness-symbolic', BRIGHTNESS_NAME);
        this._monitorBrightnessSection = new BrightnessSliderMenu();
        this.menu.addMenuItem(this._monitorBrightnessSection);

        this._manager = Main.brightnessManager;
        this._manager.connectObject('changed',
            this._onManagerChanged.bind(this), this);
        this._onManagerChanged();
    }

    _onManagerChanged() {
        this._manager.globalScale?.disconnectObject(this);
        this._manager.globalScale?.connectObject('notify::locked',
            this._sync.bind(this), this);
        this._sync();
    }

    _sync() {
        const {globalScale} = this._manager;
        this.set({
            visible: globalScale && !globalScale.locked,
            menuEnabled: this._manager.scales.length > 1,
        });

        if (!this.visible)
            return;

        this._monitorBrightnessSection.removeAll();

        this._connectSlider(this.slider, globalScale);
        for (const scale of this._manager.scales) {
            const slider = this._monitorBrightnessSection.addSlider(scale);
            this._connectSlider(slider, scale);
        }
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
