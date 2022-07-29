// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const {Gio, GObject} = imports.gi;

const {QuickSlider, SystemIndicator} = imports.ui.quickSettings;

const {loadInterfaceXML} = imports.misc.fileUtils;

const BUS_NAME = 'org.gnome.SettingsDaemon.Power';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Power';

const BrightnessInterface = loadInterfaceXML('org.gnome.SettingsDaemon.Power.Screen');
const BrightnessProxy = Gio.DBusProxy.makeProxyWrapper(BrightnessInterface);

const BrightnessItem = GObject.registerClass(
class BrightnessItem extends QuickSlider {
    _init() {
        super._init({
            iconName: 'display-brightness-symbolic',
        });

        this._proxy = new BrightnessProxy(Gio.DBus.session, BUS_NAME, OBJECT_PATH,
            (proxy, error) => {
                if (error)
                    console.error(error.message);
                else
                    this._proxy.connect('g-properties-changed', () => this._sync());
                this._sync();
            });

        this._sliderChangedId = this.slider.connect('notify::value',
            this._sliderChanged.bind(this));
        this.slider.accessible_name = _('Brightness');
    }

    _sliderChanged() {
        const percent = this.slider.value * 100;
        this._proxy.Brightness = percent;
    }

    _changeSlider(value) {
        this.slider.block_signal_handler(this._sliderChangedId);
        this.slider.value = value;
        this.slider.unblock_signal_handler(this._sliderChangedId);
    }

    _sync() {
        const brightness = this._proxy.Brightness;
        const visible = Number.isInteger(brightness) && brightness >= 0;
        this.visible = visible;
        if (visible)
            this._changeSlider(this._proxy.Brightness / 100.0);
    }
});

var Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this.quickSettingsItems.push(new BrightnessItem());
    }
});
