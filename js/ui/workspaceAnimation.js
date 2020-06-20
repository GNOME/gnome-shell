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
                x: window.x,
                y: window.y,
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
    _init(movingWindow) {
        super._init();

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
                x: window.x,
                y: window.y,
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
        switchData.stickyGroup = new StickyGroup(this.movingWindow);
        switchData.workspaces = [];
        switchData.gestureActivated = false;
        switchData.inProgress = false;

        switchData.container = new Clutter.Actor();

        wgroup.add_child(switchData.stickyGroup);
        wgroup.add_child(switchData.container);

        let x = 0;
        let y = 0;

        if (!workspaces) {
            workspaces = [];

            for (let i = 0; i < nWorkspaces; i++)
                workspaces.push(i);
        }

        for (let i of workspaces) {
            let ws = workspaceManager.get_workspace_by_index(i);
            let fullscreen = ws.list_windows().some(w => w.is_fullscreen());

            if (y > 0 && vertical && !fullscreen) {
                // We have to shift windows up or down by the height of the panel to prevent having a
                // visible gap between the windows while switching workspaces. Since fullscreen windows
                // hide the panel, they don't need to be shifted up or down.
                y -= Main.panel.height;
            }

            let info = {
                ws,
                actor: new WorkspaceGroup(ws, this.movingWindow),
                fullscreen,
                x,
                y,
            };

            if (vertical)
                info.position = info.y / global.screen_height;
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                info.position = -info.x / global.screen_width;
            else
                info.position = info.x / global.screen_width;

            switchData.workspaces[i] = info;
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

        if (global.workspace_manager.layout_rows === -1)
            switchData.container.y = -switchData.workspaces[activeWorkspaceIndex].y;
        else
            switchData.container.x = -switchData.workspaces[activeWorkspaceIndex].x;

        wgroup.set_child_above_sibling(switchData.stickyGroup, null);
    }

    _finishWorkspaceSwitch(switchData) {
        this._switchData = null;

        switchData.container.destroy();
        switchData.stickyGroup.destroy();

        this.movingWindow = null;
    }

    animateSwitchWorkspace(from, to, direction, onComplete) {
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

        let fromWs = this._switchData.workspaces[from];
        let toWs = this._switchData.workspaces[to];

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

    _findClosestWorkspace(position) {
        const distances = this._switchData.workspaces.map(ws => Math.abs(ws.position - position));
        const index = distances.indexOf(Math.min(...distances));
        return this._switchData.workspaces[index];
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

        let baseDistance = horiz ? global.screen_width : global.screen_height;

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

            cancelProgress = this._findClosestWorkspace(progress).position;
        } else {
            this._prepareWorkspaceSwitch();

            let activeIndex = workspaceManager.get_active_workspace_index();
            let ws = this._switchData.workspaces[activeIndex];

            progress = cancelProgress = ws.position;
        }

        let points = this._switchData.workspaces.map(ws => ws.position);

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

        let newWs = this._findClosestWorkspace(endProgress);

        let switchData = this._switchData;
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
