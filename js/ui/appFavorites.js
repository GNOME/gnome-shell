// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const AppStream = imports.gi.AppStreamGlib;
const Shell = imports.gi.Shell;
const Lang = imports.lang;
const Signals = imports.signals;

const Main = imports.ui.main;

// In alphabetical order
const RENAMED_DESKTOP_IDS = {
    'empathy.desktop': 'org.gnome.Empathy.desktop',
    'epiphany.desktop': 'org.gnome.Epiphany.desktop',
    'gcalctool.desktop': 'org.gnome.Calculator.desktop',
    'geary.desktop': 'org.gnome.Geary.desktop',
    'gnibbles.desktop': 'org.gnome.Nibbles.desktop',
    'gnome-calculator.desktop': 'org.gnome.Calculator.desktop',
    'gnome-nibbles.desktop': 'org.gnome.Nibbles.desktop',
    'gnome-music.desktop': 'org.gnome.Music.desktop',
};

var AppFavorites = new Lang.Class({
    Name: 'AppFavorites',

    FAVORITE_APPS_KEY: 'favorite-apps',

    _init() {
       let loadFlags = AppStream.StoreLoadFlags.APP_INFO_SYSTEM |
                       AppStream.StoreLoadFlags.APP_INFO_USER |
                       AppStream.StoreLoadFlags.APPDATA |
                       AppStream.StoreLoadFlags.DESKTOP;
        this._appDataStore = new AppStream.Store();
        this._appDataStore.load(loadFlags, null);

        this._favorites = {};
        global.settings.connect('changed::' + this.FAVORITE_APPS_KEY, this._onFavsChanged.bind(this));
        this.reload();
    },

    _onFavsChanged() {
        this.reload();
        this.emit('changed');
    },

    reload() {
        let ids = global.settings.get_strv(this.FAVORITE_APPS_KEY);
        let appSys = Shell.AppSystem.get_default();

        // Map old desktop file names to the current ones
        let updated = false;
        ids = ids.map(id => {
            let appData = this._appDataStore.get_app_by_id_with_fallbacks(id) ||
                          this._appDataStore.get_app_by_provide(AppStream.ProvideKind.ID, id);
            let newId = appData != null ? appData.get_id() : undefined;
            if (newId !== undefined &&
                newId !== id &&
                appSys.lookup_app(newId) != null) {
                updated = true;
                return newId;
            }

            newId = RENAMED_DESKTOP_IDS[id];
            if (newId !== undefined &&
                appSys.lookup_app(newId) != null) {
                updated = true;
                return newId;
            }

            return id;
        });
        // ... and write back the updated desktop file names
        if (updated)
            global.settings.set_strv(this.FAVORITE_APPS_KEY, ids);

        let apps = ids.map(id => appSys.lookup_app(id))
                      .filter(app => app != null);
        this._favorites = {};
        for (let i = 0; i < apps.length; i++) {
            let app = apps[i];
            this._favorites[app.get_id()] = app;
        }
    },

    _getIds() {
        let ret = [];
        for (let id in this._favorites)
            ret.push(id);
        return ret;
    },

    getFavoriteMap() {
        return this._favorites;
    },

    getFavorites() {
        let ret = [];
        for (let id in this._favorites)
            ret.push(this._favorites[id]);
        return ret;
    },

    isFavorite(appId) {
        return appId in this._favorites;
    },

    _addFavorite(appId, pos) {
        if (appId in this._favorites)
            return false;

        let app = Shell.AppSystem.get_default().lookup_app(appId);

        if (!app)
            return false;

        let ids = this._getIds();
        if (pos == -1)
            ids.push(appId);
        else
            ids.splice(pos, 0, appId);
        global.settings.set_strv(this.FAVORITE_APPS_KEY, ids);
        this._favorites[appId] = app;
        return true;
    },

    addFavoriteAtPos(appId, pos) {
        if (!this._addFavorite(appId, pos))
            return;

        let app = Shell.AppSystem.get_default().lookup_app(appId);

        Main.overview.setMessage(_("%s has been added to your favorites.").format(app.get_name()),
                                 { forFeedback: true,
                                   undoCallback: () => {
                                       this._removeFavorite(appId);
                                   }
                                 });
    },

    addFavorite(appId) {
        this.addFavoriteAtPos(appId, -1);
    },

    moveFavoriteToPos(appId, pos) {
        this._removeFavorite(appId);
        this._addFavorite(appId, pos);
    },

    _removeFavorite(appId) {
        if (!appId in this._favorites)
            return false;

        let ids = this._getIds().filter(id => id != appId);
        global.settings.set_strv(this.FAVORITE_APPS_KEY, ids);
        return true;
    },

    removeFavorite(appId) {
        let ids = this._getIds();
        let pos = ids.indexOf(appId);

        let app = this._favorites[appId];
        if (!this._removeFavorite(appId))
            return;

        Main.overview.setMessage(_("%s has been removed from your favorites.").format(app.get_name()),
                                 { forFeedback: true,
                                   undoCallback: () => {
                                       this._addFavorite(appId, pos);
                                   }
                                 });
    }
});
Signals.addSignalMethods(AppFavorites.prototype);

var appFavoritesInstance = null;
function getAppFavorites() {
    if (appFavoritesInstance == null)
        appFavoritesInstance = new AppFavorites();
    return appFavoritesInstance;
}
