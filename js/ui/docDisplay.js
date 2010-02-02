/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const St = imports.gi.St;
const Mainloop = imports.mainloop;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const DocInfo = imports.misc.docInfo;
const DND = imports.ui.dnd;
const GenericDisplay = imports.ui.genericDisplay;
const Main = imports.ui.main;
const Search = imports.ui.search;

const MAX_DASH_DOCS = 50;
const DASH_DOCS_ICON_SIZE = 16;

const DEFAULT_SPACING = 4;

/* This class represents a single display item containing information about a document.
 * We take the current number of seconds in the constructor to avoid looking up the current
 * time for every item when they are created in a batch.
 *
 * docInfo - DocInfo object containing information about the document
 * currentSeconds - current number of seconds since the epoch
 */
function DocDisplayItem(docInfo, currentSecs) {
    this._init(docInfo, currentSecs);
}

DocDisplayItem.prototype = {
    __proto__:  GenericDisplay.GenericDisplayItem.prototype,

    _init : function(docInfo, currentSecs) {
        GenericDisplay.GenericDisplayItem.prototype._init.call(this);
        this._docInfo = docInfo;

        this._setItemInfo(docInfo.name, "");

        this._timeoutTime = -1;
        this._resetTimeDisplay(currentSecs);
    },

    //// Public methods ////

    getUpdateTimeoutTime: function() {
        return this._timeoutTime;
    },

    // Update any relative-time based displays for this item.
    redisplay: function(currentSecs) {
        this._resetTimeDisplay(currentSecs);
    },

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

    // Returns a preview icon for the item.
    _createPreviewIcon : function() {
        return this._docInfo.createIcon(GenericDisplay.PREVIEW_ICON_SIZE);
    },

    // Creates and returns a large preview icon, but only if this._docInfo is an image file
    // and we were able to generate a pixbuf from it successfully.
    _createLargePreviewIcon : function() {
        if (this._docInfo.mimeType == null || this._docInfo.mimeType.indexOf("image/") != 0)
            return null;

        try {
            return Shell.TextureCache.get_default().load_uri_sync(Shell.TextureCachePolicy.NONE,
                                                                  this._docInfo.uri, -1, -1);
        } catch (e) {
            // An exception will be raised when the image format isn't know
            /* FIXME: http://bugzilla.gnome.org/show_bug.cgi?id=591480: should
             *        only ignore GDK_PIXBUF_ERROR_UNKNOWN_TYPE. */
            return null;
        }
    },

    //// Drag and Drop ////

    shellWorkspaceLaunch: function() {
        this.launch();
    },

    //// Private Methods ////

    // Updates the last visited time displayed in the description text for the item.
    _resetTimeDisplay: function(currentSecs) {
        let lastSecs = this._docInfo.timestamp;
        let timeDelta = currentSecs - lastSecs;
        let [text, nextUpdate] = global.format_time_relative_pretty(timeDelta);
        this._timeoutTime = currentSecs + nextUpdate;
        this._setDescriptionText(text);
    }
};

/* This class represents a display containing a collection of document items.
 * The documents are sorted by how recently they were last visited.
 */
function DocDisplay(flags) {
    this._init(flags);
}

DocDisplay.prototype = {
    __proto__:  GenericDisplay.GenericDisplay.prototype,

    _init : function(flags) {
        GenericDisplay.GenericDisplay.prototype._init.call(this, flags);
        // We keep a single timeout callback for updating last visited times
        // for all the items in the display. This avoids creating individual
        // callbacks for each item in the display. So proper time updates
        // for individual items and item details depend on the item being
        // associated with one of the displays.
        this._updateTimeoutTargetTime = -1;
        this._updateTimeoutId = 0;

        this._docManager = DocInfo.getDocManager();
        this._docsStale = true;
        this._docManager.connect('changed', Lang.bind(this, function(mgr, userData) {
            this._docsStale = true;
            // Changes in local recent files should not happen when we are in the Overview mode,
            // but redisplaying right away is cool when we use Zephyr.
            // Also, we might be displaying remote documents, like Google Docs, in the future
            // which might be edited by someone else.
            this._redisplay(GenericDisplay.RedisplayFlags.NONE);
        }));

        this.connect('destroy', Lang.bind(this, function (o) {
            if (this._updateTimeoutId > 0)
                Mainloop.source_remove(this._updateTimeoutId);
        }));
    },

    //// Protected method overrides ////

    // Gets the list of recent items from the recent items manager.
    _refreshCache : function() {
        if (!this._docsStale)
            return true;
        this._allItems = {};
        Lang.copyProperties(this._docManager.getInfosByUri(), this._allItems);
        this._docsStale = false;
        return false;
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
        // the Overview, so we are doing the sorting over the same items multiple times if the list
        // of recent items didn't change. We could store an additional array of doc ids and sort
        // them once when they are returned by this._recentManager.get_items() to avoid having to do 
        // this sorting each time, but the sorting seems to be very fast anyway, so there is no need
        // to introduce an additional class variable.
        this._matchedItems = {};
        this._matchedItemKeys = [];
        let docIdsToRemove = [];
        for (docId in this._allItems) {
            this._matchedItems[docId] = 1;
            this._matchedItemKeys.push(docId);
        }

        for (docId in docIdsToRemove) {
            delete this._allItems[docId];
        }

        this._matchedItemKeys.sort(Lang.bind(this, this._compareItems));
    },

    // Compares items associated with the item ids based on how recently the items
    // were last visited.
    // Returns an integer value indicating the result of the comparison.
   _compareItems : function(itemIdA, itemIdB) {
        let docA = this._allItems[itemIdA];
        let docB = this._allItems[itemIdB];

        return docB.timestamp - docA.timestamp;
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
    _createDisplayItem: function(itemInfo) {
        let currentSecs = new Date().getTime() / 1000;
        let docDisplayItem = new DocDisplayItem(itemInfo, currentSecs);
        this._updateTimeoutCallback(docDisplayItem, currentSecs);
        return docDisplayItem;
    },

    //// Private Methods ////

    // A callback function that redisplays the items, updating their descriptions,
    // and sets up a new timeout callback.
    _docTimeout: function () {
        let currentSecs = new Date().getTime() / 1000;
        this._updateTimeoutId = 0;
        this._updateTimeoutTargetTime = -1;
        for (let docId in this._displayedItems) {
            let docDisplayItem = this._displayedItems[docId];
            docDisplayItem.redisplay(currentSecs);          
            this._updateTimeoutCallback(docDisplayItem, currentSecs);
        }
        return false;
    },

    // Updates the timeout callback if the timeout time for the docDisplayItem 
    // is earlier than the target time for the current timeout callback.
    _updateTimeoutCallback: function (docDisplayItem, currentSecs) {
        let timeoutTime = docDisplayItem.getUpdateTimeoutTime();
        if (this._updateTimeoutTargetTime < 0 || timeoutTime < this._updateTimeoutTargetTime) {
            if (this._updateTimeoutId > 0)
                Mainloop.source_remove(this._updateTimeoutId);
            this._updateTimeoutId = Mainloop.timeout_add_seconds(timeoutTime - currentSecs, Lang.bind(this, this._docTimeout));
            this._updateTimeoutTargetTime = timeoutTime;
        }
    }
};

Signals.addSignalMethods(DocDisplay.prototype);

function DashDocDisplayItem(docInfo) {
    this._init(docInfo);
}

DashDocDisplayItem.prototype = {
    _init: function(docInfo) {
        this._info = docInfo;
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   spacing: DEFAULT_SPACING,
                                   reactive: true });
        this.actor.connect('button-release-event', Lang.bind(this, function () {
            docInfo.launch();
            Main.overview.hide();
        }));

        this.actor._delegate = this;

        this._icon = docInfo.createIcon(DASH_DOCS_ICON_SIZE);
        let iconBox = new Big.Box({ y_align: Big.BoxAlignment.CENTER });
        iconBox.append(this._icon, Big.BoxPackFlags.NONE);
        this.actor.append(iconBox, Big.BoxPackFlags.NONE);
        let name = new St.Label({ style_class: 'dash-recent-docs-item',
                                   text: docInfo.name });
        this.actor.append(name, Big.BoxPackFlags.EXPAND);

        let draggable = DND.makeDraggable(this.actor);
    },

    getUri: function() {
        return this._info.uri;
    },

    getDragActorSource: function() {
        return this._icon;
    },

    getDragActor: function(stageX, stageY) {
        this.dragActor = this._info.createIcon(DASH_DOCS_ICON_SIZE);
        return this.dragActor;
    },

    //// Drag and drop functions ////

    shellWorkspaceLaunch: function () {
        this._info.launch();
    }
};

/**
 * Class used to display two column recent documents in the dash
 */
function DashDocDisplay() {
    this._init();
}

DashDocDisplay.prototype = {
    _init: function() {
        this.actor = new Shell.GenericContainer();
        this.actor.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));
        this._workId = Main.initializeDeferredWork(this.actor, Lang.bind(this, this._redisplay));

        this._actorsByUri = {};

        this._docManager = DocInfo.getDocManager();
        this._docManager.connect('changed', Lang.bind(this, this._onDocsChanged));
        this._pendingDocsChange = true;
        this._checkDocExistence = false;
    },

    _getPreferredWidth: function(actor, forHeight, alloc) {
        let children = actor.get_children();

        // We use two columns maximum.  Just take the min and natural size of the
        // first two items, even though strictly speaking it's not correct; we'd
        // need to calculate how many items we could fit for the height, then
        // take the biggest preferred width for each column.
        // In practice the dash gets a fixed width anyways.

        // If we have one child, add its minimum and natural size
        if (children.length > 0) {
            let [minSize, naturalSize] = children[0].get_preferred_width(forHeight);
            alloc.min_size += minSize;
            alloc.natural_size += naturalSize;
        }
        // If we have two, add its size, plus DEFAULT_SPACING
        if (children.length > 1) {
            let [minSize, naturalSize] = children[1].get_preferred_width(forHeight);
            alloc.min_size += DEFAULT_SPACING + minSize;
            alloc.natural_size += DEFAULT_SPACING + naturalSize;
        }
    },

    _getPreferredHeight: function(actor, forWidth, alloc) {
        let children = actor.get_children();

        // The width of an item is our allocated width, minus spacing, divided in half.
        this._itemWidth = Math.floor((forWidth - DEFAULT_SPACING) / 2);

        let maxNatural = 0;
        for (let i = 0; i < children.length; i++) {
            let child = children[i];
            let [minSize, naturalSize] = child.get_preferred_height(this._itemWidth);
            maxNatural = Math.max(maxNatural, naturalSize);
        }

        this._itemHeight = maxNatural;

        let firstColumnChildren = Math.ceil(children.length / 2);
        alloc.natural_size = (firstColumnChildren * maxNatural +
                              (firstColumnChildren - 1) * DEFAULT_SPACING);
    },

    _allocate: function(actor, box, flags) {
        let width = box.x2 - box.x1;
        let height = box.y2 - box.y1;

        // Make sure this._itemWidth/Height have been computed, even
        // if the parent actor didn't check our size before allocating.
        // (Not clear if that is required or not as a Clutter
        // invariant; this is safe and cheap because of caching.)
        actor.get_preferred_height(width);

        let children = actor.get_children();

        let x = 0;
        let y = 0;
        let columnIndex = 0;
        let i = 0;
        // Loop over the children, going vertically down first.  When we run
        // out of vertical space (our y variable is bigger than box.y2), switch
        // to the second column.
        while (i < children.length) {
            let child = children[i];

            if (y + this._itemHeight > box.y2) {
                // Is this the second column, or we're in
                // the first column and can't even fit one
                // item?  In that case, break.
                if (columnIndex == 1 || i == 0) {
                    break;
                }
                // Set x to the halfway point.
                columnIndex += 1;
                x = x + this._itemWidth + DEFAULT_SPACING;
                // And y is back to the top.
                y = 0;
                // Retry this same item, now that we're in the second column.
                // By looping back to the top here, we re-test the size
                // again for the second column.
                continue;
            }

            let childBox = new Clutter.ActorBox();
            childBox.x1 = x;
            childBox.y1 = y;
            childBox.x2 = childBox.x1 + this._itemWidth;
            childBox.y2 = y + this._itemHeight;

            y = childBox.y2 + DEFAULT_SPACING;

            child.allocate(childBox, flags);
            this.actor.set_skip_paint(child, false);

            i++;
        }

        if (this._checkDocExistence) {
            // Now we know how many docs we are displaying, queue a check to see if any of them
            // have been deleted. If they are deleted, then we'll get a 'changed' signal; since
            // we'll now be displaying items we weren't previously, we'll check again to see
            // if they were deleted, and so forth and so on.
            // TODO: We should change this to ask for as many as we can fit in the given space:
            // https://bugzilla.gnome.org/show_bug.cgi?id=603522#c23
            this._docManager.queueExistenceCheck(i);
            this._checkDocExistence = false;
        }

        for (; i < children.length; i++)
            this.actor.set_skip_paint(children[i], true);
    },

    _onDocsChanged: function() {
        this._checkDocExistence = true;
        Main.queueDeferredWork(this._workId);
    },

    _redisplay: function() {
        // Should be kept alive by the _actorsByUri
        this.actor.remove_all();
        let docs = this._docManager.getTimestampOrderedInfos();
        for (let i = 0; i < docs.length && i < MAX_DASH_DOCS; i++) {
            let doc = docs[i];
            let display = this._actorsByUri[doc.uri];
            if (display) {
                this.actor.add_actor(display.actor);
            } else {
                let display = new DashDocDisplayItem(doc);
                this.actor.add_actor(display.actor);
                this._actorsByUri[doc.uri] = display;
            }
        }
        // Any unparented actors must have been deleted
        for (let uri in this._actorsByUri) {
            let display = this._actorsByUri[uri];
            if (display.actor.get_parent() == null) {
                display.actor.destroy();
                delete this._actorsByUri[uri];
            }
        }
        this.emit('changed');
    }
};

Signals.addSignalMethods(DashDocDisplay.prototype);

function DocSearchProvider() {
    this._init();
}

DocSearchProvider.prototype = {
    __proto__: Search.SearchProvider.prototype,

    _init: function(name) {
        Search.SearchProvider.prototype._init.call(this, _("RECENT ITEMS"));
        this._docManager = DocInfo.getDocManager();
    },

    getResultMeta: function(resultId) {
        let docInfo = this._docManager.lookupByUri(resultId);
        if (!docInfo)
            return null;
        return { 'id': resultId,
                 'name': docInfo.name,
                 'icon': docInfo.createIcon(Search.RESULT_ICON_SIZE)};
    },

    activateResult: function(id) {
        let docInfo = this._docManager.lookupByUri(id);
        docInfo.launch();
    },

    getInitialResultSet: function(terms) {
        return this._docManager.initialSearch(terms);
    },

    getSubsearchResultSet: function(previousResults, terms) {
        return this._docManager.subsearch(previousResults, terms);
    },

    expandSearch: function(terms) {
        log("TODO expand docs search");
    }
};
