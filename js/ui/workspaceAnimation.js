// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceAnimationController */

const { Clutter, GObject, Meta, Shell } = imports.gi;

const Main = imports.ui.main;
const Layout = imports.ui.layout;
const SwipeTracker = imports.ui.swipeTracker;

const WINDOW_ANIMATION_TIME = 250;

const WorkspaceGroup = GObject.registerClass(
class WorkspaceGroup extends Clutter.Actor {
    _init(controller, workspace, monitor) {
        super._init();

        this._controller = controller;
        this._workspace = workspace;
        this._monitor = monitor;
        this._windows = [];

        this._refreshWindows();

        this.connect('destroy', this._onDestroy.bind(this));
        this._restackedId = global.display.connect('restacked',
            this._refreshWindows.bind(this));
    }

    _shouldShowWindow(window) {
        if (window.get_workspace() !== this._workspace)
            return false;

        let geometry = global.display.get_monitor_geometry(this._monitor.index);
        let [intersects, intersection_] = window.get_frame_rect().intersect(geometry);
        if (!intersects)
            return false;

        if (!window.showing_on_its_workspace())
            return false;

        if (window.is_on_all_workspaces())
            return false;

        if (this._controller.movingWindow &&
            window === this._controller.movingWindow)
            return false;

        return true;
    }

    _refreshWindows() {
        if (this._windows.length > 0)
            this._removeWindows();

        let windows = global.get_window_actors();
        windows = windows.filter(w => this._shouldShowWindow(w.meta_window));

        for (let window of windows) {
            let clone = new Clutter.Clone({
                source: window,
                x: window.x - this._monitor.x,
                y: window.y - this._monitor.y,
            });

            this.add_actor(clone);
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
        for (let i = 0; i < this._windows.length; i++) {
            let w = this._windows[i];

            w.window.disconnect(w.windowDestroyId);
            w.clone.destroy();

            if (w.window.get_meta_window().get_workspace() ===
                global.workspace_manager.get_active_workspace())
                w.window.show();
        }

        this._windows = [];
    }

    _onDestroy() {
        global.display.disconnect(this._restackedId);
        this._removeWindows();
    }
});

const WorkspaceAnimation = GObject.registerClass({
    Properties: {
        'progress': GObject.ParamSpec.double(
            'progress', 'progress', 'progress',
            GObject.ParamFlags.READWRITE,
            -1, 1, 0),
    },
}, class WorkspaceAnimation extends Clutter.Actor {
    _init(controller, from, to, direction) {
        super._init();

        this.connect('destroy', this._onDestroy.bind(this));

        this._controller = controller;
        this._movingWindow = null;
        this._monitors = [];
        this._progress = 0;

        global.window_group.add_actor(this);

        let workspaceManager = global.workspace_manager;
        let curWs = workspaceManager.get_workspace_by_index(from);

        for (let monitor of Main.layoutManager.monitors) {
            let record = {
                index: monitor.index,
                clipBin: new Clutter.Actor({
                    x_expand: true,
                    y_expand: true,
                    clip_to_allocation: true,
                }),
                container: new Clutter.Actor(),
                surroundings: {},
            };

            let constraint = new Layout.MonitorConstraint({ index: monitor.index });
            record.clipBin.add_constraint(constraint);

            record.clipBin.add_actor(record.container);

            this.add_actor(record.clipBin);

            record.curGroup = new WorkspaceGroup(controller, curWs, monitor);
            record.container.add_actor(record.curGroup);

            for (let dir of Object.values(Meta.MotionDirection)) {
                let ws = null;

                if (to < 0)
                    ws = curWs.get_neighbor(dir);
                else if (dir === direction)
                    ws = workspaceManager.get_workspace_by_index(to);

                if (ws === null || ws === curWs) {
                    record.surroundings[dir] = null;
                    continue;
                }

                let [x, y] = this._getPositionForDirection(dir, curWs, ws,
                    monitor.index);
                let info = {
                    index: ws.index(),
                    actor: new WorkspaceGroup(controller, ws, monitor),
                    xDest: x,
                    yDest: y,
                };
                record.surroundings[dir] = info;
                record.container.add_actor(info.actor);
                record.container.set_child_above_sibling(info.actor, null);

                info.actor.set_position(x, y);
            }

            this._monitors.push(record);
        }

        if (this._controller.movingWindow) {
            let actor = this._controller.movingWindow.get_compositor_private();
            let container = new Clutter.Actor();

            this._movingWindow = {
                container,
                window: actor,
                parent: actor.get_parent(),
            };

            this._movingWindow.parent.remove_child(actor);
            this._movingWindow.container.add_child(actor);
            this._movingWindow.windowDestroyId = actor.connect('destroy', () => {
                this._movingWindow = null;
            });

            global.window_group.add_actor(container);
            global.window_group.set_child_above_sibling(container, null);
        }
    }

    _onDestroy() {
        this._monitors = [];

        if (this._movingWindow) {
            let record = this._movingWindow;
            record.window.disconnect(record.windowDestroyId);
            record.window.get_parent().remove_child(record.window);
            record.parent.add_child(record.window);
            record.container.destroy();

            this._movingWindow = null;
        }
    }

    _getPositionForDirection(direction, fromWs, toWs, monitor) {
        let xDest = 0, yDest = 0;

        let condition = w => w.get_monitor() === monitor && w.is_fullscreen();

        let oldWsIsFullscreen = fromWs.list_windows().some(condition);
        let newWsIsFullscreen = toWs.list_windows().some(condition);

        let geometry = Main.layoutManager.monitors[monitor];

        // We have to shift windows up or down by the height of the panel to prevent having a
        // visible gap between the windows while switching workspaces. Since fullscreen windows
        // hide the panel, they don't need to be shifted up or down.
        let shiftHeight = monitor === Main.layoutManager.primaryIndex
            ? Main.panel.height : 0;

        if (direction === Meta.MotionDirection.UP ||
            direction === Meta.MotionDirection.UP_LEFT ||
            direction === Meta.MotionDirection.UP_RIGHT)
            yDest = -geometry.height + (oldWsIsFullscreen ? 0 : shiftHeight);
        else if (direction === Meta.MotionDirection.DOWN ||
            direction === Meta.MotionDirection.DOWN_LEFT ||
            direction === Meta.MotionDirection.DOWN_RIGHT)
            yDest = geometry.height - (newWsIsFullscreen ? 0 : shiftHeight);

        if (direction === Meta.MotionDirection.LEFT ||
            direction === Meta.MotionDirection.UP_LEFT ||
            direction === Meta.MotionDirection.DOWN_LEFT)
            xDest = -geometry.width;
        else if (direction === Meta.MotionDirection.RIGHT ||
                 direction === Meta.MotionDirection.UP_RIGHT ||
                 direction === Meta.MotionDirection.DOWN_RIGHT)
            xDest = geometry.width;

        return [xDest, yDest];
    }

    directionForProgress(progress) {
        if (global.workspace_manager.layout_rows === -1) {
            return progress > 0
                ? Meta.MotionDirection.DOWN
                : Meta.MotionDirection.UP;
        } else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL) {
            return progress > 0
                ? Meta.MotionDirection.LEFT
                : Meta.MotionDirection.RIGHT;
        } else {
            return progress > 0
                ? Meta.MotionDirection.RIGHT
                : Meta.MotionDirection.LEFT;
        }
    }

    progressForDirection(dir) {
        if (global.workspace_manager.layout_rows === -1)
            return dir === Meta.MotionDirection.DOWN ? 1 : -1;
        else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
            return dir === Meta.MotionDirection.LEFT ? 1 : -1;
        else
            return dir === Meta.MotionDirection.RIGHT ? 1 : -1;
    }

    get progress() {
        return this._progress;
    }

    set progress(progress) {
        this._progress = progress;

        let direction = this.directionForProgress(progress);

        for (let monitorData of this._monitors) {
            let xPos = 0;
            let yPos = 0;

            if (global.workspace_manager.layout_rows === -1)
                yPos = -Math.round(progress * this._getDistance(monitorData, direction));
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                xPos = Math.round(progress * this._getDistance(monitorData, direction));
            else
                xPos = -Math.round(progress * this._getDistance(monitorData, direction));

            monitorData.container.set_position(xPos, yPos);
        }
    }

    _getDistance(monitorData, direction) {
        let info = monitorData.surroundings[direction];
        if (!info)
            return 0;

        switch (direction) {
        case Meta.MotionDirection.UP:
            return -info.yDest;
        case Meta.MotionDirection.DOWN:
            return info.yDest;
        case Meta.MotionDirection.LEFT:
            return -info.xDest;
        case Meta.MotionDirection.RIGHT:
            return info.xDest;
        }

        return 0;
    }

    getProgressRange(monitor) {
        let monitorData = null;
        for (let data of this._monitors) {
            if (data.index === monitor) {
                monitorData = data;
                break;
            }
        }

        if (!monitorData)
            return 0;

        let baseDistance;
        if (global.workspace_manager.layout_rows !== -1)
            baseDistance = Main.layoutManager.monitors[monitor].width;
        else
            baseDistance = Main.layoutManager.monitors[monitor].height;

        let direction = this.directionForProgress(-1);
        let distance = this._getDistance(monitorData, direction);
        let lower = -distance / baseDistance;

        direction = this.directionForProgress(1);
        distance = this._getDistance(monitorData, direction);
        let upper = distance / baseDistance;

        return [lower, upper];
    }
});

var WorkspaceAnimationController = class {
    constructor() {
        this._blockAnimations = false;
        this._movingWindow = null;
        this._inProgress = false;
        this._gestureActivated = false;
        this._animation = null;

        Main.overview.connect('showing', () => {
            if (this._gestureActivated)
                this._switchWorkspaceStop();

            this._swipeTracker.enabled = false;
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

    _prepareWorkspaceSwitch(from, to, direction) {
        if (this._animation)
            return;

        this._animation = new WorkspaceAnimation(this, from, to, direction);
    }

    _finishWorkspaceSwitch() {
        if (this._animation)
            this._animation.destroy();
        this._animation = null;
        this._inProgress = false;
        this._gestureActivated = false;
        this.movingWindow = null;
        this._monitor = null;
    }

    animateSwitchWorkspace(from, to, direction, onComplete) {
        this._prepareWorkspaceSwitch(from, to, direction);
        this._inProgress = true;

        let progress = this._animation.progressForDirection(direction);

        this._animation.ease_property('progress', progress, {
            duration: WINDOW_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            onComplete: () => {
                this._finishWorkspaceSwitch();
                onComplete();
            },
        });
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

        let activeWorkspace = workspaceManager.get_active_workspace();

        let baseDistance;
        if (horiz)
            baseDistance = Main.layoutManager.monitors[monitor].width;
        else
            baseDistance = Main.layoutManager.monitors[monitor].height;

        let progress;
        if (this._gestureActivated) {
            this._animation.remove_all_transitions();
            progress = this._animation.progress;
        } else {
            this._prepareWorkspaceSwitch(activeWorkspace.index(), -1);
            progress = 0;
        }

        this._monitor = monitor;
        let [lower, upper] = this._animation.getProgressRange(monitor);
        if (progress < 0)
            progress *= -lower;
        else if (progress > 0)
            progress *= upper;

        let points = [];
        if (lower !== 0)
            points.push(lower);

        points.push(0);

        if (upper !== 0)
            points.push(upper);

        tracker.confirmSwipe(baseDistance, points, progress, 0);
    }

    _switchWorkspaceUpdate(tracker, progress) {
        // Translate the progress into [-1;1] range
        let [lower, upper] = this._animation.getProgressRange(this._monitor);
        if (progress < 0)
            progress /= -lower;
        else if (progress > 0)
            progress /= upper;

        this._animation.progress = progress;
    }

    _switchWorkspaceEnd(tracker, duration, endProgress) {
        if (!this._animation)
            return;

        // Translate the progress into [-1;1] range
        endProgress = Math.sign(endProgress);

        let workspaceManager = global.workspace_manager;
        let activeWorkspace = workspaceManager.get_active_workspace();
        let newWs = activeWorkspace;
        if (endProgress !== 0) {
            let direction = this._animation.directionForProgress(endProgress);
            newWs = activeWorkspace.get_neighbor(direction);
        }

        this._gestureActivated = true;

        this._animation.ease_property('progress', endProgress, {
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            onComplete: () => {
                if (newWs !== activeWorkspace)
                    newWs.activate(global.get_current_time());
                this._finishWorkspaceSwitch();
            },
        });
    }

    _switchWorkspaceStop() {
        this._animation.progress = 0;
        this._finishWorkspaceSwitch();
    }

    isAnimating() {
        return this._animation !== null;
    }

    canCancelGesture() {
        return this.isAnimating() && this._gestureActivated;
    }

    set movingWindow(movingWindow) {
        this._movingWindow = movingWindow;
    }

    get movingWindow() {
        return this._movingWindow;
    }
};
