// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceAnimationController */

const { Clutter, GObject, Meta, Shell } = imports.gi;

const Main = imports.ui.main;
const SwipeTracker = imports.ui.swipeTracker;

const WINDOW_ANIMATION_TIME = 250;

const WorkspaceGroup = GObject.registerClass(
class WorkspaceGroup extends Clutter.Actor {
    _init(workspace, movingWindow) {
        super._init();

        this._workspace = workspace;
        this._movingWindow = movingWindow;
        this._windowRecords = [];

        this._createWindows();

        this.connect('destroy', this._onDestroy.bind(this));
        this._restackedId = global.display.connect('restacked',
            this._syncStacking.bind(this));
    }

    _shouldShowWindow(window) {
        if (!window.showing_on_its_workspace())
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
        const windowActors = global.get_window_actors();
        let lastSibling = null;

        for (const windowActor of windowActors) {
            this.set_child_above_sibling(windowActor, lastSibling);
            lastSibling = windowActor;
        }
    }

    _createWindows() {
        const windowActors = global.get_window_actors().filter(w =>
            this._shouldShowWindow(w.meta_window));

        for (const windowActor of windowActors) {
            const record = {
                windowActor,
                parent: windowActor.get_parent(),
            };

            record.parent.remove_child(windowActor);
            this.add_child(windowActor);
            windowActor.show();

            record.windowDestroyId = windowActor.connect('destroy', () => {
                this._windowRecords.splice(this._windowRecords.indexOf(record), 1);
            });

            this._windowRecords.push(record);
        }
    }

    _removeWindows() {
        for (const record of this._windowRecords) {
            record.windowActor.disconnect(record.windowDestroyId);
            this.remove_child(record.windowActor);
            record.parent.add_child(record.windowActor);

            // No workspace means we showed sticky windows,
            // don't hide anything in this case
            if (this._workspace &&
                !record.windowActor.get_meta_window().get_workspace().active)
                record.windowActor.hide();
        }

        this._windowRecords = [];
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
        const workspaceManager = global.workspace_manager;
        const curWs = workspaceManager.get_workspace_by_index(from);

        const switchData = {};

        this._switchData = switchData;
        switchData.curGroup = new WorkspaceGroup(curWs, this.movingWindow);
        switchData.movingWindowBin = new Clutter.Actor();
        switchData.movingWindow = null;
        switchData.surroundings = {};
        switchData.gestureActivated = false;
        switchData.inProgress = false;

        switchData.container = new Clutter.Actor();
        switchData.container.add_child(switchData.curGroup);

        wgroup.add_child(switchData.movingWindowBin);
        wgroup.add_child(switchData.container);

        for (const dir of Object.values(Meta.MotionDirection)) {
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
                actor: new WorkspaceGroup(ws, this.movingWindow),
                xDest: x,
                yDest: y,
            };
            switchData.surroundings[dir] = info;
            switchData.container.add_child(info.actor);
            switchData.container.set_child_above_sibling(info.actor, null);

            info.actor.set_position(x, y);
        }

        wgroup.set_child_above_sibling(switchData.movingWindowBin, null);

        if (this.movingWindow) {
            const windowActor = this.movingWindow.get_compositor_private();

            switchData.movingWindow = {
                windowActor,
                parent: windowActor.get_parent(),
            };

            switchData.movingWindow.parent.remove_child(windowActor);
            switchData.movingWindowBin.add_child(windowActor);
            switchData.movingWindow.windowDestroyId = windowActor.connect('destroy', () => {
                switchData.movingWindow = null;
            });
        }
    }

    _finishWorkspaceSwitch(switchData) {
        this._switchData = null;

        if (switchData.movingWindow) {
            const record = switchData.movingWindow;
            record.windowActor.disconnect(record.windowDestroyId);
            switchData.movingWindowBin.remove_child(record.windowActor);
            record.parent.add_child(record.windowActor);

            switchData.movingWindow = null;
        }

        switchData.container.destroy();
        switchData.movingWindowBin.destroy();

        this.movingWindow = null;
    }

    animateSwitch(from, to, direction, onComplete) {
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

        const horiz = global.workspace_manager.layout_rows !== -1;
        let baseDistance;
        if (horiz)
            baseDistance = global.screen_width;
        else
            baseDistance = global.screen_height;

        let direction = this._directionForProgress(-1);
        let info = this._switchData.surroundings[direction];
        if (info !== null) {
            const distance = horiz ? info.xDest : info.yDest;
            lower = -Math.abs(distance) / baseDistance;
        }

        direction = this._directionForProgress(1);
        info = this._switchData.surroundings[direction];
        if (info !== null) {
            const distance = horiz ? info.xDest : info.yDest;
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
