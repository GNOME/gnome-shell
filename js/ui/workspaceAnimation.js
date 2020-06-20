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
        if (window.get_workspace() !== this._workspace)
            return false;

        if (!window.showing_on_its_workspace())
            return false;

        let geometry = global.display.get_monitor_geometry(this._monitor.index);
        let [intersects, intersection_] = window.get_frame_rect().intersect(geometry);
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

        for (let window of windows) {
            let clone = new Clutter.Clone({
                source: window,
                x: window.x - this._monitor.x,
                y: window.y - this._monitor.y,
            });

            this.add_child(clone);
            window.hide();

            let record = { window, clone };

            record.windowDestroyId = window.connect('destroy', () => {
                clone.destroy();
                this._windows.splice(this._windows.indexOf(record), 1);
            });

            this._windows.push(record);
        }
    }

    _removeWindows() {
        for (const record of this._windows) {
            record.window.disconnect(record.windowDestroyId);
            record.clone.destroy();

            if (record.window.get_meta_window().get_workspace().active)
                record.window.show();
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

        let geometry = global.display.get_monitor_geometry(this._monitor.index);
        let [intersects, intersection_] = window.get_frame_rect().intersect(geometry);
        if (!intersects)
            return false;

        return window.is_on_all_workspaces() || window === this._movingWindow;
    }

    _refreshWindows() {
        if (this._windows.length > 0)
            this._removeWindows();

        const windows = global.get_window_actors().filter(w =>
            this._shouldShowWindow(w.meta_window));

        for (let window of windows) {
            let clone = new Clutter.Clone({
                source: window,
                x: window.x - this._monitor.x,
                y: window.y - this._monitor.y,
            });

            this.add_child(clone);
            window.hide();

            let record = { window, clone };

            record.windowDestroyId = window.connect('destroy', () => {
                clone.destroy();
                this._windows.splice(this._windows.indexOf(record), 1);
            });

            this._windows.push(record);
        }
    }

    _removeWindows() {
        for (const record of this._windows) {
            record.window.disconnect(record.windowDestroyId);
            record.clone.destroy();

            record.window.show();
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

        let wgroup = global.window_group;
        let workspaceManager = global.workspace_manager;
        let vertical = workspaceManager.layout_rows === -1;
        let nWorkspaces = workspaceManager.get_n_workspaces();
        let activeWorkspaceIndex = workspaceManager.get_active_workspace_index();

        let switchData = {};

        this._switchData = switchData;
        switchData.monitors = [];

        switchData.gestureActivated = false;
        switchData.inProgress = false;

        if (!workspaces) {
            workspaces = [];

            for (let i = 0; i < nWorkspaces; i++)
                workspaces.push(i);
        }

        for (let monitor of Main.layoutManager.monitors) {
            let record = {
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

            let constraint = new Layout.MonitorConstraint({ index: monitor.index });
            record.clipBin.add_constraint(constraint);

            record.clipBin.add_child(record.container);
            record.clipBin.add_child(record.stickyGroup);

            wgroup.add_child(record.clipBin);

            let x = 0;
            let y = 0;

            let geometry = Main.layoutManager.monitors[record.index];

            for (let i of workspaces) {
                let ws = workspaceManager.get_workspace_by_index(i);
                let fullscreen = ws.list_windows().some(w => w.get_monitor() === record.index && w.is_fullscreen());

                if (i > 0 && vertical && !fullscreen && record.index === Main.layoutManager.primaryIndex) {
                    // We have to shift windows up or down by the height of the panel to prevent having a
                    // visible gap between the windows while switching workspaces. Since fullscreen windows
                    // hide the panel, they don't need to be shifted up or down.
                    y -= Main.panel.height;
                }

                let info = {
                    ws,
                    actor: new WorkspaceGroup(ws, monitor, this.movingWindow),
                    fullscreen,
                    x,
                    y,
                };

                if (vertical)
                    info.position = info.y / geometry.height;
                else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                    info.position = -info.x / geometry.width;
                else
                    info.position = info.x / geometry.width;

                record.workspaces[i] = info;
                record.container.add_child(info.actor);
                record.container.set_child_above_sibling(info.actor, null);
                info.actor.set_position(x, y);

                if (vertical)
                    y += geometry.height;
                else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                    x -= geometry.width;
                else
                    x += geometry.width;
            }

            record.clipBin.set_child_above_sibling(record.stickyGroup, null);

            switchData.monitors.push(record);

            if (global.workspace_manager.layout_rows === -1)
                record.container.y = -record.workspaces[activeWorkspaceIndex].y;
            else
                record.container.x = -record.workspaces[activeWorkspaceIndex].x;
        }
    }

    _finishWorkspaceSwitch(switchData) {
        this._switchData = null;

        for (let monitor of switchData.monitors)
            monitor.clipBin.destroy();

        this.movingWindow = null;
    }

    animateSwitchWorkspace(from, to, direction, onComplete) {
        if (this._switchData) {
            for (let monitor of this._switchData.monitors)
                monitor.container.remove_all_transitions();
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

        for (let monitor of this._switchData.monitors) {
            let fromWs = monitor.workspaces[from];
            let toWs = monitor.workspaces[to];

            monitor.container.x = -fromWs.x;
            monitor.container.y = -fromWs.y;

            let params = {
                x: -toWs.x,
                y: -toWs.y,
                duration: WINDOW_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            };

            if (monitor.index === Main.layoutManager.primaryIndex) {
                params.onComplete = () => {
                    this._finishWorkspaceSwitch(this._switchData);
                    onComplete();
                };
            }

            monitor.container.ease(params);
        }
    }

    _findClosestWorkspaceIndex(position) {
        const record = this._switchData.monitors[Main.layoutManager.primaryIndex];
        const distances = record.workspaces.map(ws => Math.abs(ws.position - position));
        return distances.indexOf(Math.min(...distances));
    }

    _switchWorkspaceBegin(tracker, monitor) {
        if (Meta.prefs_get_workspaces_only_on_primary() &&
            monitor !== Main.layoutManager.primaryIndex)
            return;

        let workspaceManager = global.workspace_manager;
        let horiz = workspaceManager.layout_rows !== -1;
        tracker.orientation = horiz
            ? Clutter.Orientation.HORIZONTAL
            : Clutter.Orientation.VERTICAL;

        let geometry = Main.layoutManager.monitors[monitor];
        let baseDistance = horiz ? geometry.width : geometry.height;

        let progress;
        let cancelProgress;
        if (this._switchData && this._switchData.gestureActivated) {
            for (let record of this._switchData.monitors)
                record.container.remove_all_transitions();

            let record = this._switchData.monitors[monitor];

            if (!horiz)
                progress = -record.container.y / baseDistance;
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                progress = record.container.x / baseDistance;
            else
                progress = -record.container.x / baseDistance;

            let ws = record.workspaces[this._findClosestWorkspaceIndex(progress)];
            cancelProgress = ws.position;
        } else {
            this._prepareWorkspaceSwitch();

            let activeIndex = workspaceManager.get_active_workspace_index();
            let ws = this._switchData.monitors[monitor].workspaces[activeIndex];

            progress = cancelProgress = ws.position;
        }

        let points = this._switchData.monitors[monitor].workspaces.map(ws => ws.position);

        this._switchData.baseMonitor = monitor;

        tracker.confirmSwipe(baseDistance, points, progress, cancelProgress);
    }

    _interpolateProgress(progress, monitor) {
        let monitor1 = this._switchData.monitors[this._switchData.baseMonitor];
        let monitor2 = monitor;

        let points1 = monitor1.workspaces.map(ws => ws.position);
        let points2 = monitor2.workspaces.map(ws => ws.position);

        let upper = points1.indexOf(points1.find(p => p >= progress));
        let lower = points1.indexOf(points1.slice().reverse().find(p => p <= progress));

        if (points1[upper] === points1[lower])
            return points2[upper];

        let t = (progress - points1[lower]) / (points1[upper] - points1[lower]);

        return points2[lower] + (points2[upper] - points2[lower]) * t;
    }

    _switchWorkspaceUpdate(tracker, progress) {
        if (!this._switchData)
            return;

        for (let monitor of this._switchData.monitors) {
            let xPos = 0;
            let yPos = 0;

            let geometry = Main.layoutManager.monitors[monitor.index];

            let p = this._interpolateProgress(progress, monitor);

            if (global.workspace_manager.layout_rows === -1)
                yPos = -Math.round(p * geometry.height);
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                xPos = Math.round(p * geometry.width);
            else
                xPos = -Math.round(p * geometry.width);

            monitor.container.set_position(xPos, yPos);
        }
    }

    _switchWorkspaceEnd(tracker, duration, endProgress) {
        if (!this._switchData)
            return;


        let switchData = this._switchData;
        switchData.gestureActivated = true;

        let index = this._findClosestWorkspaceIndex(endProgress);

        for (let monitor of this._switchData.monitors) {
            let newWs = monitor.workspaces[index];

            let params = {
                x: -newWs.x,
                y: -newWs.y,
                duration,
                mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            };

            if (monitor.index === Main.layoutManager.primaryIndex) {
                params.onComplete = () => {
                    if (!newWs.ws.active)
                        newWs.ws.activate(global.get_current_time());
                    this._finishWorkspaceSwitch(switchData);
                };
            }

            monitor.container.ease(params);
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
