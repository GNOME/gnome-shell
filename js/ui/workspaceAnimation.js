// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceAnimationController */

const { Clutter, Meta, Shell } = imports.gi;

const Main = imports.ui.main;
const SwipeTracker = imports.ui.swipeTracker;

const WINDOW_ANIMATION_TIME = 250;

var WorkspaceAnimationController = class {
    constructor() {
        this._shellwm = global.window_manager;
        this._movingWindow = null;

        this._switchData = null;
        this._shellwm.connect('kill-switch-workspace', shellwm => {
            if (this._switchData) {
                if (this._switchData.inProgress)
                    this._switchWorkspaceDone(shellwm);
                else if (!this._switchData.gestureActivated)
                    this._finishWorkspaceSwitch(this._switchData);
            }
        });

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

        const windows = global.get_window_actors();
        let lastCurSibling = null;
        const lastDirSibling = [];
        for (const i = 0; i < windows.length; i++) {
            if (windows[i].get_parent() === this._switchData.curGroup) {
                this._switchData.curGroup.set_child_above_sibling(windows[i], lastCurSibling);
                lastCurSibling = windows[i];
            } else {
                for (const dir of Object.values(Meta.MotionDirection)) {
                    const info = this._switchData.surroundings[dir];
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

        const oldWsIsFullscreen = fromWs.list_windows().some(w => w.is_fullscreen());
        const newWsIsFullscreen = toWs.list_windows().some(w => w.is_fullscreen());

        // We have to shift windows up or down by the height of the panel to prevent having a
        // visible gap between the windows while switching workspaces. Since fullscreen windows
        // hide the panel, they don't need to be shifted up or down.
        const shiftHeight = Main.panel.height;

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

        const wgroup = global.window_group;
        const windows = global.get_window_actors();
        const switchData = {};

        this._switchData = switchData;
        switchData.curGroup = new Clutter.Actor();
        switchData.movingWindowBin = new Clutter.Actor();
        switchData.windows = [];
        switchData.surroundings = {};
        switchData.gestureActivated = false;
        switchData.inProgress = false;

        switchData.container = new Clutter.Actor();
        switchData.container.add_child(switchData.curGroup);

        wgroup.add_child(switchData.movingWindowBin);
        wgroup.add_child(switchData.container);

        const workspaceManager = global.workspace_manager;
        const curWs = workspaceManager.get_workspace_by_index(from);

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

            const [x, y] = this._getPositionForDirection(dir, curWs, ws);
            const info = {
                index: ws.index(),
                actor: new Clutter.Actor(),
                xDest: x,
                yDest: y,
            };
            switchData.surroundings[dir] = info;
            switchData.container.add_child(info.actor);
            switchData.container.set_child_above_sibling(info.actor, null);

            info.actor.set_position(x, y);
        }

        wgroup.set_child_above_sibling(switchData.movingWindowBin, null);

        for (const windowActor of windows) {
            const window = windowActor.get_meta_window();

            if (!window.showing_on_its_workspace())
                continue;

            if (window.is_on_all_workspaces())
                continue;

            const record = {
                windowActor,
                parent: windowActor.get_parent(),
            };

            if (this.movingWindow && window === this.movingWindow) {
                record.parent.remove_child(windowActor);
                switchData.movingWindow = record;
                switchData.windows.push(switchData.movingWindow);
                switchData.movingWindowBin.add_child(windowActor);
            } else if (window.get_workspace().index() === from) {
                record.parent.remove_child(windowActor);
                switchData.windows.push(record);
                switchData.curGroup.add_child(windowActor);
            } else {
                let visible = false;
                for (let dir of Object.values(Meta.MotionDirection)) {
                    let info = switchData.surroundings[dir];

                    if (!info || info.index !== window.get_workspace().index())
                        continue;

                    record.parent.remove_child(windowActor);
                    switchData.windows.push(record);
                    info.actor.add_child(windowActor);
                    visible = true;
                    break;
                }

                windowActor.visible = visible;
            }
        }

        for (const record of switchData.windows) {
            record.windowDestroyId = record.windowActor.connect('destroy', () => {
                switchData.windows.splice(switchData.windows.indexOf(record), 1);
            });
        }
    }

    _finishWorkspaceSwitch(switchData) {
        this._switchData = null;

        for (const record of switchData.windows) {
            record.windowActor.disconnect(record.windowDestroyId);
            switchData.movingWindowBin.remove_child(record.windowActor);
            record.parent.add_child(record.windowActor);

            if (!record.windowActor.get_meta_window().get_workspace().active)
                record.windowActor.hide();
        }
        switchData.container.destroy();
        switchData.movingWindowBin.destroy();

        this.movingWindow = null;
    }

    animateSwitchWorkspace(shellwm, from, to, direction) {
        this._prepareWorkspaceSwitch(from, to, direction);
        this._switchData.inProgress = true;

        const workspaceManager = global.workspace_manager;
        const fromWs = workspaceManager.get_workspace_by_index(from);
        const toWs = workspaceManager.get_workspace_by_index(to);

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
            onComplete: () => this._switchWorkspaceDone(shellwm),
        });
    }

    _switchWorkspaceDone(shellwm) {
        this._finishWorkspaceSwitch(this._switchData);
        shellwm.completed_switch_workspace();
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

        const horiz = global.workspace_manager.layout_rows !== -1;
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

        const workspaceManager = global.workspace_manager;
        const horiz = workspaceManager.layout_rows !== -1;
        tracker.orientation = horiz
            ? Clutter.Orientation.HORIZONTAL
            : Clutter.Orientation.VERTICAL;

        const activeWorkspace = workspaceManager.get_active_workspace();

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

        const points = [];
        const [lower, upper] = this._getProgressRange();

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

        const direction = this._directionForProgress(progress);
        const info = this._switchData.surroundings[direction];
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

        const workspaceManager = global.workspace_manager;
        const activeWorkspace = workspaceManager.get_active_workspace();
        let newWs = activeWorkspace;
        let xDest = 0;
        let yDest = 0;
        if (endProgress !== 0) {
            const direction = this._directionForProgress(endProgress);
            newWs = activeWorkspace.get_neighbor(direction);
            xDest = -this._switchData.surroundings[direction].xDest;
            yDest = -this._switchData.surroundings[direction].yDest;
        }

        const switchData = this._switchData;
        switchData.gestureActivated = true;

        this._switchData.container.ease({
            x: xDest,
            y: yDest,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            onComplete: () => {
                if (!newWs.active)
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

    set movingWindow(movingWindow) {
        this._movingWindow = movingWindow;
    }

    get movingWindow() {
        return this._movingWindow;
    }
};
