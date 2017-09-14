// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Gio, Meta } = imports.gi;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const SENSOR_BUS_NAME = 'net.hadess.SensorProxy';
const SENSOR_OBJECT_PATH = '/net/hadess/SensorProxy';

const SensorProxyInterface = '<node> \
<interface name="net.hadess.SensorProxy"> \
  <property name="HasAccelerometer" type="b" access="read"/> \
</interface> \
</node>';

const SensorProxy = Gio.DBusProxy.makeProxyWrapper(SensorProxyInterface);

const ORIENTATION_SCHEMA = 'org.gnome.settings-daemon.peripherals.touchscreen';
const ORIENTATION_LOCK = 'orientation-lock';

var Indicator = class extends PanelMenu.SystemIndicator {
    constructor() {
        super();

        this._settings = new Gio.Settings({ schema_id: ORIENTATION_SCHEMA });
        this._monitorManager = Meta.MonitorManager.get();

        this._settings.connect(
            'changed::' + ORIENTATION_LOCK, this._updateOrientationLock.bind(this));
        Main.layoutManager.connect(
            'monitors-changed', this._updateOrientationLock.bind(this));
        Gio.DBus.system.watch_name(SENSOR_BUS_NAME,
                                   Gio.BusNameWatcherFlags.NONE,
                                   this._sensorProxyAppeared.bind(this),
                                   () => {
                                       this._sensorProxy = null;
                                       this._updateOrientationLock();
                                   });

        this._item = new PopupMenu.PopupSubMenuMenuItem('', true);
        this._item.icon.icon_name = 'find-location-symbolic';
        this._item.label.text = _("Orientation Lock");
        this.menu.addMenuItem(this._item);

        this._onOffAction = this._item.menu.addAction(
            _("Disable"), this._onOnOffAction.bind(this));

        this._updateOrientationLock();
    }

    _sensorProxyAppeared() {
        this._sensorProxy = new SensorProxy(
            Gio.DBus.system,
            SENSOR_BUS_NAME,
            SENSOR_OBJECT_PATH,
            (proxy, error)  => {
                if (error) {
                    log(error.message);
                    return;
                }
                this._sensorProxy.connect(
                    'g-properties-changed',
                    this._updateOrientationLock.bind(this));
                this._updateOrientationLock();
            });
    }

    _updateOrientationLock() {
        if (this._sensorProxy)
            this._item.actor.visible = (this._sensorProxy.HasAccelerometer &&
                                        this._monitorManager.get_is_builtin_display_on());
        else
            this._item.actor.visible = false;

        let locked = this._settings.get_boolean('orientation-lock');
        if (locked) {
            this._item.icon.icon_name = 'rotation-locked-symbolic';
            this._onOffAction.label.text = _("Disable");
        } else {
            this._item.icon.icon_name = 'rotation-allowed-symbolic';
            this._onOffAction.label.text = _("Enable");
        }
    }

    _onOnOffAction() {
        let locked = this._settings.get_boolean('orientation-lock');
        this._settings.set_boolean('orientation-lock', !locked);
        this._updateOrientationLock();
    }
};
