/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const GenericDisplay = imports.ui.genericDisplay;

const ITEM_DISPLAY_ICON_MARGIN = 2;

/* This class represents a single display item containing information about a document.
 *
 * docInfo - GtkRecentInfo object containing information about the document
 * availableWidth - total width available for the item
 */
function DocDisplayItem(docInfo, availableWidth) {
    this._init(docInfo, availableWidth);
}

DocDisplayItem.prototype = {
    __proto__:  GenericDisplay.GenericDisplayItem.prototype,

    _init : function(docInfo, availableWidth) {
        GenericDisplay.GenericDisplayItem.prototype._init.call(this, availableWidth);     
        this._docInfo = docInfo;
    
        let name = docInfo.get_display_name();

        // we can possibly display tags in the space for description in the future
        let description = ""; 

        let icon = new Clutter.Texture();
        let pixbuf = Shell.get_thumbnail_for_recent_info(docInfo);
        if (pixbuf) {
            // We calculate the width and height of the texture so as to preserve the aspect ratio of the thumbnail.
            // Because the images generated based on thumbnails don't have an internal padding like system icons do,
            // we create a slightly smaller texture and then use extra margin when positioning it. 
            let scalingFactor = (GenericDisplay.ITEM_DISPLAY_ICON_SIZE - ITEM_DISPLAY_ICON_MARGIN * 2) / Math.max(pixbuf.get_width(), pixbuf.get_height());
            icon.set_width(Math.ceil(pixbuf.get_width() * scalingFactor));
            icon.set_height(Math.ceil(pixbuf.get_height() * scalingFactor));
            Shell.clutter_texture_set_from_pixbuf(icon, pixbuf);
            icon.x = GenericDisplay.ITEM_DISPLAY_PADDING + ITEM_DISPLAY_ICON_MARGIN;
            icon.y = GenericDisplay.ITEM_DISPLAY_PADDING + ITEM_DISPLAY_ICON_MARGIN;
        } else {
            Shell.clutter_texture_set_from_pixbuf(icon, docInfo.get_icon(GenericDisplay.ITEM_DISPLAY_ICON_SIZE));
            icon.x = GenericDisplay.ITEM_DISPLAY_PADDING;
            icon.y = GenericDisplay.ITEM_DISPLAY_PADDING;
        } 
  
        this._setItemInfo(name, description, icon); 
    },

    //// Public methods ////

    // Returns the document info associated with this display item.
    getDocInfo : function() {
        return this._docInfo;
    },
 
    //// Public method overrides ////

    // Opens a document represented by this display item.
    launch : function() {
        let appName = this._docInfo.last_application();
        let [success, appExec, count, time] = this._docInfo.get_application_info(appName);
        if (success) {
            log("Will open a document with the following command: " + appExec);
            // TODO: Change this once better support for creating GAppInfo is added to 
            // GtkRecentInfo, as right now this relies on the fact that the file uri is
            // already a part of appExec, so we don't supply any files to appInfo.launch().

            // The 'command line' passed to create_from_command_line is allowed to contain
            // '%<something>' macros that are exapnded to file name / icon name, etc,
            // so we need to escape % as %%
            appExec = appExec.replace(/%/g, "%%");

            let appInfo = Gio.app_info_create_from_commandline(appExec, null, 0, null);
            appInfo.launch([], null, null);
        } else {
            log("Failed to get application info for " + this._docInfo.get_uri());
        }
    }

};

/* This class represents a display containing a collection of document items.
 * The documents are sorted by how recently they were last visited.
 *
 * width - width available for the display
 * height - height available for the display
 */
function DocDisplay(width, height) {
    this._init(width, height);
} 

DocDisplay.prototype = {
    __proto__:  GenericDisplay.GenericDisplay.prototype,

    _init : function(width, height) {
        GenericDisplay.GenericDisplay.prototype._init.call(this, width, height);  
        let me = this;
        this._recentManager = Gtk.RecentManager.get_default();
        this._docsStale = true;
        this._recentManager.connect('changed', function(recentManager, userData) {
            me._docsStale = true;
            // Changes in local recent files should not happen when we are in the overlay mode,
            // but redisplaying right away is cool when we use Zephyr.
            // Also, we might be displaying remote documents, like Google Docs, in the future
            // which might be edited by someone else.
            me._redisplay(); 
        });
    },

    //// Protected method overrides ////

    // Gets the list of recent items from the recent items manager.
    _refreshCache : function() {
        let me = this;
        if (!this._docsStale)
            return;
        this._allItems = {};
        let docs = this._recentManager.get_items();
        for (let i = 0; i < docs.length; i++) {
            let docInfo = docs[i];
            let docId = docInfo.get_uri();
            // we use GtkRecentInfo URI as an item Id
            this._allItems[docId] = docInfo;
        }
        this._docsStale = false;
    },

    // Sets the list of the displayed items based on how recently they were last visited.
    _setDefaultList : function() {
        this._removeAllDisplayItems();

        // It seems to be an implementation detail of the Mozilla JavaScript that object
        // properties are returned during the iteration in the same order in which they were
        // defined, but it is not a guarantee according to this 
        // https://developer.mozilla.org/en/Core_JavaScript_1.5_Reference/Statements/for...in
        // While this._allItems associative array seems to always be ordered by last added,
        // as the results of this._recentManager.get_items() based on which it is constructed are,
        // we should do the sorting manually because we want the order to be based on last visited.
        //
        // This function is called each time the search string is set back to '' or we display
        // the overlay, so we are doing the sorting over the same items multiple times if the list
        // of recent items didn't change. We could store an additional array of doc ids and sort
        // them once when they are returned by this._recentManager.get_items() to avoid having to do 
        // this sorting each time, but the sorting seems to be very fast anyway, so there is no need
        // to introduce an additional class variable.
        let docIds = [];
        for (docId in this._allItems) {
            docIds.push(docId);
        }
        this._sortItems(docIds);

        let added = 0;
        for (let i = 0; i < docIds.length && added < this._maxItems; i++) {
            let docInfo = this._allItems[docIds[i]];
            // docInfo.exists() checks if the resource still exists
            if (docInfo.exists()) { 
                this._addDisplayItem(docIds[i]);
                added += 1;
            }
        }
    },

    // Sorts the list of item ids in-place based on how recently the items associated with
    // the ids were last visited.
   _sortItems : function(itemIds) {
        let me = this;
        itemIds.sort(function (a,b) {
            let docA = me._allItems[a];
            let docB = me._allItems[b];
            // We actually used get_modified() instead of get_visited() here, as GtkRecentInfo
            // doesn't updated get_visited() correctly.
            // See http://bugzilla.gnome.org/show_bug.cgi?id=567094
            if (docA.get_modified() > docB.get_modified())
                return -1;
            else if (docA.get_modified() < docB.get_modified())
                return 1;
            else 
                return 0;
        });
    },

    // Checks if the item info can be a match for the search string by checking
    // the name of the document. Item info is expected to be GtkRecentInfo.
    // Returns a boolean flag indicating if itemInfo is a match.
    _isInfoMatching : function(itemInfo, search) {
        if (!itemInfo.exists())
            return false;
 
        if (search == null || search == '')
            return true;

        let name = itemInfo.get_display_name().toLowerCase();
        if (name.indexOf(search) >= 0)
            return true;
        // TODO: we can also check doc URIs, so that
        // if you search for a directory name, we display recent files from it
        return false;
    },

    // Creates a DocDisplayItem based on itemInfo, which is expected be a GtkRecentInfo object. 
    _createDisplayItem: function(itemInfo) {
        return new DocDisplayItem(itemInfo, this._width);
    } 
};

Signals.addSignalMethods(DocDisplay.prototype);
