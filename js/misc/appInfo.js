/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Shell = imports.gi.Shell;

const Main = imports.ui.main;

// TODO - move this into GConf once we're not a plugin anymore
// but have taken over metacity
// This list is taken from GNOME Online popular applications
// http://online.gnome.org/applications
// but with nautilus removed (since it should already be running)
// and evince, totem, and gnome-file-roller removed (since they're
// usually started by opening documents, not by opening the app
// directly)
const DEFAULT_APPLICATIONS = [
    'mozilla-firefox.desktop',
    'gnome-terminal.desktop',
    'evolution.desktop',
    'gedit.desktop',
    'mozilla-thunderbird.desktop',
    'rhythmbox.desktop',
    'epiphany.desktop',
    'xchat.desktop',
    'openoffice.org-1.9-writer.desktop',
    'emacs.desktop',
    'gnome-system-monitor.desktop',
    'openoffice.org-1.9-calc.desktop',
    'eclipse.desktop',
    'openoffice.org-1.9-impress.desktop',
    'vncviewer.desktop'
];

function AppInfo(appId) {
    this._init(appId);
}

AppInfo.prototype = {
    _init : function(appId) {
        this.appId = appId;
        this._gAppInfo = Gio.DesktopAppInfo.new(appId);
        if (!this._gAppInfo)
            throw new Error('Unknown appId ' + appId);

        this.id = this._gAppInfo.get_id();
        this.name = this._gAppInfo.get_name();
        this.description = this._gAppInfo.get_description();
        this.executable = this._gAppInfo.get_executable();

        this._gicon = this._gAppInfo.get_icon();
    },

    getIcon : function(size) {
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

// getMostUsedApps:
// @count: maximum number of apps to retrieve
//
// Gets a list of #AppInfos for the @count most-frequently-used
// applications
//
// Return value: the list of #AppInfo
function getMostUsedApps(count) {
    let appMonitor = new Shell.AppMonitor();

    // Ask for more apps than we need, since the list of recently used
    // apps might contain an app we don't have a desktop file for
    let apps = appMonitor.get_most_used_apps (0, Math.round(count * 1.5));
    let matches = [], alreadyAdded = {};

    for (let i = 0; i < apps.length && matches.length <= count; i++) {
        let appId = apps[i] + ".desktop";
        let appInfo = getAppInfo(appId);
        if (appInfo) {
            matches.push(appInfo);
            alreadyAdded[appId] = true;
        }
    }

    // Fill the list with default applications it's not full yet
    for (let i = 0; i < DEFAULT_APPLICATIONS.length && matches.length <= count; i++) {
        let appId = DEFAULT_APPLICATIONS[i];
        if (alreadyAdded[appId])
            continue;

        let appInfo = getAppInfo(appId);
        if (appInfo)
            matches.push(appInfo);
    }

    return matches;
}
