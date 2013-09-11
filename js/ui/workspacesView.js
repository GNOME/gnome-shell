// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GObject = imports.gi.GObject;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;
const Signals = imports.signals;

const DND = imports.ui.dnd;
const Main = imports.ui.main;
const Overview = imports.ui.overview;
const Tweener = imports.ui.tweener;
const Workspace = imports.ui.workspace;
const WorkspaceThumbnail = imports.ui.workspaceThumbnail;

const WORKSPACE_SWITCH_TIME = 0.25;
// Note that mutter has a compile-time limit of 36
const MAX_WORKSPACES = 16;

const OVERRIDE_SCHEMA = 'org.gnome.shell.overrides';

function rectEqual(one, two) {
    if (one == two)
        return true;

    if (!one || !two)
        return false;

    return (one.x == two.x &&
            one.y == two.y &&
            one.width == two.width &&
            one.height == two.height);
}

const WorkspacesView = new Lang.Class({
    Name: 'WorkspacesView',

    _init: function(workspaces) {
        this.actor = new St.Widget({ style_class: 'workspaces-view',
                                     reactive: true });

        // The actor itself isn't a drop target, so we don't want to pick on its area
        this.actor.set_size(0, 0);

        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

        this.actor.connect('style-changed', Lang.bind(this,
            function() {
                let node = this.actor.get_theme_node();
                this._spacing = node.get_length('spacing');
                this._updateWorkspaceActors(false);
            }));

        this._fullGeometry = null;
        this._actualGeometry = null;

        this._spacing = 0;
        this._animating = false; // tweening
        this._scrolling = false; // swipe-scrolling
        this._animatingScroll = false; // programatically updating the adjustment
        this._zoomOut = false; // zoom to a larger area
        this._inDrag = false; // dragging a window

        this._settings = new Gio.Settings({ schema: OVERRIDE_SCHEMA });
        this._updateExtraWorkspacesId =
            this._settings.connect('changed::workspaces-only-on-primary',
                                   Lang.bind(this, this._updateExtraWorkspaces));

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        this._workspaces = workspaces;

        // Add workspace actors
        for (let w = 0; w < global.screen.n_workspaces; w++)
            this.actor.add_actor(this._workspaces[w].actor);
        this._workspaces[activeWorkspaceIndex].actor.raise_top();

        this._extraWorkspaces = [];
        this._updateExtraWorkspaces();

        // Position/scale the desktop windows and their children after the
        // workspaces have been created. This cannot be done first because
        // window movement depends on the Workspaces object being accessible
        // as an Overview member.
        this._overviewShowingId =
            Main.overview.connect('showing',
                                 Lang.bind(this, function() {
                for (let w = 0; w < this._workspaces.length; w++)
                    this._workspaces[w].zoomToOverview();
                for (let w = 0; w < this._extraWorkspaces.length; w++)
                    this._extraWorkspaces[w].zoomToOverview();
        }));
        this._overviewShownId =
            Main.overview.connect('shown',
                                 Lang.bind(this, function() {
                this.actor.set_clip(this._fullGeometry.x, this._fullGeometry.y,
                                    this._fullGeometry.width, this._fullGeometry.height);
        }));

        this.scrollAdjustment = new St.Adjustment({ value: activeWorkspaceIndex,
                                                    lower: 0,
                                                    page_increment: 1,
                                                    page_size: 1,
                                                    step_increment: 0,
                                                    upper: this._workspaces.length });
        this.scrollAdjustment.connect('notify::value',
                                      Lang.bind(this, this._onScroll));

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
    },

    _updateExtraWorkspaces: function() {
        this._destroyExtraWorkspaces();

        if (!this._settings.get_boolean('workspaces-only-on-primary'))
            return;

        let monitors = Main.layoutManager.monitors;
        for (let i = 0; i < monitors.length; i++) {
            if (i == Main.layoutManager.primaryIndex)
                continue;

            let ws = new Workspace.Workspace(null, i);
            ws.setFullGeometry(monitors[i]);
            ws.setActualGeometry(monitors[i]);
            Main.layoutManager.overviewGroup.add_actor(ws.actor);
            this._extraWorkspaces.push(ws);
        }
    },

    _destroyExtraWorkspaces: function() {
        for (let m = 0; m < this._extraWorkspaces.length; m++)
            this._extraWorkspaces[m].destroy();
        this._extraWorkspaces = [];
    },

    setFullGeometry: function(geom) {
        if (rectEqual(this._fullGeometry, geom))
            return;

        this._fullGeometry = geom;

        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].setFullGeometry(geom);
    },

    setActualGeometry: function(geom) {
        if (rectEqual(this._actualGeometry, geom))
            return;

        this._actualGeometry = geom;

        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].setActualGeometry(geom);
    },

    _lookupWorkspaceForMetaWindow: function (metaWindow) {
        for (let i = 0; i < this._workspaces.length; i++) {
            if (this._workspaces[i].containsMetaWindow(metaWindow))
                return this._workspaces[i];
        }
        return null;
    },

    getActiveWorkspace: function() {
        let active = global.screen.get_active_workspace_index();
        return this._workspaces[active];
    },

    hide: function() {
        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        let activeWorkspace = this._workspaces[activeWorkspaceIndex];

        activeWorkspace.actor.raise_top();

        this.actor.remove_clip();

        for (let w = 0; w < this._workspaces.length; w++)
            this._workspaces[w].zoomFromOverview();
        for (let w = 0; w < this._extraWorkspaces.length; w++)
            this._extraWorkspaces[w].zoomFromOverview();
    },

    destroy: function() {
        this.actor.destroy();
    },

    syncStacking: function(stackIndices) {
        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].syncStacking(stackIndices);
        for (let i = 0; i < this._extraWorkspaces.length; i++)
            this._extraWorkspaces[i].syncStacking(stackIndices);
    },

    _scrollToActive: function() {
        let active = global.screen.get_active_workspace_index();

        this._updateWorkspaceActors(true);
        this._updateScrollAdjustment(active);
    },

    // Update workspace actors parameters
    // @showAnimation: iff %true, transition between states
    _updateWorkspaceActors: function(showAnimation) {
        let active = global.screen.get_active_workspace_index();

        this._animating = showAnimation;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            Tweener.removeTweens(workspace.actor);

            let y = (w - active) * (this._fullGeometry.height + this._spacing);

            if (showAnimation) {
                let params = { y: y,
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
                workspace.actor.set_position(0, y);
                if (w == 0)
                    this._updateVisibility();
            }
        }
    },

    _updateVisibility: function() {
        let active = global.screen.get_active_workspace_index();

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];
            if (this._animating || this._scrolling) {
                workspace.actor.show();
            } else {
                if (this._inDrag)
                    workspace.actor.visible = (Math.abs(w - active) <= 1);
                else
                    workspace.actor.visible = (w == active);
            }
        }
    },

    _updateScrollAdjustment: function(index) {
        if (this._scrolling)
            return;

        this._animatingScroll = true;

        Tweener.addTween(this.scrollAdjustment, {
            value: index,
            time: WORKSPACE_SWITCH_TIME,
            transition: 'easeOutQuad',
            onComplete: Lang.bind(this,
                                  function() {
                                      this._animatingScroll = false;
                                  })
        });
    },

    updateWorkspaces: function(oldNumWorkspaces, newNumWorkspaces) {
        let active = global.screen.get_active_workspace_index();

        Tweener.addTween(this.scrollAdjustment,
                         { upper: newNumWorkspaces,
                           time: WORKSPACE_SWITCH_TIME,
                           transition: 'easeOutQuad'
                         });

        if (newNumWorkspaces > oldNumWorkspaces) {
            for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
                this._workspaces[w].setFullGeometry(this._fullGeometry);
                if (this._actualGeometry)
                    this._workspaces[w].setActualGeometry(this._actualGeometry);
                this.actor.add_actor(this._workspaces[w].actor);
            }

            this._updateWorkspaceActors(false);
        }
    },

    _activeWorkspaceChanged: function(wm, from, to, direction) {
        if (this._scrolling)
            return;

        this._scrollToActive();
    },

    _onDestroy: function() {
        this._destroyExtraWorkspaces();
        this.scrollAdjustment.run_dispose();
        Main.overview.disconnect(this._overviewShowingId);
        Main.overview.disconnect(this._overviewShownId);
        global.window_manager.disconnect(this._switchWorkspaceNotifyId);
        this._settings.disconnect(this._updateExtraWorkspacesId);

        if (this._inDrag)
            this._dragEnd();

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

    _dragBegin: function() {
        if (this._scrolling)
            return;

        this._inDrag = true;
        this._firstDragMotion = true;

        this._dragMonitor = {
            dragMotion: Lang.bind(this, this._onDragMotion)
        };
        DND.addDragMonitor(this._dragMonitor);
    },

    _onDragMotion: function(dragEvent) {
        if (Main.overview.animationInProgress)
             return DND.DragMotionResult.CONTINUE;

        if (this._firstDragMotion) {
            this._firstDragMotion = false;
            for (let i = 0; i < this._workspaces.length; i++)
                this._workspaces[i].setReservedSlot(dragEvent.dragActor._delegate);
            for (let i = 0; i < this._extraWorkspaces.length; i++)
                this._extraWorkspaces[i].setReservedSlot(dragEvent.dragActor._delegate);
        }

        return DND.DragMotionResult.CONTINUE;
    },

    _dragEnd: function() {
        DND.removeDragMonitor(this._dragMonitor);
        this._inDrag = false;

        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].setReservedSlot(null);
        for (let i = 0; i < this._extraWorkspaces.length; i++)
            this._extraWorkspaces[i].setReservedSlot(null);
    },

    startSwipeScroll: function() {
        this._scrolling = true;
    },

    endSwipeScroll: function() {
        this._scrolling = false;

        // Make sure title captions etc are shown as necessary
        this._scrollToActive();
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
            if (!this._workspaces[current]) {
                // The current workspace was destroyed. This could happen
                // when you are on the last empty workspace, and consolidate
                // windows using the thumbnail bar.
                // In that case, the intended behavior is to stay on the empty
                // workspace, which is the last one, so pick it.
                current = this._workspaces.length - 1;
            }

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
        let newY =  - adj.value / (adj.upper - 1) * workspacesHeight;

        let dy = newY - currentY;

        for (let i = 0; i < this._workspaces.length; i++) {
            this._workspaces[i].actor.visible = Math.abs(i - adj.value) <= 1;
            this._workspaces[i].actor.y += dy;
        }
    },

    _getWorkspaceIndexToRemove: function() {
        return global.screen.get_active_workspace_index();
    }
});
Signals.addSignalMethods(WorkspacesView.prototype);


const WorkspacesDisplay = new Lang.Class({
    Name: 'WorkspacesDisplay',

    _init: function() {
        this.actor = new St.Widget({ clip_to_allocation: true });
        this.actor.connect('notify::allocation', Lang.bind(this, this._updateWorkspacesActualGeometry));
        this.actor.connect('parent-set', Lang.bind(this, this._parentSet));

        let clickAction = new Clutter.ClickAction()
        clickAction.connect('clicked', Lang.bind(this, function(action) {
            // Only switch to the workspace when there's no application
            // windows open. The problem is that it's too easy to miss
            // an app window and get the wrong one focused.
            if (action.get_button() == 1 &&
                this._getPrimaryView().getActiveWorkspace().isEmpty())
                Main.overview.hide();
        }));
        Main.overview.addAction(clickAction);
        this.actor.bind_property('mapped', clickAction, 'enabled', GObject.BindingFlags.SYNC_CREATE);

        let panAction = new Clutter.PanAction();
        panAction.connect('pan', Lang.bind(this, this._onPan));
        panAction.connect('gesture-begin', Lang.bind(this, function() {
            for (let i = 0; i < this._workspacesViews.length; i++)
                this._workspacesViews[i].startSwipeScroll();
            return true;
        }));
        panAction.connect('gesture-cancel', Lang.bind(this, function() {
            clickAction.release();
            for (let i = 0; i < this._workspacesViews.length; i++)
                this._workspacesViews[i].endSwipeScroll();
        }));
        panAction.connect('gesture-end', Lang.bind(this, function() {
            clickAction.release();
            for (let i = 0; i < this._workspacesViews.length; i++)
                this._workspacesViews[i].endSwipeScroll();
        }));
        Main.overview.addAction(panAction);
        this.actor.bind_property('mapped', panAction, 'enabled', GObject.BindingFlags.SYNC_CREATE);

        this._primaryIndex = Main.layoutManager.primaryIndex;

        this._workspacesViews = [];
        this._workspaces = [];
        this._primaryScrollAdjustment = null;

        this._settings = new Gio.Settings({ schema: OVERRIDE_SCHEMA });
        this._settings.connect('changed::workspaces-only-on-primary',
                               Lang.bind(this,
                                         this._workspacesOnlyOnPrimaryChanged));
        this._workspacesOnlyOnPrimaryChanged();

        global.screen.connect('notify::n-workspaces',
                              Lang.bind(this, this._workspacesChanged));

        this._switchWorkspaceNotifyId = 0;

        this._notifyOpacityId = 0;
        this._scrollEventId = 0;

        this._fullGeometry = null;
    },

    _onPan: function(action) {
        let [dist, dx, dy] = action.get_motion_delta(0);
        let adjustment = this._scrollAdjustment;
        adjustment.value -= (dy / this.actor.height) * adjustment.page_size;
        return false;
    },

    show: function() {
        this._updateWorkspacesViews();

        this._restackedNotifyId =
            Main.overview.connect('windows-restacked',
                                  Lang.bind(this, this._onRestacked));
        if (this._scrollEventId == 0)
            this._scrollEventId = Main.overview.connect('scroll-event', Lang.bind(this, this._onScrollEvent));
    },

    zoomFromOverview: function() {
        for (let i = 0; i < this._workspacesViews.length; i++) {
            this._workspacesViews[i].hide();
        }
    },

    hide: function() {
        if (this._restackedNotifyId > 0){
            Main.overview.disconnect(this._restackedNotifyId);
            this._restackedNotifyId = 0;
        }
        if (this._scrollEventId > 0) {
            Main.overview.disconnect(this._scrollEventId);
            this._scrollEventId = 0;
        }

        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].destroy();
        this._workspacesViews = [];
    },

    _workspacesOnlyOnPrimaryChanged: function() {
        this._workspacesOnlyOnPrimary = this._settings.get_boolean('workspaces-only-on-primary');

        if (!Main.overview.visible)
            return;

        this._updateWorkspacesViews();
    },

    _updateWorkspacesViews: function() {
        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].destroy();

        this._workspacesViews = [];
        this._workspaces = [];
        let monitors = Main.layoutManager.monitors;
        for (let i = 0; i < monitors.length; i++) {
            if (this._workspacesOnlyOnPrimary && i != this._primaryIndex)
                continue;  // we are only interested in the primary monitor

            let monitorWorkspaces = [];
            for (let w = 0; w < global.screen.n_workspaces; w++) {
                let metaWorkspace = global.screen.get_workspace_by_index(w);
                monitorWorkspaces.push(new Workspace.Workspace(metaWorkspace, i));
            }

            this._workspaces.push(monitorWorkspaces);

            let view = new WorkspacesView(monitorWorkspaces);
            view.actor.connect('scroll-event', Lang.bind(this, this._onScrollEvent));
            if (this._workspacesOnlyOnPrimary || i == this._primaryIndex) {
                this._scrollAdjustment = view.scrollAdjustment;
                this._scrollAdjustment.connect('notify::value',
                                               Lang.bind(this, this._scrollValueChanged));
            }
            this._workspacesViews.push(view);
        }

        this._updateWorkspacesFullGeometry();
        this._updateWorkspacesActualGeometry();

        for (let i = 0; i < this._workspacesViews.length; i++)
            Main.layoutManager.overviewGroup.add_actor(this._workspacesViews[i].actor);
    },

    _scrollValueChanged: function() {
        if (this._workspacesOnlyOnPrimary)
            return;

        for (let i = 0; i < this._workspacesViews.length; i++) {
            if (i == this._primaryIndex)
                continue;

            let adjustment = this._workspacesViews[i].scrollAdjustment;
            // the adjustments work in terms of workspaces, so the
            // values map directly
            adjustment.value = this._scrollAdjustment.value;
        }
    },

    _getPrimaryView: function() {
        if (!this._workspacesViews.length)
            return null;
        if (this._workspacesOnlyOnPrimary)
            return this._workspacesViews[0];
        else
            return this._workspacesViews[this._primaryIndex];
    },

    activeWorkspaceHasMaximizedWindows: function() {
        return this._getPrimaryView().getActiveWorkspace().hasMaximizedWindows();
    },

    _parentSet: function(actor, oldParent) {
        if (oldParent && this._notifyOpacityId)
            oldParent.disconnect(this._notifyOpacityId);
        this._notifyOpacityId = 0;

        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, Lang.bind(this,
            function() {
                let newParent = this.actor.get_parent();
                if (!newParent)
                    return;

                // This is kinda hackish - we want the primary view to
                // appear as parent of this.actor, though in reality it
                // is added directly to Main.layoutManager.overviewGroup
                this._notifyOpacityId = newParent.connect('notify::opacity',
                    Lang.bind(this, function() {
                        let opacity = this.actor.get_parent().opacity;
                        let primaryView = this._getPrimaryView();
                        if (!primaryView)
                            return;
                        primaryView.actor.opacity = opacity;
                        primaryView.actor.visible = opacity != 0;
                    }));
        }));
    },

    // This geometry should always be the fullest geometry
    // the workspaces switcher can ever be allocated, as if
    // the sliding controls were never slid in at all.
    setWorkspacesFullGeometry: function(geom) {
        this._fullGeometry = geom;
        this._updateWorkspacesFullGeometry();
    },

    _updateWorkspacesFullGeometry: function() {
        if (!this._workspacesViews.length)
            return;

        let monitors = Main.layoutManager.monitors;
        let m = 0;
        for (let i = 0; i < monitors.length; i++) {
            if (i == this._primaryIndex) {
                this._workspacesViews[m].setFullGeometry(this._fullGeometry);
                m++;
            } else if (!this._workspacesOnlyOnPrimary) {
                this._workspacesViews[m].setFullGeometry(monitors[i]);
                m++;
            }
        }
    },

    _updateWorkspacesActualGeometry: function() {
        if (!this._workspacesViews.length)
            return;

        let [x, y] = this.actor.get_transformed_position();
        let width = this.actor.allocation.x2 - this.actor.allocation.x1;
        let height = this.actor.allocation.y2 - this.actor.allocation.y1;
        let geometry = { x: x, y: y, width: width, height: height };

        let monitors = Main.layoutManager.monitors;
        let m = 0;
        for (let i = 0; i < monitors.length; i++) {
            if (i == this._primaryIndex) {
                this._workspacesViews[m].setActualGeometry(geometry);
                m++;
            } else if (!this._workspacesOnlyOnPrimary) {
                this._workspacesViews[m].setActualGeometry(monitors[i]);
                m++;
            }
        }
    },

    _onRestacked: function(overview, stackIndices) {
        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].syncStacking(stackIndices);
    },

    _workspacesChanged: function() {
        if (!this._workspacesViews.length)
            return;

        let oldNumWorkspaces = this._workspaces[0].length;
        let newNumWorkspaces = global.screen.n_workspaces;
        let active = global.screen.get_active_workspace_index();

        let lostWorkspaces = [];
        if (newNumWorkspaces > oldNumWorkspaces) {
            let monitors = Main.layoutManager.monitors;
            let m = 0;
            for (let i = 0; i < monitors.length; i++) {
                if (this._workspacesOnlyOnPrimary &&
                    i != this._primaryIndex)
                    continue;

                // Assume workspaces are only added at the end
                for (let w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
                    let metaWorkspace = global.screen.get_workspace_by_index(w);
                    this._workspaces[m][w] =
                        new Workspace.Workspace(metaWorkspace, i);
                }
                m++;
            }
        } else {
            // Assume workspaces are only removed sequentially
            // (e.g. 2,3,4 - not 2,4,7)
            let removedIndex;
            let removedNum = oldNumWorkspaces - newNumWorkspaces;
            for (let w = 0; w < oldNumWorkspaces; w++) {
                let metaWorkspace = global.screen.get_workspace_by_index(w);
                if (this._workspaces[0][w].metaWorkspace != metaWorkspace) {
                    removedIndex = w;
                    break;
                }
            }

            for (let i = 0; i < this._workspaces.length; i++) {
                lostWorkspaces = this._workspaces[i].splice(removedIndex,
                                                            removedNum);

                for (let l = 0; l < lostWorkspaces.length; l++) {
                    lostWorkspaces[l].disconnectAll();
                    lostWorkspaces[l].destroy();
                }
            }
        }

        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].updateWorkspaces(oldNumWorkspaces,
                                                      newNumWorkspaces);
    },

    _onScrollEvent: function(actor, event) {
        if (!this.actor.mapped)
            return false;
        let activeWs = global.screen.get_active_workspace();
        let ws;
        switch (event.get_scroll_direction()) {
        case Clutter.ScrollDirection.UP:
            ws = activeWs.get_neighbor(Meta.MotionDirection.UP);
            break;
        case Clutter.ScrollDirection.DOWN:
            ws = activeWs.get_neighbor(Meta.MotionDirection.DOWN);
            break;
        default:
            return false;
        }
        Main.wm.actionMoveWorkspace(ws);
        return true;
    }
});
Signals.addSignalMethods(WorkspacesDisplay.prototype);
