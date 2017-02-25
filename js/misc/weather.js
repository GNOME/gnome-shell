// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Geoclue = imports.gi.Geoclue;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GWeather = imports.gi.GWeather;
const Lang = imports.lang;
const Signals = imports.signals;

const Util = imports.misc.util;

// Minimum time between updates to show loading indication
const UPDATE_THRESHOLD = 10 * GLib.TIME_SPAN_MINUTE;

const WeatherClient = new Lang.Class({
    Name: 'WeatherClient',

    _init: function() {
        this._loading = false;
        this._lastUpdate = GLib.DateTime.new_from_unix_local(0);

        this._useAutoLocation = false;
        this._mostRecentLocation = null;

        this._gclueService = null;
        this._gclueStarted = false;
        this._gclueFailed = false;
        this._gclueLocationChangedId = 0;

        this._world = GWeather.Location.get_world();

        let providers = GWeather.Provider.METAR |
                        GWeather.Provider.YR_NO |
                        GWeather.Provider.OWM;
        this._weatherInfo = new GWeather.Info({ enabled_providers: providers });
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

    get info() {
        return this._weatherInfo;
    },

    activateApp: function() {
        this._weatherAppMon.activateApp();
    },

    update: function() {
        let now = GLib.DateTime.new_now_local();
        // Update without loading indication if the current info is recent enough
        if (this._weatherInfo.is_valid() &&
            now.difference(this._lastUpdate) < UPDATE_THRESHOLD)
            this._weatherInfo.update();
        else
            this._loadInfo();
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
        if (this._gclueStarted)
            return;

        this._gclueStarted = true;
        Geoclue.Simple.new('org.gnome.Shell', Geoclue.AccuracyLevel.CITY, null,
            (o, res) => {
                try {
                    this._gclueService = Geoclue.Simple.new_finish(res);
                } catch(e) {
                    log('Failed to connect to Geoclue2 service: ' + e.message);
                    this._gclueFailed = true;
                    this._setLocation(this._mostRecentLocation);
                    return;
                }

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
        if (this._useAutoLocation == useAutoLocation)
            return;

        this._useAutoLocation = useAutoLocation;

        this._updateLocationMonitoring();

        if (this._useAutoLocation) {
            if (!this._gclueStarted)
                this._startGClueService();
        } else {
            this._setLocation(this._mostRecentLocation);
        }
    },

    _onLocationsChanged: function(settings, key) {
        let serialized = settings.get_value(key).deep_unpack().shift();
        let mostRecentLocation = null;

        if (serialized)
            mostRecentLocation = this._world.deserialize(serialized);

        if (this._locationsEqual(this._mostRecentLocation, mostRecentLocation))
            return;

        this._mostRecentLocation = mostRecentLocation;

        if (!this._useAutoLocation || this._gclueFailed)
            this._setLocation(this._mostRecentLocation);
    }
});
Signals.addSignalMethods(WeatherClient.prototype);
