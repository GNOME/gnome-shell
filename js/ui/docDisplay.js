/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const DocInfo = imports.misc.docInfo;
const GenericDisplay = imports.ui.genericDisplay;
const Main = imports.ui.main;

/* This class represents a single display item containing information about a document.
 *
 * docInfo - DocInfo object containing information about the document
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
    
        this._setItemInfo(docInfo.name, "");
    },

    //// Public methods ////

    //// Public method overrides ////

    // Opens a document represented by this display item.
    launch : function() {
        this._docInfo.launch();
    },

    //// Protected method overrides ////

    // Returns an icon for the item.
    _createIcon : function() {
        return this._docInfo.createIcon(GenericDisplay.ITEM_DISPLAY_ICON_SIZE);
    },

    // Ensures the preview icon is created.
    _ensurePreviewIconCreated : function() {
        if (!this._previewIcon)
            this._previewIcon = this._docInfo.createIcon(GenericDisplay.PREVIEW_ICON_SIZE);
    },

    // Creates and returns a large preview icon, but only if this._docInfo is an image file
    // and we were able to generate a pixbuf from it successfully.
    _createLargePreviewIcon : function(availableWidth, availableHeight) {
        if (this._docInfo.mimeType == null || this._docInfo.mimeType.indexOf("image/") != 0)
            return null;

        return Shell.TextureCache.get_default().load_uri_sync(Shell.TextureCachePolicy.NONE,
                                                              this._docInfo.uri, availableWidth, availableHeight);
    }
};

/* This class represents a display containing a collection of document items.
 * The documents are sorted by how recently they were last visited.
 *
 * width - width available for the display
 */
function DocDisplay(width) {
    this._init(width);
}

DocDisplay.prototype = {
    __proto__:  GenericDisplay.GenericDisplay.prototype,

    _init : function(width) {
        GenericDisplay.GenericDisplay.prototype._init.call(this, width);
        let me = this;

        this._docManager = DocInfo.getDocManager(GenericDisplay.ITEM_DISPLAY_ICON_SIZE);
        this._docsStale = true;
        this._docManager.connect('changed', function(mgr, userData) {
            me._docsStale = true;
            // Changes in local recent files should not happen when we are in the overlay mode,
            // but redisplaying right away is cool when we use Zephyr.
            // Also, we might be displaying remote documents, like Google Docs, in the future
            // which might be edited by someone else.
            me._redisplay(false); 
        });
    },

    //// Protected method overrides ////

    // Gets the list of recent items from the recent items manager.
    _refreshCache : function() {
        let me = this;
        if (!this._docsStale)
            return;
        this._allItems = this._docManager.getItems();
        this._docsStale = false;
    },

    // Sets the list of the displayed items based on how recently they were last visited.
    _setDefaultList : function() {
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
        this._matchedItems = [];
        let docIdsToRemove = [];
        for (docId in this._allItems) {
            // this._allItems[docId].exists() checks if the resource still exists
            if (this._allItems[docId].exists()) 
                this._matchedItems.push(docId);
            else 
                docIdsToRemove.push(docId);
        }

        for (docId in docIdsToRemove) {
            delete this._allItems[docId];
        }

        this._matchedItems.sort(Lang.bind(this, function (a,b) { return this._compareItems(a,b); }));
    },

    // Compares items associated with the item ids based on how recently the items
    // were last visited.
    // Returns an integer value indicating the result of the comparison.
   _compareItems : function(itemIdA, itemIdB) {
        let docA = this._allItems[itemIdA];
        let docB = this._allItems[itemIdB];

        return docB.lastVisited() - docA.lastVisited();
    },

    // Checks if the item info can be a match for the search string by checking
    // the name of the document. Item info is expected to be GtkRecentInfo.
    // Returns a boolean flag indicating if itemInfo is a match.
    _isInfoMatching : function(itemInfo, search) {
        if (!itemInfo.exists())
            return false;
 
        if (search == null || search == '')
            return true;

        let name = itemInfo.name.toLowerCase();
        if (name.indexOf(search) >= 0)
            return true;
        // TODO: we can also check doc URIs, so that
        // if you search for a directory name, we display recent files from it
        return false;
    },

    // Creates a DocDisplayItem based on itemInfo, which is expected to be a DocInfo object.
    _createDisplayItem: function(itemInfo, width) {
        return new DocDisplayItem(itemInfo, width);
    }
};

Signals.addSignalMethods(DocDisplay.prototype);
