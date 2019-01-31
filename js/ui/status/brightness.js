// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const { Gio, St } = imports.gi;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const Slider = imports.ui.slider;

const { loadInterfaceXML } = imports.misc.fileUtils;

const BUS_NAME = 'org.gnome.SettingsDaemon.Power';
const OBJECT_PATH = '/org/gnome/SettingsDaemon/Power';

const BrightnessInterface = loadInterfaceXML('org.gnome.SettingsDaemon.Power.Screen');
const BrightnessProxy = Gio.DBusProxy.makeProxyWrapper(BrightnessInterface);

var Indicator = class extends PanelMenu.SystemIndicator {
    constructor() {
        super('display-brightness-symbolic');
        this._proxy = new BrightnessProxy(Gio.DBus.session, BUS_NAME, OBJECT_PATH,
                                          (proxy, error) => {
                                              if (error) {
                                                  log(error.message);
                                                  return;
                                              }

                                              this._proxy.connect('g-properties-changed', this._sync.bind(this));
                                              this._sync();
                                          });

        this._item = new PopupMenu.PopupBaseMenuItem({ activate: false });
        this.menu.addMenuItem(this._item);

        this._slider = new Slider.Slider(0);
        this._slider.connect('value-changed', this._sliderChanged.bind(this));
        this._slider.actor.accessible_name = _("Brightness");

        let icon = new St.Icon({ icon_name: 'display-brightness-symbolic',
                                 style_class: 'popup-menu-icon' });
        this._item.add(icon);
        this._item.add(this._slider.actor, { expand: true });
        this._item.connect('button-press-event', (actor, event) => {
            return this._slider.startDragging(event);
        });
        this._item.connect('key-press-event', (actor, event) => {
            return this._slider.onKeyPressEvent(actor, event);
        });

    }

    _sliderChanged(slider, value) {
        let percent = value * 100;
        this._proxy.Brightness = percent;
    }

    _sync() {
        let visible = this._proxy.Brightness >= 0;
        this._item.visible = visible;
        if (visible)
            this._slider.setValue(this._proxy.Brightness / 100.0);
    }
};
