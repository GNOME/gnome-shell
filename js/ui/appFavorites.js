/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Shell = imports.gi.Shell;
const Lang = imports.lang;
const Signals = imports.signals;


function AppFavorites() {
    this._init();
}

AppFavorites.prototype = {
    FAVORITE_APPS_KEY: 'favorite_apps',

    _init: function() {
        this._favorites = {};
        this._gconf = Shell.GConf.get_default();
        this._gconf.connect('changed::' + this.FAVORITE_APPS_KEY, Lang.bind(this, this._onFavsChanged));
        this._reload();
    },

    _onFavsChanged: function() {
        this._reload();
        this.emit('changed');
    },

    _reload: function() {
        let ids = Shell.GConf.get_default().get_string_list('favorite_apps');
        let appSys = Shell.AppSystem.get_default();
        let apps = ids.map(function (id) {
                return appSys.get_app(id);
            }).filter(function (app) {
                return app != null;
            });
        this._favorites = {};
        for (let i = 0; i < apps.length; i++) {
            let app = apps[i];
            this._favorites[app.get_id()] = app;
        }
    },

    _getIds: function() {
        let ret = [];
        for (let id in this._favorites)
            ret.push(id);
        return ret;
    },

    getFavoriteMap: function() {
        return this._favorites;
    },

    getFavorites: function() {
        let ret = [];
        for (let id in this._favorites)
            ret.push(this._favorites[id]);
        return ret;
    },

    isFavorite: function(appId) {
        return appId in this._favorites;
    },

    addFavorite: function(appId) {
        if (appId in this._favorites)
            return;
        let app = Shell.AppSystem.get_default().get_app(appId);
        if (!app)
            return;
        let ids = this._getIds();
        ids.push(appId);
        this._gconf.set_string_list(this.FAVORITE_APPS_KEY, ids);
        this._favorites[appId] = app;
    },

    removeFavorite: function(appId) {
        if (!appId in this._favorites)
            return;
        let ids = this._getIds().filter(function (id) { return id != appId; });
        this._gconf.set_string_list(this.FAVORITE_APPS_KEY, ids);
    }
};
Signals.addSignalMethods(AppFavorites.prototype);

var appFavoritesInstance = null;
function getAppFavorites() {
    if (appFavoritesInstance == null)
        appFavoritesInstance = new AppFavorites();
    return appFavoritesInstance;
}
