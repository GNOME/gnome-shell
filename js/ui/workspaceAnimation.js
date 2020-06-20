// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceAnimationController */

const { Clutter, GObject, Meta, Shell } = imports.gi;

const Background = imports.ui.background;
const Layout = imports.ui.layout;
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

    get workspace() {
        return this._workspace;
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

const MonitorGroup = GObject.registerClass(
class MonitorGroup extends Clutter.Actor {
    _init(monitor) {
        super._init();

        const constraint = new Layout.MonitorConstraint({ index: monitor.index });
        this.add_constraint(constraint);

        const background = new Meta.BackgroundGroup();

        this.add_child(background);

        this._bgManager = new Background.BackgroundManager({
            container: background,
            monitorIndex: monitor.index,
            controlPosition: false,
        });

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onDestroy() {
        this._bgManager.destroy();
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
            Shell.ActionMode.NORMAL, { allowDrag: false, allowScroll: false });
        swipeTracker.connect('begin', this._switchWorkspaceBegin.bind(this));
        swipeTracker.connect('update', this._switchWorkspaceUpdate.bind(this));
        swipeTracker.connect('end', this._switchWorkspaceEnd.bind(this));
        this._swipeTracker = swipeTracker;
    }

    _prepareWorkspaceSwitch(workspaceIndices) {
        if (this._switchData)
            return;

        const workspaceManager = global.workspace_manager;
        const vertical = workspaceManager.layout_rows === -1;
        const nWorkspaces = workspaceManager.get_n_workspaces();
        const activeWorkspaceIndex = workspaceManager.get_active_workspace_index();

        const switchData = {};

        this._switchData = switchData;
        switchData.stickyGroup = new WorkspaceGroup(null, this.movingWindow);
        switchData.workspaceGroups = [];
        switchData.gestureActivated = false;
        switchData.inProgress = false;

        switchData.container = new Clutter.Actor();
        switchData.backgroundGroup = new Clutter.Actor();

        for (const monitor of Main.layoutManager.monitors)
            switchData.backgroundGroup.add_child(new MonitorGroup(monitor));

        Main.uiGroup.insert_child_above(switchData.backgroundGroup, global.window_group);
        Main.uiGroup.insert_child_above(switchData.container, switchData.backgroundGroup);
        Main.uiGroup.insert_child_above(switchData.stickyGroup, switchData.container);

        let x = 0;
        let y = 0;

        if (!workspaceIndices)
            workspaceIndices = [...Array(nWorkspaces).keys()];

        for (const i of workspaceIndices) {
            const ws = workspaceManager.get_workspace_by_index(i);
            const fullscreen = ws.list_windows().some(w => w.is_fullscreen());

            if (y > 0 && vertical && !fullscreen) {
                // We have to shift windows up or down by the height of the panel to prevent having a
                // visible gap between the windows while switching workspaces. Since fullscreen windows
                // hide the panel, they don't need to be shifted up or down.
                y -= Main.panel.height;
            }

            const group = new WorkspaceGroup(ws, this.movingWindow);

            switchData.workspaceGroups.push(group);
            switchData.container.add_child(group);
            switchData.container.set_child_above_sibling(group, null);
            group.set_position(x, y);

            if (vertical)
                y += global.screen_height;
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                x -= global.screen_width;
            else
                x += global.screen_width;
        }

        const activeGroup = this._findWorkspaceGroupByIndex(activeWorkspaceIndex);
        if (vertical)
            switchData.container.y = -activeGroup.y;
        else
            switchData.container.x = -activeGroup.x;
    }

    _finishWorkspaceSwitch(switchData) {
        this._switchData = null;

        switchData.backgroundGroup.destroy();
        switchData.container.destroy();
        switchData.stickyGroup.destroy();

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

        const fromGroup = this._findWorkspaceGroupByIndex(from);
        const toGroup = this._findWorkspaceGroupByIndex(to);

        this._switchData.container.x = -fromGroup.x;
        this._switchData.container.y = -fromGroup.y;

        this._switchData.container.ease({
            x: -toGroup.x,
            y: -toGroup.y,
            duration: WINDOW_ANIMATION_TIME,
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            onComplete: () => {
                this._finishWorkspaceSwitch(this._switchData);
                onComplete();
                this._swipeTracker.enabled = true;
            },
        });
    }

    _getProgressForWorkspace(workspaceGroup) {
        if (global.workspace_manager.layout_rows === -1)
            return workspaceGroup.y / global.screen_height;
        else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
            return -workspaceGroup.x / global.screen_width;
        else
            return workspaceGroup.x / global.screen_width;
    }

    _findWorkspaceGroupByIndex(index) {
        return this._switchData.workspaceGroups.find(g => g.workspace.index() === index);
    }

    _findClosestWorkspaceGroup(progress) {
        const distances = this._switchData.workspaceGroups.map(g => {
            const workspaceProgress = this._getProgressForWorkspace(g);
            return Math.abs(workspaceProgress - progress);
        });
        const index = distances.indexOf(Math.min(...distances));
        return this._switchData.workspaceGroups[index];
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

            const wsGroup = this._findClosestWorkspaceGroup(progress);
            cancelProgress = this._getProgressForWorkspaceGroup(wsGroup);
        } else {
            this._prepareWorkspaceSwitch();

            const activeIndex = workspaceManager.get_active_workspace_index();
            const wsGroup = this._findWorkspaceGroupByIndex(activeIndex);

            progress = cancelProgress = this._getProgressForWorkspace(wsGroup);
        }

        const points = this._switchData.workspaceGroups.map(this._getProgressForWorkspace);

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

        const newGroup = this._findClosestWorkspaceGroup(endProgress);

        const switchData = this._switchData;
        switchData.gestureActivated = true;

        this._switchData.container.ease({
            x: -newGroup.x,
            y: -newGroup.y,
            duration,
            mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            onComplete: () => {
                if (!newGroup.workspace.active)
                    newGroup.workspace.activate(global.get_current_time());
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
