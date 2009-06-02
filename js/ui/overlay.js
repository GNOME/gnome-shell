/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const Lang = imports.lang;

const AppDisplay = imports.ui.appDisplay;
const DocDisplay = imports.ui.docDisplay;
const GenericDisplay = imports.ui.genericDisplay;
const Link = imports.ui.link;
const Main = imports.ui.main;
const Panel = imports.ui.panel;
const Tweener = imports.ui.tweener;
const Workspaces = imports.ui.workspaces;

const ROOT_OVERLAY_COLOR = new Clutter.Color();
ROOT_OVERLAY_COLOR.from_pixel(0x000000bb);

// The factor to scale the overlay wallpaper with. This should not be less
// than 3/2, because the rule of thirds is used for positioning (see below).
const BACKGROUND_SCALE = 2;

const LABEL_HEIGHT = 16;
// We use DASH_PAD for the padding on the left side of the sideshow and as a gap
// between sideshow columns.
const DASH_PAD = 6;
const DASH_MIN_WIDTH = 250;
const DASH_SECTION_PADDING_TOP = 6;
const DASH_SECTION_SPACING = 6;
const DASH_COLUMNS = 1;
const DETAILS_CORNER_RADIUS = 5;
const DETAILS_BORDER_WIDTH = 1;
const DETAILS_PADDING = 6;
// This is the height of section components other than the item display.
const DASH_SECTION_MISC_HEIGHT = (LABEL_HEIGHT + DASH_SECTION_SPACING) * 2 + DASH_SECTION_PADDING_TOP;
const DASH_SEARCH_BG_COLOR = new Clutter.Color();
DASH_SEARCH_BG_COLOR.from_pixel(0xffffffff);
const DASH_TEXT_COLOR = new Clutter.Color();
DASH_TEXT_COLOR.from_pixel(0xffffffff);
const DETAILS_BORDER_COLOR = new Clutter.Color();
DETAILS_BORDER_COLOR.from_pixel(0xffffffff);

// Time for initial animation going into overlay mode
const ANIMATION_TIME = 0.25;

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

// Padding around workspace grid / Spacing between Dash and Workspaces
const WORKSPACE_GRID_PADDING = 12;

const COLUMNS_FOR_WORKSPACES_REGULAR_SCREEN = 3;
const ROWS_FOR_WORKSPACES_REGULAR_SCREEN = 6;
const WORKSPACES_X_FACTOR_ASIDE_MODE_REGULAR_SCREEN = 4 - 0.25;
const EXPANDED_DASH_COLUMNS_REGULAR_SCREEN = 2;

const COLUMNS_FOR_WORKSPACES_WIDE_SCREEN = 4;
const ROWS_FOR_WORKSPACES_WIDE_SCREEN = 8;
const WORKSPACES_X_FACTOR_ASIDE_MODE_WIDE_SCREEN = 5 - 0.25;
const EXPANDED_DASH_COLUMNS_WIDE_SCREEN = 3;

// A multi-state; PENDING is used during animations
const STATE_ACTIVE = true;
const STATE_PENDING_INACTIVE = false;
const STATE_INACTIVE = false;

let wideScreen = false;
let displayGridColumnWidth = null;
let displayGridRowHeight = null;

function SearchEntry(width) {
    this._init(width);
}

SearchEntry.prototype = {
    _init : function(width) {
        this.actor = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                   y_align: Big.BoxAlignment.CENTER,
                                   background_color: DASH_SEARCH_BG_COLOR,
                                   corner_radius: 4,
                                   spacing: 4,
                                   padding_left: 4,
                                   padding_right: 4,
                                   width: width,
                                   height: 24
                                 });

        let icontheme = Gtk.IconTheme.get_default();
        let searchIconTexture = new Clutter.Texture({});
        let searchIconPath = icontheme.lookup_icon('gtk-find', 16, 0).get_filename();
        searchIconTexture.set_from_file(searchIconPath);
        this.actor.append(searchIconTexture, 0);

        // We need to initialize the text for the entry to have the cursor displayed
        // in it. See http://bugzilla.openedhand.com/show_bug.cgi?id=1365
        this.entry = new Clutter.Text({ font_name: "Sans 14px",
                                        editable: true,
                                        activatable: true,
                                        singleLineMode: true,
                                        text: ""
                                      });
        this.entry.connect('text-changed', Lang.bind(this, function (e) {
            let text = this.entry.text;
        }));
        this.actor.append(this.entry, Big.BoxPackFlags.EXPAND);
    }
};

function Dash() {
    this._init();
}

Dash.prototype = {
    _init : function() {
        let me = this;

        let asideXFactor = wideScreen ? WORKSPACES_X_FACTOR_ASIDE_MODE_WIDE_SCREEN : WORKSPACES_X_FACTOR_ASIDE_MODE_REGULAR_SCREEN; 
        this._expandedDashColumns = wideScreen ? EXPANDED_DASH_COLUMNS_WIDE_SCREEN : EXPANDED_DASH_COLUMNS_REGULAR_SCREEN;

        this._width = displayGridColumnWidth;
        this._displayWidth = this._width - DASH_PAD;

        this._expandedWidth = displayGridColumnWidth * asideXFactor;

        // this figures out the additional width we can give to the display in the 'More' mode,
        // assuming that we want to keep the columns the same width in both modes
        this._additionalWidth = (this._width / DASH_COLUMNS) *
                                (this._expandedDashColumns - DASH_COLUMNS);

        let bottomHeight = displayGridRowHeight / 2;

        let global = Shell.Global.get();

        let previewWidth = this._expandedWidth - this._width -
                           this._additionalWidth - DASH_SECTION_SPACING;

        let previewHeight = global.screen_height - Panel.PANEL_HEIGHT - DASH_PAD - bottomHeight;

        this.actor = new Clutter.Group();
        this.actor.height = global.screen_height;

        this._appsSection = new Big.Box({ x: DASH_PAD,
                                          y: Panel.PANEL_HEIGHT + DASH_PAD,
                                          padding_top: DASH_SECTION_PADDING_TOP,
                                          spacing: DASH_SECTION_SPACING});

        this._itemDisplayHeight = global.screen_height - this._appsSection.y - DASH_SECTION_MISC_HEIGHT * 2 - bottomHeight;
        
        this._appsContent = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL });
        this._appsSection.append(this._appsContent, Big.BoxPackFlags.EXPAND);
        this._appDisplay = new AppDisplay.AppDisplay(this._displayWidth, this._itemDisplayHeight / 2, DASH_COLUMNS, DASH_PAD);
        let sideArea = this._appDisplay.getSideArea();
        sideArea.hide();
        this._appsContent.append(sideArea, Big.BoxPackFlags.NONE);
        this._appsContent.append(this._appDisplay.actor, Big.BoxPackFlags.EXPAND);

        let moreAppsBox = new Big.Box({x_align: Big.BoxAlignment.END});
        this._moreAppsLink = new Link.Link({ color: DASH_TEXT_COLOR,
                                             font_name: "Sans Bold 14px",
                                             text: "More...",
                                             height: LABEL_HEIGHT });
        moreAppsBox.append(this._moreAppsLink.actor, Big.BoxPackFlags.EXPAND);
        this._appsSection.append(moreAppsBox, Big.BoxPackFlags.EXPAND);

        this.actor.add_actor(this._appsSection);
  
        this._appsSectionDefaultHeight = this._appsSection.height;
    
        this._appsDisplayControlBox = new Big.Box({x_align: Big.BoxAlignment.CENTER});
        this._appsDisplayControlBox.append(this._appDisplay.displayControl, Big.BoxPackFlags.NONE);

        this._docsSection = new Big.Box({ x: DASH_PAD,
                                          y: this._appsSection.y + this._appsSection.height,
                                          padding_top: DASH_SECTION_PADDING_TOP,
                                          spacing: DASH_SECTION_SPACING});

        this._docsText = new Clutter.Text({ color: DASH_TEXT_COLOR,
                                            font_name: "Sans Bold 14px",
                                            text: "Recent Documents",
                                            height: LABEL_HEIGHT});
        this._docsSection.append(this._docsText, Big.BoxPackFlags.EXPAND);

        this._docDisplay = new DocDisplay.DocDisplay(this._displayWidth, this._itemDisplayHeight - this._appsContent.height, DASH_COLUMNS, DASH_PAD);
        this._docsSection.append(this._docDisplay.actor, Big.BoxPackFlags.EXPAND);

        let moreDocsBox = new Big.Box({x_align: Big.BoxAlignment.END});
        this._moreDocsLink = new Link.Link({ color: DASH_TEXT_COLOR,
                                             font_name: "Sans Bold 14px",
                                             text: "More...",
                                             height: LABEL_HEIGHT });
        moreDocsBox.append(this._moreDocsLink.actor, Big.BoxPackFlags.EXPAND);
        this._docsSection.append(moreDocsBox, Big.BoxPackFlags.EXPAND);

        this.actor.add_actor(this._docsSection);

        this._docsSectionDefaultHeight = this._docsSection.height;

        this._docsDisplayControlBox = new Big.Box({x_align: Big.BoxAlignment.CENTER});
        this._docsDisplayControlBox.append(this._docDisplay.displayControl, Big.BoxPackFlags.NONE);

        this._details = new Big.Box({ x: this._width + this._additionalWidth + DASH_SECTION_SPACING,
                                      y: Panel.PANEL_HEIGHT + DASH_PAD,
                                      width: previewWidth,
                                      height: previewHeight,
                                      corner_radius: DETAILS_CORNER_RADIUS,
                                      border: DETAILS_BORDER_WIDTH,
                                      border_color: DETAILS_BORDER_COLOR,
                                      padding: DETAILS_PADDING});
        this._appDisplay.setAvailableDimensionsForItemDetails(previewWidth - DETAILS_PADDING * 2 - DETAILS_BORDER_WIDTH * 2,
                                                              previewHeight - DETAILS_PADDING * 2 - DETAILS_BORDER_WIDTH * 2);
        this._docDisplay.setAvailableDimensionsForItemDetails(previewWidth - DETAILS_PADDING * 2 - DETAILS_BORDER_WIDTH * 2,
                                                              previewHeight - DETAILS_PADDING * 2 - DETAILS_BORDER_WIDTH * 2);
 
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
    },

    hide: function() {
        this._appsContent.hide();
        this._docDisplay.hide();
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
 
    // Updates the applications section display and the 'More...' or 'Less...' control associated with it
    // depending on the current mode. This function must only be called once after the 'More' mode has been
    // changed, which is ensured by _setMoreAppsMode() and _unsetMoreAppsMode() functions. 
    _updateAppsSection: function() {
        if (this._moreAppsMode) {
            // Subtract one from columns since we are displaying menus
            this._appDisplay.setExpanded(true, this._displayWidth, this._additionalWidth,
                                         this._itemDisplayHeight + DASH_SECTION_MISC_HEIGHT,
                                         this._expandedDashColumns - 1);
            this._moreAppsLink.setText("Less...");
            this._appsSection.insert_after(this._appsDisplayControlBox, this._appsContent, Big.BoxPackFlags.NONE);
            this.actor.add_actor(this._details);
            this._details.append(this._appDisplay.selectedItemDetails, Big.BoxPackFlags.NONE);
        } else {
            this._appDisplay.setExpanded(false, this._displayWidth, 0,
                                         this._appsSectionDefaultHeight - DASH_SECTION_MISC_HEIGHT,
                                         DASH_COLUMNS);
            this._moreAppsLink.setText("More...");
            this._appsSection.remove_actor(this._appsDisplayControlBox);
            this.actor.remove_actor(this._details);
            this._details.remove_all();
        }
        this._moreAppsLink.actor.show();
    }
};
Signals.addSignalMethods(Dash.prototype);

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

        // A scaled root pixmap actor is used as a background. It is zoomed in
        // to the lower right intersection of the lines that divide the image
        // evenly in a 3x3 grid. This is based on the rule of thirds, a
        // compositional rule of thumb in visual arts. The choice for the
        // lower right point is based on a quick survey of GNOME wallpapers.
        let background = global.create_root_pixmap_actor();
        background.width = global.screen_width * BACKGROUND_SCALE;
        background.height = global.screen_height * BACKGROUND_SCALE;
        background.x = -global.screen_width * (4 * BACKGROUND_SCALE - 3) / 6;
        background.y = -global.screen_height * (4 * BACKGROUND_SCALE - 3) / 6;
        this._group.add_actor(background);

        // Draw a semitransparent rectangle over the background for readability.
        let backOver = new Clutter.Rectangle({ color: ROOT_OVERLAY_COLOR,
                                               width: global.screen_width,
                                               height: global.screen_height - Panel.PANEL_HEIGHT,
                                               y: Panel.PANEL_HEIGHT });
        this._group.add_actor(backOver);

        this._group.hide();
        global.overlay_group.add_actor(this._group);

        // TODO - recalculate everything when desktop size changes
        this._dash = new Dash();
        this._group.add_actor(this._dash.actor);
        this._workspaces = null;
        this._dash.connect('activated', function(dash) {
            // TODO - have some sort of animation/effect while
            // transitioning to the new app.  We definitely need
            // startup-notification integration at least.
            me.hide();
        });
        this._dash.connect('more-activated', function(dash) {
            if (me._workspaces != null) {
                let asideXFactor = wideScreen ? WORKSPACES_X_FACTOR_ASIDE_MODE_WIDE_SCREEN : WORKSPACES_X_FACTOR_ASIDE_MODE_REGULAR_SCREEN;  
        
                let workspacesX = displayGridColumnWidth * asideXFactor + WORKSPACE_GRID_PADDING;
                me._workspaces.addButton.hide();
                me._workspaces.updatePosition(workspacesX, null);
            }    
        });
        this._dash.connect('less-activated', function(dash) {
            if (me._workspaces != null) {
                let workspacesX = displayGridColumnWidth + WORKSPACE_GRID_PADDING;
                me._workspaces.addButton.show();
                me._workspaces.updatePosition(workspacesX, null);
            }
        });
    },

    //// Draggable target interface ////

    handleDragOver : function(source, actor, x, y, time) {
        return false;
    },

    //// Public methods ////

    show : function() {
        if (this.visible)
            return;
        if (!Main.startModal())
            return;

        this.visible = true;

        let global = Shell.Global.get();
        let screenWidth = global.screen_width;
        let screenHeight = global.screen_height; 

        this._dash.show();

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

        // Try to make the menu not too visible behind the empty space between
        // the workspace previews by sliding in its clipping rectangle.
        // We want to finish drawing the Dash just before the top workspace fully
        // slides in on the top. Which means that we have more time to wait before
        // drawing the dash if the active workspace is displayed on the bottom of
        // the workspaces grid, and almost no time to wait if it is displayed in the top
        // row of the workspaces grid. The calculations used below try to roughly
        // capture the animation ratio for when workspaces are covering the top of the overlay
        // vs. when workspaces are already below the top of the overlay, and apply it
        // to clipping the dash. The clipping is removed in this._showDone().
        this._dash.actor.set_clip(0, 0,
                                      this._workspaces.getFullSizeX(),
                                      this._dash.actor.height);
        Tweener.addTween(this._dash.actor,
                         { clipWidthRight: this._dash._width + WORKSPACE_GRID_PADDING + this._workspaces.getWidthToTopActiveWorkspace(),
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._showDone,
                           onCompleteScope: this

                         });

        this.emit('showing');
    },

    hide : function() {
        if (!this.visible || this._hideInProgress)
            return;

        let global = Shell.Global.get();

        this._hideInProgress = true;
        // lower the Dash, so that workspaces display is on top and covers the Dash while it is sliding out
        this._dash.actor.lower(this._workspaces.actor);
        this._workspaces.hide();

        // Try to make the menu not too visible behind the empty space between
        // the workspace previews by sliding in its clipping rectangle.
        // The logic used is the same as described in this.show(). If the active workspace
        // is displayed in the top row, than almost full animation time is needed for it
        // to reach the top of the overlay and cover the Dash fully, while if the
        // active workspace is in the lower row, than the top left workspace reaches the
        // top of the overlay sooner as it is moving out of the way.
        // The clipping is removed in this._hideDone().
        this._dash.actor.set_clip(0, 0,
                                      this._dash.actor.width + WORKSPACE_GRID_PADDING + this._workspaces.getWidthToTopActiveWorkspace(),
                                      this._dash.actor.height);
        Tweener.addTween(this._dash.actor,
                         { clipWidthRight: this._workspaces.getFullSizeX() + this._workspaces.getWidthToTopActiveWorkspace() - global.screen_width,
                           time: ANIMATION_TIME,
                           transition: "easeOutQuad",
                           onComplete: this._hideDone,
                           onCompleteScope: this
                         });

        this.emit('hiding');
    },

    toggle: function() {
        if (this.visible)
            this.hide();
        else
            this.show();
    },

    //// Private methods ////

    // Raises the Dash to the top, so that we can tell if the pointer is above one of its items.
    // We need to do this once the workspaces are shown because the workspaces actor currently covers
    // the whole screen, regardless of where the workspaces are actually displayed.
    //
    // Once we rework the workspaces actor to only cover the area it actually needs, we can
    // remove this workaround. Also http://bugzilla.openedhand.com/show_bug.cgi?id=1513 requests being
    // able to pick only a reactive actor at a certain position, rather than any actor. Being able
    // to do that would allow us to not have to raise the Dash.
    _showDone: function() {
        if (this._hideInProgress)
            return;

        this._dash.actor.raise_top();
        this._dash.actor.remove_clip();

        this.emit('shown');
    },

    _hideDone: function() {
        let global = Shell.Global.get();

        global.window_group.show();

        this._workspaces.destroy();
        this._workspaces = null;

        this._dash.actor.remove_clip();
        this._dash.hide();
        this._group.hide();

        this.visible = false; 
        this._hideInProgress = false;

        Main.endModal();
        this.emit('hidden');
    }
};
Signals.addSignalMethods(Overlay.prototype);

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

Tweener.registerSpecialProperty("clipWidthRight", _clipWidthRightGet, _clipWidthRightSet);

function _clipWidthRightGet(actor) {
    let [xOffset, yOffset, clipWidth, clipHeight] = actor.get_clip();
    return clipWidth;
}

function _clipWidthRightSet(actor, clipWidth) {
    actor.set_clip(0, 0, clipWidth, actor.height);
}
