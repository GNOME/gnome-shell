// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, GObject, Meta, Shell } = imports.gi;

const Main = imports.ui.main;
const Tweener = imports.ui.tweener;
const SwipeTracker = imports.ui.swipeTracker;

var WINDOW_ANIMATION_TIME = 0.25;

var WorkspaceGroup = GObject.registerClass(
class WorkspaceGroup extends Clutter.Actor {
    _init(controller, workspace) {
        super._init();

        this._controller = controller;
        this._workspace = workspace;
        this._windows = [];

        this.refreshWindows();

        this.connect('destroy', this._onDestroy.bind(this));
        this._restackedId = global.display.connect('restacked', this.refreshWindows.bind(this));
    }

    _shouldShowWindow(window) {
        if (window.get_workspace() != this._workspace)
            return false;

        if (!window.showing_on_its_workspace())
            return false;

        if (window.is_on_all_workspaces())
            return false;

        if (this._controller.movingWindow && window == this._controller.movingWindow)
            return false;

        return true;
    }

    refreshWindows() {
        if (this._windows.length > 0)
            this.removeWindows();

        let windows = global.get_window_actors();
        windows = windows.filter(w => this._shouldShowWindow(w.meta_window));

        for (let window of windows) {
            let record = { window: window,
                           parent: window.get_parent() };

            window.reparent(this);

            record.windowDestroyId = window.connect('destroy', () => {
                this._windows.splice(this._windows.indexOf(record), 1);
            });

            this._windows.push(record);
        }
    }

    removeWindows() {
        for (let i = 0; i < this._windows.length; i++) {
            let w = this._windows[i];

            w.window.disconnect(w.windowDestroyId);
            w.window.reparent(w.parent);

            if (w.window.get_meta_window().get_workspace() !=
                global.workspace_manager.get_active_workspace())
                w.window.hide();
        }

        this._windows = [];
    }

    _onDestroy() {
        global.display.disconnect(this._restackedId);
        this.removeWindows();
    }
});

var WorkspaceAnimation = class {
    constructor(controller, from, to, direction) {
        this._controller = controller;
        this._movingWindowBin = new Clutter.Actor();
        this._movingWindow = null;
        this._surroundings = {};
        this._progress = 0;

        this._container = new Clutter.Actor();

        global.window_group.add_actor(this._movingWindowBin);
        global.window_group.add_actor(this._container);

        let workspaceManager = global.workspace_manager;
        let curWs = workspaceManager.get_workspace_by_index(from);

        this._curGroup = new WorkspaceGroup(controller, curWs);
        this._container.add_actor(this._curGroup);

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
                         actor: new WorkspaceGroup(controller, ws),
                         xDest: x,
                         yDest: y };
            this._surroundings[dir] = info;
            this._container.add_actor(info.actor);
            info.actor.raise_top();

            info.actor.set_position(x, y);
        }

        this._movingWindowBin.raise_top();

        if (this._controller.movingWindow) {
            let actor = this._controller.movingWindow.get_compositor_private();

            this._movingWindow = { window: actor,
                                   parent: actor.get_parent() };

            actor.reparent(this._movingWindowBin);
            this._movingWindow.windowDestroyId = actor.connect('destroy', () => {
                this._movingWindow = null;
            });
        }
    }

    destroy() {
        if (this._movingWindow) {
            let record = this._movingWindow;
            record.window.disconnect(record.windowDestroyId);
            record.window.reparent(record.parent);

            this._movingWindow = null;
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

    animateSwitchWorkspace(shellwm, from, to, direction, callback) {
        this._prepareWorkspaceSwitch(from, to, direction);
        this._inProgress = true;

        Tweener.addTween(this._animation,
                         { progress: direction == Meta.MotionDirection.DOWN ? 1 : -1,
                           time: WINDOW_ANIMATION_TIME,
                           transition: 'easeOutCubic',
                           onComplete: () => { this._switchWorkspaceDone(shellwm); }
                         });
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

        if (this._gestureActivated) {
            Tweener.removeTweens(this._animation);

            tracker.continueSwipe(this._animation.progress);
            return;
        }

        this._prepareWorkspaceSwitch(activeWorkspace.index(), -1);

        let baseDistance;
        if (horiz)
            baseDistance = global.screen_width;
        else
            baseDistance = global.screen_height - Main.panel.height;

        let backExtent = 0;
        let forwardExtent = 0;

        let backDirection = this._animation.directionForProgress(1);
        let forwardDirection = this._animation.directionForProgress(-1);

        let backDistance = this._animation.getDistance(backDirection);
        let forwardDistance = this._animation.getDistance(forwardDirection);

        if (!horiz) {
            if (backDistance != 0)
                backExtent = backDistance - baseDistance;
            if (forwardDistance != 0)
                forwardExtent = forwardDistance - baseDistance;
        }

        tracker.confirmSwipe((backDistance != 0), (forwardDistance != 0), baseDistance, backExtent, forwardExtent);
    }

    _switchWorkspaceUpdate(tracker, progress) {
        if (this._animation)
            this._animation.progress = progress;
    }

    _switchWorkspaceEnd(tracker, duration, endProgress) {
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
                           time: duration,
                           transition: 'easeOutCubic',
                           onComplete: () => {
                               if (newWs != activeWorkspace)
                                   newWs.activate(global.get_current_time());
                               this._finishWorkspaceSwitch();
                           }
                         });
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
