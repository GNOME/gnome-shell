/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const Lang = imports.lang;
const St = imports.gi.St;

const AppDisplay = imports.ui.appDisplay;
const DocDisplay = imports.ui.docDisplay;
const GenericDisplay = imports.ui.genericDisplay;
const Main = imports.ui.main;
const Panel = imports.ui.panel;
const Dash = imports.ui.dash;
const Tweener = imports.ui.tweener;
const WorkspacesView = imports.ui.workspacesView;

const ROOT_OVERVIEW_COLOR = new Clutter.Color();
ROOT_OVERVIEW_COLOR.from_pixel(0x000000ff);

// Time for initial animation going into Overview mode
const ANIMATION_TIME = 0.25;

// We divide the screen into a grid of rows and columns, which we use
// to help us position the Overview components, such as the side panel
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
// A common netbook resolution is 1024x600, which trips the widescreen
// ratio.  However that leaves way too few pixels for the dash.  So
// just treat this as a regular screen.
const WIDE_SCREEN_MINIMUM_HEIGHT = 768;

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

function Overview() {
    this._init();
}

Overview.prototype = {
    _init : function() {
        this._group = new Clutter.Group();
        this._group._delegate = this;

        this._workspacesViewSwitch = new WorkspacesView.WorkspacesViewSwitch();
        this._workspacesViewSwitch.connect('view-changed', Lang.bind(this, this._onViewChanged));

        this.visible = false;
        this.animationInProgress = false;
        this._hideInProgress = false;

        this._recalculateGridSizes();

        this._activeDisplayPane = null;

        // During transitions, we raise this to the top to avoid having the overview
        // area be reactive; it causes too many issues such as double clicks on
        // Dash elements, or mouseover handlers in the workspaces.
        this._coverPane = new Clutter.Rectangle({ opacity: 0,
                                                  reactive: true });
        this._group.add_actor(this._coverPane);
        this._coverPane.connect('event', Lang.bind(this, function (actor, event) { return true; }));

        // Similar to the cover pane but used for dialogs ("panes"); see the comments
        // in addPane below.
        this._transparentBackground = new Clutter.Rectangle({ opacity: 0,
                                                              reactive: true });
        this._group.add_actor(this._transparentBackground);

        // Background color for the Overview
        this._backOver = new Clutter.Rectangle({ color: ROOT_OVERVIEW_COLOR });
        this._group.add_actor(this._backOver);

        this._group.hide();
        global.overlay_group.add_actor(this._group);

        // TODO - recalculate everything when desktop size changes
        this._dash = new Dash.Dash();
        this._group.add_actor(this._dash.actor);

        // Container to hold popup pane chrome.
        this._paneContainer = new Big.Box({ orientation: Big.BoxOrientation.HORIZONTAL,
                                             spacing: 6 });
        // Note here we explicitly don't set the paneContainer to be reactive yet; that's done
        // inside the notify::visible handler on panes.
        this._paneContainer.connect('button-release-event', Lang.bind(this, function(background) {
            this._activeDisplayPane.close();
            return true;
        }));
        this._group.add_actor(this._paneContainer);

        this._transparentBackground.lower_bottom();
        this._paneContainer.lower_bottom();

        this._coverPane.lower_bottom();

        this._workspaces = null;
    },

    _createControlsBar: function() {
        this._workspacesBar = new St.BoxLayout({ 'pack-start': true,
                                                 style_class: 'workspaces-bar' });
        this._workspacesBar.move_by(this._workspacesBarX, this._workspacesBarY);

        let controlsBar = this._workspacesViewSwitch.createControlsBar();
        let bar = this._workspaces.createControllerBar();
        this._workspacesBar.add(bar, { expand: true, 'x-fill': true, 'y-fill': true,
                                       y_align: St.Align.MIDDLE, x_align: St.Align.START });
        this._workspacesBar.add(controlsBar, {x_align: St.Align.END});
        this._workspacesBar.width = this._workspacesBarWidth;

        this._group.add_actor(this._workspacesBar);
        this._workspacesBar.raise(this._workspaces.actor);
    },

    _onViewChanged: function() {
        if (!this.visible)
            return;
        //Remove old worspacesView
        this._group.remove_actor(this._workspacesBar);
        this._workspaces.hide();
        this._group.remove_actor(this._workspaces.actor);
        this._workspaces.destroy();
        this._workspacesBar.destroy();

        this._workspaces = this._workspacesViewSwitch.createCurrentWorkspaceView(this._workspacesWidth, this._workspacesHeight,
                                                                             this._workspacesX, this._workspacesY, false);

        //Show new workspacesView
        this._group.add_actor(this._workspaces.actor);
        this._dash.actor.raise(this._workspaces.actor);

        this._createControlsBar();

        // Set new position and scale to workspaces.
        this.emit('showing');
    },

    _recalculateGridSizes: function () {
        let primary = global.get_primary_monitor();
        wideScreen = (primary.width/primary.height > WIDE_SCREEN_CUT_OFF_RATIO) &&
                     (primary.height >= WIDE_SCREEN_MINIMUM_HEIGHT);

        // We divide the screen into an imaginary grid which helps us determine the layout of
        // different visual components.
        if (wideScreen) {
            displayGridColumnWidth = Math.floor(primary.width / COLUMNS_WIDE_SCREEN);
            displayGridRowHeight = Math.floor(primary.height / ROWS_WIDE_SCREEN);
        } else {
            displayGridColumnWidth = Math.floor(primary.width / COLUMNS_REGULAR_SCREEN);
            displayGridRowHeight = Math.floor(primary.height / ROWS_REGULAR_SCREEN);
        }
    },

    relayout: function () {
        let primary = global.get_primary_monitor();

        this._recalculateGridSizes();

        this._group.set_position(primary.x, primary.y);

        let contentY = Panel.PANEL_HEIGHT;
        let contentHeight = primary.height - contentY;

        this._coverPane.set_position(0, contentY);
        this._coverPane.set_size(primary.width, contentHeight);

        let workspaceColumnsUsed = wideScreen ? COLUMNS_FOR_WORKSPACES_WIDE_SCREEN : COLUMNS_FOR_WORKSPACES_REGULAR_SCREEN;
        let workspaceRowsUsed = wideScreen ? ROWS_FOR_WORKSPACES_WIDE_SCREEN : ROWS_FOR_WORKSPACES_REGULAR_SCREEN;

        this._workspacesWidth = displayGridColumnWidth * workspaceColumnsUsed
                                  - WORKSPACE_GRID_PADDING * 2;
        // We scale the vertical padding by (primary.height / primary.width)
        // so that the workspace preserves its aspect ratio.
        this._workspacesHeight = Math.floor(displayGridRowHeight * workspaceRowsUsed
                                   - WORKSPACE_GRID_PADDING * (primary.height / primary.width) * 2);

        this._workspacesX = displayGridColumnWidth + WORKSPACE_GRID_PADDING;
        this._workspacesY = Math.floor(displayGridRowHeight + WORKSPACE_GRID_PADDING * (primary.height / primary.width));

        this._dash.actor.set_position(0, contentY);
        this._dash.actor.set_size(displayGridColumnWidth, contentHeight);
        this._dash.searchArea.height = this._workspacesY - contentY;
        this._dash.sectionArea.height = this._workspacesHeight;
        this._dash.searchResults.actor.height = this._workspacesHeight;

        // place the 'Add Workspace' button in the bottom row of the grid
        this._workspacesBarX = this._workspacesX;
        this._workspacesBarWidth = primary.width - this._workspacesBarX - WORKSPACE_GRID_PADDING;
        this._workspacesBarY = primary.height - displayGridRowHeight + 5;

        // The parent (this._group) is positioned at the top left of the primary monitor
        // while this._backOver occupies the entire screen.
        this._backOver.set_position(- primary.x, - primary.y);
        this._backOver.set_size(global.screen_width, global.screen_height);

        this._paneContainer.set_position(this._dash.actor.x + this._dash.actor.width + DEFAULT_PADDING,
                                         this._workspacesY);
        // Dynamic width
        this._paneContainer.height = this._workspacesHeight;

        this._transparentBackground.set_position(this._paneContainer.x, this._paneContainer.y);
        this._transparentBackground.set_size(primary.width - this._paneContainer.x,
                                             this._paneContainer.height);

        if (this._activeDisplayPane != null)
            this._activeDisplayPane.actor.width = displayGridColumnWidth * 2;
    },

    addPane: function (pane) {
        this._paneContainer.append(pane.actor, Big.BoxPackFlags.NONE);
        // When a pane is displayed, we raise the transparent background to the top
        // and connect to button-release-event on it, then raise the pane above that.
        // The idea here is that clicking anywhere outside the pane should close it.
        // When the active pane is closed, undo the effect.
        let backgroundEventId = null;
        pane.connect('open-state-changed', Lang.bind(this, function (pane, isOpen) {
            if (isOpen) {
                pane.actor.width = displayGridColumnWidth * 2;
                this._activeDisplayPane = pane;
                this._transparentBackground.raise_top();
                this._paneContainer.raise_top();
                if (backgroundEventId != null)
                    this._transparentBackground.disconnect(backgroundEventId);
                backgroundEventId = this._transparentBackground.connect('button-release-event', Lang.bind(this, function () {
                    this._activeDisplayPane.close();
                    return true;
                }));
                this._workspaces.actor.opacity = 64;
            } else if (pane == this._activeDisplayPane) {
                this._activeDisplayPane = null;
                if (backgroundEventId != null) {
                    this._transparentBackground.disconnect(backgroundEventId);
                    backgroundEventId = null;
                }
                this._transparentBackground.lower_bottom();
                this._paneContainer.lower_bottom();
                this._workspaces.actor.opacity = 255;
            }
        }));
    },

    //// Draggable target interface ////

    // Closes any active panes if a GenericDisplayItem is being
    // dragged over the Overview, i.e. as soon as it starts being dragged.
    // This allows the user to place the item on any workspace.
    handleDragOver : function(source, actor, x, y, time) {
        if (source instanceof GenericDisplay.GenericDisplayItem
            || source instanceof AppDisplay.AppIcon) {
            if (this._activeDisplayPane != null)
                this._activeDisplayPane.close();
            return true;
        }

        return false;
    },

    //// Public methods ////

    // Returns the scale the Overview has when we just start zooming out
    // to overview mode. That is, when just the active workspace is showing.
    getZoomedInScale : function() {
        return 1 / this._workspaces.getScale();
    },

    // Returns the position the Overview has when we just start zooming out
    // to overview mode. That is, when just the active workspace is showing.
    getZoomedInPosition : function() {
        let [posX, posY] = this._workspaces.getActiveWorkspacePosition();
        let scale = this.getZoomedInScale();

        return [- posX * scale, - posY * scale];
    },

    // Returns the current scale of the Overview.
    getScale : function() {
        return this._group.scaleX;
    },

    // Returns the current position of the Overview.
    getPosition : function() {
        return [this._group.x, this._group.y];
    },

    show : function() {
        if (this.visible)
            return;
        if (!Main.pushModal(this._dash.actor))
            return;

        this.visible = true;
        this.animationInProgress = true;

        this._dash.show();

        /* TODO: make this stuff dynamic */
        this._workspaces = this._workspacesViewSwitch.createCurrentWorkspaceView(this._workspacesWidth, this._workspacesHeight,
                                                                             this._workspacesX, this._workspacesY, true);
        this._group.add_actor(this._workspaces.actor);

        // The workspaces actor is as big as the screen, so we have to raise the dash above it
        // for drag and drop to work.  In the future we should fix the workspaces to not
        // be as big as the screen.
        this._dash.actor.raise(this._workspaces.actor);

        this._createControlsBar();

        // All the the actors in the window group are completely obscured,
        // hiding the group holding them while the Overview is displayed greatly
        // increases performance of the Overview especially when there are many
        // windows visible.
        //
        // If we switched to displaying the actors in the Overview rather than
        // clones of them, this would obviously no longer be necessary.
        global.window_group.hide();
        this._group.show();

        // Create a zoom out effect. First scale the Overview group up and
        // position it so that the active workspace fills up the whole screen,
        // then transform the group to its normal dimensions and position.
        // The opposite transition is used in hide().
        this._group.scaleX = this._group.scaleY = this.getZoomedInScale();
        [this._group.x, this._group.y] = this.getZoomedInPosition();
        let primary = global.get_primary_monitor();
        Tweener.addTween(this._group,
                         { x: primary.x,
                           y: primary.y,
                           scaleX: 1,
                           scaleY: 1,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME,
                           onComplete: this._showDone,
                           onCompleteScope: this
                          });

        // Make Dash fade in so that it doesn't appear to big.
        this._dash.actor.opacity = 0;
        Tweener.addTween(this._dash.actor,
                         { opacity: 255,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME
                         });

        this._coverPane.raise_top();
        this.emit('showing');
    },

    hide: function() {
        if (!this.visible || this._hideInProgress)
            return;

        this.animationInProgress = true;
        this._hideInProgress = true;
        if (this._activeDisplayPane != null)
            this._activeDisplayPane.close();
        this._workspaces.hide();

        this._workspacesBar.destroy();
        this._workspacesBar = null;

        // Create a zoom in effect by transforming the Overview group so that
        // the active workspace fills up the whole screen. The opposite
        // transition is used in show().
        let scale = this.getZoomedInScale();
        let [posX, posY] = this.getZoomedInPosition();
        Tweener.addTween(this._group,
                         { x: posX,
                           y: posY,
                           scaleX: scale,
                           scaleY: scale,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME,
                           onComplete: this._hideDone,
                           onCompleteScope: this
                          });

        // Make Dash fade out so that it doesn't appear to big.
        Tweener.addTween(this._dash.actor,
                         { opacity: 0,
                           transition: 'easeOutQuad',
                           time: ANIMATION_TIME
                         });

        this._coverPane.raise_top();
        this.emit('hiding');
    },

    toggle: function() {
        if (this.visible)
            this.hide();
        else
            this.show();
    },

    /**
     * getWorkspacesForWindow:
     * @metaWindow: A #MetaWindow
     *
     * Returns the Workspaces object associated with the given window.
     * This method is not be accessible if the overview is not open
     * and will return %null.
     */
    getWorkspacesForWindow: function(metaWindow) {
        return this._workspaces;
    },

    /**
     * activateWindow:
     * @metaWindow: A #MetaWindow
     * @time: Event timestamp integer
     *
     * Make the given MetaWindow be the focus window, switching
     * to the workspace it's on if necessary.  This function
     * should only be used when the Overview is currently active;
     * outside of that, use the relevant methods on MetaDisplay.
     */
    activateWindow: function (metaWindow, time) {
         this._workspaces.activateWindowFromOverview(metaWindow, time);
    },

    //// Private methods ////

    _showDone: function() {
        if (this._hideInProgress)
            return;

        this.animationInProgress = false;
        this._coverPane.lower_bottom();

        this.emit('shown');
    },

    _hideDone: function() {
        global.window_group.show();

        this._workspaces.destroy();
        this._workspaces = null;

        this._dash.hide();
        this._group.hide();

        this.visible = false;
        this.animationInProgress = false;
        this._hideInProgress = false;

        this._coverPane.lower_bottom();

        Main.popModal(this._dash.actor);
        this.emit('hidden');
    }
};
Signals.addSignalMethods(Overview.prototype);
