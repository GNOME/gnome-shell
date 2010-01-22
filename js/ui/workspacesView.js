/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Big = imports.gi.Big;
const Clutter = imports.gi.Clutter;
const GdkPixbuf = imports.gi.GdkPixbuf;
const Gdk = imports.gi.Gdk;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Signals = imports.signals;

const DND = imports.ui.dnd;
const Lightbox = imports.ui.lightbox;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const Panel = imports.ui.panel;
const Tweener = imports.ui.tweener;
const Workspace = imports.ui.workspace;

const WORKSPACE_SWITCH_TIME = 0.25;
// Note that mutter has a compile-time limit of 36
const MAX_WORKSPACES = 16;

const GRID_SPACING = 15;

const WorkspacesViewType = {
    SINGLE: 0,
    MOSAIC: 1
};

function GenericWorkspacesView(width, height, x, y, animate) {
    this._init(width, height, x, y, animate);
}

GenericWorkspacesView.prototype = {
    _init: function(width, height, x, y, animate) {
        this.actor = new St.Bin({ style_class: "workspaces" });
        this._actor = new Clutter.Group();

        this.actor.add_actor(this._actor);

        this._width = width;
        this._height = height;
        this._x = x;
        this._y = y;

        this._windowSelectionAppId = null;

        this._workspaces = [];

        this._highlightWindow = null;

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();

        // Create and position workspace objects
        for (let w = 0; w < global.screen.n_workspaces; w++) {
            this._addWorkspaceActor(w);
        }
        this._workspaces[activeWorkspaceIndex].actor.raise_top();
        this._positionWorkspaces();

        // Position/scale the desktop windows and their children after the
        // workspaces have been created. This cannot be done first because
        // window movement depends on the Workspaces object being accessible
        // as an Overview member.
        this._overviewShowingId =
            Main.overview.connect('showing',
                                 Lang.bind(this, function() {
                this._onRestacked();
                for (let w = 0; w < this._workspaces.length; w++)
                    this._workspaces[w].zoomToOverview(animate);
        }));

        // Track changes to the number of workspaces
        this._nWorkspacesNotifyId =
            global.screen.connect('notify::n-workspaces',
                                  Lang.bind(this, this._workspacesChanged));
        this._switchWorkspaceNotifyId =
            global.window_manager.connect('switch-workspace',
                                          Lang.bind(this, this._activeWorkspaceChanged));
        this._restackedNotifyId =
            global.screen.connect('restacked',
                                  Lang.bind(this, this._onRestacked));
    },

    _lookupWorkspaceForMetaWindow: function (metaWindow) {
        for (let i = 0; i < this._workspaces.length; i++) {
            if (this._workspaces[i].containsMetaWindow(metaWindow))
                return this._workspaces[i];
        }
        return null;
    },

    _lookupCloneForMetaWindow: function (metaWindow) {
        for (let i = 0; i < this._workspaces.length; i++) {
            let clone = this._workspaces[i].lookupCloneForMetaWindow(metaWindow);
            if (clone)
                return clone;
        }
        return null;
    },

    setHighlightWindow: function (metaWindow) {
        // Looping over all workspaces is easier than keeping track of the last
        // highlighted window while trying to handle the window or workspace possibly
        // going away.
        for (let i = 0; i < this._workspaces.length; i++) {
            this._workspaces[i].setHighlightWindow(null);
        }
        if (metaWindow != null) {
            let workspace = this._lookupWorkspaceForMetaWindow(metaWindow);
            workspace.setHighlightWindow(metaWindow);
        }
    },

    _clearApplicationWindowSelection: function(reposition) {
        if (this._windowSelectionAppId == null)
            return;
        this._windowSelectionAppId = null;

        for (let i = 0; i < this._workspaces.length; i++) {
            this._workspaces[i].setLightboxMode(false);
            this._workspaces[i].setShowOnlyWindows(null, reposition);
        }
    },

    /**
     * setApplicationWindowSelection:
     * @appid: Application identifier string
     *
     * Enter a mode which shows only the windows owned by the
     * given application, and allow highlighting of a specific
     * window with setHighlightWindow().
     */
    setApplicationWindowSelection: function (appId) {
        if (appId == null) {
            this._clearApplicationWindowSelection(true);
            return;
        }

        if (appId == this._windowSelectionAppId)
            return;

        this._windowSelectionAppId = appId;

        let appSys = Shell.AppSystem.get_default();

        let showOnlyWindows = {};
        let app = appSys.get_app(appId);
        let windows = app.get_windows();
        for (let i = 0; i < windows.length; i++) {
            showOnlyWindows[windows[i]] = 1;
        }

        for (let i = 0; i < this._workspaces.length; i++) {
            this._workspaces[i].setLightboxMode(true);
            this._workspaces[i].setShowOnlyWindows(showOnlyWindows, true);
        }
    },

    /**
     * activateWindowFromOverview:
     * @metaWindow: A #MetaWindow
     * @time: Integer even timestamp
     *
     * This function exits the overview, switching to the given @metaWindow.
     * If an application filter is in effect, it will be cleared.
     */
    activateWindowFromOverview: function (metaWindow, time) {
        if (this._windowSelectionAppId != null) {
            this._clearApplicationWindowSelection(false);
        }

        Main.activateWindow(metaWindow, time);
        Main.overview.hide();
    },

    hide: function() {
        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        let activeWorkspace = this._workspaces[activeWorkspaceIndex];

        this._positionWorkspaces();
        activeWorkspace.actor.raise_top();

        for (let w = 0; w < this._workspaces.length; w++)
            this._workspaces[w].zoomFromOverview();
    },

    destroy: function() {
        for (let w = 0; w < this._workspaces.length; w++)
            this._workspaces[w].destroy();
        this._workspaces = [];

        this.actor.destroy();
        this.actor = null;

        Main.overview.disconnect(this._overviewShowingId);
        global.screen.disconnect(this._nWorkspacesNotifyId);
        global.window_manager.disconnect(this._switchWorkspaceNotifyId);
        global.screen.disconnect(this._restackedNotifyId);
    },

    getScale: function() {
        return this._workspaces[0].scale;
    },

    _onRestacked: function() {
        let stack = global.get_windows();
        let stackIndices = {};

        for (let i = 0; i < stack.length; i++) {
            // Use the stable sequence for an integer to use as a hash key
            stackIndices[stack[i].get_meta_window().get_stable_sequence()] = i;
        }

        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].syncStacking(stackIndices);
    },

    // Handles a drop onto the (+) button; assumes the new workspace
    // has already been added
    acceptNewWorkspaceDrop: function(source, dropActor, x, y, time) {
        return this._workspaces[this._workspaces.length - 1].acceptDrop(source, dropActor, x, y, time);
    },

    // Get the grid position of the active workspace.
    getActiveWorkspacePosition: function() {
        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        let activeWorkspace = this._workspaces[activeWorkspaceIndex];

        return [activeWorkspace.gridX, activeWorkspace.gridY];
    },

    createControllerBar: function() {
        throw new Error("Not implemented");
    },

    _positionWorkspaces: function() {
        throw new Error("Not implemented");
    },

    _workspacesChanged: function() {
        throw new Error("Not implemented");
    },

    _activeWorkspaceChanged: function() {
        throw new Error("Not implemented");
    },

    _addWorkspaceActor: function() {
        throw new Error("Not implemented");
    }
}

function MosaicView(width, height, x, y, animate) {
    this._init(width, height, x, y, animate);
}

MosaicView.prototype = {
    __proto__: GenericWorkspacesView.prototype,

    _init: function(width, height, x, y, animate) {
        GenericWorkspacesView.prototype._init.call(this, width, height, x, y, animate);

        this._workspaces[global.screen.get_active_workspace_index()].setSelected(true);

        this._removeButton = null;
        this._addButton = null;
    },

    // Assign grid positions to workspaces. We can't just do a simple
    // row-major or column-major numbering, because we don't want the
    // existing workspaces to get rearranged when we add a row or
    // column. So we alternate between adding to rows and adding to
    // columns. (So, eg, when going from a 2x2 grid of 4 workspaces to
    // a 3x2 grid of 5 workspaces, the 4 existing workspaces stay
    // where they are, and the 5th one is added to the end of the
    // first row.)
    //
    // FIXME: need to make the metacity internal layout agree with this!
    _positionWorkspaces: function() {
        let gridWidth = Math.ceil(Math.sqrt(this._workspaces.length));
        let gridHeight = Math.ceil(this._workspaces.length / gridWidth);

        let wsWidth = (this._width - (gridWidth - 1) * GRID_SPACING) / gridWidth;
        let wsHeight = (this._height - (gridHeight - 1) * GRID_SPACING) / gridHeight;
        let scale = wsWidth / global.screen_width;

        let span = 1, n = 0, row = 0, col = 0, horiz = true;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            workspace.gridRow = row;
            workspace.gridCol = col;

            workspace.gridX = this._x + workspace.gridCol * (wsWidth + GRID_SPACING);
            workspace.gridY = this._y + workspace.gridRow * (wsHeight + GRID_SPACING);
            workspace.scale = scale;

            if (horiz) {
                col++;
                if (col == span) {
                    row = 0;
                    horiz = false;
                }
            } else {
                row++;
                if (row == span) {
                    col = 0;
                    horiz = true;
                    span++;
                }
            }
        }
    },

    _workspacesChanged: function() {
        let oldNumWorkspaces = this._workspaces.length;
        let newNumWorkspaces = global.screen.n_workspaces;

        if (oldNumWorkspaces == newNumWorkspaces)
            return;

        let oldScale = this._workspaces[0].scale;
        let oldGridWidth = Math.ceil(Math.sqrt(oldNumWorkspaces));
        let oldGridHeight = Math.ceil(oldNumWorkspaces / oldGridWidth);
        let lostWorkspaces = [];

        // The old last workspace is no longer removable.

        if (newNumWorkspaces > oldNumWorkspaces) {
            // Create new workspace groups
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
                this._addWorkspaceActor(w);
            }

        } else {
            // Truncate the list of workspaces
            // FIXME: assumes that the workspaces are being removed from
            // the end of the list, not the start/middle
            lostWorkspaces = this._workspaces.splice(newNumWorkspaces);
        }

        // The new last workspace may be removable
        let newLastWorkspace = this._workspaces[this._workspaces.length - 1];

        // Figure out the new layout
        this._positionWorkspaces();
        let newScale = this._workspaces[0].scale;
        let newGridWidth = Math.ceil(Math.sqrt(newNumWorkspaces));
        let newGridHeight = Math.ceil(newNumWorkspaces / newGridWidth);

        if (newGridWidth != oldGridWidth || newGridHeight != oldGridHeight) {
            // We need to resize/move the existing workspaces/windows
            let existingWorkspaces = Math.min(oldNumWorkspaces, newNumWorkspaces);
            for (let w = 0; w < existingWorkspaces; w++)
                this._workspaces[w].resizeToGrid(oldScale);
        }

        if (newScale != oldScale) {
            // The workspace scale affects window size/positioning because we clamp
            // window size to a 1:1 ratio and never scale them up
            let existingWorkspaces = Math.min(oldNumWorkspaces, newNumWorkspaces);
            for (let w = 0; w < existingWorkspaces; w++)
                this._workspaces[w].positionWindows(Workspace.WindowPositionFlags.ANIMATE);
        }

        if (newNumWorkspaces > oldNumWorkspaces) {
            // Slide new workspaces in from offscreen
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++)
                this._workspaces[w].slideIn(oldScale);
        } else {
            // Slide old workspaces out
            for (let w = 0; w < lostWorkspaces.length; w++) {
                let workspace = lostWorkspaces[w];
                workspace.slideOut(function () { workspace.destroy(); });
            }

            // FIXME: deal with windows on the lost workspaces
        }

        // Reset the selection state; if we went from > 1 workspace to 1,
        // this has the side effect of removing the frame border
        let activeIndex = global.screen.get_active_workspace_index();
        this._workspaces[activeIndex].setSelected(true);

        this._updateButtonsVisibility();
    },

    _activeWorkspaceChanged: function(wm, from, to, direction) {
        this._workspaces[from].setSelected(false);
        this._workspaces[to].setSelected(true);
    },

    _addWorkspaceActor: function(workspaceNum) {
        let workspace  = new Workspace.Workspace(workspaceNum, this._actor);
        this._workspaces[workspaceNum] = workspace;
        this._actor.add_actor(workspace.actor);
    },

    createControllerBar: function() {
        let actor = new St.BoxLayout({ 'pack-start': true });
        let bin = new St.Bin();
        let addButton = new St.Button({ style_class: "single-view-add" });
        this._addButton = addButton;
        addButton.connect('clicked', Lang.bind(this, this._addNewWorkspace));
        addButton._delegate = addButton;
        addButton._delegate.acceptDrop = Lang.bind(this, function(source, actor, x, y, time) {
            return this._acceptNewWorkspaceDrop(source, actor, x, y, time);
        });
        actor.add(bin, { x_align: St.Align.END });
        bin.set_child(addButton);
        bin.set_alignment(St.Align.END, St.Align.START);

        bin = new St.Bin();
        let removeButton = new St.Button({ style_class: "single-view-remove" });
        this._removeButton = removeButton;
        removeButton.connect('clicked', Lang.bind(this, function() {
            if (this._workspaces.length <= 1)
                return;
            global.screen.remove_workspace(this._workspaces[this._workspaces.length - 1]._metaWorkspace, global.get_current_time());
        }));
        actor.add(bin, { expand: true, x_fill: true, x_align: St.Align.END });
        this._updateButtonsVisibility();
        bin.set_child(removeButton);
        bin.set_alignment(St.Align.END, St.Align.START);

        return actor;
    },

    _updateButtonsVisibility: function() {
        //_removeButton may yet not exist.
        if (this._removeButton == null)
            return;
        if (global.screen.n_workspaces == 1)
            this._removeButton.hide();
        else
            this._removeButton.show();
        if (this._addButton == null)
            return;
        if (global.screen.n_workspaces >= MAX_WORKSPACES)
            this._addButton.hide();
        else
            this._addButton.show();
    },

    _addNewWorkspace: function() {
        global.screen.append_new_workspace(false, global.get_current_time());
    },

    _acceptNewWorkspaceDrop: function(source, dropActor, x, y, time) {
        this._addNewWorkspace();
        return this.acceptNewWorkspaceDrop(source, dropActor, x, y, time);
    }
};

// Create a SpecialPropertyModifier to let us move windows in a
// straight line on the screen even though their containing workspace
// is also moving.
Tweener.registerSpecialPropertyModifier("workspace_relative", _workspaceRelativeModifier, _workspaceRelativeGet);

function SingleView(width, height, x, y, animate) {
    this._init(width, height, x, y, animate);
}

SingleView.prototype = {
    __proto__: GenericWorkspacesView.prototype,

    _init: function(width, height, x, y, animate) {
        this._scroll = null;
        GenericWorkspacesView.prototype._init.call(this, width, height, x, y, animate);

        this._actor.set_clip(x, y, width, height);
        this._addButton = null;
        this._removeButton = null;
        this._indicatorsPanel = null;
        this._indicatorsPanelWidth = null;

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        for (let w = 0; w < this._workspaces.length; w++) {
            if (w != activeWorkspaceIndex) {
                this._workspaces[w].actor.hide();
                continue;
            }
            this._workspaces[w].actor.show();
            this._workspaces[w]._windowOverlaysGroup.show();
        }
    },

    _positionWorkspaces: function() {
        let position = global.screen.get_active_workspace_index();
        let scale = this._width / global.screen_width;

        if (this._scroll != null)
            position = this._scroll.adjustment.value;
        let isInt = (Math.round(position) === position);

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            workspace.gridRow = 0;
            workspace.gridCol = 0;

            workspace.scale = scale;
            workspace.actor.set_scale(scale, scale);
            workspace.gridX = this._x + (w - position) * workspace.actor.width;
            workspace.gridY = this._y;
            workspace.actor.set_position(workspace.gridX, workspace.gridY);
            if (isInt) {
                if (this.actor.get_stage() != null)
                   workspace.positionWindows(0);
                if (w == position) {
                    workspace._windowOverlaysGroup.show();
                    workspace.actor.show();
                } else {
                    workspace._windowOverlaysGroup.hide();
                    workspace.actor.hide();
                }
            } else {
                workspace._windowOverlaysGroup.hide();
                if (Math.abs(w - position) <= 1)
                    workspace.actor.show();
                else
                    workspace.actor.hide();
            }
        }
    },

    _workspacesChanged: function() {
        let oldNumWorkspaces = this._workspaces.length;
        let newNumWorkspaces = global.screen.n_workspaces;

        if (oldNumWorkspaces == newNumWorkspaces)
            return;

        if (this._scroll != null) {
            let adj = this._scroll.get_adjustment();
            adj.upper = newNumWorkspaces;
            this._scroll.adjustment = adj;
        }
        let lostWorkspaces = [];

        if (newNumWorkspaces > oldNumWorkspaces) {
            // Create new workspace groups
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
                this._addWorkspaceActor(w);
                this._workspaces[w].actor.hide();
            }

        } else {
            for (let i = 0; i < this._workspaces.length; i++)
                this._workspaces[i].destroy();
            this._actor.remove_all();

            //Without this will be a lot of warnings
            this._actor.hide();

            this._workspaces = [];
            let activeWorkspaceIndex = global.screen.get_active_workspace_index();
            for (let w = 0; w < global.screen.n_workspaces; w++) {
                this._addWorkspaceActor(w);
                if (w == activeWorkspaceIndex) {
                    this._workspaces[w].actor.show();
                } else {
                    this._workspaces[w].actor.hide();
                }
            }
            this._actor.show();
        }
        this._positionWorkspaces();

        // Reset the selection state; if we went from > 1 workspace to 1,
        // this has the side effect of removing the frame border
        let activeIndex = global.screen.get_active_workspace_index();
        this._workspaces[activeIndex].actor.show();
        this._workspaces[activeIndex]._windowOverlaysGroup.show();

        this._updatePanelVisibility();
    },

    _activeWorkspaceChanged: function(wm, from, to, direction) {
        this._updatePanelVisibility();
        let showAnimation = true;

        if (this._scroll != null) {
            let adj = this._scroll.get_adjustment();
            if (Math.round(adj.value - to) != adj.value - to)
                showAnimation = false;
            if (adj.value - to == 0)
                showAnimation = false;
            adj.value = to;
            this._scroll.adjustment = adj;
        }
        if (showAnimation) {
            let fx;
            if (from > to) {
                fx = this._workspaces[0].actor.width;
            } else {
                fx = -this._workspaces[0].actor.width;
            }
            this._workspaces[from]._windowOverlaysGroup.hide();
            this._workspaces[to].actor.set_position(this._x - fx, this._workspaces[to].gridY);
            this._workspaces[to].actor.show();
            Tweener.addTween(this._workspaces[to].actor,
                             { x: this._x,
                               transition: 'easeOutQuad',
                               time: WORKSPACE_SWITCH_TIME
                              });

            Tweener.addTween(this._workspaces[from].actor,
                             { x: this._x + fx,
                               transition: 'easeOutQuad',
                               time: WORKSPACE_SWITCH_TIME,
                               onComplete: this._positionWorkspaces,
                               onCompleteScope: this
                              });
        } else
            this._positionWorkspaces();
    },

    _addWorkspaceActor: function(workspaceNum) {
        let workspace  = new Workspace.Workspace(workspaceNum, this._actor);
        this._actor.add_actor(workspace.actor);
        workspace._windowOverlaysGroup.hide();

        this._workspaces[workspaceNum] = workspace;
    },

    createControllerBar: function() {
        let panel = new St.BoxLayout({ 'pack-start': true, vertical: true });

        let actor = new St.BoxLayout({ 'pack-start': true });
        let adj = new St.Adjustment({ value: global.screen.get_active_workspace_index(),
                                      lower: 0,
                                      'page-increment': 1,
                                      'page-size': 1,
                                      'step-increment': 1,
                                      upper: this._workspaces.length });
        this._scroll = new St.ScrollBar({ adjustment: null, vertical: false, name: 'SwitchScroll' });

        this._scroll.connect('notify::adjustment', Lang.bind(this, function() {
            this._scroll.adjustment.connect('notify::value', Lang.bind(this, function () {
                if (Math.abs(Math.round(this._scroll.adjustment.value) - this._scroll.adjustment.value) < 0.1) {
                    this._scroll.adjustment.set_value (Math.round(this._scroll.adjustment.value));
                    this._workspaces[Math.round(this._scroll.adjustment.value)]._metaWorkspace.activate(global.get_current_time());
                } else
                    this._positionWorkspaces();
            }));
        }));
        this._scroll.adjustment = adj;

        let addButton = new St.Button({ style_class: "single-view-add" });
        this._addButton = addButton;
        addButton.connect('clicked', Lang.bind(this, this._addNewWorkspace));
        addButton._delegate = addButton;
        addButton._delegate.acceptDrop = Lang.bind(this, function(source, actor, x, y, time) {
            return this._acceptNewWorkspaceDrop(source, actor, x, y, time);
        });
        actor.add(addButton, {x_align: St.Align.END, y_align: St.Align.START, 'y-fill': false});

        let removeButton = new St.Button({ style_class: "single-view-remove" });
        this._removeButton = removeButton;
        removeButton.connect('clicked', Lang.bind(this, function() {
            if (this._workspaces.length <= 1)
                return;
            let index = global.screen.get_active_workspace_index();
            if (index == 0)
                return;
            global.screen.remove_workspace(this._workspaces[index]._metaWorkspace, global.get_current_time());
        }));
        actor.add(removeButton, { x_align: St.Align.END, y_align: St.Align.START, 'y-fill': false });
        this._updatePanelVisibility();

        panel.add(this._createPositionalIndicator(), {expand: true, 'x-fill': true, 'y-fill': true});
        panel.add(this._scroll, { expand: true,
                                  'x-fill': true,
                                  'y-fill': false,
                                  y_align: St.Align.START });
        // backward-stepper/forward-stepper has const width (= height)
        let separator = new St.Button({ style_class: 'scroll-separator' });
        actor.add(separator, {});

        actor.add(panel, {expand: true, 'x-fill': true, 'y-fill': true});

        separator = new St.Button({ style_class: 'scroll-separator' });
        actor.add(separator, {});

        return actor;
    },

    _addIndicatorClone: function(i, active) {
        let actor = new St.Button({ style_class: 'workspace-indicator' });
        if (active) {
            actor.style_class = 'workspace-indicator-active';
        }
        actor.connect('button-release-event', Lang.bind(this, function() {
            if (this._workspaces[i] != undefined)
                this._workspaces[i]._metaWorkspace.activate(global.get_current_time());
        }));

       actor.connect('scroll-event', Lang.bind(this, function(actor, event) {
            let direction = event.get_scroll_direction();
            let activeWorkspaceIndex = global.screen.get_active_workspace_index();
            let numWorkspaces = global.screen.n_workspaces;
            if (direction == Clutter.ScrollDirection.UP && activeWorkspaceIndex < numWorkspaces - 1) {
                this._workspaces[activeWorkspaceIndex+1]._metaWorkspace.activate(global.get_current_time());
            } else if (direction == Clutter.ScrollDirection.DOWN && activeWorkspaceIndex > 0) {
                this._workspaces[activeWorkspaceIndex-1]._metaWorkspace.activate(global.get_current_time());
            }
        }));

        this._indicatorsPanel.add_actor(actor);

        let [a, spacing] = actor.get_theme_node().get_length('border-spacing', false);
        if (this._indicatorsPanelWidth < spacing * (i + 1) + actor.width * (i + 1))
            actor.hide();
        actor.x = spacing * i + actor.width * i;
    },

    _fillPositionalIndicator: function() {
        if (this._indicatorsPanel == null || this._indicatorsPanelWidth == null)
            return;
        let width = this._indicatorsPanelWidth;
        this._indicatorsPanel.remove_all();

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        for (let i = 0; i < this._workspaces.length; i++) {
            this._addIndicatorClone(i, i == activeWorkspaceIndex);
        }
        this._indicatorsPanel.x = (this._indicatorsPanelWidth - this._indicatorsPanel.width) / 2;
    },

    _createPositionalIndicator: function() {
        let actor = new St.Bin({ style_class: 'panel-button' });
        let group = new Clutter.Group();

        this._indicatorsPanel = new Shell.GenericContainer();
        this._indicatorsPanel.connect('get-preferred-width', Lang.bind(this, function (actor, fh, alloc) {
            let children = actor.get_children();
            let width = 0;
            for (let i = 0; i < children.length; i++) {
                if (!children[i].visible)
                    continue;
                if (children[i].x + children[i].width <= width)
                    continue;
                width = children[i].x + children[i].width;
            }
            alloc.min_size = width;
            alloc.nat_size = width;
        }));
        this._indicatorsPanel.connect('get-preferred-height', Lang.bind(this, function (actor, fw, alloc) {
            let children = actor.get_children();
            let height = 0;
            if (children.length)
                height = children[0].height;
            alloc.min_size = height;
            alloc.nat_size = height;
        }));
        this._indicatorsPanel.connect('allocate', Lang.bind(this, function (actor, box, flags) {
            let children = actor.get_children();
            for (let i = 0; i < children.length; i++) {
                if (!children[i].visible)
                    continue;
                let childBox = new Clutter.ActorBox();
                childBox.x1 = children[i].x;
                childBox.y1 = 0;
                childBox.x2 = children[i].x + children[i].width;
                childBox.y2 = children[i].height;
                children[i].allocate(childBox, flags);
            }
        }));

        group.add_actor(this._indicatorsPanel);
        actor.set_child(group);
        actor.set_alignment(St.Align.START, St.Align.START);
        actor.set_fill(true, true);
        this._indicatorsPanel.hide();
        actor.connect('notify::width', Lang.bind(this, function(actor) {
            this._indicatorsPanelWidth = actor.width;
            this._updatePanelVisibility();
        }));
        actor.connect('destroy', Lang.bind(this, function() {
            this._indicatorsPanel = null;
        }));
        return actor;
    },

    _updatePanelVisibility: function() {
        let n = global.screen.n_workspaces;
        if (this._removeButton != null) {
            // set opacity here, because if hide it, _scroll will fill this space.
            if (global.screen.get_active_workspace_index() == 0)
                this._removeButton.set_opacity(0);
            else
                this._removeButton.set_opacity(255);
        }
        if (this._addButton != null) {
            // same here
            this._addButton.set_opacity((global.screen.n_workspaces < MAX_WORKSPACES) * 255);
        }
        if (this._scroll != null) {
            if (n > 1)
                this._scroll.show();
            else
                this._scroll.hide();
        }
        if (this._indicatorsPanel != null) {
            if (n == 1) {
                this._indicatorsPanel.hide();
            } else {
                this._indicatorsPanel.show();
            }
        }
        this._fillPositionalIndicator();
    },

    _addNewWorkspace: function() {
        // Button with opacity 0 is clickable.
        if (global.screen.n_workspaces >= MAX_WORKSPACES)
            return;
        global.screen.append_new_workspace(false, global.get_current_time());
        this._workspaces[this._workspaces.length - 1]._metaWorkspace.activate(Clutter.get_current_event_time());
    },

    _acceptNewWorkspaceDrop: function(source, dropActor, x, y, time) {
        this._addNewWorkspace();
        return this.acceptNewWorkspaceDrop(source, dropActor, x, y, time);
    }
};

function _workspaceRelativeModifier(workspace) {
    let [startX, startY] = Main.overview.getPosition();
    let overviewPosX, overviewPosY, overviewScale;

    if (!workspace)
        return [];

    if (workspace.leavingOverview) {
        let [zoomedInX, zoomedInY] = Main.overview.getZoomedInPosition();
        overviewPosX = { begin: startX, end: zoomedInX };
        overviewPosY = { begin: startY, end: zoomedInY };
        overviewScale = { begin: Main.overview.getScale(),
                          end: Main.overview.getZoomedInScale() };
    } else {
        overviewPosX = { begin: startX, end: 0 };
        overviewPosY = { begin: startY, end: 0 };
        overviewScale = { begin: Main.overview.getScale(), end: 1 };
    }

    return [ { name: "x",
               parameters: { workspacePos: workspace.gridX,
                             overviewPos: overviewPosX,
                             overviewScale: overviewScale } },
             { name: "y",
               parameters: { workspacePos: workspace.gridY,
                             overviewPos: overviewPosY,
                             overviewScale: overviewScale } }
           ];
}

function _workspaceRelativeGet(begin, end, time, params) {
    let curOverviewPos = (1 - time) * params.overviewPos.begin +
                         time * params.overviewPos.end;
    let curOverviewScale = (1 - time) * params.overviewScale.begin +
                           time * params.overviewScale.end;

    // Calculate the screen position of the window.
    let screen = (1 - time) *
                 ((begin + params.workspacePos) * params.overviewScale.begin +
                  params.overviewPos.begin) +
                 time *
                 ((end + params.workspacePos) * params.overviewScale.end +
                 params.overviewPos.end);

    // Return the workspace coordinates.
    return (screen - curOverviewPos) / curOverviewScale - params.workspacePos;
}

function WorkspacesViewSwitch() {
    this._init();
}

WorkspacesViewSwitch.prototype = {
    VIEW_KEY: 'view',

    _init: function() {
        this._gconf = Shell.GConf.get_default();
        this._mosaicViewButton = null;
        this._singleViewButton = null;
        this._currentViewType = this._gconf.get_int(this.VIEW_KEY);
        this._controlsBar = null;
    },

    _setView: function(view) {
        this._mosaicViewButton.set_checked(WorkspacesViewType.MOSAIC == view);
        this._singleViewButton.set_checked(WorkspacesViewType.SINGLE == view);

        if (this._currentViewType == view)
            return;
        this._currentViewType = view;
        this._gconf.set_int(this.VIEW_KEY, view);
        this.emit('view-changed');
    },

    createCurrentWorkspaceView: function(width, height, x, y, animate) {
        switch (this._currentViewType) {
            case WorkspacesViewType.SINGLE:
                return new SingleView(width, height, x, y, animate);
            case WorkspacesViewType.MOSAIC:
                return new MosaicView(width, height, x, y, animate);
            default:
                return new MosaicView(width, height, x, y, animate);
        }
    },

    createControlsBar: function() {
        let actor = new St.BoxLayout();

        this._mosaicViewButton = new St.Button({ style_class: "switch-view-mosaic" });
        this._mosaicViewButton.set_toggle_mode(true);
        this._mosaicViewButton.connect('clicked', Lang.bind(this, function() {
            this._setView(WorkspacesViewType.MOSAIC);
        }));
        actor.add(this._mosaicViewButton, {'y-fill' : false, 'y-align' : St.Align.START});

        this._singleViewButton = new St.Button({ style_class: "switch-view-single" });
        this._singleViewButton.set_toggle_mode(true);
        this._singleViewButton.connect('clicked', Lang.bind(this, function() {
            this._setView(WorkspacesViewType.SINGLE);
        }));
        actor.add(this._singleViewButton, {'y-fill' : false, 'y-align' : St.Align.START});

        if (this._currentViewType == WorkspacesViewType.MOSAIC)
            this._mosaicViewButton.set_checked(true);
        else
            this._singleViewButton.set_checked(true);

        this._nWorkspacesNotifyId =
            global.screen.connect('notify::n-workspaces',
                                  Lang.bind(this, this._workspacesChanged));

        actor.connect('destroy', Lang.bind(this, function() {
            this._controlsBar = null;
            global.screen.disconnect(this._nWorkspacesNotifyId);
        }));

        this._controlsBar = actor;
        this._workspacesChanged();
        return actor;
    },

    _workspacesChanged: function() {
        if (this._controlsBar == null)
            return;
        if (global.screen.n_workspaces == 1)
            this._controlsBar.set_opacity(0);
        else
            this._controlsBar.set_opacity(255);
    }
};

Signals.addSignalMethods(WorkspacesViewSwitch.prototype);
