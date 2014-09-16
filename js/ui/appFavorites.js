// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Shell = imports.gi.Shell;
const Lang = imports.lang;
const Signals = imports.signals;

const Main = imports.ui.main;

const RENAMED_DESKTOP_IDS = {
    'baobab.desktop': 'org.gnome.baobab.desktop',
    'cheese.desktop': 'org.gnome.Cheese.desktop',
    'dconf-editor.desktop': 'ca.desrt.dconf-editor.desktop',
    'file-roller.desktop': 'org.gnome.FileRoller.desktop',
    'gcalctool.desktop': 'gnome-calculator.desktop',
    'gedit.desktop': 'org.gnome.gedit.desktop',
    'glchess.desktop': 'gnome-chess.desktop',
    'glines.desktop': 'five-or-more.desktop',
    'gnect.desktop': 'four-in-a-row.desktop',
    'gnibbles.desktop': 'gnome-nibbles.desktop',
    'gnobots2.desktop': 'gnome-robots.desktop',
    'gnome-boxes.desktop': 'org.gnome.Boxes.desktop',
    'gnome-clocks.desktop': 'org.gnome.clocks.desktop',
    'gnome-contacts.desktop': 'org.gnome.Contacts.desktop',
    'gnome-documents.desktop': 'org.gnome.Documents.desktop',
    'gnome-font-viewer.desktop': 'org.gnome.font-viewer.desktop',
    'gnome-photos.desktop': 'org.gnome.Photos.desktop',
    'gnome-screenshot.desktop': 'org.gnome.Screenshot.desktop',
    'gnome-software.desktop': 'org.gnome.Software.desktop',
    'gnome-weather.desktop': 'org.gnome.Weather.Application.desktop',
    'gnomine.desktop': 'gnome-mines.desktop',
    'gnotravex.desktop': 'gnome-tetravex.desktop',
    'gnotski.desktop': 'gnome-klotski.desktop',
    'gtali.desktop': 'tali.desktop',
    'nautilus.desktop': 'org.gnome.Nautilus.desktop',
    'polari.desktop': 'org.gnome.Polari.desktop',
    'totem.desktop': 'org.gnome.Totem.desktop',
};

const AppFavorites = new Lang.Class({
    Name: 'AppFavorites',

    FAVORITE_APPS_KEY: 'favorite-apps',

    _init: function() {
        this._favorites = {};
        global.settings.connect('changed::' + this.FAVORITE_APPS_KEY, Lang.bind(this, this._onFavsChanged));
        this.reload();
    },

    _onFavsChanged: function() {
        this.reload();
        this.emit('changed');
    },

    reload: function() {
        let ids = global.settings.get_strv(this.FAVORITE_APPS_KEY);

        // Map old desktop file names to the current ones
        let updated = false;
        ids = ids.map(function (id) {
            let newId = RENAMED_DESKTOP_IDS[id];
            if (newId !== undefined) {
                updated = true;
                return newId;
            }
            return id;
        });
        // ... and write back the updated desktop file names
        if (updated)
            global.settings.set_strv(this.FAVORITE_APPS_KEY, ids);

        let appSys = Shell.AppSystem.get_default();
        let apps = ids.map(function (id) {
                return appSys.lookup_app(id);
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

    _addFavorite: function(appId, pos) {
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

    addFavoriteAtPos: function(appId, pos) {
        if (!this._addFavorite(appId, pos))
            return;

        let app = Shell.AppSystem.get_default().lookup_app(appId);

        Main.overview.setMessage(_("%s has been added to your favorites.").format(app.get_name()),
                                 { forFeedback: true,
                                   undoCallback: Lang.bind(this, function () {
                                                               this._removeFavorite(appId);
                                                           })
                                 });
    },

    addFavorite: function(appId) {
        this.addFavoriteAtPos(appId, -1);
    },

    moveFavoriteToPos: function(appId, pos) {
        this._removeFavorite(appId);
        this._addFavorite(appId, pos);
    },

    _removeFavorite: function(appId) {
        if (!appId in this._favorites)
            return false;

        let ids = this._getIds().filter(function (id) { return id != appId; });
        global.settings.set_strv(this.FAVORITE_APPS_KEY, ids);
        return true;
    },

    removeFavorite: function(appId) {
        let ids = this._getIds();
        let pos = ids.indexOf(appId);

        let app = this._favorites[appId];
        if (!this._removeFavorite(appId))
            return;

        Main.overview.setMessage(_("%s has been removed from your favorites.").format(app.get_name()),
                                 { forFeedback: true,
                                   undoCallback: Lang.bind(this, function () {
                                                               this._addFavorite(appId, pos);
                                                           })
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
