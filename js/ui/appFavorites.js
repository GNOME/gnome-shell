import * as MessageTray from './messageTray.js';
import Shell from 'gi://Shell';
import * as ParentalControlsManager from '../misc/parentalControlsManager.js';
import * as Signals from '../misc/signals.js';

class AppFavorites extends Signals.EventEmitter {
    constructor() {
        super();

        // Filter the apps through the user’s parental controls.
        this._parentalControlsManager = ParentalControlsManager.getDefault();
        this._parentalControlsManager.connect('app-filter-changed', () => {
            this.reload();
            this.emit('changed');
        });

        this.FAVORITE_APPS_KEY = 'favorite-apps';
        this._favorites = {};
        global.settings.connect(`changed::${this.FAVORITE_APPS_KEY}`, this._onFavsChanged.bind(this));
        this.reload();
    }

    _onFavsChanged() {
        this.reload();
        this.emit('changed');
    }

    reload() {
        let ids = global.settings.get_strv(this.FAVORITE_APPS_KEY);
        let appSys = Shell.AppSystem.get_default();
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
        if (pos === -1)
            ids.push(appId);
        else
            ids.splice(pos, 0, appId);
        global.settings.set_strv(this.FAVORITE_APPS_KEY, ids);
        return true;
    }

    addFavoriteAtPos(appId, pos) {
        if (!this._addFavorite(appId, pos))
            return;

        const app = Shell.AppSystem.get_default().lookup_app(appId);

        this._showNotification(_('%s has been pinned to the dash.').format(app.get_name()),
            null,
            () => this._removeFavorite(appId));
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

        let ids = this._getIds().filter(id => id !== appId);
        global.settings.set_strv(this.FAVORITE_APPS_KEY, ids);
        return true;
    }

    removeFavorite(appId) {
        let ids = this._getIds();
        let pos = ids.indexOf(appId);

        let app = this._favorites[appId];
        if (!this._removeFavorite(appId))
            return;

        this._showNotification(_('%s has been unpinned from the dash.').format(app.get_name()),
            null,
            () => this._addFavorite(appId, pos));
    }

    _showNotification(title, body, undoCallback) {
        const source = MessageTray.getSystemSource();
        const notification = new MessageTray.Notification({
            source,
            title,
            body,
            isTransient: true,
            forFeedback: true,
        });
        notification.addAction(_('Undo'), () => undoCallback());
        source.addNotification(notification);
    }
}

var appFavoritesInstance = null;

/**
 * @returns {AppFavorites}
 */
export function getAppFavorites() {
    if (appFavoritesInstance == null)
        appFavoritesInstance = new AppFavorites();
    return appFavoritesInstance;
}
