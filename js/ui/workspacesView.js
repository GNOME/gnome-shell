/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
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

const WORKSPACE_SWITCH_TIME = 0.25;
// Note that mutter has a compile-time limit of 36
const MAX_WORKSPACES = 16;

const WORKSPACE_DRAGGING_SCALE = 0.85;



function WorkspaceIndicator(activateWorkspace, workspaceAcceptDrop, workspaceHandleDragOver, scrollEventCb) {
    this._init(activateWorkspace, workspaceAcceptDrop, workspaceHandleDragOver, scrollEventCb);
}

WorkspaceIndicator.prototype = {
    _init: function(activateWorkspace, workspaceAcceptDrop, workspaceHandleDragOver, scrollEventCb) {
        this._activateWorkspace = activateWorkspace;
        this._workspaceAcceptDrop = workspaceAcceptDrop;
        this._workspaceHandleDragOver = workspaceHandleDragOver;
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
        actor._delegate.handleDragOver = Lang.bind(this, function(source, actor, x, y, time) {
            return this._workspaceHandleDragOver(i, source, actor, x, y, time);
        });

        actor.connect('scroll-event', this._scrollEventCb);

        this._indicatorsPanel.add_actor(actor);

        let spacing = actor.get_theme_node().get_length('border-spacing');
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

function WorkspacesView(width, height, x, y, workspaces) {
    this._init(width, height, x, y, workspaces);
}

WorkspacesView.prototype = {
    _init: function(width, height, x, y, workspaces) {
        this.actor = new St.Group({ style_class: 'workspaces-view' });
        this.actor.set_clip(x, y, width, height);

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this.actor.connect('style-changed', Lang.bind(this,
            function() {
                let node = this.actor.get_theme_node();
                this._spacing = node.get_length('spacing');
                this._computeWorkspacePositions();
            }));

        this._width = width;
        this._height = height;
        this._x = x;
        this._y = y;
        this._spacing = 0;
        this._activeWorkspaceX = 0; // x offset of active ws while dragging
        this._activeWorkspaceY = 0; // y offset of active ws while dragging
        this._lostWorkspaces = [];
        this._animating = false; // tweening
        this._scrolling = false; // dragging desktop
        this._animatingScroll = false; // programatically updating the adjustment
        this._inDrag = false; // dragging a window
        this._lastMotionTime = -1; // used to track "stopping" while dragging workspaces

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        this._workspaces = workspaces;

        // Add workspace actors
        for (let w = 0; w < global.screen.n_workspaces; w++) {
            this._workspaces[w].actor.reparent(this.actor);
            this._workspaces[w]._windowDragBeginId =
                this._workspaces[w].connect('window-drag-begin',
                                            Lang.bind(this, this._dragBegin));
            this._workspaces[w]._windowDragEndId =
                this._workspaces[w].connect('window-drag-end',
                                            Lang.bind(this, this._dragEnd));
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

        this._dragIndex = -1;

        this._buttonPressId = 0;
        this._capturedEventId = 0;
        this._timeoutId = 0;

        this._windowSelectionAppId = null;
        this._highlightWindow = null;

        this._switchWorkspaceNotifyId =
            global.window_manager.connect('switch-workspace',
                                          Lang.bind(this, this._activeWorkspaceChanged));
        this._restackedNotifyId =
            global.screen.connect('restacked',
                                  Lang.bind(this, this._onRestacked));

        this._itemDragBeginId = Main.overview.connect('item-drag-begin',
                                                      Lang.bind(this, this._dragBegin));
        this._itemDragEndId = Main.overview.connect('item-drag-end',
                                                     Lang.bind(this, this._dragEnd));
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

    _onRestacked: function() {
        let stack = global.get_window_actors();
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

        return [activeWorkspace.x, activeWorkspace.y];
    },

    canAddWorkspace: function() {
        return global.screen.n_workspaces < MAX_WORKSPACES;
    },

    addWorkspace: function() {
        let ws = null;
        if (!this.canAddWorkspace()) {
            Main.overview.shellInfo.setMessage(_("Can't add a new workspace because maximum workspaces limit has been reached."));
        } else {
            let currentTime = global.get_current_time();
            ws = global.screen.append_new_workspace(false, currentTime);
            ws.activate(currentTime);
        }

        return ws;
    },

    canRemoveWorkspace: function() {
        return this._getWorkspaceIndexToRemove() > 0;
    },

    removeWorkspace: function() {
        if (!this.canRemoveWorkspace()) {
            Main.overview.shellInfo.setMessage(_("Can't remove the first workspace."));
            return;
        }
        let index = this._getWorkspaceIndexToRemove();
        let metaWorkspace = this._workspaces[index].metaWorkspace;
        global.screen.remove_workspace(metaWorkspace,
                                       global.get_current_time());
    },

    _handleDragOverNewWorkspace: function(source, dropActor, x, y, time) {
        if (source instanceof Workspace.WindowClone)
            return DND.DragMotionResult.MOVE_DROP;
        if (source.shellWorkspaceLaunch)
            return DND.DragMotionResult.COPY_DROP;
        return DND.DragMotionResult.CONTINUE;
    },

    _acceptNewWorkspaceDrop: function(source, dropActor, x, y, time) {
        let ws = this.addWorkspace();
        if (ws == null)
            return false;
        return this.acceptNewWorkspaceDrop(source, dropActor, x, y, time);
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

            workspace.scale = scale;
            workspace.x = this._x + this._activeWorkspaceX
                          + (w - active) * (_width + this._spacing);
            workspace.y = this._y + this._activeWorkspaceY;
        }
    },

    _scrollToActive: function(showAnimation) {
        let active = global.screen.get_active_workspace_index();

        this._computeWorkspacePositions();
        this._updateWorkspaceActors(showAnimation);
        this._updateScrollAdjustment(active, showAnimation);
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

        let dragActor = this._workspaces[index].actor;

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
        if (actor != event.get_source())
            return;

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

                // If the user has moved more than half a workspace, we want to "settle"
                // to the new workspace even if the user stops dragging rather "throws"
                // by releasing during the drag.
                let noStop = Math.abs(activate - this._scrollAdjustment.value) > 0.5;

                // We detect if the user is stopped by comparing the timestamp of the button
                // release with the timestamp of the last motion. Experimentally, a difference
                // of 0 or 1 millisecond indicates that the mouse is in motion, a larger
                // difference indicates that the mouse is stopped.
                if ((this._lastMotionTime > 0 && this._lastMotionTime > event.get_time() - 2) || noStop) {
                    if (stageX > this._dragStartX && activate > 0)
                        activate--;
                    else if (stageX < this._dragStartX && activate < last)
                        activate++;
                }

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

                this._scrollAdjustment.value += (dx / primary.width);
                this._dragX = stageX;
                this._lastMotionTime = event.get_time();

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
        let targetWorkspaceCurrentX = this._workspaces[active].x;
        let dx = targetWorkspaceNewX - targetWorkspaceCurrentX;

        this._setWorkspaceDraggable(active, true);
        this._animating = showAnimation;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            Tweener.removeTweens(workspace.actor);

            workspace.x += dx;

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

            workspace.x += dx;
            workspace.actor.show();
            workspace.hideWindowsOverlays();

            if (showAnimation) {
                Tweener.addTween(workspace.actor,
                                 { x: workspace.x,
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
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
                this.actor.add_actor(this._workspaces[w].actor);
                this._workspaces[w]._windowDragBeginId = this._workspaces[w].connect('window-drag-begin',
                                                                                     Lang.bind(this, this._dragBegin));
                this._workspaces[w]._windowDragEndId = this._workspaces[w].connect('window-drag-end',
                                                                                   Lang.bind(this, this._dragEnd));
            }

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
        Main.overview.disconnect(this._overviewShowingId);
        global.window_manager.disconnect(this._switchWorkspaceNotifyId);
        global.screen.disconnect(this._restackedNotifyId);

        this._setWorkspaceDraggable(this._dragIndex, false);
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

    _dragBegin: function() {
        if (this._scrolling)
            return;

        this._inDrag = true;
        this._computeWorkspacePositions();
        this._updateWorkspaceActors(true);

        this._dragMonitor = {
            dragMotion: Lang.bind(this, this._onDragMotion)
        };
        DND.addDragMonitor(this._dragMonitor);
    },

    _onDragMotion: function(dragEvent) {
        let primary = global.get_primary_monitor();

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        let leftWorkspace  = this._workspaces[activeWorkspaceIndex - 1];
        let rightWorkspace = this._workspaces[activeWorkspaceIndex + 1];
        let hoverWorkspace = null;

        // reactive monitor edges
        let leftEdge = primary.x;
        let switchLeft = (dragEvent.x <= leftEdge && leftWorkspace);
        if (switchLeft && this._dragOverLastX != leftEdge) {
            leftWorkspace.metaWorkspace.activate(global.get_current_time());
            leftWorkspace.setReservedSlot(dragEvent.dragActor._delegate);
            this._dragOverLastX = leftEdge;

            return DND.DragMotionResult.CONTINUE;
        }
        let rightEdge = primary.x + primary.width - 1;
        let switchRight = (dragEvent.x >= rightEdge && rightWorkspace);
        if (switchRight && this._dragOverLastX != rightEdge) {
            rightWorkspace.metaWorkspace.activate(global.get_current_time());
            rightWorkspace.setReservedSlot(dragEvent.dragActor._delegate);
            this._dragOverLastX = rightEdge;

            return DND.DragMotionResult.CONTINUE;
        }
        this._dragOverLastX = dragEvent.x;
        let result = DND.DragMotionResult.CONTINUE;

        // check hover state of new workspace area / inactive workspaces
        if (leftWorkspace) {
            if (leftWorkspace.actor.contains(dragEvent.targetActor)) {
                hoverWorkspace = leftWorkspace;
                leftWorkspace.opacity = leftWorkspace.actor.opacity = 255;
                result = leftWorkspace.handleDragOver(dragEvent.source, dragEvent.dragActor);
            } else {
                leftWorkspace.opacity = leftWorkspace.actor.opacity = 200;
            }
        }

        if (rightWorkspace) {
            if (rightWorkspace.actor.contains(dragEvent.targetActor)) {
                hoverWorkspace = rightWorkspace;
                rightWorkspace.opacity = rightWorkspace.actor.opacity = 255;
                result = rightWorkspace.handleDragOver(dragEvent.source, dragEvent.dragActor);
            } else {
                rightWorkspace.opacity = rightWorkspace.actor.opacity = 200;
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
        this._computeWorkspacePositions();
        this._updateWorkspaceActors(true);

        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].setReservedSlot(null);
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
        let firstWorkspaceX = this._workspaces[0].actor.x;
        let lastWorkspaceX = this._workspaces[last].actor.x;
        let workspacesWidth = lastWorkspaceX - firstWorkspaceX;

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

        let indicator = new WorkspaceIndicator(Lang.bind(this, function(i) {
            if (this._workspaces[i] != undefined)
                this._workspaces[i].metaWorkspace.activate(global.get_current_time());
        }), Lang.bind(this, function(i, source, actor, x, y, time) {
            if (this._workspaces[i] != undefined)
                return this._workspaces[i].acceptDrop(source, actor, x, y, time);
            return false;
        }), Lang.bind(this, function(i, source, actor, x, y, time) {
            if (this._workspaces[i] != undefined)
                return this._workspaces[i].handleDragOver(source, actor, x, y, time);
            return DND.DragMotionResult.CONTINUE;
        }), Lang.bind(this, this._onScrollEvent));

        actor.add(indicator.actor, { expand: true, x_fill: true, y_fill: true });
        return actor;
    },

    _getWorkspaceIndexToRemove: function() {
        return global.screen.get_active_workspace_index();
    }
};

function WorkspacesControls() {
    this._init();
}

WorkspacesControls.prototype = {
    _init: function() {
        this.actor = new St.BoxLayout({ style_class: 'workspaces-bar' });
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this._currentView = null;

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
        this._addButton._delegate.handleDragOver = Lang.bind(this,
            function(source, actor, x, y, time) {
                return this._currentView._handleDragOverNewWorkspace(source, actor, x, y, time);
            });
        this.actor.add(this._addButton, { y_fill: false,
                                          y_align: St.Align.START });

        this._nWorkspacesNotifyId =
            global.screen.connect('notify::n-workspaces',
                                  Lang.bind(this, this.updateControlsSensitivity));
        this._switchWorkspaceNotifyId =
            global.window_manager.connect('switch-workspace',
                                          Lang.bind(this, this.updateControlsSensitivity));

        this.updateControlsSensitivity();
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
        button.opacity = sensitive ? 255 : 85;
    },

    updateControlsSensitivity: function() {
        if (this._currentView) {
            this._setButtonSensitivity(this._removeButton, this._currentView.canRemoveWorkspace());
            this._setButtonSensitivity(this._addButton, this._currentView.canAddWorkspace());
        }
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
        this._nWorkspacesNotifyId =
            global.screen.connect('notify::n-workspaces',
                                  Lang.bind(this, this._workspacesChanged));
    },

    _updateView: function() {
        let newView = new WorkspacesView(this._workspacesWidth,
                                         this._workspacesHeight,
                                         this._workspacesX,
                                         this._workspacesY,
                                         this._workspaces);

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
            global.settings.disconnect(this._viewChangedId);
        for (let w = 0; w < this._workspaces.length; w++) {
            this._workspaces[w].disconnectAll();
            this._workspaces[w].destroy();
        }
    }
};
Signals.addSignalMethods(WorkspacesManager.prototype);
