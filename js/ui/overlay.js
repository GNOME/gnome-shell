/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const AppDisplay = imports.ui.appDisplay;
const DocDisplay = imports.ui.docDisplay;
const GenericDisplay = imports.ui.genericDisplay;
const Link = imports.ui.link;
const Main = imports.ui.main;
const Panel = imports.ui.panel;
const Tweener = imports.ui.tweener;
const Workspaces = imports.ui.workspaces;

const OVERLAY_BACKGROUND_COLOR = new Clutter.Color();
OVERLAY_BACKGROUND_COLOR.from_pixel(0x000000ff);

const LABEL_HEIGHT = 16;
// We use SIDESHOW_PAD for the padding on the left side of the sideshow and as a gap
// between sideshow columns.
const SIDESHOW_PAD = 6;
const SIDESHOW_MIN_WIDTH = 250;
const SIDESHOW_SECTION_PADDING_TOP = 6;
const SIDESHOW_SECTION_SPACING = 6;
const SIDESHOW_COLUMNS = 1;
const DETAILS_CORNER_RADIUS = 5;
const DETAILS_BORDER_WIDTH = 1;
const DETAILS_PADDING = 6;
// This is the height of section components other than the item display.
const SIDESHOW_SECTION_MISC_HEIGHT = (LABEL_HEIGHT + SIDESHOW_SECTION_SPACING) * 2 + SIDESHOW_SECTION_PADDING_TOP;
const SIDESHOW_SEARCH_BG_COLOR = new Clutter.Color();
SIDESHOW_SEARCH_BG_COLOR.from_pixel(0xffffffff);
const SIDESHOW_TEXT_COLOR = new Clutter.Color();
SIDESHOW_TEXT_COLOR.from_pixel(0xffffffff);
const DETAILS_BORDER_COLOR = new Clutter.Color();
DETAILS_BORDER_COLOR.from_pixel(0xffffffff);

// Time for initial animation going into overlay mode
const ANIMATION_TIME = 0.5;

// We divide the screen into a grid of rows and columns, which we use
// to help us position the overlay components, such as the side panel
// that lists applications and documents, the workspaces display, and 
// the button for adding additional workspaces.
// In the regular mode, the side panel takes up one column on the left,
// and the workspaces display takes up the remaining columns.
// In the expanded side panel display mode, the side panel takes up two
// columns, and the workspaces display slides all the way to the right,
// being visible only in the last quarter of the right-most column.
// In the future, this mode will have more components, such as a display 
// of documents which were recently opened with a given application, which 
// will take up the remaining sections of the display.

const WIDE_SCREEN_CUT_OFF_RATIO = 1.4;

const COLUMNS_REGULAR_SCREEN = 4;
const ROWS_REGULAR_SCREEN = 8;
const COLUMNS_WIDE_SCREEN = 5;
const ROWS_WIDE_SCREEN = 10;

// Padding around workspace grid / Spacing between Sideshow and Workspaces
const WORKSPACE_GRID_PADDING = 12;

const COLUMNS_FOR_WORKSPACES_REGULAR_SCREEN = 3;
const ROWS_FOR_WORKSPACES_REGULAR_SCREEN = 6;
const WORKSPACES_X_FACTOR_ASIDE_MODE_REGULAR_SCREEN = 4 - 0.25;
const EXPANDED_SIDESHOW_COLUMNS_REGULAR_SCREEN = 2;

const COLUMNS_FOR_WORKSPACES_WIDE_SCREEN = 4;
const ROWS_FOR_WORKSPACES_WIDE_SCREEN = 8;
const WORKSPACES_X_FACTOR_ASIDE_MODE_WIDE_SCREEN = 5 - 0.25;
const EXPANDED_SIDESHOW_COLUMNS_WIDE_SCREEN = 3;

// A multi-state; PENDING is used during animations
const STATE_ACTIVE = true;
const STATE_PENDING_INACTIVE = false;
const STATE_INACTIVE = false;

let wideScreen = false;
let displayGridColumnWidth = null;
let displayGridRowHeight = null;

function Sideshow() {
    this._init();
}

Sideshow.prototype = {
    _init : function() {
        let me = this;

        this._moreAppsMode = STATE_INACTIVE;
        this._moreDocsMode = STATE_INACTIVE;

        let asideXFactor = wideScreen ? WORKSPACES_X_FACTOR_ASIDE_MODE_WIDE_SCREEN : WORKSPACES_X_FACTOR_ASIDE_MODE_REGULAR_SCREEN; 
        this._expandedSideshowColumns = wideScreen ? EXPANDED_SIDESHOW_COLUMNS_WIDE_SCREEN : EXPANDED_SIDESHOW_COLUMNS_REGULAR_SCREEN;       

        this._width = displayGridColumnWidth - SIDESHOW_PAD;

        // this figures out the additional width we can give to the display in the 'More' mode,
        // assuming that we want to keep the columns the same width in both modes
        this._additionalWidth = ((this._width + SIDESHOW_PAD) / SIDESHOW_COLUMNS) * 
                                (this._expandedSideshowColumns - SIDESHOW_COLUMNS);

        let previewWidth = displayGridColumnWidth * asideXFactor - this._width - 
                           this._additionalWidth - SIDESHOW_SECTION_SPACING * 2; 

        let global = Shell.Global.get();
        this.actor = new Clutter.Group();
        let icontheme = Gtk.IconTheme.get_default();
        this._searchBox = new Big.Box({ background_color: SIDESHOW_SEARCH_BG_COLOR,
                                        corner_radius: 4,
                                        x: SIDESHOW_PAD,
                                        y: Panel.PANEL_HEIGHT + SIDESHOW_PAD,
                                        width: this._width,
                                        height: 24});
        this.actor.add_actor(this._searchBox);

        let searchIconTexture = new Clutter.Texture({ x: SIDESHOW_PAD + 2,
                                                      y: this._searchBox.y + 2 });
        let searchIconPath = icontheme.lookup_icon('gtk-find', 16, 0).get_filename();
        searchIconTexture.set_from_file(searchIconPath);
        this.actor.add_actor(searchIconTexture);

        // We need to initialize the text for the entry to have the cursor displayed 
        // in it. See http://bugzilla.openedhand.com/show_bug.cgi?id=1365
        this._searchEntry = new Clutter.Text({ font_name: "Sans 14px",
                                               x: searchIconTexture.x
                                                   + searchIconTexture.width + 4,
                                               y: searchIconTexture.y,
                                               width: this._searchBox.width - (searchIconTexture.x),
                                               height: searchIconTexture.height,
                                               editable: true,
                                               activatable: true,
                                               singleLineMode: true,
                                               text: ""});
        this.actor.add_actor(this._searchEntry);
        this._searchQueued = false;
        this._searchEntry.connect('text-changed', function (se, prop) {
            if (me._searchQueued)
                return;
            me._searchQueued = true;
            Mainloop.timeout_add(250, function() {
                // Strip leading and trailing whitespace
                let text = me._searchEntry.text.replace(/^\s+/g, "").replace(/\s+$/g, "");
                me._searchQueued = false;
                me._appDisplay.setSearch(text);
                me._docDisplay.setSearch(text);
                return false;
            });
        });
        this._searchEntry.connect('activate', function (se) {
            // only one of the displays will have an item selected, so it's ok to
            // call activateSelected() on both of them
            me._appDisplay.activateSelected();
            me._docDisplay.activateSelected();
            return true;
        });
        this._searchEntry.connect('key-press-event', function (se, e) {
            let symbol = Shell.get_event_key_symbol(e);
            if (symbol == Clutter.Escape) {
                // We always want to hide the previews when the user hits Escape.
                // If something that should have a preview gets displayed under 
                // the mouse pointer afterwards the preview will get redisplayed.
                me._appDisplay.hidePreview(); 
                me._docDisplay.hidePreview(); 
                // Escape will keep clearing things back to the desktop.  First, if
                // we have active text, we remove it.
                if (me._searchEntry.text != '')
                    me._searchEntry.text = '';
                // Next, if we're in one of the "more" modes, close it
                else if (me._moreAppsMode)
                    me._unsetMoreAppsMode();
                else if (me._moreDocsMode)
                    me._unsetMoreDocsMode();
                else
                // Finally, just close the overlay entirely
                    me.emit('activated');
                return true;
            } else if (symbol == Clutter.Up) {
                // selectUp and selectDown wrap around in their respective displays
                // too, but there doesn't seem to be any flickering if we first select 
                // something in one display, but then unset the selection, and move
                // it to the other display, so it's ok to do that.
                if (me._moreAppsMode)
                    me._appDisplay.selectUp();
                else if (me._moreDocsMode)
                    me._docDisplay.selectUp();
                else if (me._appDisplay.hasSelected() && !me._appDisplay.selectUp() && me._docDisplay.hasItems()) {
                    me._appDisplay.unsetSelected();
                    me._docDisplay.selectLastItem();
                } else if (me._docDisplay.hasSelected() && !me._docDisplay.selectUp() && me._appDisplay.hasItems()) {
                    me._docDisplay.unsetSelected();
                    me._appDisplay.selectLastItem();
                }
                return true;
            } else if (symbol == Clutter.Down) {
                if (me._moreAppsMode)
                    me._appDisplay.selectDown();
                else if (me._moreDocsMode)
                    me._docDisplay.selectDown();
                else if (me._appDisplay.hasSelected() && !me._appDisplay.selectDown() && me._docDisplay.hasItems()) {
                    me._appDisplay.unsetSelected();
                    me._docDisplay.selectFirstItem();
                } else if (me._docDisplay.hasSelected() && !me._docDisplay.selectDown() && me._appDisplay.hasItems()) {
                    me._docDisplay.unsetSelected();
                    me._appDisplay.selectFirstItem();
                }
                return true;
            } else if (me._moreAppsMode && me._searchEntry.text == '') {
                if (symbol == Clutter.Right) {
                    me._appDisplay.moveRight();
                    return true;
                } else if (symbol == Clutter.Left) {
                    me._appDisplay.moveLeft();
                    return true;
                }
                return false;
            } else if (symbol == Clutter.Right && me._searchEntry.text == '') {
                if (me._appDisplay.hasSelected())
                    me._setMoreAppsMode();
                else
                    me._setMoreDocsMode();
                return true;
            }
            return false;
        });

        this._appsSection = new Big.Box({ background_color: OVERLAY_BACKGROUND_COLOR,
                                          x: SIDESHOW_PAD,
                                          y: this._searchBox.y + this._searchBox.height,
                                          padding_top: SIDESHOW_SECTION_PADDING_TOP,
                                          spacing: SIDESHOW_SECTION_SPACING});

        this._appsText = new Clutter.Text({ color: SIDESHOW_TEXT_COLOR,
                                            font_name: "Sans Bold 14px",
                                            text: "Applications",
                                            height: LABEL_HEIGHT});
        this._appsSection.append(this._appsText, Big.BoxPackFlags.EXPAND);


        let bottomHeight = displayGridRowHeight / 2;

        this._itemDisplayHeight = global.screen_height - this._appsSection.y - SIDESHOW_SECTION_MISC_HEIGHT * 2 - bottomHeight;
        
        this._appsContent = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL });
        this._appsSection.append(this._appsContent, Big.BoxPackFlags.EXPAND);
        this._appDisplay = new AppDisplay.AppDisplay(this._width, this._itemDisplayHeight / 2, SIDESHOW_COLUMNS, SIDESHOW_PAD);
        let sideArea = this._appDisplay.getSideArea();
        sideArea.hide();
        this._appsContent.append(sideArea, Big.BoxPackFlags.NONE);
        this._appsContent.append(this._appDisplay.actor, Big.BoxPackFlags.EXPAND);

        let moreAppsBox = new Big.Box({x_align: Big.BoxAlignment.END});
        this._moreAppsLink = new Link.Link({ color: SIDESHOW_TEXT_COLOR,
                                             font_name: "Sans Bold 14px",
                                             text: "More...",
                                             height: LABEL_HEIGHT });
        moreAppsBox.append(this._moreAppsLink.actor, Big.BoxPackFlags.EXPAND);
        this._appsSection.append(moreAppsBox, Big.BoxPackFlags.EXPAND);

        this.actor.add_actor(this._appsSection);
  
        this._appsSectionDefaultHeight = this._appsSection.height;
    
        this._appsDisplayControlBox = new Big.Box({x_align: Big.BoxAlignment.CENTER});
        this._appsDisplayControlBox.append(this._appDisplay.displayControl, Big.BoxPackFlags.NONE);

        this._docsSection = new Big.Box({ background_color: OVERLAY_BACKGROUND_COLOR,
                                          x: SIDESHOW_PAD,
                                          y: this._appsSection.y + this._appsSection.height,
                                          padding_top: SIDESHOW_SECTION_PADDING_TOP,
                                          spacing: SIDESHOW_SECTION_SPACING});

        this._docsText = new Clutter.Text({ color: SIDESHOW_TEXT_COLOR,
                                            font_name: "Sans Bold 14px",
                                            text: "Recent Documents",
                                            height: LABEL_HEIGHT});
        this._docsSection.append(this._docsText, Big.BoxPackFlags.EXPAND);

        this._docDisplay = new DocDisplay.DocDisplay(this._width, this._itemDisplayHeight - this._appsContent.height, SIDESHOW_COLUMNS, SIDESHOW_PAD);
        this._docsSection.append(this._docDisplay.actor, Big.BoxPackFlags.EXPAND);

        let moreDocsBox = new Big.Box({x_align: Big.BoxAlignment.END});
        this._moreDocsLink = new Link.Link({ color: SIDESHOW_TEXT_COLOR,
                                             font_name: "Sans Bold 14px",
                                             text: "More...",
                                             height: LABEL_HEIGHT });
        moreDocsBox.append(this._moreDocsLink.actor, Big.BoxPackFlags.EXPAND);
        this._docsSection.append(moreDocsBox, Big.BoxPackFlags.EXPAND);

        this.actor.add_actor(this._docsSection);

        this._docsSectionDefaultHeight = this._docsSection.height;

        this._docsDisplayControlBox = new Big.Box({x_align: Big.BoxAlignment.CENTER});
        this._docsDisplayControlBox.append(this._docDisplay.displayControl, Big.BoxPackFlags.NONE);

        this._details = new Big.Box({ x: SIDESHOW_PAD + this._width + this._additionalWidth + SIDESHOW_SECTION_SPACING,
                                      y: Panel.PANEL_HEIGHT + SIDESHOW_PAD,
                                      width: previewWidth,
                                      height: global.screen_height - Panel.PANEL_HEIGHT - SIDESHOW_PAD - bottomHeight,
                                      corner_radius: DETAILS_CORNER_RADIUS,
                                      border: DETAILS_BORDER_WIDTH,
                                      border_color: DETAILS_BORDER_COLOR,
                                      padding: DETAILS_PADDING});
        this._appDisplay.setAvailableWidthForItemDetails(previewWidth);
        this._docDisplay.setAvailableWidthForItemDetails(previewWidth);

        /* Proxy the activated signals */
        this._appDisplay.connect('activated', function(appDisplay) {
            me.emit('activated');
        });
        this._docDisplay.connect('activated', function(docDisplay) {
            me.emit('activated');
        });
        this._appDisplay.connect('selected', function(appDisplay) {
            // We allow clicking on any item to select it, so if an 
            // item in the app display is selected, we need to make sure that
            // no item in the doc display has the selection.
            me._docDisplay.unsetSelected();
            me._docDisplay.hidePreview();
        });
        this._docDisplay.connect('selected', function(docDisplay) {
            // We allow clicking on any item to select it, so if an 
            // item in the doc display is selected, we need to make sure that
            // no item in the app display has the selection.
            me._appDisplay.unsetSelected(); 
            me._appDisplay.hidePreview(); 
        });
        this._appDisplay.connect('redisplayed', function(appDisplay) {
            me._ensureItemSelected();
        });
        this._docDisplay.connect('redisplayed', function(docDisplay) {
            me._ensureItemSelected();
        });

        this._moreAppsLink.connect('clicked',
            function(o, event) {
                if (me._moreAppsMode) {
                    me._unsetMoreAppsMode();
                }  else {
                    me._setMoreAppsMode();
                }
            });

        this._moreDocsLink.connect('clicked',
            function(o, event) {
                if (me._moreDocsMode) {
                    me._unsetMoreDocsMode();
                }  else {
                    me._setMoreDocsMode();
                }
            });
    },

    show: function() {
        let global = Shell.Global.get();

        this._appDisplay.show();
        this._appsContent.show();
        this._docDisplay.show();
        this._appDisplay.selectFirstItem();   
        if (!this._appDisplay.hasSelected())
            this._docDisplay.selectFirstItem();
        else
            this._docDisplay.unsetSelected();
        global.stage.set_key_focus(this._searchEntry);
    },

    hide: function() {
        this._appsContent.hide();
        this._docDisplay.hide();
        this._searchEntry.text = '';
        this._unsetMoreAppsMode();
        this._unsetMoreDocsMode();
    },

    // Ensures that one of the displays has the selection if neither owns it after the
    // latest redisplay. This can be applicable if the display that earlier had the
    // selection no longer has any items, or if their is a single section being shown 
    // in the expanded view and it went from having no matching items to having some.
    // We first try to place the selection in the applications section, because it is
    // displayed above the documents section.
    _ensureItemSelected: function() { 
        if (!this._appDisplay.hasSelected() && !this._docDisplay.hasSelected()) {
            if (this._appDisplay.hasItems()) { 
                this._appDisplay.selectFirstItem();
            } else if (this._docDisplay.hasItems()) {
                this._docDisplay.selectFirstItem();
            }
        }
    },
 
    // Sets the 'More' mode for browsing applications. Updates the applications section to have more items.
    // Slides down the documents section to reveal the additional applications.
    _setMoreAppsMode: function() {
        if (this._moreAppsMode)
            return;
        
        // No corresponding STATE_PENDING_ACTIVE, because we call updateAppsSection
        // immediately below.
        this._moreAppsMode = STATE_ACTIVE;

        this._docsSection.set_clip(0, 0, this._docsSection.width, this._docsSection.height);

        this._moreAppsLink.actor.hide();
        this._appsSection.set_clip(0, 0, this._appsSection.width, this._appsSection.height);

        // Move the selection to the applications section if it was in the docs section.
        this._docDisplay.unsetSelected();
        // Because we have menus in applications, we want to reset the selection for applications
        // as well.  The default is no menu.
        this._appDisplay.unsetSelected();
        this._updateAppsSection();
 
        Tweener.addTween(this._docsSection,
                         { y: this._docsSection.y + this._docsSection.height,
                           clipHeightBottom: 0,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });

        // We need to expand the clip on the applications section so that the first additional
        // application to be displayed does not appear abruptly. 
        Tweener.addTween(this._appsSection,
                         { clipHeightBottom: this._itemDisplayHeight + SIDESHOW_SECTION_MISC_HEIGHT * 2 - LABEL_HEIGHT - SIDESHOW_SECTION_SPACING,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._onAppsSectionExpanded,
                           onCompleteScope: this
                         }); 
                   
        this.emit('more-activated'); 
    },

    // Unsets the 'More' mode for browsing applications. Slides in the documents section. 
    _unsetMoreAppsMode: function() {
        if (!this._moreAppsMode)
            return;

        this._moreAppsMode = STATE_PENDING_INACTIVE;

        this._moreAppsLink.actor.hide();

        this._appsSection.set_clip(0, 0, this._appsSection.width, this._appsSection.height);

        this._docDisplay.show();

        // We need to be reducing the clip on the applications section so that the last application to
        // be removed does not disappear abruptly.
        Tweener.addTween(this._appsSection,
                         { clipHeightBottom: this._appsSectionDefaultHeight - LABEL_HEIGHT - SIDESHOW_SECTION_SPACING,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._onAppsSectionReduced,
                           onCompleteScope: this
                         }); 

        Tweener.addTween(this._docsSection,
                         { y: this._docsSection.y - this._docsSection.height,
                           clipHeightBottom: this._docsSection.height,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });  
 
        this.emit('less-activated');
    },

    // Removes the clip from the applications section to reveal the 'Less...' text.
    // Hides the documents section so that it doesn't get updated on new searches.
    _onAppsSectionExpanded: function() {
        this._appsSection.remove_clip(); 
        this._docDisplay.hide();
    },

    // Updates the applications section to contain fewer items. Selects the first item in the 
    // documents section if applications section has no items.
    // Removes the clip from the applications section to reveal the 'More...' text. 
    // Removes the clip from the documents section, so that the clip does not limit the size of 
    // the section if it is expanded later.
    _onAppsSectionReduced: function() {
        if (this._moreAppsMode != STATE_PENDING_INACTIVE)
            return;
        this._moreAppsMode = STATE_INACTIVE;
        this._updateAppsSection();
        if (!this._appDisplay.hasItems())
            this._docDisplay.selectFirstItem();
        this._appsSection.remove_clip();
        this._docsSection.remove_clip(); 
    },

    // Updates the applications section display and the 'More...' or 'Less...' control associated with it
    // depending on the current mode. This function must only be called once after the 'More' mode has been
    // changed, which is ensured by _setMoreAppsMode() and _unsetMoreAppsMode() functions. 
    _updateAppsSection: function() {
        if (this._moreAppsMode) {
            // Subtract one from columns since we are displaying menus
            this._appDisplay.setExpanded(true, this._width, this._additionalWidth,
                                         this._itemDisplayHeight + SIDESHOW_SECTION_MISC_HEIGHT,
                                         this._expandedSideshowColumns - 1);
            this._moreAppsLink.setText("Less...");
            this._appsSection.insert_after(this._appsDisplayControlBox, this._appsContent, Big.BoxPackFlags.NONE);
            this.actor.add_actor(this._details);
            this._details.append(this._appDisplay.selectedItemDetails, Big.BoxPackFlags.NONE);
        } else {
            this._appDisplay.setExpanded(false, this._width, 0,
                                         this._appsSectionDefaultHeight - SIDESHOW_SECTION_MISC_HEIGHT,
                                         SIDESHOW_COLUMNS);
            this._moreAppsLink.setText("More...");
            this._appsSection.remove_actor(this._appsDisplayControlBox);
            this.actor.remove_actor(this._details);
            this._details.remove_all();
        }
        this._moreAppsLink.actor.show();
    },

    // Sets the 'More' mode for browsing documents. Updates the documents section to have more items.
    // Slides up the applications section and the documents section at the same time to reveal the additional
    // documents.
    _setMoreDocsMode: function() {
        if (this._moreDocsMode)
            return;

        this._moreDocsMode = true;

        if (!this._appsSection.has_clip)
            this._appsSection.set_clip(0, 0, this._appsSection.width, this._appsSection.height);

        this._moreDocsLink.actor.hide();
        this._docsSection.set_clip(0, 0, this._docsSection.width, this._docsSection.height);

        // Move the selection to the docs section if it was in the apps section.
        this._appDisplay.unsetSelected();
        if (!this._docDisplay.hasSelected())
            this._docDisplay.selectFirstItem();

        
        this._updateDocsSection();
        
        Tweener.addTween(this._appsSection,
                         { y: this._appsSection.y - this._appsSection.height,
                           clipHeightTop: 0,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });
        // We do not slide in the 'Less...' text along with the documents so that the documents appear from the same
        // edge where the last document was displayed, and not have that edge gradually move over to where the 'Less'
        // text is displayed.
        Tweener.addTween(this._docsSection,
                         { y: this._searchBox.y + this._searchBox.height,
                           clipHeightBottom: this._itemDisplayHeight + SIDESHOW_SECTION_MISC_HEIGHT * 2 - LABEL_HEIGHT - SIDESHOW_SECTION_SPACING,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._onDocsSectionExpanded,
                           onCompleteScope: this
                         });                   
        this.emit('more-activated'); 
    },

    // Unsets the 'More' mode for browsing documents. Slides in the applications section
    // and slides down the documents section. 
    _unsetMoreDocsMode: function() {
        if (!this._moreDocsMode)
            return;

        this._moreDocsMode = false;

        this._moreDocsLink.actor.hide();
         
        this._docsSection.set_clip(0, 0, this._docsSection.width, this._docsSection.height);

        this._appsContent.show();

        Tweener.addTween(this._docsSection,
                         { y: this._docsSection.y + this._appsSectionDefaultHeight,
                           clipHeightBottom: this._docsSectionDefaultHeight - LABEL_HEIGHT - SIDESHOW_SECTION_SPACING,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._onDocsSectionReduced,
                           onCompleteScope: this
                         });   
        Tweener.addTween(this._appsSection,
                         { y: this._appsSection.y + this._appsSection.height,
                           clipHeightTop: this._appsSection.height,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad"
                         });  
        this.emit('less-activated');
    },

    // Removes the clip from the documents section to reveal the 'Less...' text.
    // Hides the applications section so that it doesn't get updated on new searches.
    _onDocsSectionExpanded: function() {
        this._docsSection.remove_clip(); 
        this._appsContent.hide();
    },

    // Updates the documents section to contain fewer items. Selects the first item in the 
    // applications section if applications section has no items.
    // Removes the clip from the documents section to reveal the 'More...' text. 
    // Removes the clip from the applications section, so that the clip does not limit the size of 
    // the section if it is expanded later.
    _onDocsSectionReduced: function() {
        this._updateDocsSection();  
        if (!this._docDisplay.hasItems())
            this._appDisplay.selectFirstItem();
        this._docsSection.remove_clip(); 
        this._appsSection.remove_clip(); 
    },

    // Updates the documents section display and the 'More...' or 'Less...' control associated with it
    // depending on the current mode. This function must only be called once after the 'More' mode has been
    // changed, which is ensured by _setMoreDocsMode() and _unsetMoreDocsMode() functions. 
    _updateDocsSection: function() {
        if (this._moreDocsMode) {
            this._docDisplay.setExpanded(true, this._width, this._additionalWidth,
                                         this._itemDisplayHeight + SIDESHOW_SECTION_MISC_HEIGHT,
                                         this._expandedSideshowColumns);
            this._moreDocsLink.setText("Less...");
            this._docsSection.insert_after(this._docsDisplayControlBox, this._docDisplay.actor, Big.BoxPackFlags.NONE); 
            this.actor.add_actor(this._details);
            this._details.append(this._docDisplay.selectedItemDetails, Big.BoxPackFlags.NONE);
        } else {
            this._docDisplay.setExpanded(false, this._width, 0,
                                         this._docsSectionDefaultHeight - SIDESHOW_SECTION_MISC_HEIGHT,
                                         SIDESHOW_COLUMNS);
            this._moreDocsLink.setText("More...");
            this._docsSection.remove_actor(this._docsDisplayControlBox); 
            this.actor.remove_actor(this._details); 
            this._details.remove_all();
        }
        this._moreDocsLink.actor.show();
    }

};
Signals.addSignalMethods(Sideshow.prototype);

function Overlay() {
    this._init();
}

Overlay.prototype = {
    _init : function() {
        let me = this;

        let global = Shell.Global.get();

        wideScreen = (global.screen_width/global.screen_height > WIDE_SCREEN_CUT_OFF_RATIO);

        // We divide the screen into an imaginary grid which helps us determine the layout of
        // different visual components.  
        if (wideScreen) {
            displayGridColumnWidth = global.screen_width / COLUMNS_WIDE_SCREEN;
            displayGridRowHeight = global.screen_height / ROWS_WIDE_SCREEN;
        } else {
            displayGridColumnWidth = global.screen_width / COLUMNS_REGULAR_SCREEN;
            displayGridRowHeight = global.screen_height / ROWS_REGULAR_SCREEN;
        }

        this._group = new Clutter.Group();
        this._group._delegate = this;

        this.visible = false;
        this._hideInProgress = false;

        let background = new Clutter.Rectangle({ color: OVERLAY_BACKGROUND_COLOR,
                                                 reactive: true,
                                                 x: 0,
                                                 y: 0,
                                                 width: global.screen_width,
                                                 height: global.screen_width });
        this._group.add_actor(background);

        this._group.hide();
        global.overlay_group.add_actor(this._group);

        // TODO - recalculate everything when desktop size changes
        this._sideshow = new Sideshow();
        this._group.add_actor(this._sideshow.actor); 
        this._workspaces = null;
        this._workspacesBackground = null;
        this._sideshow.connect('activated', function(sideshow) {
            // TODO - have some sort of animation/effect while
            // transitioning to the new app.  We definitely need
            // startup-notification integration at least.
            me._deactivate();
        });
        this._sideshow.connect('more-activated', function(sideshow) {
            if (me._workspaces != null) {
                let asideXFactor = wideScreen ? WORKSPACES_X_FACTOR_ASIDE_MODE_WIDE_SCREEN : WORKSPACES_X_FACTOR_ASIDE_MODE_REGULAR_SCREEN;  
        
                let workspacesX = displayGridColumnWidth * asideXFactor + WORKSPACE_GRID_PADDING;
                me._workspaces.addButton.hide();
                me._workspaces.updatePosition(workspacesX, null);
                // lower the sideshow below the workspaces background, so that the workspaces
                // background covers the parts of the sideshow that are gradually being 
                // revealed from underneath it
                me._sideshow.actor.lower(me._workspacesBackground);
                Tweener.addTween(me._workspacesBackground,
                                 { x: displayGridColumnWidth * asideXFactor,
                                   time: ANIMATION_TIME,
                                   transition: "easeOutQuad",
                                   onComplete: me._animationDone,
                                   onCompleteScope: me
                                 });
            }    
        });
        this._sideshow.connect('less-activated', function(sideshow) {
            if (me._workspaces != null) {
                let workspacesX = displayGridColumnWidth + WORKSPACE_GRID_PADDING;
                me._workspaces.addButton.show();
                me._workspaces.updatePosition(workspacesX, null);
                // lower the sideshow below the workspaces background, so that the workspaces
                // background covers the parts of the sideshow as it slides in over them
                me._sideshow.actor.lower(me._workspacesBackground);
                Tweener.addTween(me._workspacesBackground,
                                 { x: displayGridColumnWidth,
                                   time: ANIMATION_TIME,
                                   transition: "easeOutQuad",
                                   onComplete: me._animationDone,
                                   onCompleteScope: me
                                 });  
            }
        });
    },

    //// Draggable target interface ////

    // Unsets the expanded display mode if a GenericDisplayItem is being 
    // dragged over the overlay, i.e. as soon as it starts being dragged.
    // This slides the workspaces back in and allows the user to place
    // the item on any workspace.
    handleDragOver : function(source, actor, x, y, time) {
        if (source instanceof GenericDisplay.GenericDisplayItem) {
            this._sideshow._unsetMoreAppsMode();
            this._sideshow._unsetMoreDocsMode();
            return true;
        }
  
        return false;
    },

    //// Public methods ////

    show : function() {
        if (this.visible)
            return;

        this.visible = true;

        let global = Shell.Global.get();
        let screenWidth = global.screen_width;
        let screenHeight = global.screen_height; 

        this._sideshow.show();

        let columnsUsed = wideScreen ? COLUMNS_FOR_WORKSPACES_WIDE_SCREEN : COLUMNS_FOR_WORKSPACES_REGULAR_SCREEN;
        let rowsUsed = wideScreen ? ROWS_FOR_WORKSPACES_WIDE_SCREEN : ROWS_FOR_WORKSPACES_REGULAR_SCREEN;  
         
        let workspacesWidth = displayGridColumnWidth * columnsUsed - WORKSPACE_GRID_PADDING * 2;
        // We scale the vertical padding by (screenHeight / screenWidth) so that the workspace preserves its aspect ratio.
        let workspacesHeight = displayGridRowHeight * rowsUsed - WORKSPACE_GRID_PADDING * (screenHeight / screenWidth) * 2;

        let workspacesX = displayGridColumnWidth + WORKSPACE_GRID_PADDING;
        let workspacesY = displayGridRowHeight + WORKSPACE_GRID_PADDING * (screenHeight / screenWidth);

        // place the 'Add Workspace' button in the bottom row of the grid
        let addButtonSize = Math.floor(displayGridRowHeight * 3/5);
        let addButtonX = workspacesX + workspacesWidth - addButtonSize;
        let addButtonY = screenHeight - Math.floor(displayGridRowHeight * 4/5);

        // We use the workspaces background to have it fill the full height of the overlay when we are sliding
        // the workspaces out to uncover the expanded items display and also when we are sliding the
        // workspaces back in to gradually cover the expanded items display. If we don't have such background,
        // we get a few items above or below the workspaces display that disappear or appear abruptly.  
        this._workspacesBackground = new Clutter.Rectangle({ color: OVERLAY_BACKGROUND_COLOR,
                                                             reactive: false,
                                                             x: displayGridColumnWidth,
                                                             y: Panel.PANEL_HEIGHT,
                                                             width: displayGridColumnWidth * columnsUsed,
                                                             height: global.screen_height - Panel.PANEL_HEIGHT });
        this._group.add_actor(this._workspacesBackground);

        this._workspaces = new Workspaces.Workspaces(workspacesWidth, workspacesHeight, workspacesX, workspacesY, 
                                                     addButtonSize, addButtonX, addButtonY);
        this._group.add_actor(this._workspaces.actor);

        // All the the actors in the window group are completely obscured,
        // hiding the group holding them while the overlay is displayed greatly
        // increases performance of the overlay especially when there are many
        // windows visible.
        //
        // If we switched to displaying the actors in the overlay rather than
        // clones of them, this would obviously no longer be necessary.
        global.window_group.hide();
        this._group.show();

        // Dummy tween, just waiting for the workspace animation
        Tweener.addTween(this,
                         { time: ANIMATION_TIME,
                           onComplete: this._animationDone,
                           onCompleteScope: this
                         });
    },

    hide : function() {
        if (!this.visible || this._hideInProgress)
            return;

        this._hideInProgress = true;
        // lower the sideshow, so that workspaces display is on top and covers the sideshow while it is sliding out
        this._sideshow.actor.lower(this._workspacesBackground);
        this._workspaces.hide();

        // Dummy tween, just waiting for the workspace animation
        Tweener.addTween(this,
                         { time: ANIMATION_TIME,
                           onComplete: this._hideDone,
                           onCompleteScope: this
                         });
    },

    //// Private methods ////

    // Raises the sideshow to the top, so that we can tell if the pointer is above one of its items.
    // We need to do this every time animation of the workspaces is done bacause the workspaces actor
    // currently covers the whole screen, regardless of where the workspaces are actually displayed. 
    // On the other hand, we need the workspaces to be on top when they are sliding in, out,
    // and to the side because we want them to cover the sideshow as they do that.
    //
    // Once we rework the workspaces actor to only cover the area it actually needs, we can
    // remove this workaround. Also http://bugzilla.openedhand.com/show_bug.cgi?id=1513 requests being
    // able to pick only a reactive actor at a certain position, rather than any actor. Being able
    // to do that would allow us to not have to raise the sideshow.  
    _animationDone: function() {
        if (this._hideInProgress)
            return;

       this._sideshow.actor.raise_top();
    }, 

    _hideDone: function() {
        let global = Shell.Global.get();

        global.window_group.show();

        this._workspaces.destroy();
        this._workspaces = null;

        this._workspacesBackground.destroy();
        this._workspacesBackground = null;

        this._sideshow.hide();
        this._group.hide();

        this.visible = false; 
        this._hideInProgress = false;
    },

    _deactivate : function() {
        Main.hide_overlay();
    }
};

Tweener.registerSpecialProperty("clipHeightBottom", _clipHeightBottomGet, _clipHeightBottomSet);

function _clipHeightBottomGet(actor) {
    let [xOffset, yOffset, clipWidth, clipHeight] = actor.get_clip();
    return clipHeight;
}

function _clipHeightBottomSet(actor, clipHeight) {
    actor.set_clip(0, 0, actor.width, clipHeight);
}

Tweener.registerSpecialProperty("clipHeightTop", _clipHeightTopGet, _clipHeightTopSet);

function _clipHeightTopGet(actor) {
    let [xOffset, yOffset, clipWidth, clipHeight] = actor.get_clip();
    return clipHeight;
}

function _clipHeightTopSet(actor, clipHeight) {
    actor.set_clip(0, actor.height - clipHeight, actor.width, clipHeight);
}
