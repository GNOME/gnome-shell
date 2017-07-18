// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Geoclue = imports.gi.Geoclue;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GWeather = imports.gi.GWeather;
const Lang = imports.lang;
const Signals = imports.signals;

const PermissionStore = imports.misc.permissionStore;
const Util = imports.misc.util;

// Minimum time between updates to show loading indication
const UPDATE_THRESHOLD = 10 * GLib.TIME_SPAN_MINUTE;

var WeatherClient = new Lang.Class({
    Name: 'WeatherClient',

    _init: function() {
        this._loading = false;
        this._locationValid = false;
        this._lastUpdate = GLib.DateTime.new_from_unix_local(0);

        this._autoLocationRequested = false;
        this._mostRecentLocation = null;

        this._gclueService = null;
        this._gclueStarted = false;
        this._gclueStarting = false;
        this._gclueLocationChangedId = 0;

        this._weatherAuthorized = false;
        this._permStore = new PermissionStore.PermissionStore((proxy, error) => {
            if (error) {
                log('Failed to connect to permissionStore: ' + error.message);
                return;
            }

            this._permStore.LookupRemote('gnome', 'geolocation', (res, error) => {
                if (error)
                    log('Error looking up permission: ' + error.message);

                let [perms, data] = error ? [{}, null] : res;
                let  params = ['gnome', 'geolocation', false, data, perms];
                this._onPermStoreChanged(this._permStore, '', params);
            });
        });
        this._permStore.connectSignal('Changed',
                                      Lang.bind(this, this._onPermStoreChanged));

        this._locationSettings = new Gio.Settings({ schema_id: 'org.gnome.system.location' });
        this._locationSettings.connect('changed::enabled',
                                       Lang.bind(this, this._updateAutoLocation));

        this._world = GWeather.Location.get_world();

        this._providers = GWeather.Provider.METAR |
                          GWeather.Provider.YR_NO |
                          GWeather.Provider.OWM;

        this._weatherInfo = new GWeather.Info({ enabled_providers: 0 });
        this._weatherInfo.connect_after('updated', () => {
            this._lastUpdate = GLib.DateTime.new_now_local();
            this.emit('changed');
        });

        this._weatherAppMon = new Util.AppSettingsMonitor('org.gnome.Weather.Application.desktop',
                                                          'org.gnome.Weather.Application');
        this._weatherAppMon.connect('available-changed', () => { this.emit('changed'); });
        this._weatherAppMon.watchSetting('automatic-location',
                                         Lang.bind(this, this._onAutomaticLocationChanged));
        this._weatherAppMon.watchSetting('locations',
                                         Lang.bind(this, this._onLocationsChanged));
    },

    get available() {
        return this._weatherAppMon.available;
    },

    get loading() {
        return this._loading;
    },

    get hasLocation() {
        return this._locationValid;
    },

    get info() {
        return this._weatherInfo;
    },

    activateApp: function() {
        this._weatherAppMon.activateApp();
    },

    update: function() {
        if (!this._locationValid)
            return;

        let now = GLib.DateTime.new_now_local();
        // Update without loading indication if the current info is recent enough
        if (this._weatherInfo.is_valid() &&
            now.difference(this._lastUpdate) < UPDATE_THRESHOLD)
            this._weatherInfo.update();
        else
            this._loadInfo();
    },

    get _useAutoLocation() {
        return this._autoLocationRequested &&
               this._locationSettings.get_boolean('enabled') &&
               this._weatherAuthorized;
    },

    _loadInfo: function() {
        let id = this._weatherInfo.connect('updated', () => {
            this._weatherInfo.disconnect(id);
            this._loading = false;
        });

        this._loading = true;
        this.emit('changed');

        this._weatherInfo.update();
    },

    _locationsEqual: function(loc1, loc2) {
        if (loc1 == loc2)
            return true;

        if (loc1 == null || loc2 == null)
            return false;

        return loc1.equal(loc2);
    },

    _setLocation: function(location) {
        if (this._locationsEqual(this._weatherInfo.location, location))
            return;

        this._weatherInfo.abort();
        this._weatherInfo.set_location(location);
        this._locationValid = (location != null);

        this._weatherInfo.set_enabled_providers(location ? this._providers : 0);

        if (location)
            this._loadInfo();
        else
            this.emit('changed');
    },

    _updateLocationMonitoring: function() {
        if (this._useAutoLocation) {
            if (this._gclueLocationChangedId != 0 || this._gclueService == null)
                return;

            this._gclueLocationChangedId =
                this._gclueService.connect('notify::location',
                                           Lang.bind(this, this._onGClueLocationChanged));
            this._onGClueLocationChanged();
        } else {
            if (this._gclueLocationChangedId)
                this._gclueService.disconnect(this._gclueLocationChangedId);
            this._gclueLocationChangedId = 0;
        }
    },

    _startGClueService: function() {
        if (this._gclueStarting)
            return;

        this._gclueStarting = true;

        Geoclue.Simple.new('org.gnome.Shell', Geoclue.AccuracyLevel.CITY, null,
            (o, res) => {
                try {
                    this._gclueService = Geoclue.Simple.new_finish(res);
                } catch(e) {
                    log('Failed to connect to Geoclue2 service: ' + e.message);
                    this._setLocation(this._mostRecentLocation);
                    return;
                }
                this._gclueStarted = true;
                this._gclueService.get_client().distance_threshold = 100;
                this._updateLocationMonitoring();
            });
    },

    _onGClueLocationChanged: function() {
        let geoLocation = this._gclueService.location;
        let location = GWeather.Location.new_detached(geoLocation.description,
                                                      null,
                                                      geoLocation.latitude,
                                                      geoLocation.longitude);
        this._setLocation(location);
    },

    _onAutomaticLocationChanged: function(settings, key) {
        let useAutoLocation = settings.get_boolean(key);
        if (this._autoLocationRequested == useAutoLocation)
            return;

        this._autoLocationRequested = useAutoLocation;

        this._updateAutoLocation();
    },

    _updateAutoLocation: function() {
        this._updateLocationMonitoring();

        if (this._useAutoLocation)
            this._startGClueService();
        else
            this._setLocation(this._mostRecentLocation);
    },

    _onLocationsChanged: function(settings, key) {
        let serialized = settings.get_value(key).deep_unpack().shift();
        let mostRecentLocation = null;

        if (serialized)
            mostRecentLocation = this._world.deserialize(serialized);

        if (this._locationsEqual(this._mostRecentLocation, mostRecentLocation))
            return;

        this._mostRecentLocation = mostRecentLocation;

        if (!this._useAutoLocation || !this._gclueStarted)
            this._setLocation(this._mostRecentLocation);
    },

    _onPermStoreChanged: function(proxy, sender, params) {
        let [table, id, deleted, data, perms] = params;

        if (table != 'gnome' || id != 'geolocation')
            return;

        let permission = perms['org.gnome.Weather.Application'] || ['NONE'];
        let [accuracy] = permission;
        this._weatherAuthorized = accuracy != 'NONE';

        this._updateAutoLocation();
    }
});
Signals.addSignalMethods(WeatherClient.prototype);
