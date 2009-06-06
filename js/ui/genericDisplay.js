/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gdk = imports.gi.Gdk;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
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
const PREVIEW_BOX_BACKGROUND_COLOR = new Clutter.Color();
PREVIEW_BOX_BACKGROUND_COLOR.from_pixel(0xADADADf0);

const ITEM_DISPLAY_HEIGHT = 50;
const ITEM_DISPLAY_ICON_SIZE = 48;
const ITEM_DISPLAY_PADDING = 1;
const DEFAULT_COLUMN_GAP = 6;
const LABEL_HEIGHT = 16;

const PREVIEW_ICON_SIZE = 96;
const PREVIEW_BOX_PADDING = 6;
const PREVIEW_BOX_SPACING = 4;
const PREVIEW_BOX_CORNER_RADIUS = 10; 
// how far relative to the full item width the preview box should be placed
const PREVIEW_PLACING = 3/4;
const PREVIEW_DETAILS_MIN_WIDTH = PREVIEW_ICON_SIZE * 2;

/* This is a virtual class that represents a single display item containing
 * a name, a description, and an icon. It allows selecting an item and represents 
 * it by highlighting it with a different background color than the default.
 *
 * availableWidth - total width available for the item
 */
function GenericDisplayItem(availableWidth) {
    this._init(availableWidth);
}

GenericDisplayItem.prototype = {
    _init: function(availableWidth) {
        this._availableWidth = availableWidth;
        this._showPreview = false;
        this._havePointer = false; 
        this._previewEventSourceId = null;

        this.actor = new Clutter.Group({ reactive: true,
                                         width: availableWidth,
                                         height: ITEM_DISPLAY_HEIGHT });
        this.actor._delegate = this;
        this.actor.connect('button-press-event',
                           Lang.bind(this,
                                     function(actor, e) {
                                         let clickCount = Shell.get_button_event_click_count(e);
                                         if (clickCount == 1)
                                             this.select();
                                         else if (clickCount == 2)
                                             this.activate();
                                     }));

        let draggable = DND.makeDraggable(this.actor);
        draggable.connect('drag-begin', Lang.bind(this, this._onDragBegin));

        this._bg = new Big.Box({ background_color: ITEM_DISPLAY_BACKGROUND_COLOR,
                                 corner_radius: 4,
                                 x: 0, y: 0,
                                 width: availableWidth, height: ITEM_DISPLAY_HEIGHT });
        this.actor.add_actor(this._bg);
        
        this._name = null;
        this._description = null;
        this._icon = null;
        this._preview = null;
        this._previewIcon = null; 

        this.dragActor = null;

        this.actor.connect('enter-event', Lang.bind(this, this._onEnter));
        this.actor.connect('leave-event', Lang.bind(this, this._onLeave));
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

    // Sets a boolean value that indicates whether the item should display a pop-up preview on mouse over.
    setShowPreview: function(showPreview) {
        this._showPreview = showPreview;
    },

    // Returns a boolean value that indicates whether the item displays a pop-up preview on mouse over.
    getShowPreview: function() {
        return this._showPreview;
    },

    // Displays the preview for the item.
    showPreview: function() {
        if(!this._showPreview)
            return;

        this._ensurePreviewCreated();

        let [x, y] = this.actor.get_transformed_position();
        let global = Shell.Global.get();
        let previewX = Math.min(x + this._availableWidth * PREVIEW_PLACING, global.screen_width - this._preview.width);
        let previewY = Math.min(y, global.screen_height - this._preview.height);
        this._preview.set_position(previewX, previewY);

        this._preview.show();
    },

    // Hides the preview for the item and removes the preview event source so that 
    // there is no preview scheduled to show up.
    hidePreview: function() {
        if (this._previewEventSourceId) {
            Mainloop.source_remove(this._previewEventSourceId);
            this._previewEventSourceId = null;
        }

        if (this._preview)
            this._preview.hide();
    },

    // Shows a preview when the item was drawn under the mouse pointer.
    onDrawnUnderPointer: function() {
        this._havePointer = true;
        // This code is usually triggered when we just had a different preview showing on the same spot
        // and having a delay before showing a new preview looks bad. So we just show it right away.
        this.showPreview();  
    },

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

    // Activates the item, as though it was launched
    activate: function() {
        this.hidePreview();
        this.emit('activate');
    },

    // Selects the item, as though it was clicked
    select: function() {
        this.emit('select');
    },

    /*
     * Returns an actor containing item details. In the future details can have more information than what 
     * the preview pop-up has and be item-type specific.
     *
     * availableWidth - width available for displaying details
     * availableHeight - height available for displaying details
     */ 
    createDetailsActor: function(availableWidth, availableHeight) {

        let details = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                    spacing: PREVIEW_BOX_SPACING,
                                    width: availableWidth });

        let mainDetails = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                        spacing: PREVIEW_BOX_SPACING,
                                        width: availableWidth });

        // Inner box with name and description
        let textDetails = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                        spacing: PREVIEW_BOX_SPACING });
        let detailsName = new Clutter.Text({ color: ITEM_DISPLAY_NAME_COLOR,
                                             font_name: "Sans bold 14px",
                                             line_wrap: true,
                                             text: this._name.text});
        textDetails.append(detailsName, Big.BoxPackFlags.NONE);

        let detailsDescription = new Clutter.Text({ color: ITEM_DISPLAY_NAME_COLOR,
                                                    font_name: "Sans 14px",
                                                    line_wrap: true,
                                                    text: this._description.text });
        textDetails.append(detailsDescription, Big.BoxPackFlags.NONE);

        mainDetails.append(textDetails, Big.BoxPackFlags.EXPAND);

        this._ensurePreviewIconCreated();
        let largePreviewIcon = this._createLargePreviewIcon(availableWidth, Math.max(0, availableHeight - mainDetails.height - PREVIEW_BOX_SPACING));

        if (this._previewIcon != null && largePreviewIcon == null) {
            let previewIconClone = new Clutter.Clone({ source: this._previewIcon });
            mainDetails.prepend(previewIconClone, Big.BoxPackFlags.NONE);
        }

        details.append(mainDetails, Big.BoxPackFlags.NONE);

        if (largePreviewIcon != null) {
            let largePreview = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL });
            largePreview.append(largePreviewIcon, Big.BoxPackFlags.NONE);
            details.append(largePreview, Big.BoxPackFlags.NONE);
        }
   
        // We hide the preview pop-up if the details are shown elsewhere. 
        details.connect("show", 
                        Lang.bind(this, 
                                  function() {
                                          // Right now "show" signal is emitted when an actor is added to a parent that
                                          // has not been added to anything and "visible" property is also set to true 
                                          // at this point, so checking if the parent that the actor has been added to  
                                          // has a parent of its own is a temporary workaround. That other actor is 
                                          // presumed to be displayed, which is a limitation of this workaround, but is
                                          // the case with our usage of the details actor now.         
                                          // http://bugzilla.openedhand.com/show_bug.cgi?id=1138
                                          if (details.get_parent() != null && details.get_parent().get_parent() != null)
                                              this.hidePreview();
                                  }));
        return details;
    },

    // Destoys the item, as well as a preview for the item if it exists.
    destroy: function() {
      this.actor.destroy();
      if (this._preview != null)
          this._preview.destroy();
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
     * iconActor - ClutterTexture containing the icon image which should be ITEM_DISPLAY_ICON_SIZE size
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
        // This ensures we'll create a new preview and previewIcon next time we need a preview
        if (this._preview != null) {
            this._preview.destroy();
            this._preview = null;
        }
        if (this._previewIcon != null) {
            this._previewIcon.destroy();
            this._previewIcon = null;
        }

        this._icon = iconActor;
        this.actor.add_actor(this._icon);

        let textWidth = this._availableWidth - (ITEM_DISPLAY_ICON_SIZE + 4);
        this._name = new Clutter.Text({ color: ITEM_DISPLAY_NAME_COLOR,
                                        font_name: "Sans 14px",
                                        width: textWidth,
                                        ellipsize: Pango.EllipsizeMode.END,
                                        text: nameText,
                                        x: ITEM_DISPLAY_ICON_SIZE + 4,
                                        y: ITEM_DISPLAY_PADDING });
        this.actor.add_actor(this._name);
        this._description = new Clutter.Text({ color: ITEM_DISPLAY_DESCRIPTION_COLOR,
                                               font_name: "Sans 12px",
                                               width: textWidth,
                                               ellipsize: Pango.EllipsizeMode.END,
                                               text: descriptionText ? descriptionText : "",
                                               x: this._name.x,
                                               y: this._name.height + 4 });
        this.actor.add_actor(this._description);
    },

    //// Virtual protected methods ////

    // Creates and returns a large preview icon, but only if we have a detailed image.
    _createLargePreviewIcon : function(availableWidth, availableHeight) {
        return null;
    },

    //// Pure virtual protected methods ////

    // Ensures the preview icon is created.
    _ensurePreviewIconCreated: function() {
        throw new Error("Not implemented");
    },

    //// Private methods ////

    // Ensures the preview actor is created.
    _ensurePreviewCreated: function() {
        if (!this._showPreview || this._preview)
            return;

        this._preview = new Big.Box({ background_color: PREVIEW_BOX_BACKGROUND_COLOR,
                                      orientation: Big.BoxOrientation.HORIZONTAL,
                                      corner_radius: PREVIEW_BOX_CORNER_RADIUS,
                                      padding: PREVIEW_BOX_PADDING,
                                      spacing: PREVIEW_BOX_SPACING });

        let textDetailsWidth = this._availableWidth - PREVIEW_BOX_PADDING * 2;

        this._ensurePreviewIconCreated();

        if (this._previewIcon != null) {
            this._preview.append(this._previewIcon, Big.BoxPackFlags.EXPAND);
            textDetailsWidth = this._availableWidth - this._previewIcon.width - PREVIEW_BOX_PADDING * 2 - PREVIEW_BOX_SPACING;
        }

	// Inner box with name and description
        let textDetails = new Big.Box({ orientation: Big.BoxOrientation.VERTICAL,
                                        spacing: PREVIEW_BOX_SPACING });
        let detailsName = new Clutter.Text({ color: ITEM_DISPLAY_NAME_COLOR,
                                             font_name: "Sans bold 14px",
                                             text: this._name.text});

        textDetails.width = Math.max(PREVIEW_DETAILS_MIN_WIDTH, textDetailsWidth, detailsName.width);

        textDetails.append(detailsName, Big.BoxPackFlags.NONE);

        let detailsDescription = new Clutter.Text({ color: ITEM_DISPLAY_NAME_COLOR,
                                                    font_name: "Sans 14px",
                                                    line_wrap: true,
                                                    text: this._description.text });
        textDetails.append(detailsDescription, Big.BoxPackFlags.NONE);

        this._preview.append(textDetails, Big.BoxPackFlags.EXPAND);

        // Add the preview to global stage to allow for top-level layering
        let global = Shell.Global.get();
        global.stage.add_actor(this._preview);
        this._preview.hide();
    },

    // Performs actions on mouse enter event for the item. Currently, shows the preview for the item.
    _onEnter: function(actor, event) {
        this._havePointer = true;
        let tooltipTimeout = Gtk.Settings.get_default().gtk_tooltip_timeout;
        this._previewEventSourceId = Mainloop.timeout_add(tooltipTimeout, 
                                                          Lang.bind(this,
                                                                    function() {
                                                                        if (this._havePointer) {
                                                                            this.showPreview();
                                                                        }
                                                                        this._previewEventSourceId = null;
                                                                        return false;
                                                                    }));
    },

    // Performs actions on mouse leave event for the item. Currently, hides the preview for the item.
    _onLeave: function(actor, event) {
        this._havePointer = false;
        this.hidePreview();
    },

    // Hides the preview once the item starts being dragged.
    _onDragBegin : function (draggable, time) {
        // For some reason, we are not getting leave-event signal when we are dragging an item,
        // so the preview box stays behind if we didn't have the call here. It makes sense to hide  
        // the preview as soon as the item starts being dragged anyway.
        this._havePointer = false;  
        this.hidePreview();
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
        this._expanded = false;
        this._width = null;
        this._height = null;
        this._columnWidth = null;

        this._numberOfColumns = numberOfColumns;
        this._columnGap = columnGap;
        if (this._columnGap == null)
            this._columnGap = DEFAULT_COLUMN_GAP;

        this._maxItemsPerPage = null;
        this._grid = new Tidy.Grid({width: this._width, height: this._height});

        this._setDimensionsAndMaxItems(width, 0, height);

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
        this._selectedIndex = -1;
        // These two are public - .actor is the normal "actor subclass" property,
        // but we also expose a .displayControl actor which is separate.
        // See also getSideArea.
        this.actor = this._grid;
        this.displayControl = new Big.Box({ background_color: ITEM_DISPLAY_BACKGROUND_COLOR,
                                            corner_radius: 4,
                                            height: 24,
                                            spacing: 12,
                                            orientation: Big.BoxOrientation.HORIZONTAL});

        this._availableWidthForItemDetails = this._columnWidth;
        this._availableHeightForItemDetails = this._height;
        this.selectedItemDetails = new Big.Box({});     
    },

    //// Public methods ////

    // Sets dimensions available for the item details display.
    setAvailableDimensionsForItemDetails: function(availableWidth, availableHeight) {
        this._availableWidthForItemDetails = availableWidth;
        this._availableHeightForItemDetails = availableHeight;
    },

    // Returns dimensions available for the item details display.
    getAvailableDimensionsForItemDetails: function() {
        return [this._availableWidthForItemDetails, this._availableHeightForItemDetails];
    },

    // Sets the search string and displays the matching items.
    setSearch: function(text) {
        this._search = text.toLowerCase();
        this._redisplay(true);
    },

    // Launches the item that is currently selected and emits 'activated' signal.
    activateSelected: function() {
        if (this._selectedIndex != -1) {
            let selected = this._findDisplayedByIndex(this._selectedIndex);
            selected.launch()
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

    // Hides the preview if any item has one being displayed.
    hidePreview: function() {
        for (itemId in this._displayedItems) {
            let item = this._displayedItems[itemId];
            item.hidePreview();
        }
    },

    // Returns true if the display has any displayed items.
    hasItems: function() {
        return this._displayedItemsCount > 0;
    },

    // Readjusts display layout and the items displayed based on the new dimensions.
    setExpanded: function(expanded, baseWidth, expandWidth, height, numberOfColumns) {
        this._expanded = expanded;
        this._numberOfColumns = numberOfColumns;
        this._setDimensionsAndMaxItems(baseWidth, expandWidth, height);
        this._grid.width = this._width;
        this._grid.height = this._height;
        this._pageDisplayed = 0;
        this._displayMatchedItems(true);
        let gridWidth = this._width;
        let sideArea = this.getSideArea();
        if (sideArea) {
            if (expanded)
                sideArea.show();
            else
                sideArea.hide();
        }
        this.emit('expanded');
    },

    // Updates the displayed items and makes the display actor visible.
    show: function() {
        this._grid.show();
        this._redisplay(true);
    },

    // Hides the display actor.
    hide: function() {
        this._grid.hide();
        this._filterReset();
        this._removeAllDisplayItems();
    },

    // Returns an actor which acts as a sidebar; this is used for
    // the applications category view
    getSideArea: function () {
        return null;
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

        // We currently redisplay matching items and raise the sideshow as part of two different callbacks.
        // Checking what is under the pointer after a timeout allows us to not merge these callbacks into one, at least for now.  
        Mainloop.timeout_add(5, 
                             Lang.bind(this,
                                       function() {
                                           // Check if the pointer is over one of the items and display the preview pop-up if it is.
                                           let [child, x, y, mask] = Gdk.Screen.get_default().get_root_window().get_pointer();
                                           let global = Shell.Global.get();
                                           let actor = global.stage.get_actor_at_pos(Clutter.PickMode.REACTIVE,
                                                                                     x, y);
                                           if (actor != null) {
                                               let item = this._findDisplayedByActor(actor.get_parent());
                                               if (item != null) {
                                                   item.onDrawnUnderPointer();
                                               }
                                           }
                                           return false;
                                       }));
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
        displayItem.setShowPreview(true);

        displayItem.connect('activate', 
                            Lang.bind(this,
                                      function() {
                                          // update the selection
                                          this._selectIndex(this._getIndexOfDisplayedActor(displayItem.actor));
                                          this.activateSelected();
                                      }));

        displayItem.connect('select', 
                            Lang.bind(this,
                                      function() {
                                          // update the selection
                                          this._selectIndex(this._getIndexOfDisplayedActor(displayItem.actor));
                                      }));
        this._grid.add_actor(displayItem.actor);
        this._displayedItems[itemId] = displayItem;
        this._displayedItemsCount++;
    },

    // Removes an item identifed by the itemId from the displayed items.
    _removeDisplayItem: function(itemId) {
        let displayItem = this._displayedItems[itemId];
        let displayItemIndex = this._getIndexOfDisplayedActor(displayItem.actor);

        if (this.hasSelected() && (this._displayedItemsCount == 1 || !this._grid.visible)) { 
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
            // We should not destroy the item up-front, because that would also
            // destroy the icon that was used to clone the image for the drag actor.
            // We destroy it once the dragActor is destroyed instead.             
            displayItem.dragActor.connect('destroy',
                                          function(item) {
                                              displayItem.destroy();
                                          });
           
        } else {
            displayItem.destroy();
        }
        delete this._displayedItems[itemId];
        this._displayedItemsCount--;        
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
        if (!this._grid.visible)
            return;
        
        this._refreshCache();
        if (!this._filterActive())
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
    _setDimensionsAndMaxItems: function(baseWidth, expandWidth, height) {
        this._width = baseWidth + expandWidth;
        let gridWidth;
        let sideArea = this.getSideArea();
        if (this._expanded && sideArea) {
            gridWidth = expandWidth;
            sideArea.width = baseWidth;
            sideArea.height = this._height;
        } else {
            gridWidth = this._width;
        }
        this._columnWidth = (gridWidth - this._columnGap * (this._numberOfColumns - 1)) / this._numberOfColumns;
        let maxItemsInColumn = Math.floor(height / ITEM_DISPLAY_HEIGHT);
        this._maxItemsPerPage =  maxItemsInColumn * this._numberOfColumns;
        this._height = maxItemsInColumn * ITEM_DISPLAY_HEIGHT;
        this._grid.width = gridWidth;
        this._grid.height = this._height;
    },

    _getSearchMatchedItems: function() {
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
        return matchedItemsForSearch;
    },

    // Applies the search string to the list of items to find matches,
    // and displays the matching items.
    _doSearchFilter: function() {
        let matchedItemsForSearch;

        if (this._filterActive()) {
            matchedItemsForSearch = this._getSearchMatchedItems();
        } else {
            matchedItemsForSearch = {};
            for (let itemId in this._allItems) {
                matchedItemsForSearch[itemId] = 1;
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

    // Selects (e.g. highlights) a display item at the provided index,
    // updates this.selectedItemDetails actor, and emits 'selected' signal.
    _selectIndex: function(index) {
        if (this._selectedIndex != -1) {
            let prev = this._findDisplayedByIndex(this._selectedIndex);
            prev.markSelected(false);
            // Calling destroy() gets large image previews released as quickly as
            // possible, if we just removed them, they might hang around for a while
            // until the actor was garbage collected.
            let children = this.selectedItemDetails.get_children();
            for (let i = 0; i < children.length; i++)
                children[i].destroy();
    
            this.selectedItemDetails.remove_all();
        }
        this._selectedIndex = index;
        if (index != -1 && index < this._displayedItemsCount) {
            let item = this._findDisplayedByIndex(index);
            item.markSelected(true);
            this.selectedItemDetails.append(item.createDetailsActor(this._availableWidthForItemDetails, this._availableHeightForItemDetails), Big.BoxPackFlags.NONE);  
            this.emit('selected'); 
        }
    }
};

Signals.addSignalMethods(GenericDisplay.prototype);
