// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspacesView, WorkspacesDisplay */

const { Clutter, Gio, GObject, Meta, Shell, St } = imports.gi;

const Layout = imports.ui.layout;
const Main = imports.ui.main;
const OverviewControls = imports.ui.overviewControls;
const SwipeTracker = imports.ui.swipeTracker;
const Util = imports.misc.util;
const Workspace = imports.ui.workspace;
const { ThumbnailsBox, MAX_THUMBNAIL_SCALE } = imports.ui.workspaceThumbnail;

var WORKSPACE_SWITCH_TIME = 250;

const MUTTER_SCHEMA = 'org.gnome.mutter';

const WORKSPACE_MIN_SPACING = 24;
const WORKSPACE_MAX_SPACING = 80;

const WORKSPACE_INACTIVE_SCALE = 0.94;

const SECONDARY_WORKSPACE_SCALE = 0.80;

var WorkspacesViewBase = GObject.registerClass({
    GTypeFlags: GObject.TypeFlags.ABSTRACT,
}, class WorkspacesViewBase extends St.Widget {
    _init(monitorIndex, overviewAdjustment) {
        super._init({
            style_class: 'workspaces-view',
            x_expand: true,
            y_expand: true,
        });
        this.connect('destroy', this._onDestroy.bind(this));
        global.focus_manager.add_group(this);

        this._monitorIndex = monitorIndex;

        this._inDrag = false;
        Main.overview.connectObject(
            'window-drag-begin', this._dragBegin.bind(this),
            'window-drag-end', this._dragEnd.bind(this), this);

        this._overviewAdjustment = overviewAdjustment;
        overviewAdjustment.connectObject('notify::value',
            () => this._updateWorkspaceMode(), this);
    }

    _onDestroy() {
        this._dragEnd();
    }

    _dragBegin() {
        this._inDrag = true;
    }

    _dragEnd() {
        this._inDrag = false;
    }

    _updateWorkspaceMode() {
    }

    vfunc_allocate(box) {
        this.set_allocation(box);

        for (const child of this)
            child.allocate_available_size(0, 0, box.get_width(), box.get_height());
    }

    vfunc_get_preferred_width() {
        return [0, 0];
    }

    vfunc_get_preferred_height() {
        return [0, 0];
    }
});

var FitMode = {
    SINGLE: 0,
    ALL: 1,
};

var WorkspacesView = GObject.registerClass(
class WorkspacesView extends WorkspacesViewBase {
    _init(monitorIndex, controls, scrollAdjustment, fitModeAdjustment, overviewAdjustment) {
        let workspaceManager = global.workspace_manager;

        super._init(monitorIndex, overviewAdjustment);

        this._controls = controls;
        this._fitModeAdjustment = fitModeAdjustment;
        this._fitModeAdjustment.connectObject('notify::value', () => {
            this._updateVisibility();
            this._updateWorkspacesState();
            this.queue_relayout();
        }, this);

        this._animating = false; // tweening
        this._gestureActive = false; // touch(pad) gestures

        this._scrollAdjustment = scrollAdjustment;
        this._scrollAdjustment.connectObject('notify::value',
            this._onScrollAdjustmentChanged.bind(this), this);

        this._workspaces = [];
        this._updateWorkspaces();
        workspaceManager.connectObject(
            'notify::n-workspaces', this._updateWorkspaces.bind(this),
            'workspaces-reordered', () => {
                this._workspaces.sort((a, b) => {
                    return a.metaWorkspace.index() - b.metaWorkspace.index();
                });
                this._workspaces.forEach(
                    (ws, i) => this.set_child_at_index(ws, i));
            }, this);

        global.window_manager.connectObject('switch-workspace',
            this._activeWorkspaceChanged.bind(this), this);
    }

    _getFirstFitAllWorkspaceBox(box, spacing, vertical) {
        const { nWorkspaces } = global.workspaceManager;
        const [width, height] = box.get_size();
        const [workspace] = this._workspaces;

        const fitAllBox = new Clutter.ActorBox();

        let [x1, y1] = box.get_origin();

        // Spacing here is not only the space between workspaces, but also the
        // space before the first workspace, and after the last one. This prevents
        // workspaces from touching the edges of the allocation box.
        if (vertical) {
            const availableHeight = height - spacing * (nWorkspaces + 1);
            let workspaceHeight = availableHeight / nWorkspaces;
            let [, workspaceWidth] =
                workspace.get_preferred_width(workspaceHeight);

            y1 = spacing;
            if (workspaceWidth > width) {
                [, workspaceHeight] = workspace.get_preferred_height(width);
                y1 += Math.max((availableHeight - workspaceHeight * nWorkspaces) / 2, 0);
            }

            fitAllBox.set_size(width, workspaceHeight);
        } else {
            const availableWidth = width - spacing * (nWorkspaces + 1);
            let workspaceWidth = availableWidth / nWorkspaces;
            let [, workspaceHeight] =
                workspace.get_preferred_height(workspaceWidth);

            x1 = spacing;
            if (workspaceHeight > height) {
                [, workspaceWidth] = workspace.get_preferred_width(height);
                x1 += Math.max((availableWidth - workspaceWidth * nWorkspaces) / 2, 0);
            }

            fitAllBox.set_size(workspaceWidth, height);
        }

        fitAllBox.set_origin(x1, y1);

        return fitAllBox;
    }

    _getFirstFitSingleWorkspaceBox(box, spacing, vertical) {
        const [width, height] = box.get_size();
        const [workspace] = this._workspaces;

        const rtl = this.text_direction === Clutter.TextDirection.RTL;
        const adj = this._scrollAdjustment;
        const currentWorkspace = vertical || !rtl
            ? adj.value : adj.upper - adj.value - 1;

        // Single fit mode implies centered too
        let [x1, y1] = box.get_origin();
        if (vertical) {
            const [, workspaceHeight] = workspace.get_preferred_height(width);
            y1 += (height - workspaceHeight) / 2;
            y1 -= currentWorkspace * (workspaceHeight + spacing);
        } else {
            const [, workspaceWidth] = workspace.get_preferred_width(height);
            x1 += (width - workspaceWidth) / 2;
            x1 -= currentWorkspace * (workspaceWidth + spacing);
        }

        const fitSingleBox = new Clutter.ActorBox({ x1, y1 });

        if (vertical) {
            const [, workspaceHeight] = workspace.get_preferred_height(width);
            fitSingleBox.set_size(width, workspaceHeight);
        } else {
            const [, workspaceWidth] = workspace.get_preferred_width(height);
            fitSingleBox.set_size(workspaceWidth, height);
        }

        return fitSingleBox;
    }

    _getSpacing(box, fitMode, vertical) {
        const [width, height] = box.get_size();
        const [workspace] = this._workspaces;

        let availableSpace;
        let workspaceSize;
        if (vertical) {
            [, workspaceSize] = workspace.get_preferred_height(width);
            availableSpace = (height - workspaceSize) / 2;
        } else {
            [, workspaceSize] = workspace.get_preferred_width(height);
            availableSpace = (width - workspaceSize) / 2;
        }

        const spacing = (availableSpace - workspaceSize * 0.4) * (1 - fitMode);
        const { scaleFactor } = St.ThemeContext.get_for_stage(global.stage);

        return Math.clamp(spacing, WORKSPACE_MIN_SPACING * scaleFactor,
            WORKSPACE_MAX_SPACING * scaleFactor);
    }

    _getWorkspaceModeForOverviewState(state) {
        const { ControlsState } = OverviewControls;

        switch (state) {
        case ControlsState.HIDDEN:
            return 0;
        case ControlsState.WINDOW_PICKER:
            return 1;
        case ControlsState.APP_GRID:
            return 0;
        }

        return 0;
    }

    _updateWorkspacesState() {
        const adj = this._scrollAdjustment;
        const fitMode = this._fitModeAdjustment.value;

        const { initialState, finalState, progress } =
            this._overviewAdjustment.getStateTransitionParams();

        const workspaceMode = (1 - fitMode) * Util.lerp(
            this._getWorkspaceModeForOverviewState(initialState),
            this._getWorkspaceModeForOverviewState(finalState),
            progress);

        // Fade and scale inactive workspaces
        this._workspaces.forEach((w, index) => {
            w.stateAdjustment.value = workspaceMode;

            const distanceToCurrentWorkspace = Math.abs(adj.value - index);

            const scaleProgress = 1 - Math.clamp(distanceToCurrentWorkspace, 0, 1);

            const scale = Util.lerp(WORKSPACE_INACTIVE_SCALE, 1, scaleProgress);
            w.set_scale(scale, scale);
        });
    }

    _getFitModeForState(state) {
        const { ControlsState } = OverviewControls;

        switch (state) {
        case ControlsState.HIDDEN:
        case ControlsState.WINDOW_PICKER:
            return FitMode.SINGLE;
        case ControlsState.APP_GRID:
            return FitMode.ALL;
        default:
            return FitMode.SINGLE;
        }
    }

    _getInitialBoxes(box) {
        const offsetBox = new Clutter.ActorBox();
        offsetBox.set_size(...box.get_size());

        let fitSingleBox = offsetBox;
        let fitAllBox = offsetBox;

        const { transitioning, initialState, finalState } =
            this._overviewAdjustment.getStateTransitionParams();

        const isPrimary = Main.layoutManager.primaryIndex === this._monitorIndex;

        if (isPrimary && transitioning) {
            const initialFitMode = this._getFitModeForState(initialState);
            const finalFitMode = this._getFitModeForState(finalState);

            // Only use the relative boxes when the overview is in a state
            // transition, and the corresponding fit modes are different.
            if (initialFitMode !== finalFitMode) {
                const initialBox =
                    this._controls.getWorkspacesBoxForState(initialState).copy();
                const finalBox =
                    this._controls.getWorkspacesBoxForState(finalState).copy();

                // Boxes are relative to ControlsManager, transform them;
                //   this.apply_relative_transform_to_point(controls,
                //       new Graphene.Point3D());
                // would be more correct, but also more expensive
                const [parentOffsetX, parentOffsetY] =
                    this.get_parent().allocation.get_origin();
                [initialBox, finalBox].forEach(b => {
                    b.set_origin(b.x1 - parentOffsetX, b.y1 - parentOffsetY);
                });

                if (initialFitMode === FitMode.SINGLE)
                    [fitSingleBox, fitAllBox] = [initialBox, finalBox];
                else
                    [fitAllBox, fitSingleBox] = [initialBox, finalBox];
            }
        }

        return [fitSingleBox, fitAllBox];
    }

    _updateWorkspaceMode() {
        this._updateWorkspacesState();
    }

    vfunc_allocate(box) {
        this.set_allocation(box);

        if (this.get_n_children() === 0)
            return;

        const vertical = global.workspaceManager.layout_rows === -1;
        const rtl = this.text_direction === Clutter.TextDirection.RTL;

        const fitMode = this._fitModeAdjustment.value;

        let [fitSingleBox, fitAllBox] = this._getInitialBoxes(box);
        const fitSingleSpacing =
            this._getSpacing(fitSingleBox, FitMode.SINGLE, vertical);
        fitSingleBox =
            this._getFirstFitSingleWorkspaceBox(fitSingleBox, fitSingleSpacing, vertical);

        const fitAllSpacing =
            this._getSpacing(fitAllBox, FitMode.ALL, vertical);
        fitAllBox =
            this._getFirstFitAllWorkspaceBox(fitAllBox, fitAllSpacing, vertical);

        // Account for RTL locales by reversing the list
        const workspaces = this._workspaces.slice();
        if (rtl)
            workspaces.reverse();

        const [fitSingleX1, fitSingleY1] = fitSingleBox.get_origin();
        const [fitSingleWidth, fitSingleHeight] = fitSingleBox.get_size();
        const [fitAllX1, fitAllY1] = fitAllBox.get_origin();
        const [fitAllWidth, fitAllHeight] = fitAllBox.get_size();

        workspaces.forEach(child => {
            if (fitMode === FitMode.SINGLE)
                box = fitSingleBox;
            else if (fitMode === FitMode.ALL)
                box = fitAllBox;
            else
                box = fitSingleBox.interpolate(fitAllBox, fitMode);

            child.allocate_align_fill(box, 0.5, 0.5, false, false);

            if (vertical) {
                fitSingleBox.set_origin(
                    fitSingleX1,
                    fitSingleBox.y1 + fitSingleHeight + fitSingleSpacing);
                fitAllBox.set_origin(
                    fitAllX1,
                    fitAllBox.y1 + fitAllHeight + fitAllSpacing);
            } else {
                fitSingleBox.set_origin(
                    fitSingleBox.x1 + fitSingleWidth + fitSingleSpacing,
                    fitSingleY1);
                fitAllBox.set_origin(
                    fitAllBox.x1 + fitAllWidth + fitAllSpacing,
                    fitAllY1);
            }
        });
    }

    getActiveWorkspace() {
        let workspaceManager = global.workspace_manager;
        let active = workspaceManager.get_active_workspace_index();
        return this._workspaces[active];
    }

    prepareToLeaveOverview() {
        for (let w = 0; w < this._workspaces.length; w++)
            this._workspaces[w].prepareToLeaveOverview();
    }

    syncStacking(stackIndices) {
        for (let i = 0; i < this._workspaces.length; i++)
            this._workspaces[i].syncStacking(stackIndices);
    }

    _scrollToActive() {
        const { workspaceManager } = global;
        const active = workspaceManager.get_active_workspace_index();

        this._animating = true;
        this._updateVisibility();

        this._scrollAdjustment.remove_transition('value');
        this._scrollAdjustment.ease(active, {
            duration: WORKSPACE_SWITCH_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            onComplete: () => {
                this._animating = false;
                this._updateVisibility();
            },
        });
    }

    _updateVisibility() {
        let workspaceManager = global.workspace_manager;
        let active = workspaceManager.get_active_workspace_index();

        const fitMode = this._fitModeAdjustment.value;
        const singleFitMode = fitMode === FitMode.SINGLE;

        for (let w = 0; w < this._workspaces.length; w++) {
            let workspace = this._workspaces[w];

            if (this._animating || this._gestureActive || !singleFitMode)
                workspace.show();
            else
                workspace.visible = Math.abs(w - active) <= 1;
        }
    }

    _updateWorkspaces() {
        let workspaceManager = global.workspace_manager;
        let newNumWorkspaces = workspaceManager.n_workspaces;

        for (let j = 0; j < newNumWorkspaces; j++) {
            let metaWorkspace = workspaceManager.get_workspace_by_index(j);
            let workspace;

            if (j >= this._workspaces.length) { /* added */
                workspace = new Workspace.Workspace(
                    metaWorkspace,
                    this._monitorIndex,
                    this._overviewAdjustment);
                this.add_actor(workspace);
                this._workspaces[j] = workspace;
            } else  {
                workspace = this._workspaces[j];

                if (workspace.metaWorkspace != metaWorkspace) { /* removed */
                    workspace.destroy();
                    this._workspaces.splice(j, 1);
                } /* else kept */
            }
        }

        for (let j = this._workspaces.length - 1; j >= newNumWorkspaces; j--) {
            this._workspaces[j].destroy();
            this._workspaces.splice(j, 1);
        }

        this._updateWorkspacesState();
        this._updateVisibility();
    }

    _activeWorkspaceChanged(_wm, _from, _to, _direction) {
        this._scrollToActive();
    }

    _onDestroy() {
        super._onDestroy();

        this._workspaces = [];
    }

    startTouchGesture() {
        this._gestureActive = true;

        this._updateVisibility();
    }

    endTouchGesture() {
        this._gestureActive = false;

        // Make sure title captions etc are shown as necessary
        this._scrollToActive();
        this._updateVisibility();
    }

    // sync the workspaces' positions to the value of the scroll adjustment
    // and change the active workspace if appropriate
    _onScrollAdjustmentChanged() {
        if (!this.has_allocation())
            return;

        const adj = this._scrollAdjustment;
        const allowSwitch =
            adj.get_transition('value') === null && !this._gestureActive;

        let workspaceManager = global.workspace_manager;
        let active = workspaceManager.get_active_workspace_index();
        let current = Math.round(adj.value);

        if (allowSwitch && active !== current) {
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

        this._updateWorkspacesState();
        this.queue_relayout();
    }
});

var ExtraWorkspaceView = GObject.registerClass(
class ExtraWorkspaceView extends WorkspacesViewBase {
    _init(monitorIndex, overviewAdjustment) {
        super._init(monitorIndex, overviewAdjustment);
        this._workspace =
            new Workspace.Workspace(null, monitorIndex, overviewAdjustment);
        this.add_actor(this._workspace);
    }

    _updateWorkspaceMode() {
        const overviewState = this._overviewAdjustment.value;

        const progress = Math.clamp(overviewState,
            OverviewControls.ControlsState.HIDDEN,
            OverviewControls.ControlsState.WINDOW_PICKER);

        this._workspace.stateAdjustment.value = progress;
    }

    vfunc_allocate(box) {
        this.set_allocation(box);

        const [width, height] = box.get_size();
        const [, childWidth] = this._workspace.get_preferred_width(height);

        const childBox = new Clutter.ActorBox();
        childBox.set_origin(Math.round((width - childWidth) / 2), 0);
        childBox.set_size(childWidth, height);
        this._workspace.allocate(childBox);
    }

    getActiveWorkspace() {
        return this._workspace;
    }

    prepareToLeaveOverview() {
        this._workspace.prepareToLeaveOverview();
    }

    syncStacking(stackIndices) {
        this._workspace.syncStacking(stackIndices);
    }

    startTouchGesture() {
    }

    endTouchGesture() {
    }
});

const SecondaryMonitorDisplay = GObject.registerClass(
class SecondaryMonitorDisplay extends St.Widget {
    _init(monitorIndex, controls, scrollAdjustment, fitModeAdjustment, overviewAdjustment) {
        this._monitorIndex = monitorIndex;
        this._controls = controls;
        this._scrollAdjustment = scrollAdjustment;
        this._fitModeAdjustment = fitModeAdjustment;
        this._overviewAdjustment = overviewAdjustment;

        super._init({
            style_class: 'secondary-monitor-workspaces',
            constraints: new Layout.MonitorConstraint({
                index: this._monitorIndex,
                work_area: true,
            }),
            clip_to_allocation: true,
        });

        this.connect('destroy', () => this._onDestroy());

        this._thumbnails = new ThumbnailsBox(
            this._scrollAdjustment, monitorIndex);
        this.add_child(this._thumbnails);

        this._thumbnails.connect('notify::should-show',
            () => this._updateThumbnailVisibility());

        this._overviewAdjustment.connectObject('notify::value', () => {
            this._updateThumbnailParams();
            this.queue_relayout();
        }, this);

        this._settings = new Gio.Settings({ schema_id: MUTTER_SCHEMA });
        this._settings.connect('changed::workspaces-only-on-primary',
            () => this._workspacesOnPrimaryChanged());
        this._workspacesOnPrimaryChanged();
    }

    _getThumbnailParamsForState(state) {
        const { ControlsState } = OverviewControls;

        let opacity, scale;
        switch (state) {
        case ControlsState.HIDDEN:
        case ControlsState.WINDOW_PICKER:
            opacity = 255;
            scale = 1;
            break;
        case ControlsState.APP_GRID:
            opacity = 0;
            scale = 0.5;
            break;
        default:
            opacity = 255;
            scale = 1;
            break;
        }

        return { opacity, scale };
    }

    _getThumbnailsHeight(box) {
        if (!this._thumbnails.visible)
            return 0;

        const [width, height] = box.get_size();
        const { expandFraction } = this._thumbnails;
        const [thumbnailsHeight] = this._thumbnails.get_preferred_height(width);
        return Math.min(
            thumbnailsHeight * expandFraction,
            height * MAX_THUMBNAIL_SCALE);
    }

    _getWorkspacesBoxForState(state, box, padding, thumbnailsHeight, spacing) {
        const { ControlsState } = OverviewControls;
        const workspaceBox = box.copy();
        const [width, height] = workspaceBox.get_size();

        switch (state) {
        case ControlsState.HIDDEN:
            break;
        case ControlsState.WINDOW_PICKER:
            workspaceBox.set_origin(0, padding + thumbnailsHeight + spacing);
            workspaceBox.set_size(
                width,
                height - 2 * padding - thumbnailsHeight - spacing);
            break;
        case ControlsState.APP_GRID:
            workspaceBox.set_origin(0, padding);
            workspaceBox.set_size(
                width,
                height - 2 * padding);
            break;
        }

        return workspaceBox;
    }

    vfunc_allocate(box) {
        this.set_allocation(box);

        const themeNode = this.get_theme_node();
        const contentBox = themeNode.get_content_box(box);
        const [width, height] = contentBox.get_size();
        const { expandFraction } = this._thumbnails;
        const spacing = themeNode.get_length('spacing') * expandFraction;
        const padding =
            Math.round((1 - SECONDARY_WORKSPACE_SCALE) * height / 2);

        const thumbnailsHeight = this._getThumbnailsHeight(contentBox);

        if (this._thumbnails.visible) {
            const childBox = new Clutter.ActorBox();
            childBox.set_origin(0, padding);
            childBox.set_size(width, thumbnailsHeight);
            this._thumbnails.allocate(childBox);
        }

        const {
            currentState, initialState, finalState, transitioning, progress,
        } = this._overviewAdjustment.getStateTransitionParams();

        let workspacesBox;
        const workspaceParams = [contentBox, padding, thumbnailsHeight, spacing];
        if (!transitioning) {
            workspacesBox =
                this._getWorkspacesBoxForState(currentState, ...workspaceParams);
        } else {
            const initialBox =
                this._getWorkspacesBoxForState(initialState, ...workspaceParams);
            const finalBox =
                this._getWorkspacesBoxForState(finalState, ...workspaceParams);
            workspacesBox = initialBox.interpolate(finalBox, progress);
        }
        this._workspacesView.allocate(workspacesBox);
    }

    _onDestroy() {
        if (this._settings)
            this._settings.run_dispose();
        this._settings = null;
    }

    _workspacesOnPrimaryChanged() {
        this._updateWorkspacesView();
        this._updateThumbnailVisibility();
    }

    _updateWorkspacesView() {
        if (this._workspacesView)
            this._workspacesView.destroy();

        if (this._settings.get_boolean('workspaces-only-on-primary')) {
            this._workspacesView = new ExtraWorkspaceView(
                this._monitorIndex,
                this._overviewAdjustment);
        } else {
            this._workspacesView = new WorkspacesView(
                this._monitorIndex,
                this._controls,
                this._scrollAdjustment,
                this._fitModeAdjustment,
                this._overviewAdjustment);
        }
        this.add_child(this._workspacesView);
    }

    _updateThumbnailVisibility() {
        const visible =
            this._thumbnails.should_show &&
            !this._settings.get_boolean('workspaces-only-on-primary');

        if (this._thumbnails.visible === visible)
            return;

        this._thumbnails.show();
        this._updateThumbnailParams();
        this._thumbnails.ease_property('expand-fraction', visible ? 1 : 0, {
            duration: OverviewControls.SIDE_CONTROLS_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            onComplete: () => (this._thumbnails.visible = visible),
        });
    }

    _updateThumbnailParams() {
        if (!this._thumbnails.visible)
            return;

        const { initialState, finalState, progress } =
            this._overviewAdjustment.getStateTransitionParams();

        const initialParams = this._getThumbnailParamsForState(initialState);
        const finalParams = this._getThumbnailParamsForState(finalState);

        const opacity =
            Util.lerp(initialParams.opacity, finalParams.opacity, progress);
        const scale =
            Util.lerp(initialParams.scale, finalParams.scale, progress);

        this._thumbnails.set({
            opacity,
            scale_x: scale,
            scale_y: scale,
        });
    }

    getActiveWorkspace() {
        return this._workspacesView.getActiveWorkspace();
    }

    prepareToLeaveOverview() {
        this._workspacesView.prepareToLeaveOverview();
    }

    syncStacking(stackIndices) {
        this._workspacesView.syncStacking(stackIndices);
    }

    startTouchGesture() {
        this._workspacesView.startTouchGesture();
    }

    endTouchGesture() {
        this._workspacesView.endTouchGesture();
    }
});

var WorkspacesDisplay = GObject.registerClass(
class WorkspacesDisplay extends St.Widget {
    _init(controls, scrollAdjustment, overviewAdjustment) {
        super._init({
            layout_manager: new Clutter.BinLayout(),
            reactive: true,
        });

        this._controls = controls;
        this._overviewAdjustment = overviewAdjustment;
        this._fitModeAdjustment = new St.Adjustment({
            actor: this,
            value: FitMode.SINGLE,
            lower: FitMode.SINGLE,
            upper: FitMode.ALL,
        });

        let workspaceManager = global.workspace_manager;
        this._scrollAdjustment = scrollAdjustment;

        global.window_manager.connectObject('switch-workspace',
            this._activeWorkspaceChanged.bind(this), this);

        this._swipeTracker = new SwipeTracker.SwipeTracker(
            Main.layoutManager.overviewGroup,
            Clutter.Orientation.HORIZONTAL,
            Shell.ActionMode.OVERVIEW,
            { allowDrag: false });
        this._swipeTracker.allowLongSwipes = true;
        this._swipeTracker.connect('begin', this._switchWorkspaceBegin.bind(this));
        this._swipeTracker.connect('update', this._switchWorkspaceUpdate.bind(this));
        this._swipeTracker.connect('end', this._switchWorkspaceEnd.bind(this));
        this.connect('notify::mapped', this._updateSwipeTracker.bind(this));

        workspaceManager.connectObject(
            'workspaces-reordered', this._workspacesReordered.bind(this),
            'notify::layout-rows', this._updateTrackerOrientation.bind(this), this);
        this._updateTrackerOrientation();

        Main.overview.connectObject(
            'window-drag-begin', this._windowDragBegin.bind(this),
            'window-drag-end', this._windowDragEnd.bind(this), this);

        this._primaryVisible = true;
        this._primaryIndex = Main.layoutManager.primaryIndex;
        this._workspacesViews = [];

        this._settings = new Gio.Settings({ schema_id: MUTTER_SCHEMA });

        this._inWindowDrag = false;
        this._leavingOverview = false;

        this._gestureActive = false; // touch(pad) gestures
    }

    _windowDragBegin() {
        this._inWindowDrag = true;
        this._updateSwipeTracker();
    }

    _windowDragEnd() {
        this._inWindowDrag = false;
        this._updateSwipeTracker();
    }

    _updateSwipeTracker() {
        this._swipeTracker.enabled =
            this.mapped &&
            !this._inWindowDrag &&
            !this._leavingOverview;
    }

    _workspacesReordered() {
        let workspaceManager = global.workspace_manager;

        this._scrollAdjustment.value =
            workspaceManager.get_active_workspace_index();
    }

    _activeWorkspaceChanged(_wm, _from, to, _direction) {
        if (this._gestureActive)
            return;

        this._scrollAdjustment.ease(to, {
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            duration: WORKSPACE_SWITCH_TIME,
        });
    }

    _updateTrackerOrientation() {
        const { layoutRows } = global.workspace_manager;
        this._swipeTracker.orientation = layoutRows !== -1
            ? Clutter.Orientation.HORIZONTAL
            : Clutter.Orientation.VERTICAL;
    }

    _directionForProgress(progress) {
        if (global.workspace_manager.layout_rows === -1) {
            return progress > 0
                ? Meta.MotionDirection.DOWN
                : Meta.MotionDirection.UP;
        } else if (this.text_direction === Clutter.TextDirection.RTL) {
            return progress > 0
                ? Meta.MotionDirection.LEFT
                : Meta.MotionDirection.RIGHT;
        } else {
            return progress > 0
                ? Meta.MotionDirection.RIGHT
                : Meta.MotionDirection.LEFT;
        }
    }

    _switchWorkspaceBegin(tracker, monitor) {
        if (this._workspacesOnlyOnPrimary && monitor !== this._primaryIndex)
            return;

        let workspaceManager = global.workspace_manager;
        let adjustment = this._scrollAdjustment;
        if (this._gestureActive)
            adjustment.remove_transition('value');

        const distance = global.workspace_manager.layout_rows === -1
            ? this.height : this.width;

        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].startTouchGesture();

        let progress = adjustment.value / adjustment.page_size;
        let points = Array.from(
            { length: workspaceManager.n_workspaces }, (v, i) => i);

        tracker.confirmSwipe(distance, points, progress, Math.round(progress));

        this._gestureActive = true;
    }

    _switchWorkspaceUpdate(tracker, progress) {
        let adjustment = this._scrollAdjustment;
        adjustment.value = progress * adjustment.page_size;
    }

    _switchWorkspaceEnd(tracker, duration, endProgress) {
        let workspaceManager = global.workspace_manager;
        let newWs = workspaceManager.get_workspace_by_index(endProgress);

        this._scrollAdjustment.ease(endProgress, {
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            duration,
            onComplete: () => {
                if (!newWs.active)
                    newWs.activate(global.get_current_time());
                this._endTouchGesture();
            },
        });
    }

    _endTouchGesture() {
        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].endTouchGesture();
        this._gestureActive = false;
    }

    vfunc_navigate_focus(from, direction) {
        return this._getPrimaryView()?.navigate_focus(from, direction, false);
    }

    setPrimaryWorkspaceVisible(visible) {
        if (this._primaryVisible === visible)
            return;

        this._primaryVisible = visible;

        const primaryIndex = Main.layoutManager.primaryIndex;
        const primaryWorkspace = this._workspacesViews[primaryIndex];
        if (primaryWorkspace)
            primaryWorkspace.visible = visible;
    }

    prepareToEnterOverview() {
        this.show();
        this._updateWorkspacesViews();

        Main.overview.connectObject(
            'windows-restacked', this._onRestacked.bind(this),
            'scroll-event', this._onScrollEvent.bind(this), this);

        global.stage.connectObject(
            'key-press-event', this._onKeyPressEvent.bind(this), this);
    }

    prepareToLeaveOverview() {
        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].prepareToLeaveOverview();

        this._leavingOverview = true;
        this._updateSwipeTracker();
    }

    vfunc_hide() {
        Main.overview.disconnectObject(this);
        global.stage.disconnectObject(this);

        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].destroy();
        this._workspacesViews = [];

        this._leavingOverview = false;

        super.vfunc_hide();
    }

    _updateWorkspacesViews() {
        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].destroy();

        this._primaryIndex = Main.layoutManager.primaryIndex;
        this._workspacesViews = [];
        let monitors = Main.layoutManager.monitors;
        for (let i = 0; i < monitors.length; i++) {
            let view;
            if (i === this._primaryIndex) {
                view = new WorkspacesView(i,
                    this._controls,
                    this._scrollAdjustment,
                    this._fitModeAdjustment,
                    this._overviewAdjustment);

                view.visible = this._primaryVisible;
                this.bind_property('opacity', view, 'opacity', GObject.BindingFlags.SYNC_CREATE);
                this.add_child(view);
            } else {
                view = new SecondaryMonitorDisplay(i,
                    this._controls,
                    this._scrollAdjustment,
                    this._fitModeAdjustment,
                    this._overviewAdjustment);
                Main.layoutManager.overviewGroup.add_actor(view);
            }

            this._workspacesViews.push(view);
        }
    }

    _getMonitorIndexForEvent(event) {
        let [x, y] = event.get_coords();
        let rect = new Meta.Rectangle({ x, y, width: 1, height: 1 });
        return global.display.get_monitor_index_for_rect(rect);
    }

    _getPrimaryView() {
        if (!this._workspacesViews.length)
            return null;
        return this._workspacesViews[this._primaryIndex];
    }

    activeWorkspaceHasMaximizedWindows() {
        const primaryView = this._getPrimaryView();
        return primaryView
            ? primaryView.getActiveWorkspace().hasMaximizedWindows()
            : false;
    }

    _onRestacked(overview, stackIndices) {
        for (let i = 0; i < this._workspacesViews.length; i++)
            this._workspacesViews[i].syncStacking(stackIndices);
    }

    _onScrollEvent(actor, event) {
        if (this._swipeTracker.canHandleScrollEvent(event))
            return Clutter.EVENT_PROPAGATE;

        if (!this.mapped)
            return Clutter.EVENT_PROPAGATE;

        if (this._workspacesOnlyOnPrimary &&
            this._getMonitorIndexForEvent(event) != this._primaryIndex)
            return Clutter.EVENT_PROPAGATE;

        return Main.wm.handleWorkspaceScroll(event);
    }

    _onKeyPressEvent(actor, event) {
        const { ControlsState } = OverviewControls;
        if (this._overviewAdjustment.value !== ControlsState.WINDOW_PICKER)
            return Clutter.EVENT_PROPAGATE;

        if (!this.reactive)
            return Clutter.EVENT_PROPAGATE;

        const { workspaceManager } = global;
        const vertical = workspaceManager.layout_rows === -1;
        const rtl = this.get_text_direction() === Clutter.TextDirection.RTL;

        let which;
        switch (event.get_key_symbol()) {
        case Clutter.KEY_Page_Up:
            if (vertical)
                which = Meta.MotionDirection.UP;
            else if (rtl)
                which = Meta.MotionDirection.RIGHT;
            else
                which = Meta.MotionDirection.LEFT;
            break;
        case Clutter.KEY_Page_Down:
            if (vertical)
                which = Meta.MotionDirection.DOWN;
            else if (rtl)
                which = Meta.MotionDirection.LEFT;
            else
                which = Meta.MotionDirection.RIGHT;
            break;
        case Clutter.KEY_Home:
            which = 0;
            break;
        case Clutter.KEY_End:
            which = workspaceManager.n_workspaces - 1;
            break;
        default:
            return Clutter.EVENT_PROPAGATE;
        }

        let ws;
        if (which < 0)
            // Negative workspace numbers are directions
            // with respect to the current workspace
            ws = workspaceManager.get_active_workspace().get_neighbor(which);
        else
            // Otherwise it is a workspace index
            ws = workspaceManager.get_workspace_by_index(which);

        if (ws)
            Main.wm.actionMoveWorkspace(ws);

        return Clutter.EVENT_STOP;
    }

    get _workspacesOnlyOnPrimary() {
        return this._settings.get_boolean('workspaces-only-on-primary');
    }

    get fitModeAdjustment() {
        return this._fitModeAdjustment;
    }
});
