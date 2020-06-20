// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceAnimationController */

const { Clutter, GObject, Meta, Shell } = imports.gi;

const Main = imports.ui.main;
const SwipeTracker = imports.ui.swipeTracker;

const WINDOW_ANIMATION_TIME = 250;

const WorkspaceGroup = GObject.registerClass(
class WorkspaceGroup extends Clutter.Actor {
    _init(controller, workspace) {
        super._init();

        this._controller = controller;
        this._workspace = workspace;
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
                x: window.x,
                y: window.y,
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

    _prepareWorkspaceSwitch() {
        if (this._switchData)
            return;

        let wgroup = global.window_group;
        let workspaceManager = global.workspace_manager;
        let vertical = workspaceManager.layout_rows === -1;
        let nWorkspaces = workspaceManager.get_n_workspaces();

        let switchData = {};

        this._switchData = switchData;
        switchData.movingWindowBin = new Clutter.Actor();
        switchData.movingWindow = null;
        switchData.workspaces = [];
        switchData.gestureActivated = false;
        switchData.inProgress = false;

        switchData.container = new Clutter.Actor();

        wgroup.add_actor(switchData.movingWindowBin);
        wgroup.add_actor(switchData.container);

        let x = 0;
        let y = 0;

        for (let i = 0; i < nWorkspaces; i++) {
            let ws = workspaceManager.get_workspace_by_index(i);
            let fullscreen = ws.list_windows().some(w => w.is_fullscreen());

            if (i > 0 && vertical && !fullscreen) {
                // We have to shift windows up or down by the height of the panel to prevent having a
                // visible gap between the windows while switching workspaces. Since fullscreen windows
                // hide the panel, they don't need to be shifted up or down.
                y -= Main.panel.height;
            }

            let info = {
                actor: new WorkspaceGroup(this, ws),
                fullscreen,
                x,
                y,
            };

            switchData.workspaces[i] = info;
            switchData.container.add_actor(info.actor);
            switchData.container.set_child_above_sibling(info.actor, null);
            info.actor.set_position(x, y);

            if (vertical)
                y += global.screen_height;
            else
                x += global.screen_width;
        }

        wgroup.set_child_above_sibling(switchData.movingWindowBin, null);

        if (this.movingWindow) {
            let actor = this.movingWindow.get_compositor_private();

            switchData.movingWindow = {
                window: actor,
                parent: actor.get_parent(),
            };

            switchData.movingWindow.parent.remove_child(actor);
            switchData.movingWindowBin.add_child(actor);
            switchData.movingWindow.windowDestroyId = actor.connect('destroy', () => {
                switchData.movingWindow = null;
            });
        }
    }

    _finishWorkspaceSwitch(switchData) {
        this._switchData = null;

        if (switchData.movingWindow) {
            let record = switchData.movingWindow;
            record.window.disconnect(record.windowDestroyId);
            record.window.get_parent().remove_child(record.window);
            record.parent.add_child(record.window);

            switchData.movingWindow = null;
        }

        switchData.container.destroy();
        switchData.movingWindowBin.destroy();

        this.movingWindow = null;
    }

    animateSwitchWorkspace(from, to, direction, onComplete) {
        this._prepareWorkspaceSwitch();
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

    _switchWorkspaceBegin(tracker, monitor) {
        if (Meta.prefs_get_workspaces_only_on_primary() &&
            monitor !== Main.layoutManager.primaryIndex)
            return;

        let workspaceManager = global.workspace_manager;
        let horiz = workspaceManager.layout_rows !== -1;
        tracker.orientation = horiz
            ? Clutter.Orientation.HORIZONTAL
            : Clutter.Orientation.VERTICAL;

        let baseDistance = horiz ? global.screen_width : global.screen_width;

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
            this._prepareWorkspaceSwitch();

            let activeIndex = workspaceManager.get_active_index();
            let ws = this._switchData.workspaces[activeIndex];

            if (!horiz)
                progress = -ws.y / baseDistance;
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                progress = ws.x / baseDistance;
            else
                progress = -ws.x / baseDistance;
        }

        let points = [];
        for (let ws of this._switchData.workspaces) {
            if (!horiz)
                points.push(-ws.y / baseDistance);
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                points.push(ws.x / baseDistance);
            else
                points.push(-ws.x / baseDistance);
        }

        tracker.confirmSwipe(baseDistance, points, progress, 0);
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

        let workspaceManager = global.workspace_manager;
        let activeWorkspace = workspaceManager.get_active_workspace();
        let newWs = activeWorkspace;
        let xDest = 0;
        let yDest = 0;
        if (endProgress !== 0) {
            if (global.workspace_manager.layout_rows === -1)
                yDest = -Math.round(endProgress * global.screen_height);
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                xDest = Math.round(endProgress * global.screen_width);
            else
                xDest = -Math.round(endProgress * global.screen_width);
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
