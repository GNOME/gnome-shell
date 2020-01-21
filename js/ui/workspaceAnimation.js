// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceAnimationController */

const { Clutter, Meta, Shell } = imports.gi;

const Main = imports.ui.main;
const SwipeTracker = imports.ui.swipeTracker;

const WINDOW_ANIMATION_TIME = 250;

var WorkspaceAnimationController = class {
    constructor() {
        this._movingWindow = null;
        this._switchData = null;

        global.display.connect('restacked', this._syncStacking.bind(this));

        Main.overview.connect('showing', () => {
            if (this._switchData) {
                if (this._switchData.gestureActivated)
                    this._switchWorkspaceStop();
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

    _syncStacking() {
        if (this._switchData === null)
            return;

        let windows = global.get_window_actors();
        let lastCurSibling = null;
        let lastDirSibling = [];
        for (let i = 0; i < windows.length; i++) {
            if (windows[i].get_parent() === this._switchData.curGroup) {
                this._switchData.curGroup.set_child_above_sibling(windows[i], lastCurSibling);
                lastCurSibling = windows[i];
            } else {
                for (let dir of Object.values(Meta.MotionDirection)) {
                    let info = this._switchData.surroundings[dir];
                    if (!info || windows[i].get_parent() !== info.actor)
                        continue;

                    let sibling = lastDirSibling[dir];
                    if (sibling === undefined)
                        sibling = null;

                    info.actor.set_child_above_sibling(windows[i], sibling);
                    lastDirSibling[dir] = windows[i];
                    break;
                }
            }
        }
    }

    _getPositionForDirection(direction, fromWs, toWs) {
        let xDest = 0, yDest = 0;

        let oldWsIsFullscreen = fromWs.list_windows().some(w => w.is_fullscreen());
        let newWsIsFullscreen = toWs.list_windows().some(w => w.is_fullscreen());

        // We have to shift windows up or down by the height of the panel to prevent having a
        // visible gap between the windows while switching workspaces. Since fullscreen windows
        // hide the panel, they don't need to be shifted up or down.
        let shiftHeight = Main.panel.height;

        if (direction === Meta.MotionDirection.UP ||
            direction === Meta.MotionDirection.UP_LEFT ||
            direction === Meta.MotionDirection.UP_RIGHT)
            yDest = -global.screen_height + (oldWsIsFullscreen ? 0 : shiftHeight);
        else if (direction === Meta.MotionDirection.DOWN ||
            direction === Meta.MotionDirection.DOWN_LEFT ||
            direction === Meta.MotionDirection.DOWN_RIGHT)
            yDest = global.screen_height - (newWsIsFullscreen ? 0 : shiftHeight);

        if (direction === Meta.MotionDirection.LEFT ||
            direction === Meta.MotionDirection.UP_LEFT ||
            direction === Meta.MotionDirection.DOWN_LEFT)
            xDest = -global.screen_width;
        else if (direction === Meta.MotionDirection.RIGHT ||
                 direction === Meta.MotionDirection.UP_RIGHT ||
                 direction === Meta.MotionDirection.DOWN_RIGHT)
            xDest = global.screen_width;

        return [xDest, yDest];
    }

    _prepareWorkspaceSwitch(from, to, direction) {
        if (this._switchData)
            return;

        let wgroup = global.window_group;
        let windows = global.get_window_actors();
        let switchData = {};

        this._switchData = switchData;
        switchData.curGroup = new Clutter.Actor();
        switchData.movingWindowBin = new Clutter.Actor();
        switchData.windows = [];
        switchData.surroundings = {};
        switchData.gestureActivated = false;
        switchData.inProgress = false;

        switchData.container = new Clutter.Actor();
        switchData.container.add_actor(switchData.curGroup);

        wgroup.add_actor(switchData.movingWindowBin);
        wgroup.add_actor(switchData.container);

        let workspaceManager = global.workspace_manager;
        let curWs = workspaceManager.get_workspace_by_index(from);

        for (let dir of Object.values(Meta.MotionDirection)) {
            let ws = null;

            if (to < 0)
                ws = curWs.get_neighbor(dir);
            else if (dir === direction)
                ws = workspaceManager.get_workspace_by_index(to);

            if (ws === null || ws === curWs) {
                switchData.surroundings[dir] = null;
                continue;
            }

            let [x, y] = this._getPositionForDirection(dir, curWs, ws);
            let info = {
                index: ws.index(),
                actor: new Clutter.Actor(),
                xDest: x,
                yDest: y,
            };
            switchData.surroundings[dir] = info;
            switchData.container.add_actor(info.actor);
            switchData.container.set_child_above_sibling(info.actor, null);

            info.actor.set_position(x, y);
        }

        wgroup.set_child_above_sibling(switchData.movingWindowBin, null);

        for (let i = 0; i < windows.length; i++) {
            let actor = windows[i];
            let window = actor.get_meta_window();

            if (!window.showing_on_its_workspace())
                continue;

            if (window.is_on_all_workspaces())
                continue;

            let record = {
                window: actor,
                parent: actor.get_parent(),
            };

            if (this.movingWindow && window === this.movingWindow) {
                record.parent.remove_child(actor);
                switchData.movingWindow = record;
                switchData.windows.push(switchData.movingWindow);
                switchData.movingWindowBin.add_child(actor);
            } else if (window.get_workspace().index() === from) {
                record.parent.remove_child(actor);
                switchData.windows.push(record);
                switchData.curGroup.add_child(actor);
            } else {
                let visible = false;
                for (let dir of Object.values(Meta.MotionDirection)) {
                    let info = switchData.surroundings[dir];

                    if (!info || info.index !== window.get_workspace().index())
                        continue;

                    record.parent.remove_child(actor);
                    switchData.windows.push(record);
                    info.actor.add_child(actor);
                    visible = true;
                    break;
                }

                actor.visible = visible;
            }
        }

        for (let i = 0; i < switchData.windows.length; i++) {
            let w = switchData.windows[i];

            w.windowDestroyId = w.window.connect('destroy', () => {
                switchData.windows.splice(switchData.windows.indexOf(w), 1);
            });
        }
    }

    _finishWorkspaceSwitch(switchData) {
        this._switchData = null;

        for (let i = 0; i < switchData.windows.length; i++) {
            let w = switchData.windows[i];

            w.window.disconnect(w.windowDestroyId);
            w.window.get_parent().remove_child(w.window);
            w.parent.add_child(w.window);

            if (w.window.get_meta_window().get_workspace() !==
                global.workspace_manager.get_active_workspace())
                w.window.hide();
        }
        switchData.container.destroy();
        switchData.movingWindowBin.destroy();

        this.movingWindow = null;
    }

    animateSwitchWorkspace(from, to, direction, onComplete) {
        this._prepareWorkspaceSwitch(from, to, direction);
        this._switchData.inProgress = true;

        let workspaceManager = global.workspace_manager;
        let fromWs = workspaceManager.get_workspace_by_index(from);
        let toWs = workspaceManager.get_workspace_by_index(to);

        let [xDest, yDest] = this._getPositionForDirection(direction, fromWs, toWs);

        /* @direction is the direction that the "camera" moves, so the
         * screen contents have to move one screen's worth in the
         * opposite direction.
         */
        xDest = -xDest;
        yDest = -yDest;

        this._switchData.container.ease({
            x: xDest,
            y: yDest,
            duration: WINDOW_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            onComplete: () => {
                this._finishWorkspaceSwitch(this._switchData);
                onComplete();
            },
        });
    }

    _directionForProgress(progress) {
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

    _getProgressRange() {
        if (!this._switchData)
            return [0, 0];

        let lower = 0;
        let upper = 0;

        let horiz = global.workspace_manager.layout_rows !== -1;
        let baseDistance;
        if (horiz)
            baseDistance = global.screen_width;
        else
            baseDistance = global.screen_height;

        let direction = this._directionForProgress(-1);
        let info = this._switchData.surroundings[direction];
        if (info !== null) {
            let distance = horiz ? info.xDest : info.yDest;
            lower = -Math.abs(distance) / baseDistance;
        }

        direction = this._directionForProgress(1);
        info = this._switchData.surroundings[direction];
        if (info !== null) {
            let distance = horiz ? info.xDest : info.yDest;
            upper = Math.abs(distance) / baseDistance;
        }

        return [lower, upper];
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
            baseDistance = global.screen_width;
        else
            baseDistance = global.screen_height;

        let progress;
        if (this._switchData && this._switchData.gestureActivated) {
            this._switchData.container.remove_all_transitions();
            if (!horiz)
                progress = -this._switchData.container.y / baseDistance;
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                progress = this._switchData.container.x / baseDistance;
            else
                progress = -this._switchData.container.x / baseDistance;
        } else {
            this._prepareWorkspaceSwitch(activeWorkspace.index(), -1);
            progress = 0;
        }

        let points = [];
        let [lower, upper] = this._getProgressRange();

        if (lower !== 0)
            points.push(lower);

        points.push(0);

        if (upper !== 0)
            points.push(upper);

        tracker.confirmSwipe(baseDistance, points, progress, 0);
    }

    _switchWorkspaceUpdate(tracker, progress) {
        if (!this._switchData)
            return;

        let direction = this._directionForProgress(progress);
        let info = this._switchData.surroundings[direction];
        let xPos = 0;
        let yPos = 0;
        if (info) {
            if (global.workspace_manager.layout_rows === -1)
                yPos = -Math.round(progress * global.screen_height);
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                xPos = Math.round(progress * global.screen_width);
            else
                xPos = -Math.round(progress * global.screen_width);
        }

        this._switchData.container.set_position(xPos, yPos);
    }

    _switchWorkspaceEnd(tracker, duration, endProgress) {
        if (!this._switchData)
            return;

        let workspaceManager = global.workspace_manager;
        let activeWorkspace = workspaceManager.get_active_workspace();
        let newWs = activeWorkspace;
        let xDest = 0;
        let yDest = 0;
        if (endProgress !== 0) {
            let direction = this._directionForProgress(endProgress);
            newWs = activeWorkspace.get_neighbor(direction);
            xDest = -this._switchData.surroundings[direction].xDest;
            yDest = -this._switchData.surroundings[direction].yDest;
        }

        let switchData = this._switchData;
        switchData.gestureActivated = true;

        this._switchData.container.ease({
            x: xDest,
            y: yDest,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            onComplete: () => {
                if (newWs !== activeWorkspace)
                    newWs.activate(global.get_current_time());
                this._finishWorkspaceSwitch(switchData);
            },
        });
    }

    _switchWorkspaceStop() {
        this._switchData.container.x = 0;
        this._switchData.container.y = 0;
        this._finishWorkspaceSwitch(this._switchData);
    }

    isAnimating() {
        return this._switchData !== null;
    }

    canCancelGesture() {
        return this.isAnimating() && this._switchData.gestureActivated;
    }

    set movingWindow(movingWindow) {
        this._movingWindow = movingWindow;
    }

    get movingWindow() {
        return this._movingWindow;
    }
};
