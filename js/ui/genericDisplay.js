/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gdk = imports.gi.Gdk;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Signals = imports.signals;
const Shell = imports.gi.Shell;

const Button = imports.ui.button;
const DND = imports.ui.dnd;
const Link = imports.ui.link;
const Main = imports.ui.main;

const RedisplayFlags = { NONE: 0,
                         RESET_CONTROLS: 1 << 0,
                         FULL: 1 << 1,
                         SUBSEARCH: 1 << 2 };

const ITEM_DISPLAY_NAME_COLOR = new Clutter.Color();
ITEM_DISPLAY_NAME_COLOR.from_pixel(0xffffffff);
const ITEM_DISPLAY_DESCRIPTION_COLOR = new Clutter.Color();
ITEM_DISPLAY_DESCRIPTION_COLOR.from_pixel(0xffffffbb);
const ITEM_DISPLAY_BACKGROUND_COLOR = new Clutter.Color();
ITEM_DISPLAY_BACKGROUND_COLOR.from_pixel(0x00000000);
const ITEM_DISPLAY_SELECTED_BACKGROUND_COLOR = new Clutter.Color();
ITEM_DISPLAY_SELECTED_BACKGROUND_COLOR.from_pixel(0x4f6fadaa);
const DISPLAY_CONTROL_SELECTED_COLOR = new Clutter.Color();
DISPLAY_CONTROL_SELECTED_COLOR.from_pixel(0x112288ff);
const PREVIEW_BOX_BACKGROUND_COLOR = new Clutter.Color();
PREVIEW_BOX_BACKGROUND_COLOR.from_pixel(0xADADADf0);

const DEFAULT_PADDING = 4;

const ITEM_DISPLAY_HEIGHT = 50;
const ITEM_DISPLAY_ICON_SIZE = 48;
const ITEM_DISPLAY_PADDING = 1;
const ITEM_DISPLAY_PADDING_RIGHT = 2;
const DEFAULT_COLUMN_GAP = 6;

const PREVIEW_ICON_SIZE = 96;
const PREVIEW_BOX_PADDING = 6;
const PREVIEW_BOX_SPACING = DEFAULT_PADDING;
const PREVIEW_BOX_CORNER_RADIUS = 10;
// how far relative to the full item width the preview box should be placed
const PREVIEW_PLACING = 3/4;
const PREVIEW_DETAILS_MIN_WIDTH = PREVIEW_ICON_SIZE * 2;

const INFORMATION_BUTTON_SIZE = 16;

/* This is a virtual class that represents a single display item containing
 * a name, a description, and an icon. It allows selecting an item and represents
 * it by highlighting it with a different background color than the default.
 */
function GenericDisplayItem() {
    this._init();
}

GenericDisplayItem.prototype = {
    _init: function() {
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   spacing: ITEM_DISPLAY_PADDING,
                                   reactive: true,
                                   background_color: ITEM_DISPLAY_BACKGROUND_COLOR,
                                   corner_radius: 4,
                                   height: ITEM_DISPLAY_HEIGHT });

        this.actor._delegate = this;
        this.actor.connect('button-release-event',
                           Lang.bind(this,
                                     function() {
                                         // Activates the item by launching it
                                         this.emit('activate');
                                         return true;  
                                     }));

        let draggable = DND.makeDraggable(this.actor);
        draggable.connect('drag-begin', Lang.bind(this, this._onDragBegin));

        this._infoContent = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                          spacing: DEFAULT_PADDING });
        this.actor.append(this._infoContent, Big.BoxPackFlags.EXPAND);

        this._iconBox = new Big.Box();
        this._infoContent.append(this._iconBox, Big.BoxPackFlags.NONE);

        this._infoText = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                       spacing: DEFAULT_PADDING });
        this._infoContent.append(this._infoText, Big.BoxPackFlags.EXPAND);

        let infoIconUri = "file://" + global.imagedir + "info.svg";
        let infoIcon = Shell.TextureCache.get_default().load_uri_sync(Shell.TextureCachePolicy.FOREVER,
                                                                      infoIconUri,
                                                                      INFORMATION_BUTTON_SIZE,
                                                                      INFORMATION_BUTTON_SIZE);
        this._informationButton = new Button.IconButton(this.actor, INFORMATION_BUTTON_SIZE, infoIcon);
        let buttonBox = new Big.Box({ width: INFORMATION_BUTTON_SIZE + 2 * DEFAULT_PADDING,
                                      height: INFORMATION_BUTTON_SIZE,
                                      padding_left: DEFAULT_PADDING, padding_right: DEFAULT_PADDING,
                                      y_align: Big.BoxAlignment.CENTER });
        buttonBox.append(this._informationButton.actor, Big.BoxPackFlags.NONE);
        this.actor.append(buttonBox, Big.BoxPackFlags.END);

        // Connecting to the button-press-event for the information button ensures that the actor,
        // which is a draggable actor, does not get the button-press-event and doesn't initiate
        // the dragging, which then prevents us from getting the button-release-event for the button.
        this._informationButton.actor.connect('button-press-event',
                                              Lang.bind(this,
                                                        function() {
                                                            return true;
                                                        }));
        this._informationButton.actor.connect('button-release-event',
                                              Lang.bind(this,
                                                        function() {
                                                            // Selects the item by highlighting it and displaying its details
                                                            this.emit('show-details');
                                                            return true;
                                                        }));

        this._name = null;
        this._description = null;
        this._icon = null;

        this._initialLoadComplete = false;

        // An array of details description actors that we create over time for the item.
        // It is used for updating the description text inside the details actor when
        // the description text for the item is updated.
        this._detailsDescriptions = [];
    },

    //// Draggable object interface ////

    // Returns a cloned texture of the item's icon to represent the item as it 
    // is being dragged. 
    getDragActor: function(stageX, stageY) {
        return this._createIcon();
    },

    // Returns the item icon, a separate copy of which is used to
    // represent the item as it is being dragged. This is used to
    // determine a snap-back location for the drag icon if it does
    // not get accepted by any drop target.
    getDragActorSource: function() {
        return this._icon;
    },

    //// Public methods ////

    // Shows the information button when the item was drawn under the mouse pointer.
    onDrawnUnderPointer: function() {
        this._informationButton.show();
    },

    // Highlights the item by setting a different background color than the default 
    // if isSelected is true, removes the highlighting otherwise.
    markSelected: function(isSelected) {
       let color;
       if (isSelected) {
           color = ITEM_DISPLAY_SELECTED_BACKGROUND_COLOR;
           this._informationButton.forceShow(true);
       }
       else {
           color = ITEM_DISPLAY_BACKGROUND_COLOR;
           this._informationButton.forceShow(false);
       }
       this.actor.background_color = color;
    },

    /*
     * Returns an actor containing item details. In the future details can have more information than what
     * the preview pop-up has and be item-type specific.
     */
    createDetailsActor: function() {

        let details = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                    spacing: PREVIEW_BOX_SPACING });

        let mainDetails = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                        spacing: PREVIEW_BOX_SPACING });

        // Inner box with name and description
        let textDetails = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                        spacing: PREVIEW_BOX_SPACING });
        let detailsName = new Clutter.Text({ color: ITEM_DISPLAY_NAME_COLOR,
                                             font_name: "Sans bold 14px",
                                             line_wrap: true,
                                             text: this._name.text });
        textDetails.append(detailsName, Big.BoxPackFlags.NONE);

        let detailsDescription = new Clutter.Text({ color: ITEM_DISPLAY_NAME_COLOR,
                                                    font_name: "Sans 14px",
                                                    line_wrap: true,
                                                    text: this._description.text });
        textDetails.append(detailsDescription, Big.BoxPackFlags.NONE);

        this._detailsDescriptions.push(detailsDescription);

        mainDetails.append(textDetails, Big.BoxPackFlags.EXPAND);

        let previewIcon = this._createPreviewIcon();
        let largePreviewIcon = this._createLargePreviewIcon();

        if (previewIcon != null && largePreviewIcon == null) {
            mainDetails.prepend(previewIcon, Big.BoxPackFlags.NONE);
        }

        details.append(mainDetails, Big.BoxPackFlags.NONE);

        if (largePreviewIcon != null) {
            let largePreview = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL });
            largePreview.append(largePreviewIcon, Big.BoxPackFlags.NONE);
            details.append(largePreview, Big.BoxPackFlags.NONE);
        }
   
        return details;
    },

    // Destroys the item.
    destroy: function() {
      this.actor.destroy();
    },

    //// Pure virtual public methods ////

    // Performes an action associated with launching this item, such as opening a file or an application.
    launch: function() {
        throw new Error("Not implemented");
    },

    //// Protected methods ////

    /*
     * Creates the graphical elements for the item based on the item information.
     *
     * nameText - name of the item
     * descriptionText - short description of the item
     */
    _setItemInfo: function(nameText, descriptionText) {
        if (this._name != null) {
            // this also removes this._name from the parent container,
            // so we don't need to call this.actor.remove_actor(this._name) directly
            this._name.destroy();
            this._name = null;
        } 
        if (this._description != null) {
            this._description.destroy();
            this._description = null;
        } 
        if (this._icon != null) {
            // though we get the icon from elsewhere, we assume its ownership here,
            // and therefore should be responsible for distroying it
            this._icon.destroy();
            this._icon = null;
        }

        this._icon = this._createIcon();
        this._iconBox.append(this._icon, Big.BoxPackFlags.NONE);

        this._name = new Clutter.Text({ color: ITEM_DISPLAY_NAME_COLOR,
                                        font_name: "Sans 14px",
                                        ellipsize: Pango.EllipsizeMode.END,
                                        text: nameText });
        this._infoText.append(this._name, Big.BoxPackFlags.EXPAND);

        this._description = new Clutter.Text({ color: ITEM_DISPLAY_DESCRIPTION_COLOR,
                                               font_name: "Sans 12px",
                                               ellipsize: Pango.EllipsizeMode.END,
                                               text: descriptionText ? descriptionText : ""
                                            });
        this._infoText.append(this._description, Big.BoxPackFlags.EXPAND);
    },

    // Sets the description text for the item, including the description text
    // in the details actors that have been created for the item.
    _setDescriptionText: function(text) {
        this._description.text = text;
        for (let i = 0; i < this._detailsDescriptions.length; i++) {
            let detailsDescription = this._detailsDescriptions[i];
            if (detailsDescription != null) {
                detailsDescription.text = text;
            }
        }
    },

    //// Virtual protected methods ////

    // Creates and returns a large preview icon, but only if we have a detailed image.
    _createLargePreviewIcon : function() {
        return null;
    },

    //// Pure virtual protected methods ////

    // Returns an icon for the item.
    _createIcon: function() {
        throw new Error("Not implemented");
    },

    // Returns a preview icon for the item.
    _createPreviewIcon: function() {
        throw new Error("Not implemented");
    },

    //// Private methods ////

    // Hides the information button once the item starts being dragged.
    _onDragBegin : function (draggable, time) {
        // For some reason, we are not getting leave-event signal when we are dragging an item,
        // so we should remove the link manually.
        this._informationButton.actor.hide();
    } 
};

Signals.addSignalMethods(GenericDisplayItem.prototype);

/* This is a virtual class that represents a display containing a collection of items
 * that can be filtered with a search string.
 */
function GenericDisplay() {
    this._init();
}

GenericDisplay.prototype = {
    _init : function() {
        this._search = '';
        this._expanded = false;

        this._maxItemsPerPage = null;
        this._list = new Shell.OverflowList({ spacing: 6.0,
                                              item_height: ITEM_DISPLAY_HEIGHT });

        this._list.connect('notify::n-pages', Lang.bind(this, function () {
            this._updateDisplayControl(true);
        }));
        this._list.connect('notify::page', Lang.bind(this, function () {
            this._updateDisplayControl(false);
        }));

        // map<itemId, Object> where Object represents the item info
        this._allItems = {};
        // set<itemId>
        this._matchedItems = {};
        // sorted array of items matched by search
        this._matchedItemKeys = [];
        // map<itemId, GenericDisplayItem>
        this._displayedItems = {};
        this._openDetailIndex = -1;
        this._selectedIndex = -1;
        // These two are public - .actor is the normal "actor subclass" property,
        // but we also expose a .displayControl actor which is separate.
        // See also getNavigationArea.
        this.actor = this._list;
        this.displayControl = new Big.Box({ background_color: ITEM_DISPLAY_BACKGROUND_COLOR,
                                            spacing: 12,
                                            orientation: Big.BoxOrientation.HORIZONTAL});
    },

    //// Public methods ////

    // Sets the search string and displays the matching items.
    setSearch: function(text) {
        let lowertext = text.toLowerCase();
        if (lowertext == this._search)
            return;
        let flags = RedisplayFlags.RESET_CONTROLS;
        if (this._search != '') {
            if (lowertext.indexOf(this._search) == 0)
                flags |= RedisplayFlags.SUBSEARCH;
        }
        this._search = lowertext;
        this._redisplay(flags);
    },

    // Launches the item that is currently selected, closing the Overview
    activateSelected: function() {
        if (this._selectedIndex != -1) {
            let selected = this._findDisplayedByIndex(this._selectedIndex);
            selected.launch();
            this.unsetSelected();
            Main.overview.hide();
        }
    },

    // Moves the selection one up. If the selection was already on the top item, it's moved
    // to the bottom one. Returns true if the selection actually moved up, false if it wrapped 
    // around to the bottom.
    selectUp: function() {
        let count = this._list.displayedCount;
        let selectedUp = true;
        let prev = this._selectedIndex - 1;
        if (this._selectedIndex <= 0) {
            prev = count - 1;
            selectedUp = false; 
        }
        this._selectIndex(prev);
        return selectedUp;
    },

    // Moves the selection one down. If the selection was already on the bottom item, it's moved
    // to the top one. Returns true if the selection actually moved down, false if it wrapped 
    // around to the top.
    selectDown: function() {
        let count = this._list.displayedCount;
        let selectedDown = true;
        let next = this._selectedIndex + 1;
        if (this._selectedIndex == count - 1) {
            next = 0;
            selectedDown = false;
        }
        this._selectIndex(next);
        return selectedDown;
    },

    // Selects the first item among the displayed items.
    selectFirstItem: function() {
        if (this.hasItems())
            this._selectIndex(0);
    },

    // Selects the last item among the displayed items.
    selectLastItem: function() {
        let count = this._list.displayedCount;
        if (this.hasItems())
            this._selectIndex(count - 1);
    },

    // Returns true if the display has some item selected.
    hasSelected: function() {
        return this._selectedIndex != -1;
    },

    // Removes selection if some display item is selected.
    unsetSelected: function() {
        this._selectIndex(-1);
    },

    // Returns true if the display has any displayed items.
    hasItems: function() {
        // TODO: figure out why this._list.displayedCount is returning a
        // positive number when this._mathedItems.length is 0
        // This can be triggered if a search string is entered for which there are no matches.
        // log("this._mathedItems.length: " + this._matchedItems.length + " this._list.displayedCount " + this._list.displayedCount);
        return this._matchedItemKeys.length > 0;
    },

    getMatchedItemsCount: function() {
        return this._matchedItemKeys.length;
    },

    // Load the initial state
    load: function() {
        this._redisplay(RedisplayFlags.FULL);
    },

    // Should be called when the display is closed
    resetState: function() {
        this._filterReset();
        this._openDetailIndex = -1;
    },

    // Returns an actor which acts as a sidebar; this is used for
    // the applications category view
    getNavigationArea: function () {
        return null;
    },

    createDetailsForIndex: function(index) {
        let item = this._findDisplayedByIndex(index);
        return item.createDetailsActor();
    },

    // Displays the page specified by the pageNumber argument.
    displayPage: function(pageNumber) {
        // Cleanup from the previous selection, but don't unset this._selectedIndex
        if (this.hasSelected()) {
            this._findDisplayedByIndex(this._selectedIndex).markSelected(false);
        }
        this._list.page = pageNumber;
    },

    //// Protected methods ////

    _redisplayFull: function() {
        this._removeAllDisplayItems();
        for (let itemId in this._allItems) {
            this._addDisplayItem(itemId);
        }
    },

    // Creates a display item based on the information associated with itemId
    // and adds it to the displayed items.
    _addDisplayItem : function(itemId) {
        if (this._displayedItems.hasOwnProperty(itemId)) {
            log("Tried adding a display item for " + itemId + ", but an item with this item id is already among displayed items.");
            return;
        }

        let itemInfo = this._allItems[itemId];
        let displayItem = this._createDisplayItem(itemInfo);

        displayItem.connect('activate',
                            Lang.bind(this,
                                      function() {
                                          // update the selection
                                          this._selectIndex(this._list.get_actor_index(displayItem.actor));
                                          this.activateSelected();
                                      }));

        displayItem.connect('show-details',
                            Lang.bind(this,
                                      function() {
                                          let index = this._list.get_actor_index(displayItem.actor);
                                          /* Close the details pane if already open */
                                          if (index == this._openDetailIndex) {
                                              this._openDetailIndex = -1;
                                              this.emit('show-details', -1);
                                          } else {
                                              this._openDetailIndex = index;
                                              this.emit('show-details', index);
                                          }
                                      }));
        this._list.add_actor(displayItem.actor);
        this._displayedItems[itemId] = displayItem;
    },

    // Removes an item identifed by the itemId from the displayed items.
    _removeDisplayItem: function(itemId) {
        let count = this._list.displayedCount;
        let displayItem = this._displayedItems[itemId];
        let displayItemIndex = this._list.get_actor_index(displayItem.actor);

        if (this.hasSelected() && count == 1) {
            this.unsetSelected();
        } else if (this.hasSelected() && displayItemIndex < this._selectedIndex) {
            this.selectUp();
        }

        displayItem.destroy();

        delete this._displayedItems[itemId];
    },

    // Removes all displayed items.
    _removeAllDisplayItems: function() {
        this.unsetSelected();
        for (itemId in this._displayedItems)
            this._removeDisplayItem(itemId);
     },

    // Return true if there's an active search or other constraint
    // on the list
    _filterActive: function() {
        return this._search != "";
    },

    // Called when we are resetting state
    _filterReset: function() {
        this.unsetSelected();
    },

    _compareSearchMatch: function(a, b) {
        let countA = this._matchedItems[a];
        let countB = this._matchedItems[b];
        if (countA > countB)
            return -1;
        else if (countA < countB)
            return 1;
        else
            return this._compareItems(a, b);
    },

    _setMatches: function(matches) {
        this._matchedItems = matches;
        this._matchedItemKeys = [];
        for (let itemId in this._matchedItems) {
            this._matchedItemKeys.push(itemId);
        }
        this._matchedItemKeys.sort(Lang.bind(this, this._compareSearchMatch));
    },

    /**
     * _redisplaySubSearch:
     * A somewhat more optimized function called when we know
     * that we're going to be displaying a subset of the items
     * we already had, in the same order.  In that case, we can
     * just hide the actors that shouldn't be shown.
     */
    _redisplaySubSearch: function() {
        let matches = this._getSearchMatchedItems(true);

        // Just hide all from the old set,
        // we'll show the ones we want below
        for (let itemId in this._displayedItems) {
            let item = this._displayedItems[itemId];
            item.actor.hide();
        }

        this._setMatches(matches);

        for (let itemId in matches) {
            let item = this._displayedItems[itemId];
            item.actor.show();
        }
        this._list.queue_relayout();
    },

    _redisplayReordering: function() {
        if (!this._filterActive()) {
            this._setDefaultList();
        } else {
            this._setMatches(this._getSearchMatchedItems(false));
        }
        this._list.remove_all();
        for (let i = 0; i < this._matchedItemKeys.length; i++) {
            let itemId = this._matchedItemKeys[i];
            let item = this._displayedItems[itemId];
            item.actor.show();
            this._list.add_actor(item.actor);
       }
    },

    /*
     * Updates the displayed items, applying the search string if one exists.
     * @flags: Flags controlling redisplay behavior as follows:
     *  RESET_CONTROLS - indicates if the page selection should be reset when displaying the matching results.
     *  We reset the page selection when the change in results was initiated by the user by
     *  entering a different search criteria or by viewing the results list in a different
     *  size mode, but we keep the page selection the same if the results got updated on
     *  their own while the user was browsing through the result pages.
     *  SUBSEARCH - Indicates that the current _search is a superstring of the previous
     *  one, which implies we only need to re-search through previous results.
     */
    _redisplay: function(flags) {
        let resetPage = (flags & RedisplayFlags.RESET_CONTROLS) > 0;
        let isSubSearch = (flags & RedisplayFlags.SUBSEARCH) > 0;
        let fullReload = (flags & RedisplayFlags.FULL) > 0;

        let hadSelected = this.hasSelected();

        if (!this._initialLoadComplete || !this._refreshCache())
            fullReload = true;
        if (fullReload) {
            this._initialLoadComplete = true;
            this._redisplayFull();
        } if (isSubSearch) {
            this._redisplaySubSearch();
        } else {
            this._redisplayReordering();
        }

        if (resetPage)
            this._list.page = 0;

        if (hadSelected) {
            this._selectedIndex = -1;
            this.selectFirstItem();
        }

        Mainloop.idle_add(Lang.bind(this, this._checkInformationIcon),
                          Meta.PRIORITY_BEFORE_REDRAW);

        this.emit('redisplayed');
    },

    // Check if the pointer is over one of the items and display the information button if it is.
    // We want to do this between finishing our changes to the display and the point where
    // the display is redrawn.
    _checkInformationIcon: function() {
        let [child, x, y, mask] = Gdk.Screen.get_default().get_root_window().get_pointer();
        let actor = global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE,
                                                  x, y);
        if (actor != null) {
            let item = this._findDisplayedByActor(actor);
            if (item != null) {
               item.onDrawnUnderPointer();
            }
        }
        return false;
    },

    //// Pure virtual protected methods ////

    // Performs the steps needed to have the latest information about the items.
    // Implementation should return %true if we are up to date, and %false
    // if a full reload occurred.
    _refreshCache: function() {
        throw new Error("Not implemented");
    },

    // Sets the list of the displayed items based on the default sorting order.
    // The default sorting order is specific to each implementing class.
    _setDefaultList: function() {
        throw new Error("Not implemented");
    },

	// Compares items associated with the item ids based on the order in which the
	// items should be displayed.
	// Intended to be used as a compareFunction for array.sort().
	// Returns an integer value indicating the result of the comparison.
    _compareItems: function(itemIdA, itemIdB) {
        throw new Error("Not implemented");
    },

    // Checks if the item info can be a match for the search string. 
    // Returns a boolean flag indicating if that's the case.
    _isInfoMatching: function(itemInfo, search) {
        throw new Error("Not implemented");
    },

    // Creates a display item based on itemInfo.
    _createDisplayItem: function(itemInfo) {
        throw new Error("Not implemented");
    },

    //// Private methods ////

    _getItemSearchScore: function(itemId, terms) {
        let item = this._allItems[itemId];
        let score = 0;
        for (let i = 0; i < terms.length; i++) {
            let term = terms[i];
            if (this._isInfoMatching(item, term)) {
                score++;
            }
        }
        return score;
    },

    _getSearchMatchedItems: function(isSubSearch) {
        // Break the search up into terms, and search for each
        // individual term.  Keep track of the number of terms
        // each item matched.
        let terms = this._search.split(/\s+/);
        let matchScores = {};

        if (isSubSearch) {
            for (let i = 0; i < this._matchedItemKeys.length; i++) {
                let itemId = this._matchedItemKeys[i];
                let score = this._getItemSearchScore(itemId, terms);
                if (score > 0)
                    matchScores[itemId] = score;
            }
        } else {
            for (let itemId in this._displayedItems) {
                let score = this._getItemSearchScore(itemId, terms);
                if (score > 0)
                    matchScores[itemId] = score;
            }
        }
        return matchScores;
    },

    /*
     * Updates the display control to reflect the matched items set and the page selected.
     *
     * resetDisplayControl - indicates if the display control should be re-created because
     *                       the results or the space allocated for them changed. If it's false,
     *                       the existing display control is used and only the page links are
     *                       updated to reflect the current page selection.
     */
    _updateDisplayControl: function(resetDisplayControl) {
        if (resetDisplayControl) {
            this.displayControl.remove_all();
            let nPages = this._list.n_pages;
            // Don't show the page indicator if there is only one page.
            if (nPages == 1)
                return;
            let pageNumber = this._list.page;
            for (let i = 0; i < nPages; i++) {
                let pageControl = new Link.Link({ color: (i == pageNumber) ? DISPLAY_CONTROL_SELECTED_COLOR : ITEM_DISPLAY_DESCRIPTION_COLOR,
                                                  font_name: "Sans Bold 16px",
                                                  text: (i+1) + "",
                                                  reactive: (i == pageNumber) ? false : true});
                this.displayControl.append(pageControl.actor, Big.BoxPackFlags.NONE);

                // we use pageNumberLocalScope to get the page number right in the callback function
                let pageNumberLocalScope = i;
                pageControl.connect('clicked',
                                    Lang.bind(this,
                                              function(o, event) {
                                                  this.displayPage(pageNumberLocalScope);
                                              }));
            }
        } else {
            let pageControlActors = this.displayControl.get_children();
            for (let i = 0; i < pageControlActors.length; i++) {
                let pageControlActor = pageControlActors[i];
                if (i == this._list.page) {
                    pageControlActor.color =  DISPLAY_CONTROL_SELECTED_COLOR;
                    pageControlActor.reactive = false;
                } else {
                    pageControlActor.color =  ITEM_DISPLAY_DESCRIPTION_COLOR;
                    pageControlActor.reactive = true;
                }
            } 
        }
        if (this.hasSelected()) {
            this.selectFirstItem();
        }
    },

    // Returns a display item based on its index in the ordering of the
    // display children.
    _findDisplayedByIndex: function(index) {
        let actor = this._list.get_displayed_actor(index);
        return this._findDisplayedByActor(actor);
    },

    // Returns a display item based on the actor that represents it in 
    // the display.
    _findDisplayedByActor: function(actor) {
        for (itemId in this._displayedItems) {
            let item = this._displayedItems[itemId];
            if (item.actor == actor) {
                return item;
            }
        }
        return null;
    },

    // Selects (e.g. highlights) a display item at the provided index,
    // updates this.selectedItemDetails actor, and emits 'selected' signal.
    _selectIndex: function(index) {
        // Cleanup from the previous item
        if (this.hasSelected()) {
            this._findDisplayedByIndex(this._selectedIndex).markSelected(false);
        }

        this._selectedIndex = index;
        if (index < 0)
            return

        // Mark the new item as selected and create its details pane
        let item = this._findDisplayedByIndex(index);
        item.markSelected(true);
        this.emit('selected');
    }
};

Signals.addSignalMethods(GenericDisplay.prototype);
