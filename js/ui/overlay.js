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
const Dash = imports.ui.dash;
const Tweener = imports.ui.tweener;
const Workspaces = imports.ui.workspaces;

const ROOT_OVERLAY_COLOR = new Clutter.Color();
ROOT_OVERLAY_COLOR.from_pixel(0x000000bb);

// The factor to scale the overlay wallpaper with. This should not be less
// than 3/2, because the rule of thirds is used for positioning (see below).
const BACKGROUND_SCALE = 2;

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

const DEFAULT_PADDING = 4;

// Padding around workspace grid / Spacing between Dash and Workspaces
const WORKSPACE_GRID_PADDING = 12;

const COLUMNS_FOR_WORKSPACES_REGULAR_SCREEN = 3;
const ROWS_FOR_WORKSPACES_REGULAR_SCREEN = 6;

const COLUMNS_FOR_WORKSPACES_WIDE_SCREEN = 4;
const ROWS_FOR_WORKSPACES_WIDE_SCREEN = 8;

// A multi-state; PENDING is used during animations
const STATE_ACTIVE = true;
const STATE_PENDING_INACTIVE = false;
const STATE_INACTIVE = false;

const SHADOW_COLOR = new Clutter.Color();
SHADOW_COLOR.from_pixel(0x00000033);
const TRANSPARENT_COLOR = new Clutter.Color();
TRANSPARENT_COLOR.from_pixel(0x00000000);

const SHADOW_WIDTH = 6;

const NUMBER_OF_SECTIONS_IN_SEARCH = 2;

let wideScreen = false;
let displayGridColumnWidth = null;
let displayGridRowHeight = null;

function Overlay() {
    this._init();
}

Overlay.prototype = {
    _init : function() {
        let me = this;

        let global = Shell.Global.get();

        this._group = new Clutter.Group();
        this._group._delegate = this;

        this.visible = false;
        this._hideInProgress = false;

        this._recalculateGridSizes();

        // A scaled root pixmap actor is used as a background. It is zoomed in
        // to the lower right intersection of the lines that divide the image
        // evenly in a 3x3 grid. This is based on the rule of thirds, a
        // compositional rule of thumb in visual arts. The choice for the
        // lower right point is based on a quick survey of GNOME wallpapers.
        this._background = global.create_root_pixmap_actor();
        this._group.add_actor(this._background);

        this._activeDisplayPane = null;

        // Used to catch any clicks when we have an active pane; see the comments
        // in addPane below.
        this._transparentBackground = new Clutter.Rectangle({ opacity: 0,
                                                              reactive: true });
        this._group.add_actor(this._transparentBackground);

        // Draw a semitransparent rectangle over the background for readability.
        this._backOver = new Clutter.Rectangle({ color: ROOT_OVERLAY_COLOR });
        this._group.add_actor(this._backOver);

        this._group.hide();
        global.overlay_group.add_actor(this._group);

        // TODO - recalculate everything when desktop size changes
        this._dash = new Dash.Dash(displayGridColumnWidth);
        this._group.add_actor(this._dash.actor);

        // Container to hold popup pane chrome.
        this._paneContainer = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                            spacing: 6
                                          });
        // Note here we explicitly don't set the paneContainer to be reactive yet; that's done
        // inside the notify::visible handler on panes.
        this._paneContainer.connect('button-release-event', Lang.bind(this, function(background) {
            this._activeDisplayPane.close();
            return true;
        }));
        this._group.add_actor(this._paneContainer);

        this._transparentBackground.lower_bottom();
        this._paneContainer.lower_bottom();

        this._repositionChildren();

        this._workspaces = null;
    },

    _recalculateGridSizes: function () {
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
    },

    _repositionChildren: function () {
        let global = Shell.Global.get();

        let contentHeight = global.screen_height - Panel.PANEL_HEIGHT;

        this._dash.actor.set_position(0, Panel.PANEL_HEIGHT);
        this._dash.actor.set_size(displayGridColumnWidth, contentHeight);

        this._backOver.set_position(0, Panel.PANEL_HEIGHT);
        this._backOver.set_size(global.screen_width, contentHeight);

        let bgPositionFactor = (4 * BACKGROUND_SCALE - 3) / 6;
        this._background.set_size(global.screen_width * BACKGROUND_SCALE,
                                  global.screen_height * BACKGROUND_SCALE);
        this._background.set_position(-global.screen_width * bgPositionFactor,
                                      -global.screen_height * bgPositionFactor);

        this._paneContainer.set_position(this._dash.actor.x + this._dash.actor.width + DEFAULT_PADDING,
                                         Panel.PANEL_HEIGHT);
        // Dynamic width
        this._paneContainer.height = contentHeight;

        this._transparentBackground.set_position(this._paneContainer.x, this._paneContainer.y);
        this._transparentBackground.set_size(global.screen_width - this._paneContainer.x,
                                             this._paneContainer.height);
    },

    addPane: function (pane) {
        pane.actor.width = displayGridColumnWidth * 2;
        this._paneContainer.append(pane.actor, Big.BoxPackFlags.NONE);
        // When a pane is displayed, we raise the transparent background to the top
        // and connect to button-release-event on it, then raise the pane above that.
        // The idea here is that clicking anywhere outside the pane should close it.
        // When the active pane is closed, undo the effect.
        let backgroundEventId = null;
        pane.connect('open-state-changed', Lang.bind(this, function (pane, isOpen) {
            if (isOpen) {
                this._activeDisplayPane = pane;
                this._transparentBackground.raise_top();
                this._paneContainer.raise_top();
                if (backgroundEventId != null)
                    this._transparentBackground.disconnect(backgroundEventId);
                backgroundEventId = this._transparentBackground.connect('button-release-event', Lang.bind(this, function () {
                    this._activeDisplayPane.close();
                    return true;
                }));
            } else if (pane == this._activeDisplayPane) {
                this._activeDisplayPane = null;
                if (backgroundEventId != null) {
                    this._transparentBackground.disconnect(backgroundEventId);
                    backgroundEventId = null;
                }
                this._transparentBackground.lower_bottom();
                this._paneContainer.lower_bottom();
            }
        }));
    },

    //// Draggable target interface ////

    // Closes any active panes if a GenericDisplayItem is being
    // dragged over the overlay, i.e. as soon as it starts being dragged.
    // This allows the user to place the item on any workspace.
    handleDragOver : function(source, actor, x, y, time) {
        if (source instanceof GenericDisplay.GenericDisplayItem
            || source instanceof AppDisplay.WellDisplayItem) {
            if (this._activeDisplayPane != null)
                this._activeDisplayPane.close();
            return true;
        }

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
        if (this._activeDisplayPane != null)
            this._activeDisplayPane.close();
        // lower the panes, so that workspaces display is on top while sliding out
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

    /**
     * activateWindow:
     * @metaWindow: A #MetaWindow
     * @time: Event timestamp integer
     *
     * Make the given MetaWindow be the focus window, switching
     * to the workspace it's on if necessary.  This function
     * should only be used when the overlay is currently active;
     * outside of that, use the relevant methods on MetaDisplay.
     */
    activateWindow: function (metaWindow, time) {
         this._workspaces.activateWindowFromOverlay(metaWindow, time);
         this.hide();
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
