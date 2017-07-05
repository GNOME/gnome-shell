// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported getAppFavorites */

const AppStream = imports.gi.AppStreamGlib;
const Gio = imports.gi.Gio;
const Shell = imports.gi.Shell;
const ParentalControlsManager = imports.misc.parentalControlsManager;
const Signals = imports.signals;

const Main = imports.ui.main;

Gio._promisify(AppStream.Store.prototype, 'load_async', 'load_finish');

class AppFavorites {
    constructor() {
        this._appDataStore = new AppStream.Store();
        this._appDataStore.connect('changed',
            () => this.reload());
        this._loadAppDataStore();

        // Filter the apps through the user’s parental controls.
        this._parentalControlsManager = ParentalControlsManager.getDefault();
        this._parentalControlsManager.connect('app-filter-changed', () => {
            this.reload();
            this.emit('changed');
        });

        this.FAVORITE_APPS_KEY = 'favorite-apps';
        this._favorites = {};
        global.settings.connect('changed::%s'.format(this.FAVORITE_APPS_KEY), this._onFavsChanged.bind(this));
        this.reload();
    }

    _onFavsChanged() {
        this.reload();
        this.emit('changed');
    }

    async _loadAppDataStore() {
        try {
            const loadFlags =
                AppStream.StoreLoadFlags.APP_INFO_SYSTEM |
                AppStream.StoreLoadFlags.APP_INFO_USER |
                AppStream.StoreLoadFlags.FLATPAK_SYSTEM |
                AppStream.StoreLoadFlags.FLATPAK_USER |
                AppStream.StoreLoadFlags.APPDATA |
                AppStream.StoreLoadFlags.DESKTOP;
            // emits ::changed when successful
            await this._appDataStore.load_async(loadFlags, null);
        } catch (e) {
            log('Failed to load AppData store: %s'.format(e.message));
        }
    }

    reload() {
        let ids = global.settings.get_strv(this.FAVORITE_APPS_KEY);
        let appSys = Shell.AppSystem.get_default();

        // Map old desktop file names to the current ones
        let updated = false;
        ids = ids.map(id => {
            const appData =
                this._appDataStore.get_app_by_id_with_fallbacks(id) ||
                this._appDataStore.get_app_by_provide(AppStream.ProvideKind.ID, id);

            const newId = appData?.get_id();
            if (newId !== undefined &&
                newId !== id &&
                appSys.lookup_app(newId) !== null) {
                updated = true;
                return newId;
            }
            return id;
        });
        // ... and write back the updated desktop file names
        if (updated)
            global.settings.set_strv(this.FAVORITE_APPS_KEY, ids);

        let apps = ids.map(id => appSys.lookup_app(id))
                      .filter(app => app !== null && this._parentalControlsManager.shouldShowApp(app.app_info));
        this._favorites = {};
        for (let i = 0; i < apps.length; i++) {
            let app = apps[i];
            this._favorites[app.get_id()] = app;
        }
    }

    _getIds() {
        let ret = [];
        for (let id in this._favorites)
            ret.push(id);
        return ret;
    }

    getFavoriteMap() {
        return this._favorites;
    }

    getFavorites() {
        let ret = [];
        for (let id in this._favorites)
            ret.push(this._favorites[id]);
        return ret;
    }

    isFavorite(appId) {
        return appId in this._favorites;
    }

    _addFavorite(appId, pos) {
        if (appId in this._favorites)
            return false;

        let app = Shell.AppSystem.get_default().lookup_app(appId);

        if (!app)
            return false;

        if (!this._parentalControlsManager.shouldShowApp(app.app_info))
            return false;

        let ids = this._getIds();
        if (pos == -1)
            ids.push(appId);
        else
            ids.splice(pos, 0, appId);
        global.settings.set_strv(this.FAVORITE_APPS_KEY, ids);
        return true;
    }

    addFavoriteAtPos(appId, pos) {
        if (!this._addFavorite(appId, pos))
            return;

        let app = Shell.AppSystem.get_default().lookup_app(appId);

        let msg = _("%s has been added to your favorites.").format(app.get_name());
        Main.overview.setMessage(msg, {
            forFeedback: true,
            undoCallback: () => this._removeFavorite(appId),
        });
    }

    addFavorite(appId) {
        this.addFavoriteAtPos(appId, -1);
    }

    moveFavoriteToPos(appId, pos) {
        this._removeFavorite(appId);
        this._addFavorite(appId, pos);
    }

    _removeFavorite(appId) {
        if (!(appId in this._favorites))
            return false;

        let ids = this._getIds().filter(id => id != appId);
        global.settings.set_strv(this.FAVORITE_APPS_KEY, ids);
        return true;
    }

    removeFavorite(appId) {
        let ids = this._getIds();
        let pos = ids.indexOf(appId);

        let app = this._favorites[appId];
        if (!this._removeFavorite(appId))
            return;

        let msg = _("%s has been removed from your favorites.").format(app.get_name());
        Main.overview.setMessage(msg, {
            forFeedback: true,
            undoCallback: () => this._addFavorite(appId, pos),
        });
    }
}
Signals.addSignalMethods(AppFavorites.prototype);

var appFavoritesInstance = null;
function getAppFavorites() {
    if (appFavoritesInstance == null)
        appFavoritesInstance = new AppFavorites();
    return appFavoritesInstance;
}
