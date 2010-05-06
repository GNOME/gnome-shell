/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

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

// The values here are also used for gconf, and the key and value
// names must match
const WorkspacesViewType = {
    SINGLE: 'single',
    GRID:   'grid'
};
const WORKSPACES_VIEW_KEY = 'overview/workspaces_view';

const WORKSPACE_DRAGGING_SCALE = 0.85;
const WORKSPACE_SHADOW_SCALE = (1 - WORKSPACE_DRAGGING_SCALE) / 2;

function GenericWorkspacesView(width, height, x, y, workspaces) {
    this._init(width, height, x, y, workspaces);
}

GenericWorkspacesView.prototype = {
    _init: function(width, height, x, y, workspaces) {
        this.actor = new St.Bin({ style_class: "workspaces" });
        this._actor = new Clutter.Group();

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this.actor.add_actor(this._actor);
        this.actor.connect('style-changed', Lang.bind(this,
            function() {
                let node = this.actor.get_theme_node();
                let [a, spacing] = node.get_length('spacing', false);
                this._spacing = spacing;
                if (Main.overview.animationInProgress)
                    this._computeWorkspacePositions();
                else
                    this._transitionWorkspaces();
            }));

        this._width = width;
        this._height = height;
        this._x = x;
        this._y = y;
        this._spacing = 0;

        this._windowSelectionAppId = null;
        this._highlightWindow = null;

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        this._workspaces = workspaces;

        // Add workspace actors
        for (let w = 0; w < global.screen.n_workspaces; w++)
            this._workspaces[w].actor.reparent(this._actor);
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
                    this._workspaces[w].zoomToOverview();
        }));

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
        this.actor.destroy();
    },

    _onDestroy: function() {
        Main.overview.disconnect(this._overviewShowingId);
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

    canAddWorkspace: function() {
        return global.screen.n_workspaces < MAX_WORKSPACES;
    },

    addWorkspace: function() {
        throw new Error("Not implemented");
    },

    canRemoveWorkspace: function() {
        throw new Error("Not implemented");
    },

    removeWorkspace: function() {
        throw new Error("Not implemented");
    },

    updateWorkspaces: function() {
        throw new Error("Not implemented");
    },

    _transitionWorkspaces: function() {
        throw new Error("Not implemented");
    },

    _computeWorkspacePositions: function() {
        throw new Error("Not implemented");
    },

    _activeWorkspaceChanged: function() {
        throw new Error("Not implemented");
    },

    _acceptNewWorkspaceDrop: function(source, dropActor, x, y, time) {
        this.addWorkspace();
        return this.acceptNewWorkspaceDrop(source, dropActor, x, y, time);
    }
};

function MosaicView(width, height, x, y, workspaces) {
    this._init(width, height, x, y, workspaces);
}

MosaicView.prototype = {
    __proto__: GenericWorkspacesView.prototype,

    _init: function(width, height, x, y, workspaces) {
        GenericWorkspacesView.prototype._init.call(this, width, height, x, y, workspaces);

        this.actor.add_style_class_name('mosaic');
        this._actor.set_clip(x - Workspace.FRAME_SIZE,
                             y - Workspace.FRAME_SIZE,
                             width + 2 * Workspace.FRAME_SIZE,
                             height + 2 * Workspace.FRAME_SIZE);
        this._workspaces[global.screen.get_active_workspace_index()].setSelected(true);
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
    _computeWorkspacePositions: function() {
        let gridWidth = Math.ceil(Math.sqrt(this._workspaces.length));
        let gridHeight = Math.ceil(this._workspaces.length / gridWidth);

        // adjust vertical spacing so workspaces can preserve their aspect
        // ratio without exceeding this._height
        let verticalSpacing = this._spacing * this._height / this._width;

        let wsWidth = (this._width - (gridWidth - 1) * this._spacing) / gridWidth;
        let wsHeight = (this._height - (gridHeight - 1) * verticalSpacing) / gridHeight;
        let scale = wsWidth / global.screen_width;

        let span = 1, n = 0, row = 0, col = 0, horiz = true;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            workspace.gridRow = row;
            workspace.gridCol = col;

            workspace.gridX = this._x + workspace.gridCol * (wsWidth + this._spacing);
            workspace.gridY = this._y + workspace.gridRow * (wsHeight + verticalSpacing);
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

    _transitionWorkspaces: function() {
        // update workspace parameters
        this._computeWorkspacePositions();

        let active = global.screen.get_active_workspace_index();
        let activeWorkspace = this._workspaces[active];
        // scale is the factor needed to translate from the new scale
        // (this view) to the currently active scale (previous view)
        let scale = this._workspaces[0].actor.scale_x / activeWorkspace.scale;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];
            let originX, originY;
            let dx, dy;

            // The correct transition would be a straightforward animation
            // of each workspace's old position/scale to the new one;
            // however, this looks overly busy, so we only use a zoom effect.
            // Unfortunately this implies that we cannot pretend to not knowing
            // the other view's layout at this point:
            // We position the workspaces in the grid, which we scale up so
            // that the active workspace fills the viewport.
            dx = workspace.gridX - activeWorkspace.gridX;
            dy = workspace.gridY - activeWorkspace.gridY;
            originX = this._x + scale * dx;
            originY = this._y + scale * dy;

            workspace.actor.set_position(originX, originY);

            workspace.positionWindows(Workspace.WindowPositionFlags.ANIMATE);
            workspace.setSelected(false);
            workspace.hideWindowsOverlays();

            Tweener.addTween(workspace.actor,
                             { x: workspace.gridX,
                               y: workspace.gridY,
                               scale_x: workspace.scale,
                               scale_y: workspace.scale,
                               time: Overview.ANIMATION_TIME,
                               transition: 'easeOutQuad',
                               onComplete: function() {
                                   workspace.zoomToOverview(false);
                                   if (workspace.metaWorkspace.index() == active)
                                       workspace.setSelected(true);
                             }});
        }
    },

    updateWorkspaces: function(oldNumWorkspaces, newNumWorkspaces, lostWorkspaces) {
        let oldScale = this._workspaces[0].scale;
        let oldGridWidth = Math.ceil(Math.sqrt(oldNumWorkspaces));
        let oldGridHeight = Math.ceil(oldNumWorkspaces / oldGridWidth);

        // Add actors
        if (newNumWorkspaces > oldNumWorkspaces)
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++)
                this._actor.add_actor(this._workspaces[w].actor);

        // Figure out the new layout
        this._computeWorkspacePositions();
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
            // New workspaces can contain windows.
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
                this._workspaces[w].positionWindows(0);
                this._workspaces[w].slideIn(oldScale);
            }
        } else {
            // Slide old workspaces out
            for (let w = 0; w < lostWorkspaces.length; w++) {
                let workspace = lostWorkspaces[w];
                workspace.slideOut(function () { workspace.destroy(); });
            }
        }

        // Reset the selection state; if we went from > 1 workspace to 1,
        // this has the side effect of removing the frame border
        let activeIndex = global.screen.get_active_workspace_index();
        this._workspaces[activeIndex].setSelected(true);
    },

    _activeWorkspaceChanged: function(wm, from, to, direction) {
        this._workspaces[from].setSelected(false);
        this._workspaces[to].setSelected(true);
    },

    createControllerBar: function() {
        return null;
    },

    addWorkspace: function() {
        global.screen.append_new_workspace(false, global.get_current_time());
    },

    canRemoveWorkspace: function() {
        return global.screen.n_workspaces > 1;
    },

    removeWorkspace: function() {
        let removedIndex = this._workspaces.length - 1;
        let metaWorkspace = this._workspaces[removedIndex].metaWorkspace;
        global.screen.remove_workspace(metaWorkspace,
                                       global.get_current_time());
    }
};

function NewWorkspaceArea() {
    this._init();
}

NewWorkspaceArea.prototype = {
    _init: function() {
        let width = Math.ceil(global.screen_width * WORKSPACE_SHADOW_SCALE);
        this.actor = new Clutter.Group({ width: width,
                                         height: global.screen_height,
                                         x: global.screen_width });

        this._child1 = new St.Bin({ style_class: 'new-workspace-area',
                                    width: width,
                                    height: global.screen_height });
        this._child2 =  new St.Bin({ style_class: 'new-workspace-area-internal',
                                     width: width,
                                     height: global.screen_height,
                                     reactive: true });
        this.actor.add_actor(this._child1);
        this.actor.add_actor(this._child2);
    },

    setStyle: function(isHover) {
        this._child1.set_hover(isHover);
    }
};

function WorkspaceIndicator(activateWorkspace, workspaceAcceptDrop, scrollEventCb) {
    this._init(activateWorkspace, workspaceAcceptDrop, scrollEventCb);
}

WorkspaceIndicator.prototype = {
    _init: function(activateWorkspace, workspaceAcceptDrop, scrollEventCb) {
        this._activateWorkspace = activateWorkspace;
        this._workspaceAcceptDrop = workspaceAcceptDrop;
        this._scrollEventCb = scrollEventCb;
        let actor = new St.Bin({ style_class: 'panel-button' });

        this._indicatorsPanel = new Shell.GenericContainer();
        this._indicatorsPanel.connect('get-preferred-width', Lang.bind(this, this._getPreferredWidth));
        this._indicatorsPanel.connect('get-preferred-height', Lang.bind(this, this._getPreferredHeight));
        this._indicatorsPanel.connect('allocate', Lang.bind(this, this._allocate));
        this._indicatorsPanel.clip_to_allocation = true;

        actor.set_child(this._indicatorsPanel);
        actor.set_alignment(St.Align.MIDDLE, St.Align.MIDDLE);
        this._indicatorsPanel.hide();
        actor.connect('destroy', Lang.bind(this, this._onDestroy));

        let workId = Main.initializeDeferredWork(actor, Lang.bind(this, this._workspacesChanged));
        this._nWorkspacesNotifyId =
            global.screen.connect('notify::n-workspaces', function() {
                Main.queueDeferredWork(workId);
            });
        this._switchWorkspaceNotifyId =
            global.window_manager.connect('switch-workspace', function() {
                Main.queueDeferredWork(workId);
            });

        this.actor = actor;
    },

    _workspacesChanged: function() {
        let active = global.screen.get_active_workspace_index();
        let n = global.screen.n_workspaces;
        if (n > 1)
            this._indicatorsPanel.show();
        else
            this._indicatorsPanel.hide();
        this._fillPositionalIndicator();
    },

    _onDestroy: function() {
        if (this._nWorkspacesNotifyId > 0)
            global.screen.disconnect(this._nWorkspacesNotifyId);
        this._nWorkspacesNotifyId = 0;
        if (this._switchWorkspaceNotifyId > 0)
            global.window_manager.disconnect(this._switchWorkspaceNotifyId);
        this._switchWorkspaceNotifyId = 0;
    },

    _allocate: function(actor, box, flags) {
        let children = actor.get_children();
        let childBox = new Clutter.ActorBox();
        for (let i = 0; i < children.length; i++) {
            childBox.x1 = children[i].x;
            childBox.y1 = 0;
            childBox.x2 = children[i].x + children[i].width;
            childBox.y2 = children[i].height;
            children[i].allocate(childBox, flags);
        }
    },

    _getPreferredWidth: function(actor, fh, alloc) {
        let children = actor.get_children();
        let width = 0;
        for (let i = 0; i < children.length; i++) {
            if (children[i].x + children[i].width <= width)
                continue;
            width = children[i].x + children[i].width;
        }
        alloc.min_size = 0;
        alloc.natural_size = width;
    },

    _getPreferredHeight: function(actor, fw, alloc) {
            let children = actor.get_children();
            let height = 0;
            if (children.length)
                height = children[0].height;
            alloc.min_size = height;
            alloc.natural_size = height;
    },

    _addIndicatorClone: function(i, active) {
        let actor = new St.Button({ style_class: 'workspace-indicator' });
        if (active) {
            actor.style_class = 'workspace-indicator active';
        }
        actor.connect('clicked', Lang.bind(this, function() {
            this._activateWorkspace(i);
        }));

        actor._delegate = {};
        actor._delegate.acceptDrop = Lang.bind(this, function(source, actor, x, y, time) {
            if (this._workspaceAcceptDrop(i, source, actor, x, y, time)) {
                this._activateWorkspace(i);
                return true;
            }
            else
                return false;
        });

        actor.connect('scroll-event', this._scrollEventCb);

        this._indicatorsPanel.add_actor(actor);

        let [a, spacing] = actor.get_theme_node().get_length('border-spacing', false);
        actor.x = spacing * i + actor.width * i;
    },

    _fillPositionalIndicator: function() {
        this._indicatorsPanel.remove_all();

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        let n = global.screen.n_workspaces;
        for (let i = 0; i < n; i++) {
            this._addIndicatorClone(i, i == activeWorkspaceIndex);
        }
    }
};

function SingleView(width, height, x, y, workspaces) {
    this._init(width, height, x, y, workspaces);
}

SingleView.prototype = {
    __proto__: GenericWorkspacesView.prototype,

    _init: function(width, height, x, y, workspaces) {
        this._newWorkspaceArea = new NewWorkspaceArea();
        this._leftShadow = new St.Bin({ style_class: 'left-workspaces-shadow',
                                        width: Math.ceil(global.screen_width * WORKSPACE_SHADOW_SCALE),
                                        height: global.screen_height,
                                        x: global.screen_width })
        this._rightShadow = new St.Bin({ style_class: 'right-workspaces-shadow',
                                         width: Math.ceil(global.screen_width * WORKSPACE_SHADOW_SCALE),
                                         height: global.screen_height,
                                         x: global.screen_width })

        GenericWorkspacesView.prototype._init.call(this, width, height, x, y, workspaces);

        for (let i = 0; i < this._workspaces.length; i++) {
            this._workspaces[i]._windowDragBeginId = this._workspaces[i].connect('window-drag-begin',
                                                                                 Lang.bind(this, this._onWindowDragBegin));
            this._workspaces[i]._windowDragEndId = this._workspaces[i].connect('window-drag-end',
                                                                               Lang.bind(this, this._onWindowDragEnd));
        }

        this._actor.add_actor(this._newWorkspaceArea.actor);
        this._actor.add_actor(this._leftShadow);
        this._actor.add_actor(this._rightShadow);

        this.actor.add_style_class_name('single');
        this._actor.set_clip(x, y, width, height);
        this._activeWorkspaceX = 0; // x offset of active ws while dragging
        this._activeWorkspaceY = 0; // y offset of active ws while dragging
        this._scroll = null;
        this._lostWorkspaces = [];
        this._animating = false; // tweening
        this._scrolling = false; // dragging scroll bar or desktop
        this._animatingScroll = false; // programatically move the scroll bar
        this._inDrag = false; // dragging a window

        let primary = global.get_primary_monitor();
        this._dropGroup = new Clutter.Group({ x: 0, y: 0,
                                              width: primary.width,
                                              height: primary.height });
        this._dropGroup._delegate = this;
        global.stage.add_actor(this._dropGroup);
        this._dropGroup.lower_bottom();
        this._dragIndex = -1;

        this._buttonPressId = 0;
        this._capturedEventId = 0;
        this._timeoutId = 0;
    },

    // Compute the position, scale and opacity of the workspaces, but don't
    // actually change the actors to match
    _computeWorkspacePositions: function() {
        let active = global.screen.get_active_workspace_index();

        let scale = this._width / global.screen_width;
        if (this._inDrag)
            scale *= WORKSPACE_DRAGGING_SCALE;

        this._setWorkspaceDraggable(active, true);

        let _width = this._workspaces[0].actor.width * scale;
        let _height = this._workspaces[0].actor.height * scale;

        this._activeWorkspaceX = (this._width - _width) / 2;
        this._activeWorkspaceY = (this._height - _height) / 2;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            workspace.opacity = (this._inDrag && w != active) ? 200 : 255;

            workspace.gridRow = 0;
            workspace.gridCol = 0;

            workspace.scale = scale;
            workspace.gridX = this._x + this._activeWorkspaceX
                              + (w - active) * (_width + this._spacing);
            workspace.gridY = this._y + this._activeWorkspaceY;

            workspace.setSelected(false);
        }

        this._newWorkspaceArea.scale = scale;
        this._newWorkspaceArea.gridX = this._x + this._activeWorkspaceX
                                       + (this._workspaces.length - active) * (_width + this._spacing);
        this._newWorkspaceArea.gridY = this._y + this._activeWorkspaceY;

        this._leftShadow.scale = scale;
        this._leftShadow.gridX = this._x + this._activeWorkspaceX
                                 - (this._leftShadow.width * scale + this._spacing);
        this._leftShadow.gridY = this._y + this._activeWorkspaceY;

        this._rightShadow.scale = scale;
        this._rightShadow.gridX = this._x + this._activeWorkspaceX
                                  + (_width + this._spacing);
        this._rightShadow.gridY = this._y + this._activeWorkspaceY;
    },

    _transitionWorkspaces: function() {
        // update workspace parameters
        this._computeWorkspacePositions();

        let active = global.screen.get_active_workspace_index();
        let activeActor = this._workspaces[active].actor;
        // scale is the factor needed to translate from the currently
        // active scale (previous view) to the new scale (this view)
        let scale = this._workspaces[active].scale / activeActor.scale_x;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];
            let targetX, targetY;

            // The correct transition would be a straightforward animation
            // of each workspace's old position/scale to the new one;
            // however, this looks overly busy, so we only use a zoom effect.
            // Therefore we scale up each workspace's distance to the active
            // workspace, so the latter fills the viewport while the other
            // workspaces maintain their relative position
            targetX = this._x + scale * (workspace.actor.x - activeActor.x);
            targetY = this._y + scale * (workspace.actor.y - activeActor.y);

            workspace.positionWindows(Workspace.WindowPositionFlags.ANIMATE);
            workspace.setSelected(false);
            workspace._hideAllOverlays();

            Tweener.addTween(workspace.actor,
                             { x: targetX,
                               y: targetY,
                               scale_x: workspace.scale,
                               scale_y: workspace.scale,
                               time: Overview.ANIMATION_TIME,
                               transition: 'easeOutQuad',
                               onComplete: function() {
                                   workspace.zoomToOverview(false);
                             }});
        }
    },

    _scrollToActive: function(showAnimation) {
        let active = global.screen.get_active_workspace_index();

        this._updateWorkspaceActors(showAnimation);
        this._scrollScrollBarToIndex(active, showAnimation);
    },

    // _setWorkspaceDraggable:
    // @index: workspace index
    // @draggable: whether workspace @index should be draggable
    //
    // If @draggable is %true, set up workspace @index to allow switching
    // workspaces by dragging the desktop - if a draggable workspace has
    // been set up before, it will be reset before the new one is made
    // draggable.
    // If @draggable is %false, workspace @index is reset to no longer allow
    // dragging.
    _setWorkspaceDraggable: function(index, draggable) {
        if (index < 0 || index >= global.n_workspaces)
            return;

        let dragActor = this._workspaces[index]._desktop.actor;

        if (draggable) {
            this._workspaces[index].actor.reactive = true;

            // reset old draggable workspace
            if (this._dragIndex > -1)
                this._setWorkspaceDraggable(this._dragIndex, false);

            this._dragIndex = index;
            this._buttonPressId = dragActor.connect('button-press-event',
                                      Lang.bind(this, this._onButtonPress));
        } else {
            this._dragIndex = -1;

            if (this._buttonPressId > 0) {
                if (dragActor.get_stage())
                    dragActor.disconnect(this._buttonPressId);
                this._buttonPressId = 0;
            }

            if (this._capturedEventId > 0) {
                global.stage.disconnect(this._capturedEventId);
                this._capturedEventId = 0;
            }
        }
    },

    // start dragging the active workspace
    _onButtonPress: function(actor, event) {
        if (this._dragIndex == -1)
            return;

        let [stageX, stageY] = event.get_coords();
        this._dragStartX = this._dragX = stageX;
        this._scrolling = true;
        this._capturedEventId = global.stage.connect('captured-event',
            Lang.bind(this, this._onCapturedEvent));
    },

    // handle captured events while dragging a workspace
    _onCapturedEvent: function(actor, event) {
        let active = global.screen.get_active_workspace_index();
        let stageX, stageY;

        switch (event.type()) {
            case Clutter.EventType.BUTTON_RELEASE:
                this._scrolling = false;

                [stageX, stageY] = event.get_coords();

                // default to snapping back to the original workspace
                let activate = this._dragIndex;
                let last = global.screen.n_workspaces - 1;

                // switch workspaces according to the drag direction
                if (stageX > this._dragStartX && activate > 0)
                    activate--;
                else if (stageX < this._dragStartX && activate < last)
                    activate++;

                if (activate != active) {
                    let workspace = this._workspaces[activate].metaWorkspace;
                    workspace.activate(global.get_current_time());
                } else {
                    this._scrollToActive(true);
                }

                if (stageX == this._dragStartX)
                    // no motion? It's a click!
                    return false;

                return true;

            case Clutter.EventType.MOTION:
                [stageX, stageY] = event.get_coords();
                let dx = this._dragX - stageX;
                let primary = global.get_primary_monitor();

                this._scroll.adjustment.value += (dx / primary.width);
                this._dragX = stageX;

                return true;
        }

        return false;
    },

    // Update workspace actors parameters to the values calculated in
    // _computeWorkspacePositions()
    // @showAnimation: iff %true, transition between states
    _updateWorkspaceActors: function(showAnimation) {
        let active = global.screen.get_active_workspace_index();
        let targetWorkspaceNewX = this._x + this._activeWorkspaceX;
        let targetWorkspaceCurrentX = this._workspaces[active].gridX;
        let dx = targetWorkspaceNewX - targetWorkspaceCurrentX;

        this._setWorkspaceDraggable(active, true);
        this._animating = showAnimation;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            Tweener.removeTweens(workspace.actor);

            workspace.gridX += dx;

            if (showAnimation) {
                Tweener.addTween(workspace.actor,
                    { x: workspace.gridX,
                      y: workspace.gridY,
                      scale_x: workspace.scale,
                      scale_y: workspace.scale,
                      opacity: workspace.opacity,
                      time: WORKSPACE_SWITCH_TIME,
                      transition: 'easeOutQuad'
                    });
            } else {
                workspace.actor.set_scale(workspace.scale, workspace.scale);
                workspace.actor.set_position(workspace.gridX, workspace.gridY);
                workspace.actor.opacity = workspace.opacity;
            }
        }

        for (let l = 0; l < this._lostWorkspaces.length; l++) {
            let workspace = this._lostWorkspaces[l];

            Tweener.removeTweens(workspace.actor);

            workspace.gridX += dx;
            workspace.actor.show();
            workspace.hideWindowsOverlays();

            if (showAnimation) {
                Tweener.addTween(workspace.actor,
                                 { x: workspace.gridX,
                                   time: WORKSPACE_SWITCH_TIME,
                                   transition: 'easeOutQuad',
                                   onComplete: Lang.bind(this,
                                                         this._cleanWorkspaces)
                                 });
            } else {
                this._cleanWorkspaces();
            }
        }

        Tweener.removeTweens(this._newWorkspaceArea.actor);
        Tweener.removeTweens(this._leftShadow);
        Tweener.removeTweens(this._rightShadow);

        this._newWorkspaceArea.gridX += dx;
        if (showAnimation) {
            // we have to call _updateVisibility() once before the
            // animation and once afterwards - it does not really
            // matter which tween we use, as long as it's not inside
            // a loop ...
            this._updateVisibility();
            Tweener.addTween(this._newWorkspaceArea.actor,
                             { x: this._newWorkspaceArea.gridX,
                               y: this._newWorkspaceArea.gridY,
                               scale_x: this._newWorkspaceArea.scale,
                               scale_y: this._newWorkspaceArea.scale,
                               time: WORKSPACE_SWITCH_TIME,
                               transition: 'easeOutQuad',
                               onComplete: Lang.bind(this, function() {
                                   this._animating = false;
                                   this._updateVisibility();
                               })
                             });
            this._leftShadow.x = this._leftShadow.gridX;
            Tweener.addTween(this._leftShadow,
                             { y: this._leftShadow.gridY,
                               scale_x: this._leftShadow.scale,
                               scale_y: this._leftShadow.scale,
                               time: WORKSPACE_SWITCH_TIME,
                               transition: 'easeOutQuad'
                             });
            this._rightShadow.x = this._rightShadow.gridX;
            Tweener.addTween(this._rightShadow,
                             { y: this._rightShadow.gridY,
                               scale_x: this._rightShadow.scale,
                               scale_y: this._rightShadow.scale,
                               time: WORKSPACE_SWITCH_TIME,
                               transition: 'easeOutQuad'
                             });
        } else {
            this._newWorkspaceArea.actor.set_scale(this._newWorkspaceArea.scale,
                                                   this._newWorkspaceArea.scale);
            this._newWorkspaceArea.actor.set_position(this._newWorkspaceArea.gridX,
                                                      this._newWorkspaceArea.gridY);
            this._leftShadow.set_scale(this._leftShadow.scale,
                                       this._leftShadow.scale);
            this._leftShadow.set_position(this._leftShadow.gridX,
                                          this._leftShadow.gridY);
            this._rightShadow.set_scale(this._rightShadow.scale,
                                        this._rightShadow.scale);
            this._rightShadow.set_position(this._rightShadow.gridX,
                                           this._rightShadow.gridY);
            this._updateVisibility();
        }
    },

    _updateVisibility: function() {
        let active = global.screen.get_active_workspace_index();

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];
            if (this._animating || this._scrolling) {
                workspace.hideWindowsOverlays();
                workspace.actor.show();
            } else {
                workspace.showWindowsOverlays();
                if (this._inDrag)
                    workspace.actor.visible = (Math.abs(w - active) <= 1);
                else
                    workspace.actor.visible = (w == active);
            }
        }

        if (this._inDrag) {
            this._leftShadow.raise_top();
            this._rightShadow.raise_top();

            if (active > 0)
                this._leftShadow.show();
            else
                this._leftShadow.hide();

            if (active < this._workspaces.length - 1)
                this._rightShadow.show();
            else
                this._rightShadow.hide();
        } else {
            this._leftShadow.hide();
            this._rightShadow.hide();
        }
    },

    _cleanWorkspaces: function() {
        if (this._lostWorkspaces.length == 0)
            return;

        for (let l = 0; l < this._lostWorkspaces.length; l++)
            this._lostWorkspaces[l].destroy();
        this._lostWorkspaces = [];

        this._computeWorkspacePositions();
        this._updateWorkspaceActors(false);
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

    updateWorkspaces: function(oldNumWorkspaces, newNumWorkspaces, lostWorkspaces) {
	let active = global.screen.get_active_workspace_index();

        for (let l = 0; l < lostWorkspaces.length; l++)
            lostWorkspaces[l].disconnectAll();

        if (this._scroll != null)
            this._scroll.adjustment.upper = newNumWorkspaces;

        if (newNumWorkspaces > oldNumWorkspaces) {
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
                this._actor.add_actor(this._workspaces[w].actor);
                this._workspaces[w]._windowDragBeginId = this._workspaces[w].connect('window-drag-begin',
                                                                                     Lang.bind(this, this._onWindowDragBegin));
                this._workspaces[w]._windowDragEndId = this._workspaces[w].connect('window-drag-end',
                                                                                   Lang.bind(this, this._onWindowDragEnd));
            }

            this._computeWorkspacePositions();
            this._updateWorkspaceActors(false);
            this._scrollScrollBarToIndex(active + 1, false);
        } else {
            this._lostWorkspaces = lostWorkspaces;
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

    _onDestroy: function() {
        GenericWorkspacesView.prototype._onDestroy.call(this);
        this._setWorkspaceDraggable(this._dragIndex, false);
        this._dropGroup.destroy();
        if (this._timeoutId) {
            Mainloop.source_remove(this._timeoutId);
            this._timeoutId = 0;
        }
        for (let w = 0; w < this._workspaces.length; w++) {
            if (this._workspaces[w]._windowDragBeginId) {
                this._workspaces[w].disconnect(this._workspaces[w]._windowDragBeginId);
                this._workspaces[w]._windowDragBeginId = 0;
            }
            if (this._workspaces[w]._windowDragEndId) {
                this._workspaces[w].disconnect(this._workspaces[w]._windowDragEndId);
                this._workspaces[w]._windowDragEndId = 0;
            }
        }
    },

    acceptDrop: function(source, dropActor, x, y, time) {
        if (x < this._x || y < this._y || y > this._y + this._height) {
            this._dropGroup.lower_bottom();
            dropActor.hide();
            let target = global.stage.get_actor_at_pos(Clutter.PickMode.ALL, x, y);
            dropActor.show();

            if (target._delegate && target._delegate != this && target._delegate.acceptDrop) {
                let [targX, targY] = target.get_transformed_position();
                return target._delegate.acceptDrop(source, dropActor,
                                                   (x - targX) / target.scale_x,
                                                   (y - targY) / target.scale_y,
                                                   time);
            }
            return false;
        }

        for (let i = 0; i < this._workspaces.length; i++) {
            let [dx, dy] = this._workspaces[i].actor.get_transformed_position();
            let [dw, dh] = this._workspaces[i].actor.get_transformed_size();

            if (x > dx && x < dx + dw && y > dy && y < dy + dh) {
                let accept = this._workspaces[i].acceptDrop(source, dropActor, x, y, time);
                if (accept)
                    this._workspaces[i].metaWorkspace.activate(time);
                return accept;
            }
        }

        let [dx, dy] = this._newWorkspaceArea.actor.get_transformed_position();
        let [dw, dh] = this._newWorkspaceArea.actor.get_transformed_size();
        if (x > dx && y > dy && y < dy + dh)
            return this._acceptNewWorkspaceDrop(source, dropActor, x, y, time);

        return false;
    },

    _onWindowDragBegin: function(w, actor) {
        if (!this._scroll || this._scrolling)
            return;

        this._inDrag = true;
        this._computeWorkspacePositions();
        this._updateWorkspaceActors(true);

        this._dropGroup.raise_top();
    },

    handleDragOver: function(self, actor, x, y) {
        let onPanel = false;

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        if (x == 0 && activeWorkspaceIndex > 0 && this._dragOverLastX !== 0) {
            this._workspaces[activeWorkspaceIndex - 1].metaWorkspace.activate(global.get_current_time());
            this._workspaces[activeWorkspaceIndex - 1].setReservedSlot(actor._delegate);
            this._dragOverLastX = 0;
            return;
        }
        if (x == global.screen_width - 1 && this._workspaces[activeWorkspaceIndex + 1] &&
            this._dragOverLastX != global.screen_width - 1) {
            this._workspaces[activeWorkspaceIndex + 1].metaWorkspace.activate(global.get_current_time());
            this._workspaces[activeWorkspaceIndex + 1].setReservedSlot(actor._delegate);
            this._dragOverLastX = global.screen_width - 1;
            return;
        }

        this._dragOverLastX = x;

        let [dx, dy] = this._newWorkspaceArea.actor.get_transformed_position();
        let [dw, dh] = this._newWorkspaceArea.actor.get_transformed_size();
        this._newWorkspaceArea.setStyle(x > dx && y > dy && y < dy + dh);

        [dx, dy] = this._leftShadow.get_transformed_position();
        [dw, dh] = this._leftShadow.get_transformed_size();
        if (this._workspaces[activeWorkspaceIndex - 1]) {
            if (x > dx && x < dx + dw && y > dy && y < dy + dh) {
                onPanel = -1;
                this._workspaces[activeWorkspaceIndex - 1].actor.opacity = 255;
            } else
                this._workspaces[activeWorkspaceIndex - 1].actor.opacity = 200;
        }

        [dx, dy] = this._rightShadow.get_transformed_position();
        [dw, dh] = this._rightShadow.get_transformed_size();
        if (this._workspaces[activeWorkspaceIndex + 1]) {
            if (x > dx && x < dx + dw && y > dy && y < dy + dh) {
                onPanel = 1;
                this._workspaces[activeWorkspaceIndex + 1].actor.opacity = 255;
            } else
                this._workspaces[activeWorkspaceIndex + 1].actor.opacity = 200;
        }
        if (onPanel) {
            if (!this._timeoutId)
                this._timeoutId = Mainloop.timeout_add_seconds (1, Lang.bind(this, function() {
                   let i = global.screen.get_active_workspace_index();
                   if (this._workspaces[i + onPanel]) {
                       this._workspaces[i + onPanel].metaWorkspace.activate(global.get_current_time());
                       this._workspaces[i + onPanel].setReservedSlot(actor._delegate);
                   }
                   return true;
                }));
        } else {
            if (this._timeoutId) {
                Mainloop.source_remove(this._timeoutId);
                this._timeoutId = 0;
            }
        }
    },

    _onWindowDragEnd: function(w, actor) {
        if (this._timeoutId) {
            Mainloop.source_remove(this._timeoutId);
            this._timeoutId = 0;
        }
        this._dropGroup.lower_bottom();
        actor.opacity = 255;
        this._inDrag = false;
        this._computeWorkspacePositions();
        this._updateWorkspaceActors(true);

        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].setReservedSlot(null);
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
            let metaWorkspace = this._workspaces[current].metaWorkspace;

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
            this._workspaces[i].actor.visible = Math.abs(i - adj.value) <= 1;
            this._workspaces[i].actor.x += dx;
        }

        if (!this._scrolling && active == adj.value) {
            // Again, work around the paging in StScrollBar: simulate
            // the effect of scroll-stop
            this._updateWorkspaceActors(false);
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
            let metaWorkspace = this._workspaces[activate].metaWorkspace;
            metaWorkspace.activate(global.get_current_time());
        }
    },

    createControllerBar: function() {
        let actor = new St.BoxLayout({ style_class: 'single-view-controls',
                                       pack_start: true,
                                       vertical: true });

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

        let indicator = new WorkspaceIndicator(Lang.bind(this, function(i) {
            if (this._workspaces[i] != undefined)
                this._workspaces[i].metaWorkspace.activate(global.get_current_time());
        }), Lang.bind(this, function(i, source, actor, x, y, time) {
            if (this._workspaces[i] != undefined)
                return this._workspaces[i].acceptDrop(source, actor, x, y, time);
            return false;
        }), Lang.bind(this, this._onScrollEvent));

        actor.add(indicator.actor, { expand: true, x_fill: true, y_fill: true });
        actor.add(this._scroll, { expand: true,
                                  x_fill: true,
                                  y_fill: false,
                                  y_align: St.Align.START });

        this._updatePanelVisibility();

        return actor;
    },

    canRemoveWorkspace: function() {
        return global.screen.get_active_workspace_index() != 0;
    },

    _updatePanelVisibility: function() {
        let showSwitches = (global.screen.n_workspaces > 1);
        if (this._scroll != null) {
            if (showSwitches)
                this._scroll.show();
            else
                this._scroll.hide();
        }
    },

    addWorkspace: function() {
        let ws = global.screen.append_new_workspace(false,
                                                    global.get_current_time());
        ws.activate(global.get_current_time());
    },

    removeWorkspace: function() {
        let removedIndex = global.screen.get_active_workspace_index();
        let metaWorkspace = this._workspaces[removedIndex].metaWorkspace;
        global.screen.remove_workspace(metaWorkspace,
                                       global.get_current_time());
    }
};

function WorkspacesControls() {
    this._init();
}

WorkspacesControls.prototype = {
    _init: function() {
        this.actor = new St.BoxLayout({ style_class: 'workspaces-bar' });
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._gconf = Shell.GConf.get_default();

        let view = this._gconf.get_string(WORKSPACES_VIEW_KEY).toUpperCase();
        if (view in WorkspacesViewType)
            this._currentViewType = WorkspacesViewType[view];
        else
            this._currentViewType = WorkspacesViewType.SINGLE;

        this._currentView = null;

        // View switcher button
        this._toggleViewButton = new St.Button();
        this._updateToggleButtonStyle();

        this._toggleViewButton.connect('clicked', Lang.bind(this, function() {
            if (this._currentViewType == WorkspacesViewType.SINGLE)
                this._setView(WorkspacesViewType.GRID);
            else
                this._setView(WorkspacesViewType.SINGLE);
         }));

        this.actor.add(this._toggleViewButton, { y_fill: false, y_align: St.Align.START });

        // View specific controls
        this._viewControls = new St.Bin({ x_fill: true, y_fill: true });
        this.actor.add(this._viewControls, { expand: true, x_fill: true });

        // Add/remove workspace buttons
        this._removeButton = new St.Button({ style_class: 'workspace-controls remove' });
        this._removeButton.connect('clicked', Lang.bind(this, function() {
            this._currentView.removeWorkspace();
        }));
        this.actor.add(this._removeButton, { y_fill: false,
                                             y_align: St.Align.START });

        this._addButton = new St.Button({ style_class: 'workspace-controls add' });
        this._addButton.connect('clicked', Lang.bind(this, function() {
            this._currentView.addWorkspace();
        }));
        this._addButton._delegate = this._addButton;
        this._addButton._delegate.acceptDrop = Lang.bind(this,
            function(source, actor, x, y, time) {
                return this._currentView._acceptNewWorkspaceDrop(source, actor, x, y, time);
            });
        this.actor.add(this._addButton, { y_fill: false,
                                          y_align: St.Align.START });

        this._nWorkspacesNotifyId =
            global.screen.connect('notify::n-workspaces',
                                  Lang.bind(this, this._workspacesChanged));
        this._switchWorkspaceNotifyId =
            global.window_manager.connect('switch-workspace',
                                          Lang.bind(this, this.updateControlsSensitivity));

        this._workspacesChanged();
    },

    updateControls: function(view) {
        this._currentView = view;

        this.updateControlsSensitivity();

        let newControls = this._currentView.createControllerBar();
        if (newControls) {
            this._viewControls.child = newControls;
            this._viewControls.child.opacity = 0;
            Tweener.addTween(this._viewControls.child,
                             { opacity: 255,
                               time: Overview.ANIMATION_TIME,
                               transition: 'easeOutQuad' });
        } else {
            if (this._viewControls.child)
                Tweener.addTween(this._viewControls.child,
                                 { opacity: 0,
                                   time: Overview.ANIMATION_TIME,
                                   transition: 'easeOutQuad',
                                   onComplete: Lang.bind(this, function() {
                                       this._viewControls.child.destroy();
                                 })});
        }
    },

    _updateToggleButtonStyle: function() {
       if (this._currentViewType == WorkspacesViewType.SINGLE)
            this._toggleViewButton.set_style_class_name('workspace-controls switch-mosaic');
        else
            this._toggleViewButton.set_style_class_name('workspace-controls switch-single');
    },

    _setView: function(view) {
        if (this._currentViewType == view)
            return;

        if (WorkspacesViewType.SINGLE == view)
            this._toggleViewButton.set_style_class_name('workspace-controls switch-mosaic');
        else
            this._toggleViewButton.set_style_class_name('workspace-controls switch-single');

        this._currentViewType = view;
        this._gconf.set_string(WORKSPACES_VIEW_KEY, view);
    },

    _onDestroy: function() {
        if (this._nWorkspacesNotifyId > 0) {
            global.screen.disconnect(this._nWorkspacesNotifyId);
            this._nWorkspacesNotifyId = 0;
        }
        if (this._switchWorkspaceNotifyId > 0) {
            global.window_manager.disconnect(this._switchWorkspaceNotifyId);
            this._switchWorkspaceNotifyId = 0;
        }
    },

    _setButtonSensitivity: function(button, sensitive) {
        if (button == null)
            return;
        button.reactive = sensitive;
        button.opacity = sensitive ? 255 : 85;
    },

    updateControlsSensitivity: function() {
        if (this._currentView) {
            this._setButtonSensitivity(this._removeButton, this._currentView.canRemoveWorkspace());
            this._setButtonSensitivity(this._addButton, this._currentView.canAddWorkspace());
        }
    },

    _workspacesChanged: function() {
        if (global.screen.n_workspaces == 1)
            this._toggleViewButton.hide();
        else
            this._toggleViewButton.show();
        this.updateControlsSensitivity();
    }
};
Signals.addSignalMethods(WorkspacesControls.prototype);

function WorkspacesManager(width, height, x, y) {
    this._init(width, height, x, y);
}

WorkspacesManager.prototype = {
    _init: function(width, height, x, y) {
        this._workspacesWidth = width;
        this._workspacesHeight = height;
        this._workspacesX = x;
        this._workspacesY = y;

        this._workspaces = [];
        for (let w = 0; w < global.screen.n_workspaces; w++) {
            let metaWorkspace = global.screen.get_workspace_by_index(w);
            this._workspaces[w] = new Workspace.Workspace(metaWorkspace);
        }

        this.workspacesView = null;
        this.controlsBar = new WorkspacesControls();
        this._updateView();

        this.controlsBar.actor.connect('destroy',
                                       Lang.bind(this, this._onDestroy));
        this._viewChangedId =
            Shell.GConf.get_default().connect('changed::' + WORKSPACES_VIEW_KEY,
                                              Lang.bind(this, this._updateView));
        this._nWorkspacesNotifyId =
            global.screen.connect('notify::n-workspaces',
                                  Lang.bind(this, this._workspacesChanged));
    },

    _updateView: function() {
        let viewType, newView;

        let view = Shell.GConf.get_default().get_string(WORKSPACES_VIEW_KEY).toUpperCase();
        if (view in WorkspacesViewType)
            viewType = WorkspacesViewType[view];
        else
            viewType = WorkspacesViewType.SINGLE;

        switch (viewType) {
            case WorkspacesViewType.SINGLE:
                newView = new SingleView(this._workspacesWidth,
                                         this._workspacesHeight,
                                         this._workspacesX,
                                         this._workspacesY,
                                         this._workspaces);
                break;
            case WorkspacesViewType.GRID:
            default:
                newView = new MosaicView(this._workspacesWidth,
                                         this._workspacesHeight,
                                         this._workspacesX,
                                         this._workspacesY,
                                         this._workspaces);
                break;
        }
        if (this.workspacesView)
            this.workspacesView.destroy();
        this.workspacesView = newView;

        this.controlsBar.updateControls(this.workspacesView);

        this.emit('view-changed');
    },

    _workspacesChanged: function() {
        let oldNumWorkspaces = this._workspaces.length;
        let newNumWorkspaces = global.screen.n_workspaces;
        let active = global.screen.get_active_workspace_index();

        if (oldNumWorkspaces == newNumWorkspaces)
            return;

        let lostWorkspaces = [];
        if (newNumWorkspaces > oldNumWorkspaces) {
            // Assume workspaces are only added at the end
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
                let metaWorkspace = global.screen.get_workspace_by_index(w);
                this._workspaces[w] = new Workspace.Workspace(metaWorkspace);
            }
        } else {
            // Assume workspaces are only removed sequentially
            // (e.g. 2,3,4 - not 2,4,7)
            let removedIndex;
            let removedNum = oldNumWorkspaces - newNumWorkspaces;
            for (let w = 0; w < oldNumWorkspaces; w++) {
                let metaWorkspace = global.screen.get_workspace_by_index(w);
                if (this._workspaces[w].metaWorkspace != metaWorkspace) {
                    removedIndex = w;
                    break;
                }
            }

            lostWorkspaces = this._workspaces.splice(removedIndex,
                                                     removedNum);

            // Don't let the user try to select this workspace as it's
            // making its exit.
            for (let l = 0; l < lostWorkspaces.length; l++)
                lostWorkspaces[l].setReactive(false);
        }

        this.workspacesView.updateWorkspaces(oldNumWorkspaces,
                                             newNumWorkspaces,
                                             lostWorkspaces);
    },

    _onDestroy: function() {
        if (this._nWorkspacesNotifyId > 0)
            global.screen.disconnect(this._nWorkspacesNotifyId);
        if (this._viewChangedId > 0)
            Shell.GConf.get_default().disconnect(this._viewChangedId);
        for (let w = 0; w < this._workspaces.length; w++) {
            this._workspaces[w].disconnectAll();
            this._workspaces[w].destroy();
        }
    }
};
Signals.addSignalMethods(WorkspacesManager.prototype);
