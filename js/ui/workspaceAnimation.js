// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceAnimationController */

const { Clutter, GObject, Meta, Shell } = imports.gi;

const Main = imports.ui.main;
const Tweener = imports.ui.tweener;
const SwipeTracker = imports.ui.swipeTracker;

var WINDOW_ANIMATION_TIME = 0.25;

var WorkspaceAnimation = class {
    constructor(controller, from, to, direction) {
        this._controller = controller;
        this._curGroup = new Clutter.Actor();
        this._movingWindowBin = new Clutter.Actor();
        this._windows = [];
        this._surroundings = {};
        this._progress = 0;

        let wgroup = global.window_group;
        let windows = global.get_window_actors();

        this._container = new Clutter.Actor();
        this._container.add_actor(this._curGroup);

        wgroup.add_actor(this._movingWindowBin);
        wgroup.add_actor(this._container);

        let workspaceManager = global.workspace_manager;
        let curWs = workspaceManager.get_workspace_by_index(from);

        for (let dir of Object.values(Meta.MotionDirection)) {
            let ws = null;

            if (to < 0)
                ws = curWs.get_neighbor(dir);
            else if (dir == direction)
                ws = workspaceManager.get_workspace_by_index(to);

            if (ws == null || ws == curWs) {
                this._surroundings[dir] = null;
                continue;
            }

            let [x, y] = this._getPositionForDirection(dir, curWs, ws);
            let info = { index: ws.index(),
                         actor: new Clutter.Actor(),
                         xDest: x,
                         yDest: y };
            this._surroundings[dir] = info;
            this._container.add_actor(info.actor);
            info.actor.raise_top();

            info.actor.set_position(x, y);
        }

        this._movingWindowBin.raise_top();

        for (let i = 0; i < windows.length; i++) {
            let actor = windows[i];
            let window = actor.get_meta_window();

            if (!window.showing_on_its_workspace())
                continue;

            if (window.is_on_all_workspaces())
                continue;

            let record = { window: actor,
                           parent: actor.get_parent() };

            if (this._controller.movingWindow && window == this._controller.movingWindow) {
                this._movingWindow = record;
                this._windows.push(this._movingWindow);
                actor.reparent(this._movingWindowBin);
            } else if (window.get_workspace().index() == from) {
                this._windows.push(record);
                actor.reparent(this._curGroup);
            } else {
                let visible = false;
                for (let dir of Object.values(Meta.MotionDirection)) {
                    let info = this._surroundings[dir];

                    if (!info || info.index != window.get_workspace().index())
                        continue;

                    this._windows.push(record);
                    actor.reparent(info.actor);
                    visible = true;
                    break;
                }

                actor.visible = visible;
            }
        }

        for (let i = 0; i < this._windows.length; i++) {
            let w = this._windows[i];

            w.windowDestroyId = w.window.connect('destroy', () => {
                this._windows.splice(this._windows.indexOf(w), 1);
            });
        }

        global.display.connect('restacked', this._syncStacking.bind(this));
    }

    destroy() {
        for (let i = 0; i < this._windows.length; i++) {
            let w = this._windows[i];

            w.window.disconnect(w.windowDestroyId);
            w.window.reparent(w.parent);

            if (w.window.get_meta_window().get_workspace() !=
                global.workspace_manager.get_active_workspace())
                w.window.hide();
        }

        this._container.destroy();
        this._movingWindowBin.destroy();
    }

    _getPositionForDirection(direction, fromWs, toWs) {
        let xDest = 0, yDest = 0;

        let oldWsIsFullscreen = fromWs.list_windows().some(w => w.is_fullscreen());
        let newWsIsFullscreen = toWs.list_windows().some(w => w.is_fullscreen());

        // We have to shift windows up or down by the height of the panel to prevent having a
        // visible gap between the windows while switching workspaces. Since fullscreen windows
        // hide the panel, they don't need to be shifted up or down.
        let shiftHeight = Main.panel.height;

        if (direction == Meta.MotionDirection.UP ||
            direction == Meta.MotionDirection.UP_LEFT ||
            direction == Meta.MotionDirection.UP_RIGHT)
            yDest = -global.screen_height + (oldWsIsFullscreen ? 0 : shiftHeight);
        else if (direction == Meta.MotionDirection.DOWN ||
            direction == Meta.MotionDirection.DOWN_LEFT ||
            direction == Meta.MotionDirection.DOWN_RIGHT)
            yDest = global.screen_height - (newWsIsFullscreen ? 0 : shiftHeight);

        if (direction == Meta.MotionDirection.LEFT ||
            direction == Meta.MotionDirection.UP_LEFT ||
            direction == Meta.MotionDirection.DOWN_LEFT)
            xDest = -global.screen_width;
        else if (direction == Meta.MotionDirection.RIGHT ||
                 direction == Meta.MotionDirection.UP_RIGHT ||
                 direction == Meta.MotionDirection.DOWN_RIGHT)
            xDest = global.screen_width;

        return [xDest, yDest];
    }

    _syncStacking() {
        let windows = global.get_window_actors();
        let lastCurSibling = null;
        let lastDirSibling = [];
        for (let i = 0; i < windows.length; i++) {
            if (windows[i].get_parent() == this._curGroup) {
                this._curGroup.set_child_above_sibling(windows[i], lastCurSibling);
                lastCurSibling = windows[i];
            } else {
                for (let dir of Object.values(Meta.MotionDirection)) {
                    let info = this._surroundings[dir];
                    if (!info || windows[i].get_parent() != info.actor)
                        continue;

                    let sibling = lastDirSibling[dir];
                    if (sibling == undefined)
                        sibling = null;

                    info.actor.set_child_above_sibling(windows[i], sibling);
                    lastDirSibling[dir] = windows[i];
                    break;
                }
            }
        }
    }

    directionForProgress(progress) {
        if (global.workspace_manager.layout_rows == -1)
            return (progress > 0) ? Meta.MotionDirection.DOWN : Meta.MotionDirection.UP;
        else if (Clutter.get_default_text_direction () == Clutter.TextDirection.RTL)
            return (progress > 0) ? Meta.MotionDirection.LEFT : Meta.MotionDirection.RIGHT;
        else
            return (progress > 0) ? Meta.MotionDirection.RIGHT : Meta.MotionDirection.LEFT;
    }

    get progress() {
        return this._progress;
    }

    set progress(progress) {
        this._progress = progress;

        let direction = this.directionForProgress(progress);
        let info = this._surroundings[direction];
        let xPos = 0;
        let yPos = 0;
        if (info) {
            if (global.workspace_manager.layout_rows == -1)
                yPos = Math.round(Math.abs(progress) * -info.yDest);
            else
                xPos = Math.round(Math.abs(progress) * -info.xDest);
        }

        this._container.set_position(xPos, yPos);
    }

    getDistance(direction) {
        let info = this._surroundings[direction];
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
};

var WorkspaceAnimationController = class {
    constructor() {
        this._shellwm = global.window_manager;
        this._blockAnimations = false;
        this._movingWindow = null;
        this._inProgress = false;
        this._gestureActivated = false;
        this._animation = null;

        this._shellwm.connect('kill-switch-workspace', (shellwm) => {
            if (this._animation) {
                if (this._inProgress)
                    this._switchWorkspaceDone(shellwm);
                else if (!this._gestureActivated)
                    this._finishWorkspaceSwitch();
            }
        });

        Main.overview.connect('showing', () => {
            if (this._gestureActivated)
                this._switchWorkspaceStop();

            this._swipeTracker.enabled = false;
        });
        Main.overview.connect('hiding', () => {
            this._swipeTracker.enabled = true;
        });

        let allowedModes = Shell.ActionMode.NORMAL;
        let swipeTracker = new SwipeTracker.SwipeTracker(global.stage, allowedModes, false, false);
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
        Tweener.removeTweens(this._animation);
        this._animation.destroy();
        this._animation = null;
        this._inProgress = false;
        this._gestureActivated = false;
        this.movingWindow = null;
    }

    animateSwitchWorkspace(shellwm, from, to, direction) {
        this._prepareWorkspaceSwitch(from, to, direction);
        this._inProgress = true;

        Tweener.addTween(this._animation,
                         { progress: direction == Meta.MotionDirection.DOWN ? 1 : -1,
                           time: WINDOW_ANIMATION_TIME / 1000,
                           transition: 'easeOutCubic',
                           onComplete: this._switchWorkspaceDone,
                           onCompleteScope: this,
                           onCompleteParams: [shellwm] });
    }

    _switchWorkspaceDone(shellwm) {
        this._finishWorkspaceSwitch();
        shellwm.completed_switch_workspace();
    }

    _switchWorkspaceBegin(tracker, monitor) {
        if (Meta.prefs_get_workspaces_only_on_primary() && monitor != Main.layoutManager.primaryIndex)
            return;

        let horiz = (global.workspace_manager.layout_rows != -1);
        tracker.orientation = horiz ? Clutter.Orientation.HORIZONTAL : Clutter.Orientation.VERTICAL;

        let workspaceManager = global.workspace_manager;
        let activeWorkspace = workspaceManager.get_active_workspace();

        let progress = 0;
        if (this._gestureActivated) {
            Tweener.removeTweens(this._animation);
            progress = this._animation.progress;
        } else {
            this._prepareWorkspaceSwitch(activeWorkspace.index(), -1);
            progress = 0;
        }

        let points = [];
        let baseDistance;
        if (horiz)
            baseDistance = global.screen_width;
        else
            baseDistance = global.screen_height;

        let direction = this._animation.directionForProgress(-1);
        let distance = this._animation.getDistance(direction);
        if (distance != 0)
            points.push(-distance / baseDistance);

        points.push(0);

        direction = this._animation.directionForProgress(1);
        distance = this._animation.getDistance(direction);
        if (distance != 0)
            points.push(distance / baseDistance);

        tracker.confirmSwipe(baseDistance, points, progress, 0);
    }

    _switchWorkspaceUpdate(_tracker, progress) {
        if (this._animation)
            this._animation.progress = progress;
    }

    _switchWorkspaceEnd(_tracker, duration, endProgress) {
        if (!this._animation)
            return;

        let workspaceManager = global.workspace_manager;
        let activeWorkspace = workspaceManager.get_active_workspace();
        let newWs = activeWorkspace;
        if (endProgress != 0) {
            let direction = this._animation.directionForProgress(endProgress);
            newWs = activeWorkspace.get_neighbor(direction);
        }

        if (duration == 0) {
            if (newWs != activeWorkspace)
                newWs.activate(global.get_current_time());
            this._switchWorkspaceStop();
            return;
        }

        this._gestureActivated = true;

        Tweener.addTween(this._animation,
                         { progress: endProgress,
                           time: duration / 1000,
                           transition: 'easeOutCubic',
                           onComplete: () => {
                               if (newWs != activeWorkspace)
                                   newWs.activate(global.get_current_time());
                               this._finishWorkspaceSwitch();
                           } });
    }

    _switchWorkspaceStop() {
        this._animation.progress = 0;
        this._finishWorkspaceSwitch();
    }

    isAnimating() {
        return this._animation != null;
    }

    set movingWindow(movingWindow) {
        this._movingWindow = movingWindow;
    }

    get movingWindow() {
        return this._movingWindow;
    }
};
