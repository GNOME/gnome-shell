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

var WORKSPACE_SWITCH_TIME = 0.25;

var AnimationType = {
    ZOOM: 0,
    FADE: 1
};

const MUTTER_SCHEMA = 'org.gnome.mutter';

var WorkspacesViewBase = new Lang.Class({
    Name: 'WorkspacesViewBase',

    _init(monitorIndex) {
        this.actor = new St.Widget({ style_class: 'workspaces-view',
                                     reactive: true });
        this.actor.connect('destroy', this._onDestroy.bind(this));
        global.focus_manager.add_group(this.actor);

        // The actor itself isn't a drop target, so we don't want to pick on its area
        this.actor.set_size(0, 0);

        this._monitorIndex = monitorIndex;

        this._fullGeometry = null;
        this._actualGeometry = null;

        this._inDrag = false;
        this._windowDragBeginId = Main.overview.connect('window-drag-begin', this._dragBegin.bind(this));
        this._windowDragEndId = Main.overview.connect('window-drag-end', this._dragEnd.bind(this));
    },

    _onDestroy() {
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

    _dragBegin(overview, window) {
        this._inDrag = true;
        this._setReservedSlot(window);
    },

    _dragEnd() {
        this._inDrag = false;
        this._setReservedSlot(null);
    },

    destroy() {
        this.actor.destroy();
    },

    setFullGeometry(geom) {
        this._fullGeometry = geom;
        this._syncFullGeometry();
    },

    setActualGeometry(geom) {
        this._actualGeometry = geom;
        this._syncActualGeometry();
    },
});

var WorkspacesView = new Lang.Class({
    Name: 'WorkspacesView',
    Extends: WorkspacesViewBase,

    _init(monitorIndex) {
        let workspaceManager = global.workspace_manager;

        this.parent(monitorIndex);

        this._animating = false; // tweening
        this._scrolling = false; // swipe-scrolling
        this._animatingScroll = false; // programatically updating the adjustment

        let activeWorkspaceIndex = workspaceManager.get_active_workspace_index();
        this.scrollAdjustment = new St.Adjustment({ value: activeWorkspaceIndex,
                                                    lower: 0,
                                                    page_increment: 1,
                                                    page_size: 1,
                                                    step_increment: 0,
                                                    upper: workspaceManager.n_workspaces });
        this.scrollAdjustment.connect('notify::value',
                                      this._onScroll.bind(this));

        this._workspaces = [];
        this._updateWorkspaces();
        this._updateWorkspacesId =
            workspaceManager.connect('notify::n-workspaces',
                                     this._updateWorkspaces.bind(this));

        this._overviewShownId =
            Main.overview.connect('shown', () => {
                this.actor.set_clip(this._fullGeometry.x, this._fullGeometry.y,
                                    this._fullGeometry.width, this._fullGeometry.height);
            });

        this._switchWorkspaceNotifyId =
            global.window_manager.connect('switch-workspace',
                                          this._activeWorkspaceChanged.bind(this));
    },

    _setReservedSlot(window) {
        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].setReservedSlot(window);
    },

    _syncFullGeometry() {
        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].setFullGeometry(this._fullGeometry);
    },

    _syncActualGeometry() {
        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].setActualGeometry(this._actualGeometry);
    },

    getActiveWorkspace() {
        let workspaceManager = global.workspace_manager;
        let active = workspaceManager.get_active_workspace_index();
        return this._workspaces[active];
    },

    animateToOverview(animationType) {
        for (let w = 0; w < this._workspaces.length; w++) {
            if (animationType == AnimationType.ZOOM)
                this._workspaces[w].zoomToOverview();
            else
                this._workspaces[w].fadeToOverview();
        }
        this._updateWorkspaceActors(false);
    },

    animateFromOverview(animationType) {
        this.actor.remove_clip();

        for (let w = 0; w < this._workspaces.length; w++) {
            if (animationType == AnimationType.ZOOM)
                this._workspaces[w].zoomFromOverview();
            else
                this._workspaces[w].fadeFromOverview();
        }
    },

    syncStacking(stackIndices) {
        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].syncStacking(stackIndices);
    },

    _scrollToActive() {
        let workspaceManager = global.workspace_manager;
        let active = workspaceManager.get_active_workspace_index();

        this._updateWorkspaceActors(true);
        this._updateScrollAdjustment(active);
    },

    // Update workspace actors parameters
    // @showAnimation: iff %true, transition between states
    _updateWorkspaceActors(showAnimation) {
        let workspaceManager = global.workspace_manager;
        let active = workspaceManager.get_active_workspace_index();

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
                    params.onComplete = () => {
                        this._animating = false;
                        this._updateVisibility();
                    };
                }
                Tweener.addTween(workspace.actor, params);
            } else {
                workspace.actor.set_position(0, y);
                if (w == 0)
                    this._updateVisibility();
            }
        }
    },

    _updateVisibility() {
        let workspaceManager = global.workspace_manager;
        let active = workspaceManager.get_active_workspace_index();

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

    _updateScrollAdjustment(index) {
        if (this._scrolling)
            return;

        this._animatingScroll = true;

        Tweener.addTween(this.scrollAdjustment, {
            value: index,
            time: WORKSPACE_SWITCH_TIME,
            transition: 'easeOutQuad',
            onComplete: () => {
                this._animatingScroll = false;
            }
        });
    },

    _updateWorkspaces() {
        let workspaceManager = global.workspace_manager;
        let newNumWorkspaces = workspaceManager.n_workspaces;

        this.scrollAdjustment.upper = newNumWorkspaces;

        let needsUpdate = false;
        for (let j = 0; j < newNumWorkspaces; j++) {
            let metaWorkspace = workspaceManager.get_workspace_by_index(j);
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

    _activeWorkspaceChanged(wm, from, to, direction) {
        if (this._scrolling)
            return;

        this._scrollToActive();
    },

    _onDestroy() {
        this.parent();

        this.scrollAdjustment.run_dispose();
        Main.overview.disconnect(this._overviewShownId);
        global.window_manager.disconnect(this._switchWorkspaceNotifyId);
        let workspaceManager = global.workspace_manager;
        workspaceManager.disconnect(this._updateWorkspacesId);
    },

    startSwipeScroll() {
        this._scrolling = true;
    },

    endSwipeScroll() {
        this._scrolling = false;

        // Make sure title captions etc are shown as necessary
        this._scrollToActive();
        this._updateVisibility();
    },

    // sync the workspaces' positions to the value of the scroll adjustment
    // and change the active workspace if appropriate
    _onScroll(adj) {
        if (this._animatingScroll)
            return;

        let workspaceManager = global.workspace_manager;
        let active = workspaceManager.get_active_workspace_index();
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

var ExtraWorkspaceView = new Lang.Class({
    Name: 'ExtraWorkspaceView',
    Extends: WorkspacesViewBase,

    _init(monitorIndex) {
        this.parent(monitorIndex);
        this._workspace = new Workspace.Workspace(null, monitorIndex);
        this.actor.add_actor(this._workspace.actor);
    },

    _setReservedSlot(window) {
        this._workspace.setReservedSlot(window);
    },

    _syncFullGeometry() {
        this._workspace.setFullGeometry(this._fullGeometry);
    },

    _syncActualGeometry() {
        this._workspace.setActualGeometry(this._actualGeometry);
    },

    getActiveWorkspace() {
        return this._workspace;
    },

    animateToOverview(animationType) {
        if (animationType == AnimationType.ZOOM)
            this._workspace.zoomToOverview();
        else
            this._workspace.fadeToOverview();
    },

    animateFromOverview(animationType) {
        if (animationType == AnimationType.ZOOM)
            this._workspace.zoomFromOverview();
        else
            this._workspace.fadeFromOverview();
    },

    syncStacking(stackIndices) {
        this._workspace.syncStacking(stackIndices);
    },

    startSwipeScroll() {
    },
    endSwipeScroll() {
    },
});

var DelegateFocusNavigator = new Lang.Class({
    Name: 'DelegateFocusNavigator',
    Extends: St.Widget,

    vfunc_navigate_focus(from, direction) {
        return this._delegate.navigateFocus(from, direction);
    },
});

var WorkspacesDisplay = new Lang.Class({
    Name: 'WorkspacesDisplay',

    _init() {
        this.actor = new DelegateFocusNavigator({ clip_to_allocation: true });
        this.actor._delegate = this;
        this.actor.connect('notify::allocation', this._updateWorkspacesActualGeometry.bind(this));
        this.actor.connect('parent-set', this._parentSet.bind(this));

        let clickAction = new Clutter.ClickAction();
        clickAction.connect('clicked', action => {
            // Only switch to the workspace when there's no application
            // windows open. The problem is that it's too easy to miss
            // an app window and get the wrong one focused.
            let event = Clutter.get_current_event();
            let index = this._getMonitorIndexForEvent(event);
            if ((action.get_button() == 1 || action.get_button() == 0) &&
                this._workspacesViews[index].getActiveWorkspace().isEmpty())
                Main.overview.hide();
        });
        Main.overview.addAction(clickAction);
        this.actor.bind_property('mapped', clickAction, 'enabled', GObject.BindingFlags.SYNC_CREATE);

        let panAction = new Clutter.PanAction({ threshold_trigger_edge: Clutter.GestureTriggerEdge.AFTER });
        panAction.connect('pan', this._onPan.bind(this));
        panAction.connect('gesture-begin', () => {
            if (this._workspacesOnlyOnPrimary) {
                let event = Clutter.get_current_event();
                if (this._getMonitorIndexForEvent(event) != this._primaryIndex)
                    return false;
            }

            for (let i = 0; i < this._workspacesViews.length; i++)
                this._workspacesViews[i].startSwipeScroll();
            return true;
        });
        panAction.connect('gesture-cancel', () => {
            clickAction.release();
            for (let i = 0; i < this._workspacesViews.length; i++)
                this._workspacesViews[i].endSwipeScroll();
        });
        panAction.connect('gesture-end', () => {
            clickAction.release();
            for (let i = 0; i < this._workspacesViews.length; i++)
                this._workspacesViews[i].endSwipeScroll();
        });
        Main.overview.addAction(panAction);
        this.actor.bind_property('mapped', panAction, 'enabled', GObject.BindingFlags.SYNC_CREATE);

        this._primaryIndex = Main.layoutManager.primaryIndex;

        this._workspacesViews = [];
        this._primaryScrollAdjustment = null;

        this._settings = new Gio.Settings({ schema_id: MUTTER_SCHEMA });
        this._settings.connect('changed::workspaces-only-on-primary',
                               this._workspacesOnlyOnPrimaryChanged.bind(this));
        this._workspacesOnlyOnPrimaryChanged();

        this._switchWorkspaceNotifyId = 0;

        this._notifyOpacityId = 0;
        this._restackedNotifyId = 0;
        this._scrollEventId = 0;
        this._keyPressEventId = 0;

        this._fullGeometry = null;
    },

    _onPan(action) {
        let [dist, dx, dy] = action.get_motion_delta(0);
        let adjustment = this._scrollAdjustment;
        adjustment.value -= (dy / this.actor.height) * adjustment.page_size;
        return false;
    },

    navigateFocus(from, direction) {
        return this._getPrimaryView().actor.navigate_focus(from, direction, false);
    },

    show(fadeOnPrimary) {
        this._updateWorkspacesViews();
        for (let i = 0; i < this._workspacesViews.length; i++) {
            let animationType;
            if (fadeOnPrimary && i == this._primaryIndex)
                animationType = AnimationType.FADE;
            else
                animationType = AnimationType.ZOOM;
            this._workspacesViews[i].animateToOverview(animationType);
        }

        this._restackedNotifyId =
            Main.overview.connect('windows-restacked',
                                  this._onRestacked.bind(this));
        if (this._scrollEventId == 0)
            this._scrollEventId = Main.overview.connect('scroll-event', this._onScrollEvent.bind(this));

        if (this._keyPressEventId == 0)
            this._keyPressEventId = global.stage.connect('key-press-event', this._onKeyPressEvent.bind(this));
    },

    animateFromOverview(fadeOnPrimary) {
        for (let i = 0; i < this._workspacesViews.length; i++) {
            let animationType;
            if (fadeOnPrimary && i == this._primaryIndex)
                animationType = AnimationType.FADE;
            else
                animationType = AnimationType.ZOOM;
            this._workspacesViews[i].animateFromOverview(animationType);
        }
    },

    hide() {
        if (this._restackedNotifyId > 0){
            Main.overview.disconnect(this._restackedNotifyId);
            this._restackedNotifyId = 0;
        }
        if (this._scrollEventId > 0) {
            Main.overview.disconnect(this._scrollEventId);
            this._scrollEventId = 0;
        }
        if (this._keyPressEventId > 0) {
            global.stage.disconnect(this._keyPressEventId);
            this._keyPressEventId = 0;
        }
        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].destroy();
        this._workspacesViews = [];
    },

    _workspacesOnlyOnPrimaryChanged() {
        this._workspacesOnlyOnPrimary = this._settings.get_boolean('workspaces-only-on-primary');

        if (!Main.overview.visible)
            return;

        this._updateWorkspacesViews();
    },

    _updateWorkspacesViews() {
        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].destroy();

        this._primaryIndex = Main.layoutManager.primaryIndex;
        this._workspacesViews = [];
        let monitors = Main.layoutManager.monitors;
        for (let i = 0; i < monitors.length; i++) {
            let view;
            if (this._workspacesOnlyOnPrimary && i != this._primaryIndex)
                view = new ExtraWorkspaceView(i);
            else
                view = new WorkspacesView(i);

            view.actor.connect('scroll-event', this._onScrollEvent.bind(this));
            if (i == this._primaryIndex) {
                this._scrollAdjustment = view.scrollAdjustment;
                this._scrollAdjustment.connect('notify::value',
                                               this._scrollValueChanged.bind(this));
            }

            this._workspacesViews.push(view);
            Main.layoutManager.overviewGroup.add_actor(view.actor);
        }

        this._updateWorkspacesFullGeometry();
        this._updateWorkspacesActualGeometry();
    },

    _scrollValueChanged() {
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

    _getMonitorIndexForEvent(event) {
        let [x, y] = event.get_coords();
        let rect = new Meta.Rectangle({ x: x, y: y, width: 1, height: 1 });
        return global.display.get_monitor_index_for_rect(rect);
    },

    _getPrimaryView() {
        if (!this._workspacesViews.length)
            return null;
        return this._workspacesViews[this._primaryIndex];
    },

    activeWorkspaceHasMaximizedWindows() {
        return this._getPrimaryView().getActiveWorkspace().hasMaximizedWindows();
    },

    _parentSet(actor, oldParent) {
        if (oldParent && this._notifyOpacityId)
            oldParent.disconnect(this._notifyOpacityId);
        this._notifyOpacityId = 0;

        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
            let newParent = this.actor.get_parent();
            if (!newParent)
                return;

            // This is kinda hackish - we want the primary view to
            // appear as parent of this.actor, though in reality it
            // is added directly to Main.layoutManager.overviewGroup
            this._notifyOpacityId = newParent.connect('notify::opacity', () => {
                let opacity = this.actor.get_parent().opacity;
                let primaryView = this._getPrimaryView();
                if (!primaryView)
                    return;
                primaryView.actor.opacity = opacity;
                primaryView.actor.visible = opacity != 0;
            });
        });
    },

    // This geometry should always be the fullest geometry
    // the workspaces switcher can ever be allocated, as if
    // the sliding controls were never slid in at all.
    setWorkspacesFullGeometry(geom) {
        this._fullGeometry = geom;
        this._updateWorkspacesFullGeometry();
    },

    _updateWorkspacesFullGeometry() {
        if (!this._workspacesViews.length)
            return;

        let monitors = Main.layoutManager.monitors;
        for (let i = 0; i < monitors.length; i++) {
            let geometry = (i == this._primaryIndex) ? this._fullGeometry : monitors[i];
            this._workspacesViews[i].setFullGeometry(geometry);
        }
    },

    _updateWorkspacesActualGeometry() {
        if (!this._workspacesViews.length)
            return;

        let [x, y] = this.actor.get_transformed_position();
        let allocation = this.actor.allocation;
        let width = allocation.x2 - allocation.x1;
        let height = allocation.y2 - allocation.y1;
        let primaryGeometry = { x: x, y: y, width: width, height: height };

        let monitors = Main.layoutManager.monitors;
        for (let i = 0; i < monitors.length; i++) {
            let geometry = (i == this._primaryIndex) ? primaryGeometry : monitors[i];
            this._workspacesViews[i].setActualGeometry(geometry);
        }
    },

    _onRestacked(overview, stackIndices) {
        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].syncStacking(stackIndices);
    },

    _onScrollEvent(actor, event) {
        if (!this.actor.mapped)
            return Clutter.EVENT_PROPAGATE;

        if (this._workspacesOnlyOnPrimary &&
            this._getMonitorIndexForEvent(event) != this._primaryIndex)
            return Clutter.EVENT_PROPAGATE;

        let workspaceManager = global.workspace_manager;
        let activeWs = workspaceManager.get_active_workspace();
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
    },

    _onKeyPressEvent(actor, event) {
        if (!this.actor.mapped)
            return Clutter.EVENT_PROPAGATE;
        let workspaceManager = global.workspace_manager;
        let activeWs = workspaceManager.get_active_workspace();
        let ws;
        switch (event.get_key_symbol()) {
        case Clutter.KEY_Page_Up:
            ws = activeWs.get_neighbor(Meta.MotionDirection.UP);
            break;
        case Clutter.KEY_Page_Down:
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
