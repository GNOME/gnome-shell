// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Meta, Shell } = imports.gi;

const Main = imports.ui.main;
const Tweener = imports.ui.tweener;
const SwipeTracker = imports.ui.swipeTracker;

var WINDOW_ANIMATION_TIME = 0.25;

var WorkspaceAnimationController = class {
    constructor() {
        this._shellwm = global.window_manager;
        this._movingWindow = null;

        this._switchData = null;
        this._shellwm.connect('kill-switch-workspace', (shellwm) => {
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

        let allowedModes = Shell.ActionMode.NORMAL;
        let swipeTracker = new SwipeTracker.SwipeTracker(global.stage, allowedModes, false, false);
        swipeTracker.connect('begin', this._switchWorkspaceBegin.bind(this));
        swipeTracker.connect('update', this._switchWorkspaceUpdate.bind(this));
        swipeTracker.connect('end', this._switchWorkspaceEnd.bind(this));
        swipeTracker.connect('cancel', this._switchWorkspaceCancel.bind(this));
        this._swipeTracker = swipeTracker;
    }

    _syncStacking() {
        if (this._switchData == null)
            return;

        let windows = global.get_window_actors();
        let lastCurSibling = null;
        let lastDirSibling = [];
        for (let i = 0; i < windows.length; i++) {
            if (windows[i].get_parent() == this._switchData.curGroup) {
                this._switchData.curGroup.set_child_above_sibling(windows[i], lastCurSibling);
                lastCurSibling = windows[i];
            } else {
                for (let dir of Object.values(Meta.MotionDirection)) {
                    let info = this._switchData.surroundings[dir];
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
        switchData.progress = 0;

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
            else if (dir == direction)
                ws = workspaceManager.get_workspace_by_index(to);

            if (ws == null || ws == curWs) {
                switchData.surroundings[dir] = null;
                continue;
            }

            let [x, y] = this._getPositionForDirection(dir, curWs, ws);
            let info = { index: ws.index(),
                         actor: new Clutter.Actor(),
                         xDest: x,
                         yDest: y };
            switchData.surroundings[dir] = info;
            switchData.container.add_actor(info.actor);
            info.actor.raise_top();

            info.actor.set_position(x, y);
        }

        switchData.movingWindowBin.raise_top();

        for (let i = 0; i < windows.length; i++) {
            let actor = windows[i];
            let window = actor.get_meta_window();

            if (!window.showing_on_its_workspace())
                continue;

            if (window.is_on_all_workspaces())
                continue;

            let record = { window: actor,
                           parent: actor.get_parent() };

            if (this.movingWindow && window == this.movingWindow) {
                switchData.movingWindow = record;
                switchData.windows.push(switchData.movingWindow);
                actor.reparent(switchData.movingWindowBin);
            } else if (window.get_workspace().index() == from) {
                switchData.windows.push(record);
                actor.reparent(switchData.curGroup);
            } else {
                let visible = false;
                for (let dir of Object.values(Meta.MotionDirection)) {
                    let info = switchData.surroundings[dir];

                    if (!info || info.index != window.get_workspace().index())
                        continue;

                    switchData.windows.push(record);
                    actor.reparent(info.actor);
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
            w.window.reparent(w.parent);

            if (w.window.get_meta_window().get_workspace() !=
                global.workspace_manager.get_active_workspace())
                w.window.hide();
        }
        Tweener.removeTweens(switchData);
        Tweener.removeTweens(switchData.container);
        switchData.container.destroy();
        switchData.movingWindowBin.destroy();

        this.movingWindow = null;
    }

    animateSwitchWorkspace(shellwm, from, to, direction, callback) {
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

        Tweener.addTween(this._switchData,
                         { progress: direction == Meta.MotionDirection.DOWN ? 1 : -1,
                           time: WINDOW_ANIMATION_TIME,
                           transition: 'easeOutCubic'
                         });
        Tweener.addTween(this._switchData.container,
                         { x: xDest,
                           y: yDest,
                           time: WINDOW_ANIMATION_TIME,
                           transition: 'easeOutCubic',
                           onComplete: () => { this._switchWorkspaceDone(shellwm); }
                         });
    }

    _switchWorkspaceDone(shellwm) {
        this._finishWorkspaceSwitch(this._switchData);
        shellwm.completed_switch_workspace();
    }

    _switchWorkspaceBegin(tracker, monitor) {
        if (Meta.prefs_get_workspaces_only_on_primary() && monitor != Main.layoutManager.primaryIndex)
            return;

        let workspaceManager = global.workspace_manager;
        let activeWorkspace = workspaceManager.get_active_workspace();

        if (this._switchData && this._switchData.gestureActivated) {
            Tweener.removeTweens(this._switchData);
            Tweener.removeTweens(this._switchData.container);

            tracker.continueSwipe(this._switchData.progress);
            return;
        }

        this._prepareWorkspaceSwitch(activeWorkspace.index(), -1);

        // TODO: horizontal

        let baseDistance = global.screen_height - Main.panel.height;

        let direction = Meta.MotionDirection.DOWN;
        let backInfo = this._switchData.surroundings[direction];
        let backExtent = backInfo ? (backInfo.yDest - baseDistance) : 0;

        direction = Meta.MotionDirection.UP;
        let forwardInfo = this._switchData.surroundings[direction];
        let forwardExtent = forwardInfo ? (-forwardInfo.yDest - baseDistance) : 0;

        tracker.confirmSwipe((backInfo != null), (forwardInfo != null), baseDistance, backExtent, forwardExtent);
    }

    _switchWorkspaceUpdate(tracker, progress) {
        if (!this._switchData)
            return;

        this._switchData.progress = progress;

        let direction = (progress > 0) ? Meta.MotionDirection.DOWN : Meta.MotionDirection.UP;
        let info = this._switchData.surroundings[direction];
        let distance = info ? Math.abs(info.yDest) : 0;

        this._switchData.container.set_position(0, Math.round(-progress * distance));
    }

    _switchWorkspaceEnd(tracker, duration, isBack) {
        if (!this._switchData)
            return;

        let direction = isBack ? Meta.MotionDirection.DOWN : Meta.MotionDirection.UP;

        let workspaceManager = global.workspace_manager;
        let activeWorkspace = workspaceManager.get_active_workspace();
        let newWs = activeWorkspace.get_neighbor(direction);

        if (newWs == activeWorkspace) {
            // FIXME: throw an error
            log('this should never happen')
            return;
        }

        let xDest = -this._switchData.surroundings[direction].xDest;
        let yDest = -this._switchData.surroundings[direction].yDest;

        let switchData = this._switchData;
        this._switchData.gestureActivated = true;

        Tweener.addTween(switchData,
                         { progress: direction == Meta.MotionDirection.DOWN ? 1 : -1,
                           time: duration,
                           transition: 'easeOutCubic'
                         });
        Tweener.addTween(switchData.container,
                         { x: xDest,
                           y: yDest,
                           time: duration,
                           transition: 'easeOutCubic',
                           onComplete: () => {
                               newWs.activate(global.get_current_time());
                               this._finishWorkspaceSwitch(switchData);
                           }
                         });
    }

    _switchWorkspaceCancel(tracker, duration) {
        if (!this._switchData)
            return;

        if (duration == 0) {
            this._switchWorkspaceStop();
            return;
        }

        let switchData = this._switchData;

        Tweener.addTween(switchData,
                         { progress: 0,
                           time: duration,
                           transition: 'easeOutCubic'
                         });
        Tweener.addTween(switchData.container,
                         { x: 0,
                           y: 0,
                           time: duration,
                           transition: 'easeOutCubic',
                           onComplete: this._finishWorkspaceSwitch,
                           onCompleteScope: this,
                           onCompleteParams: [switchData],
                         });
    }

    _switchWorkspaceStop() {
        this._switchData.progress = 0;
        this._switchData.container.x = 0;
        this._switchData.container.y = 0;
        this._finishWorkspaceSwitch(this._switchData);
    }

    isAnimating() {
        return this._switchData != null;
    }

    set movingWindow(movingWindow) {
        this._movingWindow = movingWindow;
    }

    get movingWindow() {
        return this._movingWindow;
    }
};
