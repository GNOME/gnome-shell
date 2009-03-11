/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Pango = imports.gi.Pango;
const Signals = imports.signals;
const Shell = imports.gi.Shell;
const Tidy = imports.gi.Tidy;

const DND = imports.ui.dnd;
const Link = imports.ui.link;

const ITEM_DISPLAY_NAME_COLOR = new Clutter.Color();
ITEM_DISPLAY_NAME_COLOR.from_pixel(0xffffffff);
const ITEM_DISPLAY_DESCRIPTION_COLOR = new Clutter.Color();
ITEM_DISPLAY_DESCRIPTION_COLOR.from_pixel(0xffffffbb);
const ITEM_DISPLAY_BACKGROUND_COLOR = new Clutter.Color();
ITEM_DISPLAY_BACKGROUND_COLOR.from_pixel(0x00000000);
const ITEM_DISPLAY_SELECTED_BACKGROUND_COLOR = new Clutter.Color();
ITEM_DISPLAY_SELECTED_BACKGROUND_COLOR.from_pixel(0x00ff0055);
const DISPLAY_CONTROL_SELECTED_COLOR = new Clutter.Color();
DISPLAY_CONTROL_SELECTED_COLOR.from_pixel(0x112288ff);

const ITEM_DISPLAY_HEIGHT = 50;
const ITEM_DISPLAY_ICON_SIZE = 48;
const ITEM_DISPLAY_PADDING = 1;
const DEFAULT_COLUMN_GAP = 6;
const LABEL_HEIGHT = 16;

/* This is a virtual class that represents a single display item containing
 * a name, a description, and an icon. It allows selecting an item and represents 
 * it by highlighting it with a different background color than the default.
 *
 * availableWidth - total width available for the item
 */
function GenericDisplayItem(availableWidth) {
    this._init(name, description, icon, availableWidth);
}

GenericDisplayItem.prototype = {
    _init: function(availableWidth) {
        this._availableWidth = availableWidth;

        this.actor = new Clutter.Group({ reactive: true,
                                         width: availableWidth,
                                         height: ITEM_DISPLAY_HEIGHT });
        this.actor._delegate = this;
        this.actor.connect('button-release-event',
                           Lang.bind(this,
                                     function(draggable, e) {
                                         this.activate();
                                     }));

        DND.makeDraggable(this.actor);
        this._bg = new Big.Box({ background_color: ITEM_DISPLAY_BACKGROUND_COLOR,
                                 corner_radius: 4,
                                 x: 0, y: 0,
                                 width: availableWidth, height: ITEM_DISPLAY_HEIGHT });
        this.actor.add_actor(this._bg);
        
        this._name = null;
        this._description = null;
        this._icon = null;

        this.dragActor = null;
    },

    //// Draggable object interface ////

    // Returns a cloned texture of the item's icon to represent the item as it 
    // is being dragged. 
    getDragActor: function(stageX, stageY) {
        this.dragActor = new Clutter.Clone({ source: this._icon });
        [this.dragActor.width, this.dragActor.height] = this._icon.get_transformed_size();

        // If the user dragged from the icon itself, then position
        // the dragActor over the original icon. Otherwise center it
        // around the pointer
        let [iconX, iconY] = this._icon.get_transformed_position();
        let [iconWidth, iconHeight] = this._icon.get_transformed_size();
        if (stageX > iconX && stageX <= iconX + iconWidth &&
            stageY > iconY && stageY <= iconY + iconHeight)
            this.dragActor.set_position(iconX, iconY);
        else
            this.dragActor.set_position(stageX - this.dragActor.width / 2, stageY - this.dragActor.height / 2);
        return this.dragActor;
    },
    
    // Returns the original icon that is being used as a source for the cloned texture
    // that represents the item as it is being dragged.
    getDragActorSource: function() {
        return this._icon;
    },

    //// Public methods ////

    // Highlights the item by setting a different background color than the default 
    // if isSelected is true, removes the highlighting otherwise.
    markSelected: function(isSelected) {
       let color;
       if (isSelected)
           color = ITEM_DISPLAY_SELECTED_BACKGROUND_COLOR;
       else
           color = ITEM_DISPLAY_BACKGROUND_COLOR;
       this._bg.background_color = color;
    },

    // Activates the item, as though it was clicked
    activate: function() {
        this.emit('activate');
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
     * iconActor - ClutterTexture containing the icon image which should be 48x48 pixels
     */
    _setItemInfo: function(nameText, descriptionText, iconActor) {
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

        this._icon = iconActor;
        this.actor.add_actor(this._icon);

        let text_width = this._availableWidth - (ITEM_DISPLAY_ICON_SIZE + 4);
        this._name = new Clutter.Text({ color: ITEM_DISPLAY_NAME_COLOR,
                                        font_name: "Sans 14px",
                                        width: text_width,
                                        ellipsize: Pango.EllipsizeMode.END,
                                        text: nameText,
                                        x: ITEM_DISPLAY_ICON_SIZE + 4,
                                        y: ITEM_DISPLAY_PADDING });
        this.actor.add_actor(this._name);
        this._description = new Clutter.Text({ color: ITEM_DISPLAY_DESCRIPTION_COLOR,
                                               font_name: "Sans 12px",
                                               width: text_width,
                                               ellipsize: Pango.EllipsizeMode.END,
                                               text: descriptionText ? descriptionText : "",
                                               x: this._name.x,
                                               y: this._name.height + 4 });
        this.actor.add_actor(this._description);
    }
};

Signals.addSignalMethods(GenericDisplayItem.prototype);

/* This is a virtual class that represents a display containing a collection of items
 * that can be filtered with a search string.
 *
 * width - width available for the display
 * height - height available for the display
 */
function GenericDisplay(width, height, numberOfColumns, columnGap) {
    this._init(width, height, numberOfColumns, columnGap);
}

GenericDisplay.prototype = {
    _init : function(width, height, numberOfColumns, columnGap) {
        this._search = '';
        this._width = null;
        this._height = null;
        this._columnWidth = null;

        this._numberOfColumns = numberOfColumns;
        this._columnGap = columnGap;
        if (this._columnGap == null)
            this._columnGap = DEFAULT_COLUMN_GAP;

        this._maxItemsPerPage = null;
        this._setDimensionsAndMaxItems(width, height);
        this._grid = new Tidy.Grid({width: this._width, height: this._height});
        this._grid.column_major = true;
        this._grid.column_gap = this._columnGap;
        // map<itemId, Object> where Object represents the item info
        this._allItems = {}; 
        // an array of itemIds of items that match the current request 
        // in the order in which the items should be displayed
        this._matchedItems = [];
        // map<itemId, GenericDisplayItem>
        this._displayedItems = {};  
        this._displayedItemsCount = 0;
        this._pageDisplayed = 0;
        // GenericDisplayItem
        this._activatedItem = null;
        this._selectedIndex = -1;
        this._keepDisplayCurrent = false;
        this.actor = this._grid;
        this.displayControl = new Big.Box({ background_color: ITEM_DISPLAY_BACKGROUND_COLOR,
                                            corner_radius: 4,
                                            height: 24,
                                            spacing: 12,
                                            orientation: Big.BoxOrientation.HORIZONTAL});
    },

    //// Public methods ////

    // Sets the search string and displays the matching items.
    setSearch: function(text) {
        this._search = text.toLowerCase();
        this._redisplay(true);
    },

    // Sets this._activatedItem to the item that is selected and emits 'activated' signal.
    // The reason we don't call launch() on the activated item right away is because we want
    // the class that contains the display to do all other necessary actions and then call
    // doActivate(). Currently, when a selected item is activated we only clear the search 
    // entry, but when an item that was not selected is clicked, we want to move the selection
    // to the clicked item first. This needs to happen in the class that contains the display
    // because the selection might be moved from some other display that class contains.
    activateSelected: function() {
        if (this._selectedIndex != -1) {
            let selected = this._findDisplayedByIndex(this._selectedIndex);
            this._activatedItem = selected;
            this.emit('activated');
        }
    },

    // Moves the selection one up. If the selection was already on the top item, it's moved
    // to the bottom one. Returns true if the selection actually moved up, false if it wrapped 
    // around to the bottom.
    selectUp: function() {
        let selectedUp = true;
        let prev = this._selectedIndex - 1;
        if (this._selectedIndex <= 0) {
            prev = this._displayedItemsCount - 1; 
            selectedUp = false; 
        }
        this._selectIndex(prev);
        return selectedUp;
    },

    // Moves the selection one down. If the selection was already on the bottom item, it's moved
    // to the top one. Returns true if the selection actually moved down, false if it wrapped 
    // around to the top.
    selectDown: function() {
        let selectedDown = true;
        let next = this._selectedIndex + 1;
        if (this._selectedIndex == this._displayedItemsCount - 1) {
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
        if (this.hasItems())
            this._selectIndex(this._displayedItemsCount - 1);
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
        return this._displayedItemsCount > 0;
    },

    // Highlights the activated item and launches it.
    doActivate: function() {
        if (this._activatedItem != null) {
            // We update the selection, so that in case an item was selected by clicking on it and 
            // it is different from an item that was previously selected, we can highlight the new selection.
            this._selectIndex(this._getIndexOfDisplayedActor(this._activatedItem.actor));
            this._activatedItem.launch();    
        } 
    },

    // Readjusts display layout and the items displayed based on the new dimensions.
    updateDimensions: function(width, height, numberOfColumns) {
        this._numberOfColumns = numberOfColumns;
        this._setDimensionsAndMaxItems(width, height);
        this._grid.width = this._width;
        this._grid.height = this._height;
        this._pageDisplayed = 0;
        this._displayMatchedItems(true);
    },

    // Updates the displayed items and makes the display actor visible.
    show: function() {
        this._keepDisplayCurrent = true;
        this._redisplay(true);
        this._grid.show();
    },

    // Hides the display actor.
    hide: function() {
        this._grid.hide();
        this._removeAllDisplayItems();
        this._keepDisplayCurrent = false;
    },

    //// Protected methods ////

    /*
     * Displays items that match the current request and should show up on the current page.
     * Updates the display control to reflect the matched items set and the page selected.
     *
     * resetDisplayControl - indicates if the display control should be re-created because 
     *                       the results or the space allocated for them changed. If it's false,
     *                       the existing display control is used and only the page links are
     *                       updated to reflect the current page selection.
     */
    _displayMatchedItems: function(resetDisplayControl) {
        // When generating a new list to display, we first remove all the old
        // displayed items which will unset the selection. So we need 
        // to keep a flag which indicates if this display had the selection.
        let hadSelected = this.hasSelected();

        this._removeAllDisplayItems();

        for (let i = this._maxItemsPerPage * this._pageDisplayed; i < this._matchedItems.length && i < this._maxItemsPerPage * (this._pageDisplayed + 1); i++) {
            
            this._addDisplayItem(this._matchedItems[i]);
        }

        if (hadSelected) {
            this._selectedIndex = -1;
            this.selectFirstItem();
        }

        this._updateDisplayControl(resetDisplayControl);
    },

    // Creates a display item based on the information associated with itemId 
    // and adds it to the displayed items.
    _addDisplayItem : function(itemId) {
        if (this._displayedItems.hasOwnProperty(itemId)) {
            log("Tried adding a display item for " + itemId + ", but an item with this item id is already among displayed items.");
            return;
        }

        let me = this;

        let itemInfo = this._allItems[itemId];
        let displayItem = this._createDisplayItem(itemInfo);

        displayItem.connect('activate', function() {
            me._activatedItem = displayItem;
            me.emit('activated');
        });
        this._grid.add_actor(displayItem.actor);
        this._displayedItems[itemId] = displayItem;
        this._displayedItemsCount++;
    },

    // Removes an item identifed by the itemId from the displayed items.
    _removeDisplayItem: function(itemId) {
        let displayItem = this._displayedItems[itemId];
        let displayItemIndex = this._getIndexOfDisplayedActor(displayItem.actor);

        if (this.hasSelected() && this._displayedItemsCount == 1) {
            this.unsetSelected();
        } else if (this.hasSelected() && displayItemIndex < this._selectedIndex) {
            this.selectUp();
        } 

        if (displayItem.dragActor) {
            // The user might be handling a dragActor when the list of items 
            // changes (for example, if the dragging caused us to transition
            // from an expanded overlay view to the regular view). So we need
            // to keep the item around so that the drag and drop action initiated
            // by the user can be completed. However, we remove the item from the list.
            // 
            // For some reason, just removing the displayItem.actor
            // is not enough to get displayItem._icon.visible
            // to return false, so we hide the display item and
            // all its children first. (We check displayItem._icon.visible
            // when deciding if a dragActor has a place to snap back to
            // in case the drop was not accepted by any actor.)
            displayItem.actor.hide_all();
            this._grid.remove_actor(displayItem.actor);
            // We should not destroy the actor up-front, because that would also
            // destroy the icon that was used to clone the image for the drag actor.
            // We destroy it once the dragActor is destroyed instead.             
            displayItem.dragActor.connect('destroy',
                                          function(item) {
                                              displayItem.actor.destroy();
                                          });
           
        } else {
            displayItem.actor.destroy();
        }
        delete this._displayedItems[itemId];
        this._displayedItemsCount--;        
    },

    // Removes all displayed items.
    _removeAllDisplayItems: function() {
        for (itemId in this._displayedItems)
            this._removeDisplayItem(itemId);
     },

    /*
     * Updates the displayed items, applying the search string if one exists.
     *
     * resetPage - indicates if the page selection should be reset when displaying the matching results.
     *             We reset the page selection when the change in results was initiated by the user by  
     *             entering a different search criteria or by viewing the results list in a different
     *             size mode, but we keep the page selection the same if the results got updated on 
     *             their own while the user was browsing through the result pages.
     */
    _redisplay: function(resetPage) {
        if (!this._keepDisplayCurrent)
            return;

        this._refreshCache();
        if (!this._search)
            this._setDefaultList();
        else
            this._doSearchFilter();

        if (resetPage)
            this._pageDisplayed = 0;

        this._displayMatchedItems(true);

        this.emit('redisplayed');
    },

    //// Pure virtual protected methods ////
 
    // Performs the steps needed to have the latest information about the items.
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

    // Sets this._width, this._height, this._columnWidth, and this._maxItemsPerPage based on the 
    // space available for the display, number of columns, and the number of items it can fit.
    _setDimensionsAndMaxItems: function(width, height) {
        this._width = width; 
        this._columnWidth = (this._width - this._columnGap * (this._numberOfColumns - 1)) / this._numberOfColumns; 
        let maxItemsInColumn = Math.floor(height / ITEM_DISPLAY_HEIGHT);
        this._maxItemsPerPage =  maxItemsInColumn * this._numberOfColumns;
        this._height = maxItemsInColumn * ITEM_DISPLAY_HEIGHT;
    },

    // Applies the search string to the list of items to find matches, and displays the matching items.
    _doSearchFilter: function() {
        let matchedItemsForSearch = {};

        // Break the search up into terms, and search for each
        // individual term.  Keep track of the number of terms
        // each item matched.
        let terms = this._search.split(/\s+/);
        for (let i = 0; i < terms.length; i++) {
            let term = terms[i];
            for (itemId in this._allItems) {
                let item = this._allItems[itemId];
                if (this._isInfoMatching(item, term)) {
                    let count = matchedItemsForSearch[itemId];
                    if (!count)
                        count = 0;
                    count += 1;
                    matchedItemsForSearch[itemId] = count;
                }
            }
        }

        this._matchedItems = [];
        for (itemId in matchedItemsForSearch) {
            this._matchedItems.push(itemId);
        }
        this._matchedItems.sort(Lang.bind(this, function (a, b) {
            let countA = matchedItemsForSearch[a];
            let countB = matchedItemsForSearch[b];
            if (countA > countB)
                return -1;
            else if (countA < countB)
                return 1;
            else
                return this._compareItems(a, b);
        }));        
    },

    // Displays the page specified by the pageNumber argument. The pageNumber is 0-based.
    _displayPage: function(pageNumber) {
        this._pageDisplayed = pageNumber;
        this._displayMatchedItems(false);
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
            let pageNumber = 0;
            for (let i = 0; i < this._matchedItems.length; i = i + this._maxItemsPerPage) {
                let pageControl = new Link.Link({ color: (pageNumber == this._pageDisplayed) ? DISPLAY_CONTROL_SELECTED_COLOR : ITEM_DISPLAY_DESCRIPTION_COLOR,
                                                  font_name: "Sans Bold 16px",
                                                  text: (pageNumber + 1) + "",
                                                  height: LABEL_HEIGHT,
                                                  reactive: (pageNumber == this._pageDisplayed) ? false : true});
                this.displayControl.append(pageControl.actor, Big.BoxPackFlags.NONE);

                // we use pageNumberLocalScope to get the page number right in the callback function
                let pageNumberLocalScope = pageNumber;         
                pageControl.connect('clicked',
                                    Lang.bind(this,
                                              function(o, event) {
                                                  this._displayPage(pageNumberLocalScope);
                                              }));
                pageNumber ++; 
            }
        } else {
            let pageControlActors = this.displayControl.get_children();
            for (let i = 0; i < pageControlActors.length; i++) { 
                let pageControlActor = pageControlActors[i];
                if (i == this._pageDisplayed) {
                    pageControlActor.color =  DISPLAY_CONTROL_SELECTED_COLOR;
                    pageControlActor.reactive = false;
                } else {
                    pageControlActor.color =  ITEM_DISPLAY_DESCRIPTION_COLOR;
                    pageControlActor.reactive = true;
                }
            } 
        }
    },

    // Returns a display item based on its index in the ordering of the 
    // display children.
    _findDisplayedByIndex: function(index) {
        let displayedActors = this._grid.get_children();
        let actor = displayedActors[index];
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

    // Returns and index that the actor has in the ordering of the display's
    // children.
    _getIndexOfDisplayedActor: function(actor) {
        let children = this._grid.get_children();
        for (let i = 0; i < children.length; i++) {
            if (children[i] == actor) 
                return i;
        }
        return -1;
    },

    // Selects (e.g. highlights) a display item at the provided index.
    _selectIndex: function(index) {
        if (this._selectedIndex != -1) {
            let prev = this._findDisplayedByIndex(this._selectedIndex);
            prev.markSelected(false);
        }
        this._selectedIndex = index;
        if (index != -1 && index < this._displayedItemsCount) {
            let item = this._findDisplayedByIndex(index);
            item.markSelected(true);
        }
    }
};

Signals.addSignalMethods(GenericDisplay.prototype);
