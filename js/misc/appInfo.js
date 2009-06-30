/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Shell = imports.gi.Shell;

const Main = imports.ui.main;

function AppInfo(appId) {
    this._init(appId);
}

AppInfo.prototype = {
    _init : function(appId) {
        this.appId = appId;
        this._gAppInfo = Gio.DesktopAppInfo.new(appId);
        if (!this._gAppInfo) {
            throw new Error('Unknown appId ' + appId);
        }

        this.id = this._gAppInfo.get_id();
        this.name = this._gAppInfo.get_name();
        this.description = this._gAppInfo.get_description();
        this.executable = this._gAppInfo.get_executable();

        this._gicon = this._gAppInfo.get_icon();
    },

    createIcon : function(size) {
        if (this._gicon)
            return Shell.TextureCache.get_default().load_gicon(this._gicon, size);
        else
            return new Clutter.Texture({ width: size, height: size });
    },

    getIconPath : function(size) {
        if (this._gicon) {
            let iconTheme = Gtk.IconTheme.get_default();
            let previewIconInfo = iconTheme.lookup_by_gicon(this._gicon, size, 0);
            if (previewIconInfo)
                return previewIconInfo.get_filename();
        }
        return null;
    },

    launch : function() {
        this._gAppInfo.launch([], Main.createAppLaunchContext());
    }
};

var _infos = {};

// getAppInfo:
// @appId: an appId
//
// Gets an #AppInfo for @appId. This is preferable to calling
// new AppInfo() directly, because it caches #AppInfos.
//
// Return value: the new or cached #AppInfo, or %null if @appId
// doesn't point to a valid .desktop file
function getAppInfo(appId) {
    let info = _infos[appId];
    if (info === undefined) {
        try {
            info = _infos[appId] = new AppInfo(appId);
        } catch (e) {
            info = _infos[appId] = null;
        }
    }
    return info;
}

// getTopApps:
// @count: maximum number of apps to retrieve
//
// Gets a list of #AppInfos for the @count most-frequently-used
// applications, with explicitly-chosen favorites first.
//
// Return value: the list of #AppInfo
function getTopApps(count) {
    let appMonitor = Shell.AppMonitor.get_default();

    let matches = [], alreadyAdded = {};

    let favs = getFavorites();
    for (let i = 0; i < favs.length && favs.length <= count; i++) {
        let appId = favs[i].appId;

        if (alreadyAdded[appId])
            continue;
        alreadyAdded[appId] = true;

        matches.push(favs[i]);
    }

    // Ask for more apps than we need, since the list of recently used
    // apps might contain an app we don't have a desktop file for
    let apps = appMonitor.get_most_used_apps (0, Math.round(count * 1.5));
    for (let i = 0; i < apps.length && matches.length <= count; i++) {
        let appId = apps[i] + ".desktop";
        if (alreadyAdded[appId])
            continue;
        alreadyAdded[appId] = true;
        let appInfo = getAppInfo(appId);
        if (appInfo) {
            matches.push(appInfo);
        }
    }

    return matches;
}

function _idListToInfos(ids) {
    let infos = [];
    for (let i = 0; i < ids.length; i++) {
        let display = getAppInfo(ids[i]);
        if (display == null)
            continue;
        infos.push(display);
    }
    return infos;
}

function getFavorites() {
    let system = Shell.AppSystem.get_default();

    return _idListToInfos(system.get_favorites());
}

function getRunning() {
    let monitor = Shell.AppMonitor.get_default();
    return _idListToInfos(monitor.get_running_app_ids());
}
