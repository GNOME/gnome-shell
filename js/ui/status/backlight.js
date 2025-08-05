/* exported Indicator */

import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import St from 'gi://St';

import {QuickMenuToggle, SystemIndicator} from '../quickSettings.js';

import * as PopupMenu from '../popupMenu.js';
import {Slider} from '../slider.js';

import {loadInterfaceXML} from '../../misc/fileUtils.js';

const BUS_NAME = 'org.gnome.SettingsDaemon.Power';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Power';

const BrightnessInterface = loadInterfaceXML('org.gnome.SettingsDaemon.Power.Keyboard');
const BrightnessProxy = Gio.DBusProxy.makeProxyWrapper(BrightnessInterface);

const SliderItem = GObject.registerClass({
    Properties: {
        'value': GObject.ParamSpec.int(
            'value', null, null,
            GObject.ParamFlags.READWRITE,
            0, 100, 0),
    },
}, class SliderItem extends PopupMenu.PopupBaseMenuItem {
    constructor() {
        super({
            activate: false,
            style_class: 'keyboard-brightness-item',
        });

        this._slider = new Slider(0);

        this._sliderChangedId = this._slider.connect('notify::value',
            () => this.notify('value'));
        this._slider.accessible_name = _('Keyboard Brightness');

        this.add_child(this._slider);
    }

    get value() {
        return this._slider.value * 100;
    }

    set value(value) {
        if (this.value === value)
            return;

        this._slider.block_signal_handler(this._sliderChangedId);
        this._slider.value = value / 100;
        this._slider.unblock_signal_handler(this._sliderChangedId);

        this.notify('value');
    }

    vfunc_key_press_event(event) {
        const key = event.get_key_symbol();
        if (key === Clutter.KEY_Left || key === Clutter.KEY_Right)
            return this._slider.vfunc_key_press_event(event);
        else
            return super.vfunc_key_press_event(event);
    }
});

const DiscreteItem = GObject.registerClass({
    Properties: {
        'value': GObject.ParamSpec.int(
            'value', null, null,
            GObject.ParamFlags.READWRITE,
            0, 100, 0),
        'n-levels': GObject.ParamSpec.int(
            'n-levels', null, null,
            GObject.ParamFlags.READWRITE,
            1, 3, 1),
    },
}, class DiscreteItem extends St.BoxLayout {
    constructor() {
        super({
            style_class: 'popup-menu-item',
            reactive: true,
        });

        this._levelButtons = new Map();
        this._addLevelButton('off', _('Off'), 'keyboard-brightness-off-symbolic');
        this._addLevelButton('low', _('Low'), 'keyboard-brightness-medium-symbolic');
        this._addLevelButton('high', _('High'), 'keyboard-brightness-high-symbolic');

        this.connect('notify::n-levels', () => this._syncLevels());
        this.connect('notify::value', () => this._syncChecked());
        this._syncLevels();
    }

    _valueToLevel(value) {
        const checkedIndex = Math.round(value * (this.nLevels - 1) / 100);
        if (checkedIndex === this.nLevels - 1)
            return 'high';

        return [...this._levelButtons.keys()].at(checkedIndex);
    }

    _levelToValue(level) {
        const keyIndex = [...this._levelButtons.keys()].indexOf(level);
        return 100 * Math.min(keyIndex, this.nLevels - 1) / (this.nLevels - 1);
    }

    _addLevelButton(key, labelText, iconName) {
        const box = new St.BoxLayout({
            style_class: 'keyboard-brightness-level',
            orientation: Clutter.Orientation.VERTICAL,
            x_expand: true,
        });

        const label = new St.Label({
            text: labelText,
            x_align: Clutter.ActorAlign.CENTER,
        });

        box.button = new St.Button({
            styleClass: 'icon-button',
            canFocus: true,
            iconName,
            labelActor: label,
        });
        box.add_child(box.button);

        box.button.connect('clicked', () => {
            this.value = this._levelToValue(key);
        });

        box.add_child(label);

        this.add_child(box);
        this._levelButtons.set(key, box);
    }

    vfunc_key_press_event(event) {
        return global.focus_manager.navigate_from_event(event);
    }

    _syncLevels() {
        this._levelButtons.get('off').visible = this.nLevels > 0;
        this._levelButtons.get('high').visible = this.nLevels > 1;
        this._levelButtons.get('low').visible = this.nLevels > 2;
        this._syncChecked();
    }

    _syncChecked() {
        const checkedKey = this._valueToLevel(this.value);
        this._levelButtons.forEach((b, k) => {
            b.button.checked = k === checkedKey;
        });
    }
});

const KeyboardBrightnessToggle = GObject.registerClass(
class KeyboardBrightnessToggle extends QuickMenuToggle {
    _init() {
        super._init({
            title: _('Keyboard'),
            iconName: 'display-brightness-symbolic',
            menuButtonAccessibleName: _('Open keyboard brightness menu'),
        });

        this._proxy = new BrightnessProxy(Gio.DBus.session, BUS_NAME, OBJECT_PATH,
            (proxy, error) => {
                if (error)
                    console.error(error.message);
                else
                    this._proxy.connect('g-properties-changed', () => this._sync());
                this._sync();
            });

        this.connect('clicked', () => {
            this._proxy.Brightness = this.checked ? 0 : 100;
        });

        this._sliderItem = new SliderItem();
        this.menu.box.add_child(this._sliderItem);
        const sliderAccessible = this._sliderItem._slider.get_accessible();
        sliderAccessible.set_parent(this.menu.box.get_accessible());
        this._sliderItem.set_accessible(sliderAccessible);


        this._discreteItem = new DiscreteItem();
        this.menu.box.add_child(this._discreteItem);

        this._sliderItem.bind_property('visible',
            this._discreteItem, 'visible',
            GObject.BindingFlags.INVERT_BOOLEAN |
            GObject.BindingFlags.SYNC_CREATE);

        this._sliderItem.bind_property('value',
            this._discreteItem, 'value',
            GObject.BindingFlags.SYNC_CREATE);

        this._sliderItemChangedId = this._sliderItem.connect('notify::value', () => {
            if (this._sliderItem.visible)
                this._proxy.Brightness = this._sliderItem.value;
        });

        this._discreteItemChangedId = this._discreteItem.connect('notify::value', () => {
            if (this._discreteItem.visible)
                this._proxy.Brightness = this._discreteItem.value;
        });
    }

    _sync() {
        const brightness = this._proxy.Brightness;
        const visible = Number.isInteger(brightness) && brightness >= 0;
        this.visible = visible;
        if (!visible)
            return;

        this.checked = brightness > 0;
        const useSlider = this._proxy.Steps >= 4;

        this._sliderItem.block_signal_handler(this._sliderItemChangedId);
        this._discreteItem.block_signal_handler(this._discreteItemChangedId);

        this._sliderItem.set({
            visible: useSlider,
            value: brightness,
        });

        if (!useSlider)
            this._discreteItem.nLevels = this._proxy.Steps;

        this._sliderItem.unblock_signal_handler(this._sliderItemChangedId);
        this._discreteItem.unblock_signal_handler(this._discreteItemChangedId);
    }
});

export const Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this.quickSettingsItems.push(new KeyboardBrightnessToggle());
    }
});
