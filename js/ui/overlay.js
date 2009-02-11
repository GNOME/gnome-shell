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
const SIDESHOW_SECTION_MARGIN = 10;
const SIDESHOW_SECTION_LABEL_MARGIN_BOTTOM = 6;
const SIDESHOW_COLUMNS = 1;
const EXPANDED_SIDESHOW_COLUMNS = 2;
const SIDESHOW_SEARCH_BG_COLOR = new Clutter.Color();
SIDESHOW_SEARCH_BG_COLOR.from_pixel(0xffffffff);
const SIDESHOW_TEXT_COLOR = new Clutter.Color();
SIDESHOW_TEXT_COLOR.from_pixel(0xffffffff);

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

const COLUMNS_FOR_WORKSPACES_WIDE_SCREEN = 4;
const ROWS_FOR_WORKSPACES_WIDE_SCREEN = 8;
const WORKSPACES_X_FACTOR_ASIDE_MODE_WIDE_SCREEN = 5 - 0.25;

let wideScreen = false;
let displayGridColumnWidth = null;
let displayGridRowHeight = null;

function Sideshow(parent, width) {
    this._init(parent, width);
}

Sideshow.prototype = {
    _init : function(parent, width) {
        let me = this;

        this._moreMode = false;

        this._width = width - SIDESHOW_PAD;

        let global = Shell.Global.get();
        this.actor = new Clutter.Group();
        parent.add_actor(this.actor);
        let icontheme = Gtk.IconTheme.get_default();
        let rect = new Big.Box({ background_color: SIDESHOW_SEARCH_BG_COLOR,
                                 corner_radius: 4,
                                 x: SIDESHOW_PAD,
                                 y: Panel.PANEL_HEIGHT + SIDESHOW_PAD,
                                 width: this._width,
                                 height: 24});
        this.actor.add_actor(rect);

        let searchIconTexture = new Clutter.Texture({ x: SIDESHOW_PAD + 2,
                                                      y: rect.y + 2 });
        let searchIconPath = icontheme.lookup_icon('gtk-find', 16, 0).get_filename();
        searchIconTexture.set_from_file(searchIconPath);
        this.actor.add_actor(searchIconTexture);

        // We need to initialize the text for the entry to have the cursor displayed 
        // in it. See http://bugzilla.openedhand.com/show_bug.cgi?id=1365
        this._searchEntry = new Clutter.Entry({
                                             font_name: "Sans 14px",
                                             x: searchIconTexture.x
                                                 + searchIconTexture.width + 4,
                                             y: searchIconTexture.y,
                                             width: rect.width - (searchIconTexture.x),
                                             height: searchIconTexture.height, 
                                             text: ""});
        this.actor.add_actor(this._searchEntry);
        this._searchQueued = false;
        this._searchActive = false;
        this._searchEntry.connect('notify::text', function (se, prop) {
            if (me._searchQueued)
                return;
            Mainloop.timeout_add(250, function() {
                // Strip leading and trailing whitespace
                let text = me._searchEntry.text.replace(/^\s+/g, "").replace(/\s+$/g, "");
                me._searchQueued = false;
                me._searchActive = text != '';
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
            let code = e.get_code();
            if (code == 9) {
                // A single escape clears the entry, two of them hides the overlay
                if (me._searchEntry.text == '')
                    me.emit('activated');
                else
                    me._searchEntry.text = '';
                return true;
            } else if (code == 111) {
                // selectUp and selectDown wrap around in their respective displays
                // too, but there doesn't seem to be any flickering if we first select 
                // something in one display, but then unset the selection, and move
                // it to the other display, so it's ok to do that.
                if (me._appDisplay.hasSelected() && !me._appDisplay.selectUp() && me._docDisplay.hasItems()) {
                    me._appDisplay.unsetSelected();
                    me._docDisplay.selectLastItem();
                } else if (me._docDisplay.hasSelected() && !me._docDisplay.selectUp() && me._appDisplay.hasItems()) {
                    me._docDisplay.unsetSelected();
                    me._appDisplay.selectLastItem();
                }
                return true;
            } else if (code == 116) {
                if (me._appDisplay.hasSelected() && !me._appDisplay.selectDown() && me._docDisplay.hasItems()) {
                    me._appDisplay.unsetSelected();
                    me._docDisplay.selectFirstItem();
                } else if (me._docDisplay.hasSelected() && !me._docDisplay.selectDown() && me._appDisplay.hasItems()) {
                    me._docDisplay.unsetSelected();
                    me._appDisplay.selectFirstItem();
                }
                return true;
            }
            return false;
        });

        this._appsText = new Clutter.Label({ color: SIDESHOW_TEXT_COLOR,
                                           font_name: "Sans Bold 14px",
                                           text: "Applications",
                                           x: SIDESHOW_PAD,
                                           y: this._searchEntry.y + this._searchEntry.height + SIDESHOW_SECTION_MARGIN,
                                           height: LABEL_HEIGHT});
        this.actor.add_actor(this._appsText);

        let sectionLabelHeight = LABEL_HEIGHT + SIDESHOW_SECTION_LABEL_MARGIN_BOTTOM;
        let menuY = this._appsText.y + sectionLabelHeight;

        let bottomHeight = displayGridRowHeight / 2;

        // extra LABEL_HEIGHT is for the More link
        this._itemDisplayHeight = global.screen_height - menuY - SIDESHOW_SECTION_MARGIN - sectionLabelHeight - bottomHeight - LABEL_HEIGHT;
        this._appDisplay = new AppDisplay.AppDisplay(this._width, this._itemDisplayHeight / 2, SIDESHOW_COLUMNS, SIDESHOW_PAD);
        this._appDisplay.actor.x = SIDESHOW_PAD;
        this._appDisplay.actor.y = menuY;
        this.actor.add_actor(this._appDisplay.actor);

        this._moreAppsText = new Clutter.Label({ color: SIDESHOW_TEXT_COLOR,
                                               font_name: "Sans Bold 14px",
                                               text: "More...",
                                               y: this._appDisplay.actor.y + this._appDisplay.actor.height,
                                               height: LABEL_HEIGHT,
                                               reactive: true});

        // This sets right-alignment manually.
        this._moreAppsText.x = this._width - this._moreAppsText.width + SIDESHOW_PAD;
        this.actor.add_actor(this._moreAppsText);

        this._docsText = new Clutter.Label({ color: SIDESHOW_TEXT_COLOR,
                                           font_name: "Sans Bold 14px",
                                           text: "Recent Documents",
                                           x: SIDESHOW_PAD,
                                           y: this._moreAppsText.y + this._moreAppsText.height + SIDESHOW_SECTION_MARGIN,
                                           height: LABEL_HEIGHT});
        this.actor.add_actor(this._docsText);

        this._docDisplay = new DocDisplay.DocDisplay(this._width, this._itemDisplayHeight - this._appDisplay.actor.height, SIDESHOW_COLUMNS, SIDESHOW_PAD);
        this._docDisplay.actor.x = SIDESHOW_PAD;
        this._docDisplay.actor.y = this._docsText.y + sectionLabelHeight;
        this.actor.add_actor(this._docDisplay.actor);

        // When we are sliding out documents for the applcations 'More' mode, we need to know what fraction of the 
        // animation time we'll spend sliding out the "Recent Documents" section header, so that we can fully clip
        // the document items in the remaining fraction of time. We do the animation in a linear fashion to make the
        // two stage tweening process look right.
        this._docsTextAnimationTimeRatio = me._docsText.height /  me._docDisplay.actor.height;

        /* Proxy the activated signals */
        this._appDisplay.connect('activated', function(appDisplay) {
            // we allow clicking on an item to launch it, and this unsets the selection
            // so that we can move it to the item that was clicked on
            me._appDisplay.unsetSelected(); 
            me._docDisplay.unsetSelected();
            me._appDisplay.doActivate();
            me.emit('activated');
        });
        this._docDisplay.connect('activated', function(docDisplay) {
            // we allow clicking on an item to launch it, and this unsets the selection
            // so that we can move it to the item that was clicked on
            me._appDisplay.unsetSelected(); 
            me._docDisplay.unsetSelected();
            me._docDisplay.doActivate();
            me.emit('activated');
        });
        this._appDisplay.connect('redisplayed', function(appDisplay) {
            // This can be applicable if app display previously had the selection,
            // but it got updated and now has no items, so we can try to move
            // the selection to the doc display. 
            if (!me._appDisplay.hasSelected() && !me._docDisplay.hasSelected())
                me._docDisplay.selectFirstItem();    
        });
        this._docDisplay.connect('redisplayed', function(docDisplay) {
            if (!me._docDisplay.hasSelected() && !me._appDisplay.hasSelected())
                me._appDisplay.selectFirstItem();
        });

        this._moreAppsText.connect('button-press-event',
            function(o, event) {
                if (me._moreMode) {
                    me._unsetMoreMode();
                }  else {
                    me._setMoreMode();
                }
                return true;
            });
    },

    show: function() {
        let global = Shell.Global.get();

        this._appDisplay.show(); 
        this._docDisplay.show();
        this._appDisplay.selectFirstItem();   
        if (!this._appDisplay.hasSelected())
            this._docDisplay.selectFirstItem();
        else
            this._docDisplay.unsetSelected();
        global.stage.set_key_focus(this._searchEntry);
    },

    hide: function() {
        this._appDisplay.hide(); 
        this._docDisplay.hide();
        this._searchEntry.text = '';
        this._unsetMoreMode();
    },

    // Sets the 'More' mode for browsing applications. Slides down the documents section. Gives more space to 
    // the applications section once sliding of the documents section is completed. 
    _setMoreMode: function() {
        if (this._moreMode)
            return;

        this._moreMode = true;

        if (!this._docDisplay.actor.has_clip)
            this._docDisplay.actor.set_clip(0, 0, this._docDisplay.actor.width, this._docDisplay.actor.height);

        // Move the selection to the applications section if it was in the docs section.
        this._docDisplay.unsetSelected();
        if (!this._appDisplay.hasSelected())
            this._appDisplay.selectFirstItem();

        this._moreAppsText.hide(); 

        Tweener.addTween(this._docDisplay.actor,
                         { y: this._docDisplay.actor.y + this._docDisplay.actor.height,
                           clipHeight: 0,
                           time: ANIMATION_TIME * (1 - this._docsTextAnimationTimeRatio),
                           transition: "linear"
                         });
        Tweener.addTween(this._docsText,
                         { y: this._docsText.y + this._docDisplay.actor.height,
                           time: ANIMATION_TIME * (1 - this._docsTextAnimationTimeRatio),
                           transition: "linear",
                           onComplete: this._removeDocsSection,
                           onCompleteScope: this
                         });
                   
        this.emit('more-activated'); 
    },

    // Unsets the 'More' mode for browsing applications. Updates applications section to have
    // smaller dimensions. Slides in the documents section. 
    _unsetMoreMode: function() {
        if (!this._moreMode)
            return;

        this._moreMode = false;

        this._moreAppsText.hide();  
        this._updateAppsSection();

        this._docDisplay.show();
        Tweener.addTween(this._docsText,
                         { y: this._docsText.y - this._docsText.height,
                           clipHeight: this._docsText.height,
                           time: ANIMATION_TIME * this._docsTextAnimationTimeRatio,
                           transition: "linear",
                           onComplete: this._restoreDocsSection,
                           onCompleteScope: this
                         });   
        this.emit('less-activated');
    },

    // Completes sliding out the documents section and hides it so that it doesn't
    // get updated on new searchs. Once that's completed, updates the dimensions of
    // the applications section.
    _removeDocsSection: function() {
        this._docDisplay.hide();

        Tweener.addTween(this._docsText,
                         { y: this._docsText.y + this._docsText.height,
                           clipHeight: 0,
                           time: ANIMATION_TIME * this._docsTextAnimationTimeRatio,
                           transition: "linear",
                           onComplete: this._updateAppsSection,
                           onCompleteScope: this
                         });        
    },

    // Completes restoring the documents section.
    _restoreDocsSection: function() {                    
        Tweener.addTween(this._docsText,
                         { y: this._docsText.y - this._docDisplay.actor.height,
                           time: ANIMATION_TIME * (1 - this._docsTextAnimationTimeRatio),
                           transition: "linear"
                         }); 
        Tweener.addTween(this._docDisplay.actor,
                         { y: this._docDisplay.actor.y - this._docDisplay.actor.height,
                           clipHeight: this._docDisplay.actor.height,
                           time: ANIMATION_TIME * (1 - this._docsTextAnimationTimeRatio),
                           transition: "linear",
                           onComplete: this._onDocsSectionRestored,
                           onCompleteScope: this
                         });   
    },

    // Selects the first item in the documents section if applications section has no items.
    _onDocsSectionRestored: function() { 
        if (!this._appDisplay.hasItems())
            this._docDisplay.selectFirstItem();
    },

    // Updates the applications section display and the 'More...' or 'Less...' control associated with it
    // depending on the current mode. This function must only be called once after the 'More' mode has been
    // changed, which is ensured by _setMoreMode() and _unsetMoreMode() functions. 
    _updateAppsSection: function() {
        let additionalWidth = ((this._width + SIDESHOW_PAD) / SIDESHOW_COLUMNS) * 
                              (EXPANDED_SIDESHOW_COLUMNS - SIDESHOW_COLUMNS);
        if (this._moreMode) {
            this._appDisplay.updateDimensions(this._width + additionalWidth, 
                                              this._itemDisplayHeight + LABEL_HEIGHT * 2 + SIDESHOW_SECTION_LABEL_MARGIN_BOTTOM,
                                              EXPANDED_SIDESHOW_COLUMNS);
            this._moreAppsText.x = this._moreAppsText.x + additionalWidth;
            this._moreAppsText.y = this._appDisplay.actor.y + this._appDisplay.actor.height;
            this._moreAppsText.text = "Less...";
        } else {
            this._appDisplay.updateDimensions(this._width, this._itemDisplayHeight / 2, SIDESHOW_COLUMNS);
            this._moreAppsText.x = this._moreAppsText.x - additionalWidth;
            this._moreAppsText.y = this._appDisplay.actor.y + this._appDisplay.actor.height;
            this._moreAppsText.text = "More...";
        }
        this._moreAppsText.show(); 
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
        this.visible = false;

        let background = new Clutter.Rectangle({ color: OVERLAY_BACKGROUND_COLOR,
                                                 reactive: true,
                                                 x: 0,
                                                 y: Panel.PANEL_HEIGHT,
                                                 width: global.screen_width,
                                                 height: global.screen_width - Panel.PANEL_HEIGHT });
        this._group.add_actor(background);

        this._group.hide();
        global.overlay_group.add_actor(this._group);

        // TODO - recalculate everything when desktop size changes
        let sideshowWidth = displayGridColumnWidth;  
         
        this._sideshow = new Sideshow(this._group, sideshowWidth);
        this._workspaces = null;
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
            }    
        });
        this._sideshow.connect('less-activated', function(sideshow) {
            if (me._workspaces != null) {
                let workspacesX = displayGridColumnWidth + WORKSPACE_GRID_PADDING;
                me._workspaces.addButton.show();
                me._workspaces.updatePosition(workspacesX, null);
            }
        });
    },

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

        this._workspaces = new Workspaces.Workspaces(workspacesWidth, workspacesHeight, workspacesX, workspacesY, 
                                                     addButtonSize, addButtonX, addButtonY);
        this._group.add_actor(this._workspaces.actor);
        this._workspaces.actor.raise_top();

        // All the the actors in the window group are completely obscured,
        // hiding the group holding them while the overlay is displayed greatly
        // increases performance of the overlay especially when there are many
        // windows visible.
        //
        // If we switched to displaying the actors in the overlay rather than
        // clones of them, this would obviously no longer be necessary.
        global.window_group.hide();
        this._group.show();
    },

    hide : function() {
        if (!this.visible)
            return;

        this._workspaces.hide();

        // Dummy tween, just waiting for the workspace animation
        Tweener.addTween(this,
                         { time: ANIMATION_TIME,
                           onComplete: this._hideDone,
                           onCompleteScope: this
                         });
    },

    _hideDone: function() {
        let global = Shell.Global.get();

        this.visible = false;
        global.window_group.show();

        this._workspaces.destroy();
        this._workspaces = null;

        this._sideshow.hide();
        this._group.hide();
    },

    _deactivate : function() {
        Main.hide_overlay();
    }
};

Tweener.registerSpecialProperty("clipHeight", _clipHeightGet, _clipHeightSet);

function _clipHeightGet(actor) {
    let [xOffset, yOffset, clipHeight, clipWidth] = actor.get_clip();
    return clipHeight;
}

function _clipHeightSet(actor, clipHeight) {
    actor.set_clip(0, 0, actor.width, clipHeight);
}
