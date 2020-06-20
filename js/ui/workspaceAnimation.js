// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceAnimationController */

const { Clutter, GObject, Meta, Shell, St } = imports.gi;

const Main = imports.ui.main;
const Layout = imports.ui.layout;
const SwipeTracker = imports.ui.swipeTracker;

const WINDOW_ANIMATION_TIME = 250;

const WorkspaceGroup = GObject.registerClass(
class WorkspaceGroup extends Clutter.Actor {
    _init(workspace, monitor, movingWindow) {
        super._init();

        this._workspace = workspace;
        this._monitor = monitor;
        this._movingWindow = movingWindow;
        this._windows = [];

        this._refreshWindows();

        this.connect('destroy', this._onDestroy.bind(this));
        this._restackedId = global.display.connect('restacked',
            this._refreshWindows.bind(this));
    }

    _shouldShowWindow(window) {
        if (!window.located_on_workspace(this._workspace))
            return false;

        if (!window.showing_on_its_workspace())
            return false;

        const geometry = global.display.get_monitor_geometry(this._monitor.index);
        const [intersects, intersection_] = window.get_frame_rect().intersect(geometry);
        if (!intersects)
            return false;

        if (window.is_on_all_workspaces())
            return false;

        if (this._movingWindow && window === this._movingWindow)
            return false;

        return true;
    }

    _refreshWindows() {
        if (this._windows.length > 0)
            this._removeWindows();

        const windows = global.get_window_actors().filter(w =>
            this._shouldShowWindow(w.meta_window));

        for (const windowActor of windows) {
            const clone = new Clutter.Clone({
                source: windowActor,
                x: windowActor.x - this._monitor.x,
                y: windowActor.y - this._monitor.y,
            });

            this.add_child(clone);
            windowActor.hide();

            const record = { windowActor, clone };

            record.windowDestroyId = windowActor.connect('destroy', () => {
                clone.destroy();
                this._windows.splice(this._windows.indexOf(record), 1);
            });

            this._windows.push(record);
        }
    }

    _removeWindows() {
        for (const record of this._windows) {
            record.windowActor.disconnect(record.windowDestroyId);
            record.clone.destroy();

            if (record.windowActor.get_meta_window().get_workspace().active)
                record.windowActor.show();
        }

        this._windows = [];
    }

    _onDestroy() {
        global.display.disconnect(this._restackedId);
        this._removeWindows();
    }
});

const StickyGroup = GObject.registerClass(
class StickyGroup extends Clutter.Actor {
    _init(monitor, movingWindow) {
        super._init();

        this._monitor = monitor;
        this._movingWindow = movingWindow;
        this._windows = [];

        this._refreshWindows();

        this.connect('destroy', this._onDestroy.bind(this));
        this._restackedId = global.display.connect('restacked',
            this._refreshWindows.bind(this));
    }

    _shouldShowWindow(window) {
        if (!window.showing_on_its_workspace())
            return false;

        const geometry = global.display.get_monitor_geometry(this._monitor.index);
        const [intersects, intersection_] = window.get_frame_rect().intersect(geometry);
        if (!intersects)
            return false;

        return window.is_on_all_workspaces() || window === this._movingWindow;
    }

    _refreshWindows() {
        if (this._windows.length > 0)
            this._removeWindows();

        const windows = global.get_window_actors().filter(w =>
            this._shouldShowWindow(w.meta_window));

        for (const windowActor of windows) {
            const clone = new Clutter.Clone({
                source: windowActor,
                x: windowActor.x - this._monitor.x,
                y: windowActor.y - this._monitor.y,
            });

            this.add_child(clone);
            windowActor.hide();

            const record = { windowActor, clone };

            record.windowDestroyId = windowActor.connect('destroy', () => {
                clone.destroy();
                this._windows.splice(this._windows.indexOf(record), 1);
            });

            this._windows.push(record);
        }
    }

    _removeWindows() {
        for (const record of this._windows) {
            record.windowActor.disconnect(record.windowDestroyId);
            record.clone.destroy();

            record.windowActor.show();
        }

        this._windows = [];
    }

    _onDestroy() {
        global.display.disconnect(this._restackedId);
        this._removeWindows();
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

        let swipeTracker = new SwipeTracker.SwipeTracker(global.stage,
            Shell.ActionMode.NORMAL, { allowDrag: false, allowScroll: false });
        swipeTracker.connect('begin', this._switchWorkspaceBegin.bind(this));
        swipeTracker.connect('update', this._switchWorkspaceUpdate.bind(this));
        swipeTracker.connect('end', this._switchWorkspaceEnd.bind(this));
        this._swipeTracker = swipeTracker;
    }

    _prepareWorkspaceSwitch(workspaces) {
        if (this._switchData)
            return;

        const wgroup = global.window_group;
        const workspaceManager = global.workspace_manager;
        const vertical = workspaceManager.layout_rows === -1;
        const nWorkspaces = workspaceManager.get_n_workspaces();
        const activeWorkspaceIndex = workspaceManager.get_active_workspace_index();

        const switchData = {};

        this._switchData = switchData;
        switchData.monitors = {};

        switchData.gestureActivated = false;
        switchData.inProgress = false;

        if (!workspaces) {
            workspaces = [];

            for (let i = 0; i < nWorkspaces; i++)
                workspaces.push(i);
        }

        for (const monitor of Main.layoutManager.monitors) {
            if (Meta.prefs_get_workspaces_only_on_primary() &&
                monitor.index !== Main.layoutManager.primaryIndex)
                continue;

            const record = {
                index: monitor.index,
                clipBin: new St.Widget({
                    x_expand: true,
                    y_expand: true,
                    clip_to_allocation: true,
                }),
                container: new Clutter.Actor(),
                stickyGroup: new StickyGroup(monitor, this.movingWindow),
                workspaces: [],
            };

            const constraint = new Layout.MonitorConstraint({ index: monitor.index });
            record.clipBin.add_constraint(constraint);

            record.clipBin.add_child(record.container);
            record.clipBin.add_child(record.stickyGroup);

            wgroup.add_child(record.clipBin);

            let x = 0;
            let y = 0;

            for (const i of workspaces) {
                const ws = workspaceManager.get_workspace_by_index(i);
                const fullscreen = ws.list_windows().some(w => w.get_monitor() === record.index && w.is_fullscreen());

                if (i > 0 && vertical && !fullscreen && record.index === Main.layoutManager.primaryIndex) {
                    // We have to shift windows up or down by the height of the panel to prevent having a
                    // visible gap between the windows while switching workspaces. Since fullscreen windows
                    // hide the panel, they don't need to be shifted up or down.
                    y -= Main.panel.height;
                }

                const info = {
                    ws,
                    actor: new WorkspaceGroup(ws, monitor, this.movingWindow),
                    fullscreen,
                    x,
                    y,
                };

                record.workspaces[i] = info;
                record.container.add_child(info.actor);
                record.container.set_child_above_sibling(info.actor, null);
                info.actor.set_position(x, y);

                if (vertical)
                    y += monitor.height;
                else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                    x -= monitor.width;
                else
                    x += monitor.width;
            }

            record.clipBin.set_child_above_sibling(record.stickyGroup, null);

            switchData.monitors[monitor.index] = record;

            if (vertical)
                record.container.y = -record.workspaces[activeWorkspaceIndex].y;
            else
                record.container.x = -record.workspaces[activeWorkspaceIndex].x;
        }
    }

    _finishWorkspaceSwitch(switchData) {
        this._switchData = null;

        for (const monitorInfo of Object.values(switchData.monitors))
            monitorInfo.clipBin.destroy();

        this.movingWindow = null;
    }

    animateSwitch(from, to, direction, onComplete) {
        if (this._switchData) {
            for (const monitorInfo of Object.values(this._switchData.monitors))
                monitorInfo.container.remove_all_transitions();
        }

        let workspaces = [];

        switch (direction) {
        case Meta.MotionDirection.UP:
        case Meta.MotionDirection.LEFT:
        case Meta.MotionDirection.UP_LEFT:
        case Meta.MotionDirection.UP_RIGHT:
            workspaces = [to, from];
            break;

        case Meta.MotionDirection.DOWN:
        case Meta.MotionDirection.RIGHT:
        case Meta.MotionDirection.DOWN_LEFT:
        case Meta.MotionDirection.DOWN_RIGHT:
            workspaces = [from, to];
            break;
        }

        if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL &&
            direction !== Meta.MotionDirection.UP &&
            direction !== Meta.MotionDirection.DOWN)
            workspaces.reverse();

        this._prepareWorkspaceSwitch(workspaces);
        this._switchData.inProgress = true;

        for (const monitorInfo of Object.values(this._switchData.monitors)) {
            const fromWs = monitorInfo.workspaces[from];
            const toWs = monitorInfo.workspaces[to];

            monitorInfo.container.x = -fromWs.x;
            monitorInfo.container.y = -fromWs.y;

            const params = {
                x: -toWs.x,
                y: -toWs.y,
                duration: WINDOW_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            };

            if (monitorInfo.index === Main.layoutManager.primaryIndex) {
                params.onComplete = () => {
                    this._finishWorkspaceSwitch(this._switchData);
                    onComplete();
                };
            }

            monitorInfo.container.ease(params);
        }
    }

    _getProgressForWorkspace(workspaceInfo, monitorIndex) {
        const geometry = Main.layoutManager.monitors[monitorIndex];

        if (global.workspace_manager.layout_rows === -1)
            return workspaceInfo.y / geometry.height;
        else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
            return -workspaceInfo.x / geometry.width;
        else
            return workspaceInfo.x / geometry.width;
    }

    _findClosestWorkspaceIndex(progress) {
        const index = Main.layoutManager.primaryIndex;
        const distances = this._switchData.monitors[index].workspaces.map(ws => {
            const workspaceProgress = this._getProgressForWorkspace(ws, index);
            return Math.abs(workspaceProgress - progress);
        });
        return distances.indexOf(Math.min(...distances));
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

        const geometry = Main.layoutManager.monitors[monitor];
        const baseDistance = horiz ? geometry.width : geometry.height;

        let progress;
        let cancelProgress;
        if (this._switchData && this._switchData.gestureActivated) {
            for (const monitorInfo of Object.values(this._switchData.monitors))
                monitorInfo.container.remove_all_transitions();

            const record = this._switchData.monitors[monitor];

            if (!horiz)
                progress = -record.container.y / baseDistance;
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                progress = record.container.x / baseDistance;
            else
                progress = -record.container.x / baseDistance;

            const ws = record.workspaces[this._findClosestWorkspaceIndex(progress)];
            cancelProgress = this._getProgressForWorkspace(ws, monitor);
        } else {
            this._prepareWorkspaceSwitch();

            const activeIndex = workspaceManager.get_active_workspace_index();
            const ws = this._switchData.monitors[monitor].workspaces[activeIndex];

            progress = cancelProgress = this._getProgressForWorkspace(ws, monitor);
        }

        const points = this._switchData.monitors[monitor].workspaces.map(ws => this._getProgressForWorkspace(ws, monitor));

        this._switchData.baseMonitor = monitor;

        tracker.confirmSwipe(baseDistance, points, progress, cancelProgress);
    }

    _interpolateProgress(progress, monitor) {
        const monitor1 = this._switchData.monitors[this._switchData.baseMonitor];
        const monitor2 = monitor;

        const points1 = monitor1.workspaces.map(ws => this._getProgressForWorkspace(ws, monitor1.index));
        const points2 = monitor2.workspaces.map(ws => this._getProgressForWorkspace(ws, monitor2.index));

        const upper = points1.indexOf(points1.find(p => p >= progress));
        const lower = points1.indexOf(points1.slice().reverse().find(p => p <= progress));

        if (points1[upper] === points1[lower])
            return points2[upper];

        const t = (progress - points1[lower]) / (points1[upper] - points1[lower]);

        return points2[lower] + (points2[upper] - points2[lower]) * t;
    }

    _switchWorkspaceUpdate(tracker, progress) {
        if (!this._switchData)
            return;

        for (const monitorInfo of Object.values(this._switchData.monitors)) {
            let xPos = 0;
            let yPos = 0;

            const geometry = Main.layoutManager.monitors[monitorInfo.index];

            const p = this._interpolateProgress(progress, monitorInfo);

            if (global.workspace_manager.layout_rows === -1)
                yPos = -Math.round(p * geometry.height);
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                xPos = Math.round(p * geometry.width);
            else
                xPos = -Math.round(p * geometry.width);

            monitorInfo.container.set_position(xPos, yPos);
        }
    }

    _switchWorkspaceEnd(tracker, duration, endProgress) {
        if (!this._switchData)
            return;


        const switchData = this._switchData;
        switchData.gestureActivated = true;

        const index = this._findClosestWorkspaceIndex(endProgress);

        for (const monitorInfo of Object.values(this._switchData.monitors)) {
            const newWs = monitorInfo.workspaces[index];

            const params = {
                x: -newWs.x,
                y: -newWs.y,
                duration,
                mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            };

            if (monitorInfo.index === Main.layoutManager.primaryIndex) {
                params.onComplete = () => {
                    if (!newWs.ws.active)
                        newWs.ws.activate(global.get_current_time());
                    this._finishWorkspaceSwitch(switchData);
                };
            }

            monitorInfo.container.ease(params);
        }
    }

    isAnimating() {
        return this._switchData !== null;
    }

    canCancelGesture() {
        return this.isAnimating() && this._switchData.gestureActivated;
    }

    cancelSwitchAnimation() {
        if (!this._switchData)
            return;

        if (this._switchData.inProgress || !this._switchData.gestureActivated)
            this._finishWorkspaceSwitch(this._switchData);
    }

    set movingWindow(movingWindow) {
        this._movingWindow = movingWindow;
    }

    get movingWindow() {
        return this._movingWindow;
    }
};
