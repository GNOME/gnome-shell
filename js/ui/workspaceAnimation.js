// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceAnimationController */

const { Clutter, GObject, Meta, Shell } = imports.gi;

const Background = imports.ui.background;
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
        const windowActors = global.get_window_actors().filter(w =>
            this._shouldShowWindow(w.meta_window));

        let lastRecord;

        for (const windowActor of windowActors) {
            const record = this._windowRecords.find(r => r.windowActor === windowActor);

            this.set_child_above_sibling(record.clone, lastRecord ? lastRecord.clone : null);
            lastRecord = record;
        }
    }

    _createWindows() {
        const windowActors = global.get_window_actors().filter(w =>
            this._shouldShowWindow(w.meta_window));

        for (const windowActor of windowActors) {
            const clone = new Clutter.Clone({
                source: windowActor,
                x: windowActor.x,
                y: windowActor.y,
            });

            this.add_child(clone);

            const record = { windowActor, clone };

            record.windowDestroyId = windowActor.connect('destroy', () => {
                clone.destroy();
                this._windowRecords.splice(this._windowRecords.indexOf(record), 1);
            });

            this._windowRecords.push(record);
        }
    }

    _removeWindows() {
        for (const record of this._windowRecords) {
            record.windowActor.disconnect(record.windowDestroyId);
            record.clone.destroy();
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

        const workspaceManager = global.workspace_manager;
        const vertical = workspaceManager.layout_rows === -1;
        const nWorkspaces = workspaceManager.get_n_workspaces();
        const activeWorkspaceIndex = workspaceManager.get_active_workspace_index();

        const switchData = {};

        this._switchData = switchData;
        switchData.movingWindowBin = new Clutter.Actor();
        switchData.movingWindow = null;
        switchData.workspaces = [];
        switchData.gestureActivated = false;
        switchData.inProgress = false;

        switchData.container = new Clutter.Actor();
        switchData.background = new Meta.BackgroundGroup();

        Main.uiGroup.insert_child_above(switchData.background, global.window_group);
        Main.uiGroup.insert_child_above(switchData.container, switchData.background);
        Main.uiGroup.insert_child_above(switchData.movingWindowBin, switchData.container);

        let x = 0;
        let y = 0;

        if (!workspaces) {
            workspaces = [];

            for (let i = 0; i < nWorkspaces; i++)
                workspaces.push(i);
        }

        for (const i of workspaces) {
            const ws = workspaceManager.get_workspace_by_index(i);
            const fullscreen = ws.list_windows().some(w => w.is_fullscreen());

            if (y > 0 && vertical && !fullscreen) {
                // We have to shift windows up or down by the height of the panel to prevent having a
                // visible gap between the windows while switching workspaces. Since fullscreen windows
                // hide the panel, they don't need to be shifted up or down.
                y -= Main.panel.height;
            }

            const info = {
                ws,
                actor: new WorkspaceGroup(ws, this.movingWindow),
                fullscreen,
                x,
                y,
            };

            switchData.workspaces.push(info);
            switchData.container.add_child(info.actor);
            switchData.container.set_child_above_sibling(info.actor, null);
            info.actor.set_position(x, y);

            if (vertical)
                y += global.screen_height;
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                x -= global.screen_width;
            else
                x += global.screen_width;
        }

        const activeInfo = this._findWorkspaceInfoByIndex(activeWorkspaceIndex);
        if (vertical)
            switchData.container.y = -activeInfo.y;
        else
            switchData.container.x = -activeInfo.x;

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

        switchData.bgManager = new Background.BackgroundManager({
            container: switchData.background,
            monitorIndex: Main.layoutManager.primaryIndex,
        });
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

        switchData.background.destroy();
        switchData.container.destroy();
        switchData.movingWindowBin.destroy();

        this.movingWindow = null;
    }

    animateSwitch(from, to, direction, onComplete) {
        if (this._switchData)
            this._switchData.container.remove_all_transitions();

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

        const fromWs = this._findWorkspaceInfoByIndex(from);
        const toWs = this._findWorkspaceInfoByIndex(to);

        this._switchData.container.x = -fromWs.x;
        this._switchData.container.y = -fromWs.y;

        this._switchData.container.ease({
            x: -toWs.x,
            y: -toWs.y,
            duration: WINDOW_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            onComplete: () => {
                this._finishWorkspaceSwitch(this._switchData);
                onComplete();
            },
        });
    }

    _getProgressForWorkspace(workspaceInfo) {
        if (global.workspace_manager.layout_rows === -1)
            return workspaceInfo.y / global.screen_height;
        else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
            return -workspaceInfo.x / global.screen_width;
        else
            return workspaceInfo.x / global.screen_width;
    }

    _findWorkspaceInfoByIndex(index) {
        const workspace = global.workspace_manager.get_workspace_by_index(index);
        return this._switchData.workspaces.find(ws => ws.ws == workspace);
    }

    _findClosestWorkspace(progress) {
        const distances = this._switchData.workspaces.map(ws => {
            const workspaceProgress = this._getProgressForWorkspace(ws);
            return Math.abs(workspaceProgress - progress);
        });
        const index = distances.indexOf(Math.min(...distances));
        return this._findWorkspaceInfoByIndex(index);
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

        const baseDistance = horiz ? global.screen_width : global.screen_height;

        let progress;
        let cancelProgress;
        if (this._switchData && this._switchData.gestureActivated) {
            this._switchData.container.remove_all_transitions();
            if (!horiz)
                progress = -this._switchData.container.y / baseDistance;
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                progress = this._switchData.container.x / baseDistance;
            else
                progress = -this._switchData.container.x / baseDistance;

            const ws = this._findClosestWorkspace(progress);
            cancelProgress = this._getProgressForWorkspace(ws);
        } else {
            this._prepareWorkspaceSwitch();

            const activeIndex = workspaceManager.get_active_workspace_index();
            const ws = this._findWorkspaceInfoByIndex(activeIndex);

            progress = cancelProgress = this._getProgressForWorkspace(ws);
        }

        const points = this._switchData.workspaces.map(this._getProgressForWorkspace);

        tracker.confirmSwipe(baseDistance, points, progress, cancelProgress);
    }

    _switchWorkspaceUpdate(tracker, progress) {
        if (!this._switchData)
            return;

        let xPos = 0;
        let yPos = 0;

        if (global.workspace_manager.layout_rows === -1)
            yPos = -Math.round(progress * global.screen_height);
        else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
            xPos = Math.round(progress * global.screen_width);
        else
            xPos = -Math.round(progress * global.screen_width);

        this._switchData.container.set_position(xPos, yPos);
    }

    _switchWorkspaceEnd(tracker, duration, endProgress) {
        if (!this._switchData)
            return;

        const newWs = this._findClosestWorkspace(endProgress);

        const switchData = this._switchData;
        switchData.gestureActivated = true;

        this._switchData.container.ease({
            x: -newWs.x,
            y: -newWs.y,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            onComplete: () => {
                if (!newWs.ws.active)
                    newWs.ws.activate(global.get_current_time());
                this._finishWorkspaceSwitch(switchData);
            },
        });
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
