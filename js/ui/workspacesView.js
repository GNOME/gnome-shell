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
        this.actor.connect('style-changed', Lang.bind(this,
            function() {
                let node = this.actor.get_theme_node();
                let [a, spacing] = node.get_length('spacing', false);
                this._spacing = spacing;
                this._positionWorkspaces();
            }));

        this._width = width;
        this._height = height;
        this._x = x;
        this._y = y;
        this._spacing = 0;

        this._windowSelectionAppId = null;

        this._workspaces = [];

        this._highlightWindow = null;

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();

        // Create and position workspace objects
        for (let w = 0; w < global.screen.n_workspaces; w++) {
            this._addWorkspaceActor(w);
        }
        this._workspaces[activeWorkspaceIndex].actor.raise_top();

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

    hide: function() {
        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        let activeWorkspace = this._workspaces[activeWorkspaceIndex];

        if (this._windowSelectionAppId != null)
            this._clearApplicationWindowSelection(false);

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

    _setButtonSensitivity: function(button, sensitive) {
        if (button == null)
            return;
        if (sensitive && !button.reactive) {
            button.reactive = true;
            button.opacity = 255;
        } else if (!sensitive && button.reactive) {
            button.reactive = false;
            button.opacity = 85;
        }
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

        this.actor.style_class = "workspaces mosaic";
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

        let wsWidth = (this._width - (gridWidth - 1) * this._spacing) / gridWidth;
        let wsHeight = (this._height - (gridHeight - 1) * this._spacing) / gridHeight;
        let scale = wsWidth / global.screen_width;

        let span = 1, n = 0, row = 0, col = 0, horiz = true;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            workspace.gridRow = row;
            workspace.gridCol = col;

            workspace.gridX = this._x + workspace.gridCol * (wsWidth + this._spacing);
            workspace.gridY = this._y + workspace.gridRow * (wsHeight + this._spacing);
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
        let addButton = new St.Button({ style_class: "workspace-controls add" });
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
        let removeButton = new St.Button({ style_class: "workspace-controls remove" });
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
        let canRemove = (global.screen.n_workspaces > 1);
        let canAdd = (global.screen.n_workspaces < MAX_WORKSPACES);

        this._setButtonSensitivity(this._removeButton, canRemove);
        this._setButtonSensitivity(this._addButton, canAdd);
    },

    _addNewWorkspace: function() {
        global.screen.append_new_workspace(false, global.get_current_time());
    },

    _acceptNewWorkspaceDrop: function(source, dropActor, x, y, time) {
        this._addNewWorkspace();
        return this.acceptNewWorkspaceDrop(source, dropActor, x, y, time);
    }
};

function SingleView(width, height, x, y, animate) {
    this._init(width, height, x, y, animate);
}

SingleView.prototype = {
    __proto__: GenericWorkspacesView.prototype,

    _init: function(width, height, x, y, animate) {
        GenericWorkspacesView.prototype._init.call(this, width, height, x, y, animate);

        this.actor.style_class = "workspaces single";
        this._actor.set_clip(x, y, width, height);
        this._addButton = null;
        this._removeButton = null;
        this._indicatorsPanel = null;
        this._indicatorsPanelWidth = null;
        this._scroll = null;
        this._lostWorkspaces = [];
        this._scrolling = false;
        this._animatingScroll = false;
    },

    _positionWorkspaces: function() {
        let scale = this._width / global.screen_width;
        let active = global.screen.get_active_workspace_index();

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            workspace.gridRow = 0;
            workspace.gridCol = 0;

            workspace.scale = scale;
            let _width = workspace.actor.width * scale;
            workspace.gridX = this._x + (w - active) * (_width + this._spacing);
            workspace.gridY = this._y;

            workspace.setSelected(false);
        }
    },

    _updateWorkspaceActors: function() {
        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            workspace.actor.set_scale(workspace.scale, workspace.scale);
            workspace.actor.set_position(workspace.gridX, workspace.gridY);
            workspace.positionWindows(0);
        }
    },

    _scrollToActive: function(showAnimation) {
        let active = global.screen.get_active_workspace_index();

        this._scrollWorkspacesToIndex(active, showAnimation);
        this._scrollScrollBarToIndex(active, showAnimation);
    },

    _scrollWorkspacesToIndex: function(index, showAnimation) {
        let active = global.screen.get_active_workspace_index();
        let targetWorkspaceNewX = this._x;
        let targetWorkspaceCurrentX = this._workspaces[index].gridX;
        let dx = targetWorkspaceNewX - targetWorkspaceCurrentX;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            workspace.gridX += dx;
            workspace.actor.show();
            workspace._hideAllOverlays();

            let visible = (w == active);
            if (showAnimation) {
                Tweener.addTween(workspace.actor,
                    { x: workspace.gridX,
                      time: WORKSPACE_SWITCH_TIME,
                      transition: 'easeOutQuad',
                      onComplete: function() {
                          if (visible)
                              workspace._fadeInAllOverlays();
                          else
                              workspace.actor.hide();
                    }});
            } else {
                workspace.actor.x = workspace.gridX;
                if (visible)
                    workspace._fadeInAllOverlays();
                else
                    workspace.actor.hide();
            }
        }

        for (let l = 0; l < this._lostWorkspaces.length; l++) {
            let workspace = this._lostWorkspaces[l];

            workspace.gridX += dx;
            workspace.actor.show();
            workspace._hideAllOverlays();

            if (showAnimation) {
                Tweener.addTween(workspace.actor,
                    { x: workspace.gridX,
                      time: WORKSPACE_SWITCH_TIME,
                      transition: 'easeOutQuad',
                      onComplete: Lang.bind(this, this._cleanWorkspaces)
                    });
            } else {
                this._cleanWorkspaces();
            }
        }
    },

    _cleanWorkspaces: function() {
        if (this._lostWorkspaces.length == 0)
            return;

        for (let l = 0; l < this._lostWorkspaces.length; l++)
            this._lostWorkspaces[l].destroy();
        this._lostWorkspaces = [];

        this._positionWorkspaces();
        this._updateWorkspaceActors();
    },

    _scrollScrollBarToIndex: function(index, showAnimation) {
        if (!this._scroll || this._scrolling)
            return;

        this._animatingScroll = true;

        if (showAnimation) {
            Tweener.addTween(this._scroll.adjustment, {
               value: index,
               time: WORKSPACE_SWITCH_TIME,
               transition: 'easeOutQuad',
               onComplete: Lang.bind(this,
                   function() {
                       this._animatingScroll = false;
                   })
            });
        } else {
            this._scroll.adjustment.value = index;
            this._animatingScroll = false;
        }
    },

    _workspacesChanged: function() {
        let oldNumWorkspaces = this._workspaces.length;
        let newNumWorkspaces = global.screen.n_workspaces;
	let active = global.screen.get_active_workspace_index();

        if (oldNumWorkspaces == newNumWorkspaces)
            return;

        if (this._scroll != null)
            this._scroll.adjustment.upper = newNumWorkspaces;

        if (newNumWorkspaces > oldNumWorkspaces) {
            // Create new workspace groups
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++)
                this._addWorkspaceActor(w);
            this._positionWorkspaces();
            this._updateWorkspaceActors();
            this._scrollScrollBarToIndex(active + 1, false);

        } else {
            let active = global.screen.get_active_workspace_index();
            let removedNum = oldNumWorkspaces - newNumWorkspaces;
            let removedIndex = active + 1;
            this._lostWorkspaces = this._workspaces.splice(removedIndex,
                                                           removedNum);

            // Don't let the user try to select this workspace as it's
            // making its exit.
            for (let l = 0; l < this._lostWorkspaces.length; l++)
                this._lostWorkspaces[l]._desktop.actor.reactive = false;

            // reassign workspaceNum and metaWorkspace, as lost workspaces
            // have not necessarily been removed from the end
            for (let i = removedIndex; i < this._workspaces.length; i++) {
                let metaWorkspace = global.screen.get_workspace_by_index(i);
                this._workspaces[i].workspaceNum = i;
                this._workspaces[i]._metaWorkspace = metaWorkspace;
            }

            this._scrollScrollBarToIndex(active, false);
            this._scrollToActive(true);
        }

        this._updatePanelVisibility();
    },

    _activeWorkspaceChanged: function(wm, from, to, direction) {
        this._updatePanelVisibility();

        if (this._scrolling)
            return;

        this._scrollToActive(true);
    },

    _addWorkspaceActor: function(workspaceNum) {
        let workspace  = new Workspace.Workspace(workspaceNum, this._actor);

        this._actor.add_actor(workspace.actor);
        this._workspaces[workspaceNum] = workspace;
    },

    // handle changes to the scroll bar's adjustment:
    // sync the workspaces' positions to the position of the scroll bar handle
    // and change the active workspace if appropriate
    _onScroll: function(adj) {
        if (this._animatingScroll)
            return;

        let active = global.screen.get_active_workspace_index();
        let current = Math.round(adj.value);

        if (active != current) {
            let metaWorkspace = this._workspaces[current]._metaWorkspace;

            if (!this._scrolling) {
                // This here is a little tricky - we get here when StScrollBar
                // animates paging; we switch the active workspace, but
                // leave out any extra animation (just like we would do when
                // the handle was dragged)
                // If StScrollBar emitted scroll-start before and scroll-stop
                // after the animation, this would not be necessary
                this._scrolling = true;
                metaWorkspace.activate(global.get_current_time());
                this._scrolling = false;
            } else {
                metaWorkspace.activate(global.get_current_time());
            }
        }

        let last = this._workspaces.length - 1;
        let firstWorkspaceX = this._workspaces[0].actor.x;
        let lastWorkspaceX = this._workspaces[last].actor.x;
        let workspacesWidth = lastWorkspaceX - firstWorkspaceX;

        // The scrollbar is hidden when there is only one workspace, so
        // adj.upper should at least be 2 - but better be safe than sorry
        if (adj.upper == 1)
            return;

        let currentX = firstWorkspaceX;
        let newX = this._x - adj.value / (adj.upper - 1) * workspacesWidth;

        let dx = newX - currentX;

        for (let i = 0; i < this._workspaces.length; i++) {
            this._workspaces[i]._hideAllOverlays();
            if (Math.abs(i - adj.value) <= 1)
                this._workspaces[i].actor.show();
            else
                this._workspaces[i].actor.hide();
            this._workspaces[i].actor.x += dx;
        }

        if (!this._scrolling && active == adj.value) {
            // Again, work around the paging in StScrollBar: simulate
            // the effect of scroll-stop
            this._scrolling = true;
            this._scrollToActive(false);
            this._scrolling = false;
        }
    },

    // handle scroll wheel events:
    // activate the next or previous workspace and let the signal handler
    // manage the animation
    _onScrollEvent: function(actor, event) {
        let direction = event.get_scroll_direction();
        let current = global.screen.get_active_workspace_index();
        let last = global.screen.n_workspaces - 1;
        let activate = current;
        if (direction == Clutter.ScrollDirection.DOWN && current < last)
            activate++;
        else if (direction == Clutter.ScrollDirection.UP && current > 0)
            activate--;

        if (activate != current) {
            let metaWorkspace = this._workspaces[activate]._metaWorkspace;
            metaWorkspace.activate(global.get_current_time());
        }
    },

    createControllerBar: function() {
        let panel = new St.BoxLayout({ 'pack-start': true, vertical: true });

        let actor = new St.BoxLayout({ 'pack-start': true });
        let active = global.screen.get_active_workspace_index();
        let adj = new St.Adjustment({ value: active,
                                      lower: 0,
                                      page_increment: 1,
                                      page_size: 1,
                                      step_increment: 0,
                                      upper: this._workspaces.length });
        this._scroll = new St.ScrollBar({ adjustment: adj,
                                          vertical: false,
                                          name: 'SwitchScroll' });

        // we have set adj.step_increment to 0, so all scroll wheel events
        // are processed with this handler - this allows us to animate the
        // workspace switch
        this._scroll.connect('scroll-event',
            Lang.bind(this, this._onScrollEvent));

        this._scroll.adjustment.connect('notify::value',
            Lang.bind(this, this._onScroll));


        this._scroll.connect('scroll-start', Lang.bind(this,
            function() {
                this._scrolling = true;
            }));
        this._scroll.connect('scroll-stop', Lang.bind(this,
            function() {
                this._scrolling = false;
                this._scrollToActive(true);
            }));

        let addButton = new St.Button({ style_class: "workspace-controls add" });
        this._addButton = addButton;
        addButton.connect('clicked', Lang.bind(this, this._addNewWorkspace));
        addButton._delegate = addButton;
        addButton._delegate.acceptDrop = Lang.bind(this, function(source, actor, x, y, time) {
            return this._acceptNewWorkspaceDrop(source, actor, x, y, time);
        });
        actor.add(addButton, {x_align: St.Align.END, y_align: St.Align.START, 'y-fill': false});

        let removeButton = new St.Button({ style_class: "workspace-controls remove" });
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
        actor.add(panel, {expand: true, 'x-fill': true, 'y-fill': true});

        return actor;
    },

    _addIndicatorClone: function(i, active) {
        let actor = new St.Button({ style_class: 'workspace-indicator' });
        if (active) {
            actor.style_class = 'workspace-indicator active';
        }
        actor.connect('clicked', Lang.bind(this, function() {
            if (this._workspaces[i] != undefined)
                this._workspaces[i]._metaWorkspace.activate(global.get_current_time());
        }));

        actor._delegate = {};
        actor._delegate.acceptDrop = Lang.bind(this, function(source, actor, x, y, time) {
            if (this._workspaces[i].acceptDrop(source, actor, x, y, time)) {
                this._workspaces[i]._metaWorkspace.activate(global.get_current_time());
                return true;
            }
            else
                return false;
        });

        actor.connect('scroll-event', Lang.bind(this, this._onScrollEvent));

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
            alloc.natural_size = width;
        }));
        this._indicatorsPanel.connect('get-preferred-height', Lang.bind(this, function (actor, fw, alloc) {
            let children = actor.get_children();
            let height = 0;
            if (children.length)
                height = children[0].height;
            alloc.min_size = height;
            alloc.natural_size = height;
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
        let canRemove = (global.screen.get_active_workspace_index() != 0);
        let canAdd = (global.screen.n_workspaces < MAX_WORKSPACES);

        this._setButtonSensitivity(this._removeButton, canRemove);
        this._setButtonSensitivity(this._addButton, canAdd);

        let showSwitches = (global.screen.n_workspaces > 1);
        if (this._scroll != null) {
            if (showSwitches)
                this._scroll.show();
            else
                this._scroll.hide();
        }
        if (this._indicatorsPanel != null) {
            if (showSwitches)
                this._indicatorsPanel.show();
            else
                this._indicatorsPanel.hide();
        }
        this._fillPositionalIndicator();
    },

    _addNewWorkspace: function() {
        let ws = global.screen.append_new_workspace(false,
                                                    global.get_current_time());
        ws.activate(global.get_current_time());
    },

    _acceptNewWorkspaceDrop: function(source, dropActor, x, y, time) {
        this._addNewWorkspace();
        return this.acceptNewWorkspaceDrop(source, dropActor, x, y, time);
    }
};

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

        this._mosaicViewButton = new St.Button({ style_class: "workspace-controls switch-mosaic" });
        this._mosaicViewButton.set_toggle_mode(true);
        this._mosaicViewButton.connect('clicked', Lang.bind(this, function() {
            this._setView(WorkspacesViewType.MOSAIC);
        }));
        actor.add(this._mosaicViewButton, {'y-fill' : false, 'y-align' : St.Align.START});

        this._singleViewButton = new St.Button({ style_class: "workspace-controls switch-single" });
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
