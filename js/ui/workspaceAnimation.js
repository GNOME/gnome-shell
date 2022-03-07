// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceAnimationController, WorkspaceGroup */

const { Clutter, GObject, Meta, Shell, St } = imports.gi;

const Background = imports.ui.background;
const Layout = imports.ui.layout;
const Main = imports.ui.main;
const SwipeTracker = imports.ui.swipeTracker;

const WINDOW_ANIMATION_TIME = 250;
const WORKSPACE_SPACING = 100;

var WorkspaceGroup = GObject.registerClass(
class WorkspaceGroup extends Clutter.Actor {
    _init(workspace, monitor, movingWindow) {
        super._init();

        this._workspace = workspace;
        this._monitor = monitor;
        this._movingWindow = movingWindow;
        this._windowRecords = [];

        if (this._workspace) {
            this._background = new Meta.BackgroundGroup();

            this.add_actor(this._background);

            this._bgManager = new Background.BackgroundManager({
                container: this._background,
                monitorIndex: this._monitor.index,
                controlPosition: false,
            });
        }

        this.width = monitor.width;
        this.height = monitor.height;
        this.clip_to_allocation = true;

        this._createWindows();

        this.connect('destroy', this._onDestroy.bind(this));
        global.display.connectObject('restacked',
            this._syncStacking.bind(this), this);
    }

    get workspace() {
        return this._workspace;
    }

    _shouldShowWindow(window) {
        if (!window.showing_on_its_workspace())
            return false;

        const geometry = global.display.get_monitor_geometry(this._monitor.index);
        const [intersects] = window.get_frame_rect().intersect(geometry);
        if (!intersects)
            return false;

        const isSticky =
            window.is_on_all_workspaces() || window === this._movingWindow;

        // No workspace means we should show windows that are on all workspaces
        if (!this._workspace)
            return isSticky;

        // Otherwise only show windows that are (only) on that workspace
        return !isSticky && window.located_on_workspace(this._workspace);
    }

    _syncStacking() {
        const windowActors = global.get_window_actors().filter(w =>
            this._shouldShowWindow(w.meta_window));

        let lastRecord;
        const bottomActor = this._background ?? null;

        for (const windowActor of windowActors) {
            const record = this._windowRecords.find(r => r.windowActor === windowActor);

            this.set_child_above_sibling(record.clone,
                lastRecord ? lastRecord.clone : bottomActor);
            lastRecord = record;
        }
    }

    _createWindows() {
        const windowActors = global.get_window_actors().filter(w =>
            this._shouldShowWindow(w.meta_window));

        for (const windowActor of windowActors) {
            const clone = new Clutter.Clone({
                source: windowActor,
                x: windowActor.x - this._monitor.x,
                y: windowActor.y - this._monitor.y,
            });

            this.add_child(clone);

            const record = { windowActor, clone };

            windowActor.connectObject('destroy', () => {
                clone.destroy();
                this._windowRecords.splice(this._windowRecords.indexOf(record), 1);
            }, this);

            this._windowRecords.push(record);
        }
    }

    _removeWindows() {
        for (const record of this._windowRecords)
            record.clone.destroy();

        this._windowRecords = [];
    }

    _onDestroy() {
        this._removeWindows();

        if (this._workspace)
            this._bgManager.destroy();
    }
});

const MonitorGroup = GObject.registerClass({
    Properties: {
        'progress': GObject.ParamSpec.double(
            'progress', 'progress', 'progress',
            GObject.ParamFlags.READWRITE,
            -Infinity, Infinity, 0),
    },
}, class MonitorGroup extends St.Widget {
    _init(monitor, workspaceIndices, movingWindow) {
        super._init({
            clip_to_allocation: true,
            style_class: 'workspace-animation',
        });

        this._monitor = monitor;

        const constraint = new Layout.MonitorConstraint({ index: monitor.index });
        this.add_constraint(constraint);

        this._container = new Clutter.Actor();
        this.add_child(this._container);

        const stickyGroup = new WorkspaceGroup(null, monitor, movingWindow);
        this.add_child(stickyGroup);

        this._workspaceGroups = [];

        const workspaceManager = global.workspace_manager;
        const vertical = workspaceManager.layout_rows === -1;
        const activeWorkspace = workspaceManager.get_active_workspace();

        let x = 0;
        let y = 0;

        for (const i of workspaceIndices) {
            const ws = workspaceManager.get_workspace_by_index(i);
            const fullscreen = ws.list_windows().some(w => w.get_monitor() === monitor.index && w.is_fullscreen());

            if (i > 0 && vertical && !fullscreen && monitor.index === Main.layoutManager.primaryIndex) {
                // We have to shift windows up or down by the height of the panel to prevent having a
                // visible gap between the windows while switching workspaces. Since fullscreen windows
                // hide the panel, they don't need to be shifted up or down.
                y -= Main.panel.height;
            }

            const group = new WorkspaceGroup(ws, monitor, movingWindow);

            this._workspaceGroups.push(group);
            this._container.add_child(group);
            group.set_position(x, y);

            if (vertical)
                y += this.baseDistance;
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                x -= this.baseDistance;
            else
                x += this.baseDistance;
        }

        this.progress = this.getWorkspaceProgress(activeWorkspace);
    }

    get baseDistance() {
        const spacing = WORKSPACE_SPACING * St.ThemeContext.get_for_stage(global.stage).scale_factor;

        if (global.workspace_manager.layout_rows === -1)
            return this._monitor.height + spacing;
        else
            return this._monitor.width + spacing;
    }

    get progress() {
        if (global.workspace_manager.layout_rows === -1)
            return -this._container.y / this.baseDistance;
        else if (this.get_text_direction() === Clutter.TextDirection.RTL)
            return this._container.x / this.baseDistance;
        else
            return -this._container.x / this.baseDistance;
    }

    set progress(p) {
        if (global.workspace_manager.layout_rows === -1)
            this._container.y = -Math.round(p * this.baseDistance);
        else if (this.get_text_direction() === Clutter.TextDirection.RTL)
            this._container.x = Math.round(p * this.baseDistance);
        else
            this._container.x = -Math.round(p * this.baseDistance);
    }

    get index() {
        return this._monitor.index;
    }

    getWorkspaceProgress(workspace) {
        const group = this._workspaceGroups.find(g =>
            g.workspace.index() === workspace.index());
        return this._getWorkspaceGroupProgress(group);
    }

    _getWorkspaceGroupProgress(group) {
        if (global.workspace_manager.layout_rows === -1)
            return group.y / this.baseDistance;
        else if (this.get_text_direction() === Clutter.TextDirection.RTL)
            return -group.x / this.baseDistance;
        else
            return group.x / this.baseDistance;
    }

    getSnapPoints() {
        return this._workspaceGroups.map(g =>
            this._getWorkspaceGroupProgress(g));
    }

    findClosestWorkspace(progress) {
        const distances = this.getSnapPoints().map(p =>
            Math.abs(p - progress));
        const index = distances.indexOf(Math.min(...distances));
        return this._workspaceGroups[index].workspace;
    }

    _interpolateProgress(progress, monitorGroup) {
        if (this.index === monitorGroup.index)
            return progress;

        const points1 = monitorGroup.getSnapPoints();
        const points2 = this.getSnapPoints();

        const upper = points1.indexOf(points1.find(p => p >= progress));
        const lower = points1.indexOf(points1.slice().reverse().find(p => p <= progress));

        if (points1[upper] === points1[lower])
            return points2[upper];

        const t = (progress - points1[lower]) / (points1[upper] - points1[lower]);

        return points2[lower] + (points2[upper] - points2[lower]) * t;
    }

    updateSwipeForMonitor(progress, monitorGroup) {
        this.progress = this._interpolateProgress(progress, monitorGroup);
    }
});

var WorkspaceAnimationController = class {
    constructor() {
        this._movingWindow = null;
        this._switchData = null;

        Main.overview.connect('showing', () => {
            if (this._switchData) {
                if (this._switchData.gestureActivated)
                    this._finishWorkspaceSwitch(this._switchData);
                this._swipeTracker.enabled = false;
            }
        });
        Main.overview.connect('hiding', () => {
            this._swipeTracker.enabled = true;
        });

        const swipeTracker = new SwipeTracker.SwipeTracker(global.stage,
            Clutter.Orientation.HORIZONTAL,
            Shell.ActionMode.NORMAL,
            { allowDrag: false });
        swipeTracker.connect('begin', this._switchWorkspaceBegin.bind(this));
        swipeTracker.connect('update', this._switchWorkspaceUpdate.bind(this));
        swipeTracker.connect('end', this._switchWorkspaceEnd.bind(this));
        this._swipeTracker = swipeTracker;

        global.display.bind_property('compositor-modifiers',
            this._swipeTracker, 'scroll-modifiers',
            GObject.BindingFlags.SYNC_CREATE);
    }

    _prepareWorkspaceSwitch(workspaceIndices) {
        if (this._switchData)
            return;

        const workspaceManager = global.workspace_manager;
        const nWorkspaces = workspaceManager.get_n_workspaces();

        const switchData = {};

        this._switchData = switchData;
        switchData.monitors = [];

        switchData.gestureActivated = false;
        switchData.inProgress = false;

        if (!workspaceIndices)
            workspaceIndices = [...Array(nWorkspaces).keys()];

        const monitors = Meta.prefs_get_workspaces_only_on_primary()
            ? [Main.layoutManager.primaryMonitor] : Main.layoutManager.monitors;

        for (const monitor of monitors) {
            if (Meta.prefs_get_workspaces_only_on_primary() &&
                monitor.index !== Main.layoutManager.primaryIndex)
                continue;

            const group = new MonitorGroup(monitor, workspaceIndices, this.movingWindow);

            Main.uiGroup.insert_child_above(group, global.window_group);

            switchData.monitors.push(group);
        }

        Meta.disable_unredirect_for_display(global.display);
    }

    _finishWorkspaceSwitch(switchData) {
        Meta.enable_unredirect_for_display(global.display);

        this._switchData = null;

        switchData.monitors.forEach(m => m.destroy());

        this.movingWindow = null;
    }

    animateSwitch(from, to, direction, onComplete) {
        this._swipeTracker.enabled = false;

        let workspaceIndices = [];

        switch (direction) {
        case Meta.MotionDirection.UP:
        case Meta.MotionDirection.LEFT:
        case Meta.MotionDirection.UP_LEFT:
        case Meta.MotionDirection.UP_RIGHT:
            workspaceIndices = [to, from];
            break;

        case Meta.MotionDirection.DOWN:
        case Meta.MotionDirection.RIGHT:
        case Meta.MotionDirection.DOWN_LEFT:
        case Meta.MotionDirection.DOWN_RIGHT:
            workspaceIndices = [from, to];
            break;
        }

        if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL &&
            direction !== Meta.MotionDirection.UP &&
            direction !== Meta.MotionDirection.DOWN)
            workspaceIndices.reverse();

        this._prepareWorkspaceSwitch(workspaceIndices);
        this._switchData.inProgress = true;

        const fromWs = global.workspace_manager.get_workspace_by_index(from);
        const toWs = global.workspace_manager.get_workspace_by_index(to);

        for (const monitorGroup of this._switchData.monitors) {
            monitorGroup.progress = monitorGroup.getWorkspaceProgress(fromWs);
            const progress = monitorGroup.getWorkspaceProgress(toWs);

            const params = {
                duration: WINDOW_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            };

            if (monitorGroup.index === Main.layoutManager.primaryIndex) {
                params.onComplete = () => {
                    this._finishWorkspaceSwitch(this._switchData);
                    onComplete();
                    this._swipeTracker.enabled = true;
                };
            }

            monitorGroup.ease_property('progress', progress, params);
        }
    }

    canHandleScrollEvent(event) {
        return this._swipeTracker.canHandleScrollEvent(event);
    }

    _findMonitorGroup(monitorIndex) {
        return this._switchData.monitors.find(m => m.index === monitorIndex);
    }

    _switchWorkspaceBegin(tracker, monitor) {
        if (Meta.prefs_get_workspaces_only_on_primary() &&
            monitor !== Main.layoutManager.primaryIndex)
            return;

        const workspaceManager = global.workspace_manager;
        const horiz = workspaceManager.layout_rows !== -1;
        tracker.orientation = horiz
            ? Clutter.Orientation.HORIZONTAL
            : Clutter.Orientation.VERTICAL;

        if (this._switchData && this._switchData.gestureActivated) {
            for (const group of this._switchData.monitors)
                group.remove_all_transitions();
        } else {
            this._prepareWorkspaceSwitch();
        }

        const monitorGroup = this._findMonitorGroup(monitor);
        const baseDistance = monitorGroup.baseDistance;
        const progress = monitorGroup.progress;

        const closestWs = monitorGroup.findClosestWorkspace(progress);
        const cancelProgress = monitorGroup.getWorkspaceProgress(closestWs);
        const points = monitorGroup.getSnapPoints();

        this._switchData.baseMonitorGroup = monitorGroup;

        tracker.confirmSwipe(baseDistance, points, progress, cancelProgress);
    }

    _switchWorkspaceUpdate(tracker, progress) {
        if (!this._switchData)
            return;

        for (const monitorGroup of this._switchData.monitors)
            monitorGroup.updateSwipeForMonitor(progress, this._switchData.baseMonitorGroup);
    }

    _switchWorkspaceEnd(tracker, duration, endProgress) {
        if (!this._switchData)
            return;

        const switchData = this._switchData;
        switchData.gestureActivated = true;

        const newWs = switchData.baseMonitorGroup.findClosestWorkspace(endProgress);
        const endTime = Clutter.get_current_event_time();

        for (const monitorGroup of this._switchData.monitors) {
            const progress = monitorGroup.getWorkspaceProgress(newWs);

            const params = {
                duration,
                mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            };

            if (monitorGroup.index === Main.layoutManager.primaryIndex) {
                params.onComplete = () => {
                    if (!newWs.active)
                        newWs.activate(endTime);
                    this._finishWorkspaceSwitch(switchData);
                };
            }

            monitorGroup.ease_property('progress', progress, params);
        }
    }

    get gestureActive() {
        return this._switchData !== null && this._switchData.gestureActivated;
    }

    cancelSwitchAnimation() {
        if (!this._switchData)
            return;

        if (this._switchData.gestureActivated)
            return;

        this._finishWorkspaceSwitch(this._switchData);
    }

    set movingWindow(movingWindow) {
        this._movingWindow = movingWindow;
    }

    get movingWindow() {
        return this._movingWindow;
    }
};
