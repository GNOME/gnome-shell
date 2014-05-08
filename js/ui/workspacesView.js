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

const WorkspacesViewBase = new Lang.Class({
    Name: 'WorkspacesViewBase',

    _init: function(monitorIndex) {
        this.actor = new St.Widget({ style_class: 'workspaces-view',
                                     reactive: true });
        this.actor.connect('destroy', Lang.bind(this, this._onDestroy));
        global.focus_manager.add_group(this.actor);

        // The actor itself isn't a drop target, so we don't want to pick on its area
        this.actor.set_size(0, 0);

        this._monitorIndex = monitorIndex;

        this._fullGeometry = null;
        this._actualGeometry = null;

        this._inDrag = false;
        this._windowDragBeginId = Main.overview.connect('window-drag-begin', Lang.bind(this, this._dragBegin));
        this._windowDragEndId = Main.overview.connect('window-drag-end', Lang.bind(this, this._dragEnd));
    },

    _onDestroy: function() {
        this._dragEnd();

        if (this._windowDragBeginId > 0) {
            Main.overview.disconnect(this._windowDragBeginId);
            this._windowDragBeginId = 0;
        }
        if (this._windowDragEndId > 0) {
            Main.overview.disconnect(this._windowDragEndId);
            this._windowDragEndId = 0;
        }
    },

    _dragBegin: function(overview, clone) {
        this._inDrag = true;
        this._setReservedSlot(clone);
    },

    _dragEnd: function() {
        this._inDrag = false;
        this._setReservedSlot(null);
    },

    destroy: function() {
        this.actor.destroy();
    },

    setFullGeometry: function(geom) {
        this._fullGeometry = geom;
        this._syncFullGeometry();
    },

    setActualGeometry: function(geom) {
        this._actualGeometry = geom;
        this._syncActualGeometry();
    },
});

const WorkspacesView = new Lang.Class({
    Name: 'WorkspacesView',
    Extends: WorkspacesViewBase,

    _init: function(monitorIndex) {
        this.parent(monitorIndex);

        this._animating = false; // tweening
        this._scrolling = false; // swipe-scrolling
        this._animatingScroll = false; // programatically updating the adjustment

        this._settings = new Gio.Settings({ schema_id: OVERRIDE_SCHEMA });

        let activeWorkspaceIndex = global.screen.get_active_workspace_index();
        this.scrollAdjustment = new St.Adjustment({ value: activeWorkspaceIndex,
                                                    lower: 0,
                                                    page_increment: 1,
                                                    page_size: 1,
                                                    step_increment: 0,
                                                    upper: 0 });
        this.scrollAdjustment.connect('notify::value',
                                      Lang.bind(this, this._onScroll));

        this._workspaces = [];
        this._updateWorkspaces();
        this._updateWorkspacesId = global.screen.connect('notify::n-workspaces', Lang.bind(this, this._updateWorkspaces));

        this._overviewShownId =
            Main.overview.connect('shown',
                                 Lang.bind(this, function() {
                this.actor.set_clip(this._fullGeometry.x, this._fullGeometry.y,
                                    this._fullGeometry.width, this._fullGeometry.height);
        }));

        this._switchWorkspaceNotifyId =
            global.window_manager.connect('switch-workspace',
                                          Lang.bind(this, this._activeWorkspaceChanged));
    },

    _setReservedSlot: function(clone) {
        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].setReservedSlot(clone);
    },

    _syncFullGeometry: function() {
        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].setFullGeometry(this._fullGeometry);
    },

    _syncActualGeometry: function() {
        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].setActualGeometry(this._actualGeometry);
    },

    getActiveWorkspace: function() {
        let active = global.screen.get_active_workspace_index();
        return this._workspaces[active];
    },

    zoomToOverview: function() {
        for (let w = 0; w < this._workspaces.length; w++)
            this._workspaces[w].zoomToOverview();
        this._updateWorkspaceActors(false);
    },

    zoomFromOverview: function() {
        this.actor.remove_clip();

        for (let w = 0; w < this._workspaces.length; w++)
            this._workspaces[w].zoomFromOverview();
    },

    syncStacking: function(stackIndices) {
        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].syncStacking(stackIndices);
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

            let y = (w - active) * this._fullGeometry.height;

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

    _updateWorkspaces: function() {
        let newNumWorkspaces = global.screen.n_workspaces;

        this.scrollAdjustment.upper = newNumWorkspaces;

        let needsUpdate = false;
        for (let j = 0; j < newNumWorkspaces; j++) {
            let metaWorkspace = global.screen.get_workspace_by_index(j);
            let workspace;

            if (j >= this._workspaces.length) { /* added */
                workspace = new Workspace.Workspace(metaWorkspace, this._monitorIndex);
                this.actor.add_actor(workspace.actor);
                this._workspaces[j] = workspace;
            } else  {
                workspace = this._workspaces[j];

                if (workspace.metaWorkspace != metaWorkspace) { /* removed */
                    workspace.destroy();
                    this._workspaces.splice(j, 1);
                } /* else kept */
            }
        }

        if (this._fullGeometry) {
            this._updateWorkspaceActors(false);
            this._syncFullGeometry();
        }
        if (this._actualGeometry)
            this._syncActualGeometry();
    },

    _activeWorkspaceChanged: function(wm, from, to, direction) {
        if (this._scrolling)
            return;

        this._scrollToActive();
    },

    _onDestroy: function() {
        this.parent();

        this.scrollAdjustment.run_dispose();
        Main.overview.disconnect(this._overviewShownId);
        global.window_manager.disconnect(this._switchWorkspaceNotifyId);
        global.screen.disconnect(this._updateWorkspacesId);
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
});
Signals.addSignalMethods(WorkspacesView.prototype);

const ExtraWorkspaceView = new Lang.Class({
    Name: 'ExtraWorkspaceView',
    Extends: WorkspacesViewBase,

    _init: function(monitorIndex) {
        this.parent(monitorIndex);
        this._workspace = new Workspace.Workspace(null, monitorIndex);
        this.actor.add_actor(this._workspace.actor);
    },

    _setReservedSlot: function(clone) {
        this._workspace.setReservedSlot(clone);
    },

    _syncFullGeometry: function() {
        this._workspace.setFullGeometry(this._fullGeometry);
    },

    _syncActualGeometry: function() {
        this._workspace.setActualGeometry(this._actualGeometry);
    },

    zoomToOverview: function() {
        this._workspace.zoomToOverview();
    },

    zoomFromOverview: function() {
        this._workspace.zoomFromOverview();
    },

    syncStacking: function(stackIndices) {
        this._workspace.syncStacking(stackIndices);
    },

    startSwipeScroll: function() {
    },
    endSwipeScroll: function() {
    },
});

const DelegateFocusNavigator = new Lang.Class({
    Name: 'DelegateFocusNavigator',
    Extends: St.Widget,

    vfunc_navigate_focus: function(from, direction) {
        return this._delegate.navigateFocus(from, direction);
    },
});

const WorkspacesDisplay = new Lang.Class({
    Name: 'WorkspacesDisplay',

    _init: function() {
        this.actor = new DelegateFocusNavigator({ clip_to_allocation: true });
        this.actor._delegate = this;
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

        let panAction = new Clutter.PanAction({ threshold_trigger_edge: Clutter.GestureTriggerEdge.AFTER });
        panAction.connect('pan', Lang.bind(this, this._onPan));
        panAction.connect('gesture-begin', Lang.bind(this, function() {
            for (let i = 0; i < this._workspacesViews.length; i++)
                this._workspacesViews[i].startSwipeScroll();
            return true;
        }));
        panAction.connect('gesture-cancel', Lang.bind(this, function() {
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
        this._primaryScrollAdjustment = null;

        this._settings = new Gio.Settings({ schema_id: OVERRIDE_SCHEMA });
        this._settings.connect('changed::workspaces-only-on-primary',
                               Lang.bind(this,
                                         this._workspacesOnlyOnPrimaryChanged));
        this._workspacesOnlyOnPrimaryChanged();

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

    navigateFocus: function(from, direction) {
        return this._getPrimaryView().actor.navigate_focus(from, direction, false);
    },

    show: function() {
        this._updateWorkspacesViews();
        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].zoomToOverview();

        this._restackedNotifyId =
            Main.overview.connect('windows-restacked',
                                  Lang.bind(this, this._onRestacked));
        if (this._scrollEventId == 0)
            this._scrollEventId = Main.overview.connect('scroll-event', Lang.bind(this, this._onScrollEvent));
    },

    zoomFromOverview: function() {
        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].zoomFromOverview();
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
        let monitors = Main.layoutManager.monitors;
        for (let i = 0; i < monitors.length; i++) {
            let view;
            if (this._workspacesOnlyOnPrimary && i != this._primaryIndex)
                view = new ExtraWorkspaceView(i);
            else
                view = new WorkspacesView(i);

            view.actor.connect('scroll-event', Lang.bind(this, this._onScrollEvent));
            if (i == this._primaryIndex) {
                this._scrollAdjustment = view.scrollAdjustment;
                this._scrollAdjustment.connect('notify::value',
                                               Lang.bind(this, this._scrollValueChanged));
            }

            this._workspacesViews.push(view);
            Main.layoutManager.overviewGroup.add_actor(view.actor);
        }

        this._updateWorkspacesFullGeometry();
        this._updateWorkspacesActualGeometry();
    },

    _scrollValueChanged: function() {
        for (let i = 0; i < this._workspacesViews.length; i++) {
            if (i == this._primaryIndex)
                continue;

            let adjustment = this._workspacesViews[i].scrollAdjustment;
            if (!adjustment)
                continue;

            // the adjustments work in terms of workspaces, so the
            // values map directly
            adjustment.value = this._scrollAdjustment.value;
        }
    },

    _getPrimaryView: function() {
        if (!this._workspacesViews.length)
            return null;
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
        for (let i = 0; i < monitors.length; i++) {
            let geometry = (i == this._primaryIndex) ? this._fullGeometry : monitors[i];
            this._workspacesViews[i].setFullGeometry(geometry);
        }
    },

    _updateWorkspacesActualGeometry: function() {
        if (!this._workspacesViews.length)
            return;

        let [x, y] = this.actor.get_transformed_position();
        let width = this.actor.allocation.x2 - this.actor.allocation.x1;
        let height = this.actor.allocation.y2 - this.actor.allocation.y1;
        let primaryGeometry = { x: x, y: y, width: width, height: height };

        let monitors = Main.layoutManager.monitors;
        for (let i = 0; i < monitors.length; i++) {
            let geometry = (i == this._primaryIndex) ? primaryGeometry : monitors[i];
            this._workspacesViews[i].setActualGeometry(geometry);
        }
    },

    _onRestacked: function(overview, stackIndices) {
        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].syncStacking(stackIndices);
    },

    _onScrollEvent: function(actor, event) {
        if (!this.actor.mapped)
            return Clutter.EVENT_PROPAGATE;
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
            return Clutter.EVENT_PROPAGATE;
        }
        Main.wm.actionMoveWorkspace(ws);
        return Clutter.EVENT_STOP;
    }
});
Signals.addSignalMethods(WorkspacesDisplay.prototype);
