// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

import Clutter from 'gi://Clutter';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';

import * as Background from './background.js';
import * as Layout from './layout.js';
import * as SwipeTracker from './swipeTracker.js';
import * as Util from '../misc/util.js';

import * as Main from './main.js';

const WINDOW_ANIMATION_TIME = 250;

export const WorkspaceGroup = GObject.registerClass(
class WorkspaceGroup extends Shell.WorkspaceGroup {
    _init(workspace, monitor, movingWindow) {
        super._init({
            width: monitor.width,
            height: monitor.height,
            clip_to_allocation: true,
            workspace: workspace,
        });

        this._monitor = monitor;
        this._movingWindow = movingWindow;
        this._windowRecords = [];

        if (workspace) {
            this._background = new Meta.BackgroundGroup();

            this.add_actor(this._background);

            this._bgManager = new Background.BackgroundManager({
                container: this._background,
                monitorIndex: this._monitor.index,
                controlPosition: false,
            });
            this._createDesktopWindows();
        }

        this._createWindows();

        this.connect('destroy', this._onDestroy.bind(this));
        global.display.connectObject('restacked',
            this._syncStacking.bind(this), this);
    }

    _shouldShowWindow(window) {
        if (!window.showing_on_its_workspace() || this._isDesktopWindow(window))
            return false;

        if (!this._windowIsOnThisMonitor(window))
            return false;

        const isSticky =
            window.is_on_all_workspaces() || window === this._movingWindow;

        // No workspace means we should show windows that are on all workspaces
        if (!this.workspace)
            return isSticky;

        // Otherwise only show windows that are (only) on that workspace
        return !isSticky && window.located_on_workspace(this.workspace);
    }

    _syncStacking() {
        const windowActors = global.get_window_actors().filter(w =>
            this._shouldShowWindow(w.meta_window));

        let lastRecord;
        const bottomActor = this._background ?? null;

        for (const windowActor of windowActors) {
            const record = this._windowRecords.find(r => r.windowActor === windowActor);

            this.set_child_above_sibling(record.clone,
                lastRecord ? lastRecord.clone : bottomActor);
            lastRecord = record;
        }
    }

    _isDesktopWindow(metaWindow) {
        return metaWindow.get_window_type() === Meta.WindowType.DESKTOP;
    }

    _windowIsOnThisMonitor(metawindow) {
        const geometry = global.display.get_monitor_geometry(this._monitor.index);
        const [intersects] = metawindow.get_frame_rect().intersect(geometry);
        return intersects;
    }

    _createDesktopWindows() {
        const desktopActors = global.get_window_actors().filter(w => {
            return this._isDesktopWindow(w.meta_window) && this._windowIsOnThisMonitor(w.meta_window);
        });
        desktopActors.map(a => this._createClone(a)).forEach(clone => this._background.add_child(clone));
    }

    _createWindows() {
        const windowActors = global.get_window_actors().filter(w =>
            this._shouldShowWindow(w.meta_window));

        windowActors.map(a => this._createClone(a)).forEach(clone => this.add_child(clone));
    }

    _createClone(windowActor) {
        const clone = new Clutter.Clone({
            source: windowActor,
            x: windowActor.x - this._monitor.x,
            y: windowActor.y - this._monitor.y,
        });

        const record = {windowActor, clone};

        windowActor.connectObject('destroy', () => {
            clone.destroy();
            this._windowRecords.splice(this._windowRecords.indexOf(record), 1);
        }, this);

        this._windowRecords.push(record);
        return clone;
    }

    _removeWindows() {
        for (const record of this._windowRecords)
            record.clone.destroy();

        this._windowRecords = [];
    }

    _onDestroy() {
        this._removeWindows();

        if (this.workspace)
            this._bgManager.destroy();
    }
});

export const MonitorGroup = GObject.registerClass(
class MonitorGroup extends Shell.MonitorGroup {
    _init(monitor, workspaceIndices, movingWindow) {
        super._init({
            clip_to_allocation: true,
            style_class: 'workspace-animation',
            index: monitor.index,
            monitor_width: monitor.width,
            monitor_height: monitor.height,
        });

        const constraint = new Layout.MonitorConstraint({index: monitor.index});
        this.add_constraint(constraint);

        const stickyGroup = new WorkspaceGroup(null, monitor, movingWindow);
        this.add_child(stickyGroup);

        const workspaceManager = global.workspace_manager;
        const vertical = workspaceManager.layout_rows === -1;
        const activeWorkspace = workspaceManager.get_active_workspace();

        let x = 0;
        let y = 0;

        for (const i of workspaceIndices) {
            const ws = workspaceManager.get_workspace_by_index(i);
            const fullscreen = ws.list_windows().some(w => w.get_monitor() === monitor.index && w.is_fullscreen());

            if (i > 0 && vertical && !fullscreen && monitor.index === Main.layoutManager.primaryIndex) {
                // We have to shift windows up or down by the height of the panel to prevent having a
                // visible gap between the windows while switching workspaces. Since fullscreen windows
                // hide the panel, they don't need to be shifted up or down.
                y -= Main.panel.height;
            }

            this.add_group(new WorkspaceGroup(ws, monitor, movingWindow), x, y);

            if (vertical)
                y += this.base_distance;
            else if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
                x -= this.base_distance;
            else
                x += this.base_distance;
        }

        this.progress = this.get_workspace_progress(activeWorkspace);

        if (monitor.index === Main.layoutManager.primaryIndex) {
            this._workspacesAdjustment = Main.createWorkspacesAdjustment(this);
            this.bind_property_full('progress',
                this._workspacesAdjustment, 'value',
                GObject.BindingFlags.SYNC_CREATE,
                (bind, source) => {
                    const indices = [
                        workspaceIndices[Math.floor(source)],
                        workspaceIndices[Math.ceil(source)],
                    ];
                    return [true, Util.lerp(...indices, source % 1.0)];
                },
                null);

            this.connect('destroy', () => {
                delete this._workspacesAdjustment;
            });
        }
    }
});

export class WorkspaceAnimationController {
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
            Clutter.Orientation.HORIZONTAL,
            Shell.ActionMode.NORMAL,
            {allowDrag: false});
        swipeTracker.connect('begin', this._switchWorkspaceBegin.bind(this));
        swipeTracker.connect('update', this._switchWorkspaceUpdate.bind(this));
        swipeTracker.connect('end', this._switchWorkspaceEnd.bind(this));
        this._swipeTracker = swipeTracker;

        global.display.bind_property('compositor-modifiers',
            this._swipeTracker, 'scroll-modifiers',
            GObject.BindingFlags.SYNC_CREATE);
    }

    _prepareWorkspaceSwitch(workspaceIndices) {
        if (this._switchData)
            return;

        const workspaceManager = global.workspace_manager;
        const nWorkspaces = workspaceManager.get_n_workspaces();

        const switchData = {};

        this._switchData = switchData;
        switchData.monitors = [];

        switchData.gestureActivated = false;
        switchData.inProgress = false;

        if (!workspaceIndices)
            workspaceIndices = [...Array(nWorkspaces).keys()];

        const monitors = Meta.prefs_get_workspaces_only_on_primary()
            ? [Main.layoutManager.primaryMonitor] : Main.layoutManager.monitors;

        for (const monitor of monitors) {
            if (Meta.prefs_get_workspaces_only_on_primary() &&
                monitor.index !== Main.layoutManager.primaryIndex)
                continue;

            const group = new MonitorGroup(monitor, workspaceIndices, this.movingWindow);

            Main.uiGroup.insert_child_above(group, global.window_group);

            switchData.monitors.push(group);
        }

        Meta.disable_unredirect_for_display(global.display);
    }

    _finishWorkspaceSwitch(switchData) {
        Meta.enable_unredirect_for_display(global.display);

        this._switchData = null;

        switchData.monitors.forEach(m => m.destroy());

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

        const fromWs = global.workspace_manager.get_workspace_by_index(from);
        const toWs = global.workspace_manager.get_workspace_by_index(to);

        for (const monitorGroup of this._switchData.monitors) {
            monitorGroup.progress = monitorGroup.get_workspace_progress(fromWs);
            const progress = monitorGroup.get_workspace_progress(toWs);

            const params = {
                duration: WINDOW_ANIMATION_TIME,
                mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            };

            if (monitorGroup.index === Main.layoutManager.primaryIndex) {
                params.onComplete = () => {
                    this._finishWorkspaceSwitch(this._switchData);
                    onComplete();
                    this._swipeTracker.enabled = true;
                };
            }

            monitorGroup.ease_property('progress', progress, params);
        }
    }

    canHandleScrollEvent(event) {
        return this._swipeTracker.canHandleScrollEvent(event);
    }

    _findMonitorGroup(monitorIndex) {
        return this._switchData.monitors.find(m => m.index === monitorIndex);
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

        if (this._switchData && this._switchData.gestureActivated) {
            for (const group of this._switchData.monitors)
                group.remove_all_transitions();
        } else {
            this._prepareWorkspaceSwitch();
        }

        const monitorGroup = this._findMonitorGroup(monitor);
        const baseDistance = monitorGroup.base_distance;
        const progress = monitorGroup.progress;

        const closestWs = monitorGroup.find_closest_workspace(progress);
        const cancelProgress = monitorGroup.get_workspace_progress(closestWs);
        const points = monitorGroup.get_snap_points();

        this._switchData.baseMonitorGroup = monitorGroup;

        tracker.confirmSwipe(baseDistance, points, progress, cancelProgress);
    }

    _switchWorkspaceUpdate(tracker, progress) {
        if (!this._switchData)
            return;

        for (const monitorGroup of this._switchData.monitors)
            monitorGroup.update_swipe_for_monitor(progress, this._switchData.baseMonitorGroup);
    }

    _switchWorkspaceEnd(tracker, duration, endProgress) {
        if (!this._switchData)
            return;

        const switchData = this._switchData;
        switchData.gestureActivated = true;

        const newWs = switchData.baseMonitorGroup.find_closest_workspace(endProgress);
        const endTime = Clutter.get_current_event_time();

        for (const monitorGroup of this._switchData.monitors) {
            const progress = monitorGroup.get_workspace_progress(newWs);

            const params = {
                duration,
                mode: Clutter.AnimationMode.EASE_OUT_CUBIC,
            };

            if (monitorGroup.index === Main.layoutManager.primaryIndex) {
                params.onComplete = () => {
                    if (!newWs.active)
                        newWs.activate(endTime);
                    this._finishWorkspaceSwitch(switchData);
                };
            }

            monitorGroup.ease_property('progress', progress, params);
        }
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
}
