// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Geoclue, Gio, GLib, GWeather, Shell } = imports.gi;
const Signals = imports.signals;

const PermissionStore = imports.misc.permissionStore;

const { loadInterfaceXML } = imports.misc.fileUtils;

const WeatherIntegrationIface = loadInterfaceXML('org.gnome.Shell.WeatherIntegration');

const WEATHER_BUS_NAME = 'org.gnome.Weather';
const WEATHER_OBJECT_PATH = '/org/gnome/Weather';
const WEATHER_INTEGRATION_IFACE = 'org.gnome.Shell.WeatherIntegration';

const WEATHER_APP_ID = 'org.gnome.Weather.desktop';

// Minimum time between updates to show loading indication
var UPDATE_THRESHOLD = 10 * GLib.TIME_SPAN_MINUTE;

var WeatherClient = class {
    constructor() {
        this._loading = false;
        this._locationValid = false;
        this._lastUpdate = GLib.DateTime.new_from_unix_local(0);

        this._autoLocationRequested = false;
        this._mostRecentLocation = null;

        this._gclueService = null;
        this._gclueStarted = false;
        this._gclueStarting = false;
        this._gclueLocationChangedId = 0;

        this._needsAuth = true;
        this._weatherAuthorized = false;
        this._permStore = new PermissionStore.PermissionStore((proxy, error) => {
            if (error) {
                log(`Failed to connect to permissionStore: ${error.message}`);
                return;
            }

            if (this._permStore.g_name_owner == null) {
                // Failed to auto-start, likely because xdg-desktop-portal
                // isn't installed; don't restrict access to location service
                this._weatherAuthorized = true;
                this._updateAutoLocation();
                return;
            }

            this._permStore.LookupRemote('gnome', 'geolocation', (res, err) => {
                if (err)
                    log(`Error looking up permission: ${err.message}`);

                let [perms, data] = err ? [{}, null] : res;
                let  params = ['gnome', 'geolocation', false, data, perms];
                this._onPermStoreChanged(this._permStore, '', params);
            });
        });
        this._permStore.connectSignal('Changed',
                                      this._onPermStoreChanged.bind(this));

        this._locationSettings = new Gio.Settings({ schema_id: 'org.gnome.system.location' });
        this._locationSettings.connect('changed::enabled',
                                       this._updateAutoLocation.bind(this));

        this._world = GWeather.Location.get_world();

        this._providers = GWeather.Provider.METAR |
                          GWeather.Provider.YR_NO |
                          GWeather.Provider.OWM;

        this._weatherInfo = new GWeather.Info({ enabled_providers: 0 });
        this._weatherInfo.connect_after('updated', () => {
            this._lastUpdate = GLib.DateTime.new_now_local();
            this.emit('changed');
        });

        this._weatherApp = null;
        this._weatherProxy = null;

        let nodeInfo = Gio.DBusNodeInfo.new_for_xml(WeatherIntegrationIface);
        Gio.DBusProxy.new(
            Gio.DBus.session,
            Gio.DBusProxyFlags.DO_NOT_AUTO_START | Gio.DBusProxyFlags.GET_INVALIDATED_PROPERTIES,
            nodeInfo.lookup_interface(WEATHER_INTEGRATION_IFACE),
            WEATHER_BUS_NAME,
            WEATHER_OBJECT_PATH,
            WEATHER_INTEGRATION_IFACE,
            null,
            this._onWeatherProxyReady.bind(this));

        this._settings = new Gio.Settings({
            schema_id: 'org.gnome.shell.weather',
        });
        this._settings.connect('changed::automatic-location',
            this._onAutomaticLocationChanged.bind(this));
        this._onAutomaticLocationChanged();
        this._settings.connect('changed::locations',
            this._onLocationsChanged.bind(this));
        this._onLocationsChanged();

        this._appSystem = Shell.AppSystem.get_default();
        this._appSystem.connect('installed-changed',
            this._onInstalledChanged.bind(this));
        this._onInstalledChanged();
    }

    get available() {
        return this._weatherApp != null;
    }

    get loading() {
        return this._loading;
    }

    get hasLocation() {
        return this._locationValid;
    }

    get info() {
        return this._weatherInfo;
    }

    activateApp() {
        if (this._weatherApp)
            this._weatherApp.activate();
    }

    update() {
        if (!this._locationValid)
            return;

        let now = GLib.DateTime.new_now_local();
        // Update without loading indication if the current info is recent enough
        if (this._weatherInfo.is_valid() &&
            now.difference(this._lastUpdate) < UPDATE_THRESHOLD)
            this._weatherInfo.update();
        else
            this._loadInfo();
    }

    get _useAutoLocation() {
        return this._autoLocationRequested &&
               this._locationSettings.get_boolean('enabled') &&
               (!this._needsAuth || this._weatherAuthorized);
    }

    _onWeatherProxyReady(o, res) {
        try {
            this._weatherProxy = Gio.DBusProxy.new_finish(res);
        } catch (e) {
            log(`Failed to create GNOME Weather proxy: ${e}`);
            return;
        }

        this._weatherProxy.connect('g-properties-changed',
            this._onWeatherPropertiesChanged.bind(this));
        this._onWeatherPropertiesChanged();
    }

    _onWeatherPropertiesChanged() {
        if (this._weatherProxy.g_name_owner == null)
            return;

        this._settings.set_boolean('automatic-location',
            this._weatherProxy.AutomaticLocation);
        this._settings.set_value('locations',
            new GLib.Variant('av', this._weatherProxy.Locations));
    }

    _onInstalledChanged() {
        let hadApp = this._weatherApp != null;
        this._weatherApp = this._appSystem.lookup_app(WEATHER_APP_ID);
        let haveApp = this._weatherApp != null;

        if (hadApp !== haveApp)
            this.emit('changed');

        let neededAuth = this._needsAuth;
        this._needsAuth = this._weatherApp === null ||
                          this._weatherApp.app_info.has_key('X-Flatpak');

        if (neededAuth !== this._needsAuth)
            this._updateAutoLocation();
    }

    _loadInfo() {
        let id = this._weatherInfo.connect('updated', () => {
            this._weatherInfo.disconnect(id);
            this._loading = false;
        });

        this._loading = true;
        this.emit('changed');

        this._weatherInfo.update();
    }

    _locationsEqual(loc1, loc2) {
        if (loc1 == loc2)
            return true;

        if (loc1 == null || loc2 == null)
            return false;

        return loc1.equal(loc2);
    }

    _setLocation(location) {
        if (this._locationsEqual(this._weatherInfo.location, location))
            return;

        this._weatherInfo.abort();
        this._weatherInfo.set_location(location);
        this._locationValid = location != null;

        this._weatherInfo.set_enabled_providers(location ? this._providers : 0);

        if (location)
            this._loadInfo();
        else
            this.emit('changed');
    }

    _updateLocationMonitoring() {
        if (this._useAutoLocation) {
            if (this._gclueLocationChangedId != 0 || this._gclueService == null)
                return;

            this._gclueLocationChangedId =
                this._gclueService.connect('notify::location',
                                           this._onGClueLocationChanged.bind(this));
            this._onGClueLocationChanged();
        } else {
            if (this._gclueLocationChangedId)
                this._gclueService.disconnect(this._gclueLocationChangedId);
            this._gclueLocationChangedId = 0;
        }
    }

    _startGClueService() {
        if (this._gclueStarting)
            return;

        this._gclueStarting = true;

        Geoclue.Simple.new('org.gnome.Shell', Geoclue.AccuracyLevel.CITY, null,
            (o, res) => {
                try {
                    this._gclueService = Geoclue.Simple.new_finish(res);
                } catch (e) {
                    log(`Failed to connect to Geoclue2 service: ${e.message}`);
                    this._setLocation(this._mostRecentLocation);
                    return;
                }
                this._gclueStarted = true;
                this._gclueService.get_client().distance_threshold = 100;
                this._updateLocationMonitoring();
            });
    }

    _onGClueLocationChanged() {
        let geoLocation = this._gclueService.location;
        let location = GWeather.Location.new_detached(geoLocation.description,
                                                      null,
                                                      geoLocation.latitude,
                                                      geoLocation.longitude);
        this._setLocation(location);
    }

    _onAutomaticLocationChanged() {
        let useAutoLocation = this._settings.get_boolean('automatic-location');
        if (this._autoLocationRequested == useAutoLocation)
            return;

        this._autoLocationRequested = useAutoLocation;

        this._updateAutoLocation();
    }

    _updateAutoLocation() {
        this._updateLocationMonitoring();

        if (this._useAutoLocation)
            this._startGClueService();
        else
            this._setLocation(this._mostRecentLocation);
    }

    _onLocationsChanged() {
        let locations = this._settings.get_value('locations').deep_unpack();
        let serialized = locations.shift();
        let mostRecentLocation = null;

        if (serialized)
            mostRecentLocation = this._world.deserialize(serialized);

        if (this._locationsEqual(this._mostRecentLocation, mostRecentLocation))
            return;

        this._mostRecentLocation = mostRecentLocation;

        if (!this._useAutoLocation || !this._gclueStarted)
            this._setLocation(this._mostRecentLocation);
    }

    _onPermStoreChanged(proxy, sender, params) {
        let [table, id, deleted_, data_, perms] = params;

        if (table != 'gnome' || id != 'geolocation')
            return;

        let permission = perms['org.gnome.Weather'] || ['NONE'];
        let [accuracy] = permission;
        this._weatherAuthorized = accuracy != 'NONE';

        this._updateAutoLocation();
    }
};
Signals.addSignalMethods(WeatherClient.prototype);
