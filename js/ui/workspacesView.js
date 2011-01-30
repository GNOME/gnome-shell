/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Signals = imports.signals;
const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const DND = imports.ui.dnd;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const Tweener = imports.ui.tweener;
const Workspace = imports.ui.workspace;
const WorkspaceThumbnail = imports.ui.workspaceThumbnail;

const WORKSPACE_SWITCH_TIME = 0.25;
// Note that mutter has a compile-time limit of 36
const MAX_WORKSPACES = 16;


const CONTROLS_POP_IN_FRACTION = 0.8;
const CONTROLS_POP_IN_TIME = 0.1;


function WorkspacesView(width, height, x, y, zoomScale, workspaces) {
    this._init(width, height, x, y, zoomScale, workspaces);
}

WorkspacesView.prototype = {
    _init: function(width, height, x, y, zoomScale, workspaces) {
        this.actor = new St.Group({ style_class: 'workspaces-view' });
        this.actor.set_clip(x, y, width, height);

        // The actor itself isn't a drop target, so we don't want to pick on its aea
        this.actor.set_size(0, 0);

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this.actor.connect('style-changed', Lang.bind(this,
            function() {
                let node = this.actor.get_theme_node();
                this._spacing = node.get_length('spacing');
                this._computeWorkspacePositions();
            }));
        this.actor.connect('notify::mapped',
                           Lang.bind(this, this._onMappedChanged));

        this._width = width;
        this._height = height;
        this._x = x;
        this._y = y;
        this._zoomScale = zoomScale;
        this._spacing = 0;
        this._activeWorkspaceX = 0; // x offset of active ws while dragging
        this._activeWorkspaceY = 0; // y offset of active ws while dragging
        this._lostWorkspaces = [];
        this._animating = false; // tweening
        this._scrolling = false; // swipe-scrolling
        this._animatingScroll = false; // programatically updating the adjustment
        this._zoomOut = false; // zoom to a larger area
        this._inDrag = false; // dragging a window

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        this._workspaces = workspaces;

        // Add workspace actors
        for (let w = 0; w < global.screen.n_workspaces; w++)
            this._workspaces[w].actor.reparent(this.actor);
        this._workspaces[activeWorkspaceIndex].actor.raise_top();

        // Position/scale the desktop windows and their children after the
        // workspaces have been created. This cannot be done first because
        // window movement depends on the Workspaces object being accessible
        // as an Overview member.
        this._overviewShowingId =
            Main.overview.connect('showing',
                                 Lang.bind(this, function() {
                for (let w = 0; w < this._workspaces.length; w++)
                    this._workspaces[w].zoomToOverview();
        }));

        this._scrollAdjustment = new St.Adjustment({ value: activeWorkspaceIndex,
                                                     lower: 0,
                                                     page_increment: 1,
                                                     page_size: 1,
                                                     step_increment: 0,
                                                     upper: this._workspaces.length });
        this._scrollAdjustment.connect('notify::value',
                                       Lang.bind(this, this._onScroll));

        this._timeoutId = 0;

        this._windowSelectionAppId = null;
        this._highlightWindow = null;

        this._switchWorkspaceNotifyId =
            global.window_manager.connect('switch-workspace',
                                          Lang.bind(this, this._activeWorkspaceChanged));

        this._itemDragBeginId = Main.overview.connect('item-drag-begin',
                                                      Lang.bind(this, this._dragBegin));
        this._itemDragEndId = Main.overview.connect('item-drag-end',
                                                     Lang.bind(this, this._dragEnd));
        this._windowDragBeginId = Main.overview.connect('window-drag-begin',
                                                        Lang.bind(this, this._dragBegin));
        this._windowDragEndId = Main.overview.connect('window-drag-end',
                                                      Lang.bind(this, this._dragEnd));
        this._swipeScrollBeginId = 0;
        this._swipeScrollEndId = 0;
    },

    _lookupWorkspaceForMetaWindow: function (metaWindow) {
        for (let i = 0; i < this._workspaces.length; i++) {
            if (this._workspaces[i].containsMetaWindow(metaWindow))
                return this._workspaces[i];
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

    getActiveWorkspace: function() {
        let active = global.screen.get_active_workspace_index();
        return this._workspaces[active];
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

    getScale: function() {
        return this._workspaces[0].scale;
    },

    syncStacking: function(stackIndices) {
        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].syncStacking(stackIndices);
    },

    // Get the grid position of the active workspace.
    getActiveWorkspacePosition: function() {
        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        let activeWorkspace = this._workspaces[activeWorkspaceIndex];

        return [activeWorkspace.x, activeWorkspace.y];
    },

    zoomOut: function() {
        if (this._zoomOut)
            return;

        this._zoomOut = true;
        this._computeWorkspacePositions();
        this._updateWorkspaceActors(true);
    },

    zoomIn: function() {
        if (!this._zoomOut)
            return;

        this._zoomOut = false;
        this._computeWorkspacePositions();
        this._updateWorkspaceActors(true);
    },

    // Compute the position, scale and opacity of the workspaces, but don't
    // actually change the actors to match
    _computeWorkspacePositions: function() {
        let active = global.screen.get_active_workspace_index();
        let zoomScale = this._zoomOut ? this._zoomScale : 1;
        let scale = zoomScale * this._width / global.screen_width;

        let _width = this._workspaces[0].actor.width * scale;
        let _height = this._workspaces[0].actor.height * scale;

        this._activeWorkspaceX = (this._width - _width) / 2;
        this._activeWorkspaceY = (this._height - _height) / 2;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            workspace.opacity = (this._inDrag && w != active) ? 200 : 255;

            workspace.scale = scale;
            workspace.x = this._x + this._activeWorkspaceX;

            // We adjust the center because the zoomScale is to leave space for
            // the expanded workspace control so we want to zoom to either the
            // left part of the area or the right part of the area
            let offset = 0.5 * (1 - this._zoomScale) * this._width;
            let rtl = (St.Widget.get_default_direction () == St.TextDirection.RTL);
            if (this._zoomOut)
                workspace.x += rtl ? offset : - offset;

            // We divide by zoomScale so that adjacent workspaces are always offscreen
            // except when we are switching between workspaces
            workspace.y = this._y + this._activeWorkspaceY
                              + (w - active) * (_height + this._spacing) / zoomScale;
        }
    },

    _scrollToActive: function(showAnimation) {
        let active = global.screen.get_active_workspace_index();

        this._computeWorkspacePositions();
        this._updateWorkspaceActors(showAnimation);
        this._updateScrollAdjustment(active, showAnimation);
    },

    // Update workspace actors parameters to the values calculated in
    // _computeWorkspacePositions()
    // @showAnimation: iff %true, transition between states
    _updateWorkspaceActors: function(showAnimation) {
        let active = global.screen.get_active_workspace_index();
        let targetWorkspaceNewY = this._y + this._activeWorkspaceY;
        let targetWorkspaceCurrentY = this._workspaces[active].y;
        let dy = targetWorkspaceNewY - targetWorkspaceCurrentY;

        this._animating = showAnimation;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            Tweener.removeTweens(workspace.actor);

            workspace.y += dy;

            if (showAnimation) {
                let params = { x: workspace.x,
                               y: workspace.y,
                               scale_x: workspace.scale,
                               scale_y: workspace.scale,
                               opacity: workspace.opacity,
                               time: WORKSPACE_SWITCH_TIME,
                               transition: 'easeOutQuad'
                             };
                // we have to call _updateVisibility() once before the
                // animation and once afterwards - it does not really
                // matter which tween we use, so we pick the first one ...
                if (w == 0) {
                    this._updateVisibility();
                    params.onComplete = Lang.bind(this,
                        function() {
                            this._animating = false;
                            this._updateVisibility();
                        });
                }
                Tweener.addTween(workspace.actor, params);
            } else {
                workspace.actor.set_scale(workspace.scale, workspace.scale);
                workspace.actor.set_position(workspace.x, workspace.y);
                workspace.actor.opacity = workspace.opacity;
                if (w == 0)
                    this._updateVisibility();
            }
        }

        for (let l = 0; l < this._lostWorkspaces.length; l++) {
            let workspace = this._lostWorkspaces[l];

            Tweener.removeTweens(workspace.actor);

            workspace.y += dy;
            workspace.actor.show();
            workspace.hideWindowsOverlays();

            if (showAnimation) {
                Tweener.addTween(workspace.actor,
                                 { y: workspace.x,
                                   time: WORKSPACE_SWITCH_TIME,
                                   transition: 'easeOutQuad',
                                   onComplete: Lang.bind(this,
                                                         this._cleanWorkspaces)
                                 });
            } else {
                this._cleanWorkspaces();
            }
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

    _updateScrollAdjustment: function(index, showAnimation) {
        if (this._scrolling)
            return;

        this._animatingScroll = true;

        if (showAnimation) {
            Tweener.addTween(this._scrollAdjustment, {
               value: index,
               time: WORKSPACE_SWITCH_TIME,
               transition: 'easeOutQuad',
               onComplete: Lang.bind(this,
                   function() {
                       this._animatingScroll = false;
                   })
            });
        } else {
            this._scrollAdjustment.value = index;
            this._animatingScroll = false;
        }
    },

    updateWorkspaces: function(oldNumWorkspaces, newNumWorkspaces, lostWorkspaces) {
        let active = global.screen.get_active_workspace_index();

        for (let l = 0; l < lostWorkspaces.length; l++)
            lostWorkspaces[l].disconnectAll();

        Tweener.addTween(this._scrollAdjustment,
                         { upper: newNumWorkspaces,
                           time: WORKSPACE_SWITCH_TIME,
                           transition: 'easeOutQuad'
                         });

        if (newNumWorkspaces > oldNumWorkspaces) {
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++)
                this.actor.add_actor(this._workspaces[w].actor);

            this._computeWorkspacePositions();
            this._updateWorkspaceActors(false);
        } else {
            this._lostWorkspaces = lostWorkspaces;
        }

        this._scrollToActive(true);
    },

    _activeWorkspaceChanged: function(wm, from, to, direction) {
        if (this._scrolling)
            return;

        this._scrollToActive(true);
    },

    _onDestroy: function() {
        this._scrollAdjustment.run_dispose();
        Main.overview.disconnect(this._overviewShowingId);
        global.window_manager.disconnect(this._switchWorkspaceNotifyId);

        if (this._timeoutId) {
            Mainloop.source_remove(this._timeoutId);
            this._timeoutId = 0;
        }
        if (this._itemDragBeginId > 0) {
            Main.overview.disconnect(this._itemDragBeginId);
            this._itemDragBeginId = 0;
        }
        if (this._itemDragEndId > 0) {
            Main.overview.disconnect(this._itemDragEndId);
            this._itemDragEndId = 0;
        }
        if (this._windowDragBeginId > 0) {
            Main.overview.disconnect(this._windowDragBeginId);
            this._windowDragBeginId = 0;
        }
        if (this._windowDragEndId > 0) {
            Main.overview.disconnect(this._windowDragEndId);
            this._windowDragEndId = 0;
        }
    },

    _onMappedChanged: function() {
        if (this.actor.mapped) {
            let direction = Overview.SwipeScrollDirection.VERTICAL;
            Main.overview.setScrollAdjustment(this._scrollAdjustment,
                                              direction);
            this._swipeScrollBeginId = Main.overview.connect('swipe-scroll-begin',
                                                             Lang.bind(this, this._swipeScrollBegin));
            this._swipeScrollEndId = Main.overview.connect('swipe-scroll-end',
                                                           Lang.bind(this, this._swipeScrollEnd));
        } else {
            Main.overview.disconnect(this._swipeScrollBeginId);
            Main.overview.disconnect(this._swipeScrollEndId);
        }
    },

    _dragBegin: function() {
        if (this._scrolling)
            return;

        this._inDrag = true;

        this._dragMonitor = {
            dragMotion: Lang.bind(this, this._onDragMotion)
        };
        DND.addDragMonitor(this._dragMonitor);
    },

    _onDragMotion: function(dragEvent) {
        if (Main.overview.animationInProgress)
             return DND.DragMotionResult.CONTINUE;

        let primary = global.get_primary_monitor();

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        let topWorkspace, bottomWorkspace;
        topWorkspace  = this._workspaces[activeWorkspaceIndex - 1];
        bottomWorkspace = this._workspaces[activeWorkspaceIndex + 1];
        let hoverWorkspace = null;

        // reactive monitor edges
        let topEdge = primary.y;
        let switchTop = (dragEvent.y <= topEdge && topWorkspace);
        if (switchTop && this._dragOverLastY != topEdge) {
            topWorkspace.metaWorkspace.activate(global.get_current_time());
            topWorkspace.setReservedSlot(dragEvent.dragActor._delegate);
            this._dragOverLastY = topEdge;

            return DND.DragMotionResult.CONTINUE;
        }
        let bottomEdge = primary.y + primary.height - 1;
        let switchBottom = (dragEvent.y >= bottomEdge && bottomWorkspace);
        if (switchBottom && this._dragOverLastY != bottomEdge) {
            bottomWorkspace.metaWorkspace.activate(global.get_current_time());
            bottomWorkspace.setReservedSlot(dragEvent.dragActor._delegate);
            this._dragOverLastY = bottomEdge;

            return DND.DragMotionResult.CONTINUE;
        }
        this._dragOverLastY = dragEvent.y;
        let result = DND.DragMotionResult.CONTINUE;

        // check hover state of new workspace area / inactive workspaces
        if (topWorkspace) {
            if (topWorkspace.actor.contains(dragEvent.targetActor)) {
                hoverWorkspace = topWorkspace;
                topWorkspace.opacity = topWorkspace.actor.opacity = 255;
                result = topWorkspace.handleDragOver(dragEvent.source, dragEvent.dragActor);
            } else {
                topWorkspace.opacity = topWorkspace.actor.opacity = 200;
            }
        }

        if (bottomWorkspace) {
            if (bottomWorkspace.actor.contains(dragEvent.targetActor)) {
                hoverWorkspace = bottomWorkspace;
                bottomWorkspace.opacity = bottomWorkspace.actor.opacity = 255;
                result = bottomWorkspace.handleDragOver(dragEvent.source, dragEvent.dragActor);
            } else {
                bottomWorkspace.opacity = bottomWorkspace.actor.opacity = 200;
            }
        }

        // handle delayed workspace switches
        if (hoverWorkspace) {
            if (!this._timeoutId)
                this._timeoutId = Mainloop.timeout_add_seconds(1,
                    Lang.bind(this, function() {
                       hoverWorkspace.metaWorkspace.activate(global.get_current_time());
                       hoverWorkspace.setReservedSlot(dragEvent.dragActor._delegate);
                       return false;
                    }));
        } else {
            if (this._timeoutId) {
                Mainloop.source_remove(this._timeoutId);
                this._timeoutId = 0;
            }
        }

        return result;
    },

    _dragEnd: function() {
        if (this._timeoutId) {
            Mainloop.source_remove(this._timeoutId);
            this._timeoutId = 0;
        }
        DND.removeMonitor(this._dragMonitor);
        this._inDrag = false;

        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].setReservedSlot(null);
    },

    _swipeScrollBegin: function() {
        this._scrolling = true;
    },

    _swipeScrollEnd: function(overview, result) {
        this._scrolling = false;

        if (result == Overview.SwipeScrollResult.CLICK) {
            let [x, y, mod] = global.get_pointer();
            let actor = global.stage.get_actor_at_pos(Clutter.PickMode.ALL,
                                                      x, y);

            // Only switch to the workspace when there's no application
            // windows open. The problem is that it's too easy to miss
            // an app window and get the wrong one focused.
            let active = global.screen.get_active_workspace_index();
            if (this._workspaces[active].isEmpty() &&
                this.actor.contains(actor))
                Main.overview.hide();
        }

        if (result == Overview.SwipeScrollResult.SWIPE)
            // The active workspace has changed; while swipe-scrolling
            // has already taken care of the positioning, the cached
            // positions need to be updated
            this._computeWorkspacePositions();

        // Make sure title captions etc are shown as necessary
        this._updateVisibility();
    },

    // sync the workspaces' positions to the value of the scroll adjustment
    // and change the active workspace if appropriate
    _onScroll: function(adj) {
        if (this._animatingScroll)
            return;

        let active = global.screen.get_active_workspace_index();
        let current = Math.round(adj.value);

        if (active != current) {
            let metaWorkspace = this._workspaces[current].metaWorkspace;
            metaWorkspace.activate(global.get_current_time());
        }

        let last = this._workspaces.length - 1;
        let firstWorkspaceY = this._workspaces[0].actor.y;
        let lastWorkspaceY = this._workspaces[last].actor.y;
        let workspacesHeight = lastWorkspaceY - firstWorkspaceY;

        if (adj.upper == 1)
            return;

        let currentY = firstWorkspaceY;
        let newY = this._y - adj.value / (adj.upper - 1) * workspacesHeight;

        let dy = newY - currentY;

        for (let i = 0; i < this._workspaces.length; i++) {
            this._workspaces[i]._hideAllOverlays();
            this._workspaces[i].actor.visible = Math.abs(i - adj.value) <= 1;
            this._workspaces[i].actor.y += dy;
        }
    },

    _getWorkspaceIndexToRemove: function() {
        return global.screen.get_active_workspace_index();
    }
};
Signals.addSignalMethods(WorkspacesView.prototype);


function WorkspaceControlsContainer(controls) {
    this._init(controls);
}

WorkspaceControlsContainer.prototype = {
    _init: function(controls) {
        this.actor = new Shell.GenericContainer({ clip_to_allocation: true });
        this.actor.connect('get-preferred-width',
                           Lang.bind(this, this._getPreferredWidth));
        this.actor.connect('get-preferred-height',
                           Lang.bind(this, this._getPreferredHeight));
        this.actor.connect('allocate', Lang.bind(this, this._allocate));

        this.actor.add_actor(controls);

        this._controls = controls;
    },

    show: function() {
        this._controls.x = this._poppedInX();
    },

    hide: function() {
    },

    _getPreferredWidth: function(actor, forHeight, alloc) {
        let [minWidth, natWidth] = this._controls.get_preferred_width(-1);
        alloc.min_size = minWidth;
        alloc.natural_size = natWidth;
    },

    // Always request the full width ...
    _getPreferredHeight: function(actor, forWidth, alloc) {
        let [minHeight, natHeight] = this._controls.get_preferred_height(-1);
        alloc.min_size = minHeight;
        alloc.natural_size = natHeight;
    },

    // ... even when the controls are popped in, to keep the width constant.
    // This is necessary as the workspace size is determined once before
    // entering the overview, when the controls are popped in - if the width
    // varied, the workspaces would be given too much width, and the controls
    // would be overlapped by the workspaces when popped out, rendering them
    // useless.
    _allocate: function(actor, box, flags) {
        let childBox = new Clutter.ActorBox();
        childBox.x1 = this._controls.x;
        childBox.x2 = this._controls.x + this._controls.width;
        childBox.y1 = 0;
        childBox.y2 = box.y2 - box.y1;
        this._controls.allocate(childBox, flags);
    },

    _poppedInX: function() {
        let x = CONTROLS_POP_IN_FRACTION * this._controls.width;
        if (St.Widget.get_default_direction() == St.TextDirection.RTL)
            return -x;
        return x;
    },

    popOut: function() {
        Tweener.addTween(this._controls,
                         { x: 0,
                           time: CONTROLS_POP_IN_TIME,
                           transition: 'easeOutQuad' });
    },

};

function WorkspacesDisplay() {
    this._init();
}

WorkspacesDisplay.prototype = {
    _init: function() {
        this.actor = new St.Group();
        this.actor.set_size(0, 0);

        let controls = new St.BoxLayout({ vertical: true,
                                          style_class: 'workspace-controls' });
        this._controls = controls;
        this.actor.add_actor(controls);

        controls.reactive = true;
        controls.track_hover = true;
        controls.connect('notify::hover',
                         Lang.bind(this, this._onControlsHoverChanged));

        this._thumbnailsBox = new St.BoxLayout({ vertical: true,
                                                 style_class: 'workspace-thumbnails' });
        controls.add(this._thumbnailsBox, { expand: false });

        let indicator = new St.Bin({ style_class: 'workspace-thumbnail-indicator',
                                     fixed_position_set: true });

        // We don't want the indicator to affect drag-and-drop
        Shell.util_set_hidden_from_pick(indicator, true);

        this._thumbnailIndicator = indicator;
        this._thumbnailsBox.add(this._thumbnailIndicator);
        this._thumbnailIndicatorConstraints = [];
        this._thumbnailIndicatorConstraints.push(new Clutter.BindConstraint({ coordinate: Clutter.BindCoordinate.X }));
        this._thumbnailIndicatorConstraints.push(new Clutter.BindConstraint({ coordinate: Clutter.BindCoordinate.Y }));
        this._thumbnailIndicatorConstraints.push(new Clutter.BindConstraint({ coordinate: Clutter.BindCoordinate.WIDTH }));
        this._thumbnailIndicatorConstraints.push(new Clutter.BindConstraint({ coordinate: Clutter.BindCoordinate.HEIGHT }));
        this._thumbnailIndicatorConstraints.forEach(function(constraint) {
                                                        indicator.add_constraint(constraint);
                                                    });

        this.workspacesView = null;

        this._inDrag = false;
        this._zoomOut = false;

        this._nWorkspacesNotifyId = 0;
        this._switchWorkspaceNotifyId = 0;

        this._itemDragBeginId = 0;
        this._itemDragEndId = 0;
        this._windowDragBeginId = 0;
        this._windowDragEndId = 0;
    },

   show: function() {
        this._controls.show();

        this._workspaces = [];
        this._workspaceThumbnails = [];
        for (let i = 0; i < global.screen.n_workspaces; i++) {
            let metaWorkspace = global.screen.get_workspace_by_index(i);
            this._workspaces[i] = new Workspace.Workspace(metaWorkspace);

            let thumbnail = new WorkspaceThumbnail.WorkspaceThumbnail(metaWorkspace);
            this._workspaceThumbnails[i] = thumbnail;
            this._thumbnailsBox.add(thumbnail.actor);
        }

        // The thumbnails indicator actually needs to be on top of the thumbnails, but
        // there is also something more subtle going on as well - actors in a StBoxLayout
        // are allocated from bottom to to top (start to end), and we need the
        // thumnail indicator to be allocated after the actors it is constrained to.
        this._thumbnailIndicator.raise_top();

        let rtl = (St.Widget.get_default_direction () == St.TextDirection.RTL);

        let totalAllocation = this.actor.allocation;
        let totalWidth = totalAllocation.x2 - totalAllocation.x1;
        // XXXX: 50 is just a hack for message tray compensation
        let totalHeight = totalAllocation.y2 - totalAllocation.y1 - 50;

        let [controlsMin, controlsNatural] = this._controls.get_preferred_width(-1);
        let controlsReserved = controlsNatural * (1 - CONTROLS_POP_IN_FRACTION);

        totalWidth -= controlsReserved;

        // Workspaces expect to have the same ratio as the screen, so take
        // this into account when fitting the workspace into the available space
        let width, height;
        let totalRatio = totalWidth / totalHeight;
        let wsRatio = global.screen_width / global.screen_height;
        if (wsRatio > totalRatio) {
            width = totalWidth;
            height = Math.floor(totalWidth / wsRatio);
        } else {
            width = Math.floor(totalHeight * wsRatio);
            height = totalHeight;
        }

        // Position workspaces in the available space
        let [x, y] = this.actor.get_transformed_position();
        x = Math.floor(x + Math.abs(totalWidth - width) / 2);
        y = Math.floor(y + Math.abs(totalHeight - height) / 2);

        if (rtl)
            x += controlsReserved;

        this._controls.x = this._getControlsX();
        this._controls.height = totalHeight;

        let zoomScale = (totalWidth - controlsNatural) / totalWidth;
        let newView = new WorkspacesView(width, height, x, y, zoomScale, this._workspaces);

        if (this.workspacesView)
            this.workspacesView.destroy();
        this.workspacesView = newView;

        this._nWorkspacesNotifyId =
            global.screen.connect('notify::n-workspaces',
                                  Lang.bind(this, this._workspacesChanged));
        this._switchWorkspaceNotifyId =
            global.window_manager.connect('switch-workspace',
                                          Lang.bind(this, this._activeWorkspaceChanged));

        this._restackedNotifyId =
            global.screen.connect('restacked',
                                  Lang.bind(this, this._onRestacked));

        if (this._itemDragBeginId == 0)
            this._itemDragBeginId = Main.overview.connect('item-drag-begin',
                                                          Lang.bind(this, this._dragBegin));
        if (this._itemDragEndId == 0)
            this._itemDragEndId = Main.overview.connect('item-drag-end',
                                                        Lang.bind(this, this._dragEnd));
        if (this._windowDragBeginId == 0)
            this._windowDragBeginId = Main.overview.connect('window-drag-begin',
                                                            Lang.bind(this, this._dragBegin));
        if (this._windowDragEndId == 0)
            this._windowDragEndId = Main.overview.connect('window-drag-end',
                                                          Lang.bind(this, this._dragEnd));

        this._onRestacked();
        this._constrainThumbnailIndicator();
        this._zoomOut = false;
        this._updateZoom();
    },

    hide: function() {
        this._controls.hide();

        if (this._nWorkspacesNotifyId > 0) {
            global.screen.disconnect(this._nWorkspacesNotifyId);
            this._nWorkspacesNotifyId = 0;
        }
        if (this._switchWorkspaceNotifyId > 0) {
            global.window_manager.disconnect(this._switchWorkspaceNotifyId);
            this._switchWorkspaceNotifyId = 0;
        }
        if (this._restackedNotifyId > 0){
            global.screen.disconnect(this._restackedNotifyId);
            this._restackedNotifyId = 0;
        }
        if (this._itemDragBeginId > 0) {
            Main.overview.disconnect(this._itemDragBeginId);
            this._itemDragBeginId = 0;
        }
        if (this._itemEndBeginId > 0) {
            Main.overview.disconnect(this._itemDragEndId);
            this._itemDragEndId = 0;
        }
        if (this._windowDragBeginId > 0) {
            Main.overview.disconnect(this._windowDragBeginId);
            this._windowDragBeginId = 0;
        }
        if (this._windowDragEndId > 0) {
            Main.overview.disconnect(this._windowDragEndId);
            this._windowDragEndId = 0;
        }

        this.workspacesView.destroy();
        this.workspacesView = null;
        this._unconstrainThumbnailIndicator();
        for (let w = 0; w < this._workspaces.length; w++) {
            this._workspaces[w].disconnectAll();
            this._workspaces[w].destroy();
            this._workspaceThumbnails[w].destroy();
        }
    },

    _constrainThumbnailIndicator: function() {
        let active = global.screen.get_active_workspace_index();
        let thumbnail = this._workspaceThumbnails[active];

        this._thumbnailIndicatorConstraints.forEach(function(constraint) {
                                                        constraint.set_source(thumbnail.actor);
                                                        constraint.set_enabled(true);
                                                    });
    },

    _unconstrainThumbnailIndicator: function() {
        this._thumbnailIndicatorConstraints.forEach(function(constraint) {
                                                        constraint.set_enabled(false);
                                                    });
    },

    _activeWorkspaceChanged: function(wm, from, to, direction) {
        let active = global.screen.get_active_workspace_index();
        let thumbnail = this._workspaceThumbnails[active];

        this._unconstrainThumbnailIndicator();
        let oldAllocation = this._thumbnailIndicator.allocation;
        this._thumbnailIndicator.x = oldAllocation.x1;
        this._thumbnailIndicator.y = oldAllocation.y1;
        this._thumbnailIndicator.width = oldAllocation.x2 - oldAllocation.x1;
        this._thumbnailIndicator.height = oldAllocation.y2 - oldAllocation.y1;

        Tweener.addTween(this._thumbnailIndicator,
                         { x: thumbnail.actor.allocation.x1,
                           y: thumbnail.actor.allocation.y1,
                           time: WORKSPACE_SWITCH_TIME,
                           transition: 'easeOutQuad',
                           onComplete: Lang.bind(this,
                                                 this._constrainThumbnailIndicator)
                         });
    },

    _onRestacked: function() {
        let stack = global.get_window_actors();
        let stackIndices = {};

        for (let i = 0; i < stack.length; i++) {
            // Use the stable sequence for an integer to use as a hash key
            stackIndices[stack[i].get_meta_window().get_stable_sequence()] = i;
        }

        this.workspacesView.syncStacking(stackIndices);
        for (let i = 0; i < this._workspaceThumbnails.length; i++)
            this._workspaceThumbnails[i].syncStacking(stackIndices);
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

                let thumbnail = new WorkspaceThumbnail.WorkspaceThumbnail(metaWorkspace);
                this._workspaceThumbnails[w] = thumbnail;
                this._thumbnailsBox.add(thumbnail.actor);
            }
            this._thumbnailIndicator.raise_top();
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

            for (let k = removedIndex; k < removedIndex + removedNum; k++)
                this._workspaceThumbnails[k].destroy();
            this._workspaceThumbnails.splice(removedIndex, removedNum);
        }

        // If we removed the current workspace then we'll be animating workspace indicator
        // to an adjacent workspace, but that is wrong, since now that adjacent workspace
        // is in the current slot, so remove the animation
        Tweener.removeTweens(this._thumbnailIndicator);
        this._constrainThumbnailIndicator();

        this.workspacesView.updateWorkspaces(oldNumWorkspaces,
                                             newNumWorkspaces,
                                             lostWorkspaces);
    },

    _getControlsX: function() {
        let totalAllocation = this.actor.allocation;
        let totalWidth = totalAllocation.x2 - totalAllocation.x1;
        let [controlsMin, controlsNatural] = this._controls.get_preferred_width(-1);
        let controlsReserved = controlsNatural * (1 - CONTROLS_POP_IN_FRACTION);

        let rtl = (St.Widget.get_default_direction () == St.TextDirection.RTL);
        let width = this._zoomOut ? controlsNatural : controlsReserved;
        if (rtl)
            return width;
        else
            return totalWidth - width;
    },

    _updateZoom : function() {
        if (Main.overview.animationInProgress)
            return;

        let shouldZoom = this._controls.hover || this._inDrag;
        if (shouldZoom != this._zoomOut) {
            this._zoomOut = shouldZoom;

            if (!this.workspacesView)
                return;

            Tweener.addTween(this._controls,
                             { x: this._getControlsX(),
                               time: WORKSPACE_SWITCH_TIME,
                               transition: 'easeOutQuad' });

            if (shouldZoom)
                this.workspacesView.zoomOut();
            else
                this.workspacesView.zoomIn();
        }
    },

    _onControlsHoverChanged: function() {
        this._updateZoom();
    },

    _dragBegin: function() {
        this._inDrag = true;
        this._updateZoom();
    },

    _dragEnd: function() {
        this._inDrag = false;

        // We do this deferred because drag-end is emitted before dnd.js emits
        // event/leave events that were suppressed during the drag. If we didn't
        // defer this, we'd zoom out then immediately zoom in because of the
        // enter event we received. That would normally be invisible but we
        // might as well avoid it.
        Meta.later_add(Meta.LaterType.BEFORE_REDRAW,
                       Lang.bind(this, this._updateZoom));
    }
};
Signals.addSignalMethods(WorkspacesDisplay.prototype);
