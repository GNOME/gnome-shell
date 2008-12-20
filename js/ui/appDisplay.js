/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Signals = imports.signals;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Shell = imports.gi.Shell;

const GenericDisplay = imports.ui.genericDisplay;

// TODO - move this into GConf once we're not a plugin anymore
// but have taken over metacity
// This list is taken from GNOME Online popular applications
// http://online.gnome.org/applications
// but with nautilus removed
const DEFAULT_APPLICATIONS = [
    'mozilla-firefox.desktop',
    'gnome-terminal.desktop',
    'evolution.desktop',
    'evince.desktop',
    'gedit.desktop',
    'mozilla-thunderbird.desktop',
    'totem.desktop',
    'gnome-file-roller.desktop',
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

/* This class represents a single display item containing information about an application.
 *
 * appInfo - GAppInfo object containing information about the application
 * availableWidth - total width available for the item
 */
function AppDisplayItem(appInfo, availableWidth) {
    this._init(appInfo, availableWidth);
}

AppDisplayItem.prototype = {
    __proto__:  GenericDisplay.GenericDisplayItem.prototype,

    _init : function(appInfo, availableWidth) {
        GenericDisplay.GenericDisplayItem.prototype._init.call(this, availableWidth); 
        this._appInfo = appInfo;

        let name = appInfo.get_name();

        let description = appInfo.get_description();

        let iconTheme = Gtk.IconTheme.get_default();

        let icon = new Clutter.Texture({ width: 48, height: 48});
        let gicon = appInfo.get_icon();
        let path = null;
        if (gicon != null) {
            let iconinfo = iconTheme.lookup_by_gicon(gicon, 48, Gtk.IconLookupFlags.NO_SVG);
            if (iconinfo)
                path = iconinfo.get_filename();
        }

        if (path) {
            try {
                icon.set_from_file(path);
            } catch (e) {
                // we can get an error here if the file path doesn't exist on the system
                log('Error loading AppDisplayItem icon ' + e);
            }
        }
        this._setItemInfo(name, description, icon); 
    },

    //// Public methods ////

    // Returns the application info associated with this display item.
    getAppInfo : function () {
        return this._appInfo;
    },

    //// Public method overrides ////

    // Opens an application represented by this display item.
    launch : function() {
        this._appInfo.launch([], null);
    }

};

/* This class represents a display containing a collection of application items.
 * The applications are sorted based on their popularity by default, and based on
 * their name if some search filter is applied.
 *
 * width - width available for the display
 * height - height available for the display
 */
function AppDisplay(width, height) {
    this._init(width, height);
}

AppDisplay.prototype = {
    __proto__:  GenericDisplay.GenericDisplay.prototype,

    _init : function(width, height) {
        GenericDisplay.GenericDisplay.prototype._init.call(this, width, height);  
        let me = this;
        this._appMonitor = new Shell.AppMonitor();
        this._appsStale = true;
        this._appMonitor.connect('changed', function(mon) {
            me._appsStale = true;
            // We still need to determine what events other than search can trigger
            // a change in the set of applications that are being shown while the
            // user in in the overlay mode, however let's redisplay just in case.
            me._redisplay(); 
        });
    },

    //// Protected method overrides //// 

    // Gets information about all applications by calling Gio.app_info_get_all().
    _refreshCache : function() {
        let me = this;
        if (!this._appsStale)
            return;
        this._allItems = {};
        let apps = Gio.app_info_get_all();
        for (let i = 0; i < apps.length; i++) {
            let appInfo = apps[i];
            let appId = appInfo.get_id();
            this._allItems[appId] = appInfo;
        }
        this._appsStale = false;
    },

    // Sets the list of the displayed items based on the list of DEFAULT_APPLICATIONS.
    _setDefaultList : function() {
        this._removeAllDisplayItems();
        let added = 0;
        for (let i = 0; i < DEFAULT_APPLICATIONS.length && added < this._maxItems; i++) {
            let appId = DEFAULT_APPLICATIONS[i];
            let appInfo = this._allItems[appId];
            if (appInfo) {
                this._addDisplayItem(appId);
                added += 1;
            }
        }
    },

    // Sorts the list of item ids in-place based on the alphabetical order of the names of 
    // the items associated with the ids.
    _sortItems : function(itemIds) {
        let me = this;
        itemIds.sort(function (a,b) {
            let appA = me._allItems[a];
            let appB = me._allItems[b];
            return appA.get_name().localeCompare(appB.get_name());
        });
    },

    // Checks if the item info can be a match for the search string by checking
    // the name, description, and execution command for the application. 
    // Item info is expected to be GAppInfo.
    // Returns a boolean flag indicating if itemInfo is a match.
    _isInfoMatching : function(itemInfo, search) {
        if (search == null || search == '')
            return true;
        let name = itemInfo.get_name().toLowerCase();
        if (name.indexOf(search) >= 0)
            return true;
        let description = itemInfo.get_description();
        if (description) {
            description = description.toLowerCase();
            if (description.indexOf(search) >= 0)
                return true;
        }
        let exec = itemInfo.get_executable().toLowerCase();
        if (exec.indexOf(search) >= 0)
            return true;
        return false;
    },

    // Creates an AppDisplayItem based on itemInfo, which is expected be a GAppInfo object.
    _createDisplayItem: function(itemInfo) {
        return new AppDisplayItem(itemInfo, this._width);
    } 
};

Signals.addSignalMethods(AppDisplay.prototype);
