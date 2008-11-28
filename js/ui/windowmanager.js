/* -*- mode: js2; js2-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Tweener = imports.tweener.tweener;

const Main = imports.ui.main;

const SWITCH_ANIMATION_TIME = 0.5;

function WindowManager() {
    this._init();
}

WindowManager.prototype = {
_init : function() {
    let me = this;

    this._global = Shell.Global.get();
    this._shellwm = this._global.window_manager;

    this._switchData = null;
    this._shellwm.connect('switch-workspace',
            function(o, from, to, direction) {
        let actors = me._shellwm.get_switch_workspace_actors();
        me.switchWorkspace(actors, from, to, direction);
    });
    this._shellwm.connect('kill-switch-workspace',
            function(o) {
        me.switchWorkspaceDone();
    });
},

switchWorkspace : function(windows, from, to, direction) {
    /* @direction is the direction that the "camera" moves, so the
     * screen contents have to move one screen's worth in the
     * opposite direction.
     */
    let xDest = 0, yDest = 0;

    if (direction == Meta.MotionDirection.UP ||
            direction == Meta.MotionDirection.UP_LEFT ||
            direction == Meta.MotionDirection.UP_RIGHT)
        yDest = this._global.screen_height;
    else if (direction == Meta.MotionDirection.DOWN ||
            direction == Meta.MotionDirection.DOWN_LEFT ||
            direction == Meta.MotionDirection.DOWN_RIGHT)
        yDest = -this._global.screen_height;

    if (direction == Meta.MotionDirection.LEFT ||
            direction == Meta.MotionDirection.UP_LEFT ||
            direction == Meta.MotionDirection.DOWN_LEFT)
        xDest = this._global.screen_width;
    else if (direction == Meta.MotionDirection.RIGHT ||
            direction == Meta.MotionDirection.UP_RIGHT ||
            direction == Meta.MotionDirection.DOWN_RIGHT)
        xDest = -this._global.screen_width;

    let switchData = {};
    this._switchData = switchData;
    switchData.inGroup = new Clutter.Group();
    switchData.outGroup = new Clutter.Group();
    switchData.windows = [];

    let wgroup = this._global.window_group;
    wgroup.add_actor(switchData.inGroup);
    wgroup.add_actor(switchData.outGroup);

    for (let i = 0; i < windows.length; i++) {
        let window = windows[i];
        if (window.get_workspace() == from) {
            switchData.windows.push({ window: window,
                parent: window.get_parent() });
            window.reparent(switchData.outGroup);
        } else if (window.get_workspace() == to) {
            switchData.windows.push({ window: window,
                parent: window.get_parent() });
            window.reparent(switchData.inGroup);
            window.show_all();
        }
    }

    switchData.inGroup.set_position(-xDest, -yDest);
    switchData.inGroup.raise_top();

    Tweener.addTween(switchData.outGroup,
            { x: xDest,
        y: yDest,
        time: SWITCH_ANIMATION_TIME,
        transition: "easeOutBack",
        onComplete: this.switchWorkspaceDone
            });
    Tweener.addTween(switchData.inGroup,
            { x: 0,
        y: 0,
        time: SWITCH_ANIMATION_TIME,
        transition: "easeOutBack"
            });
},

switchWorkspaceDone : function() {
    let switchData = this._switchData;
    if (!switchData)
        return;
    this._switchData = null;

    for (let i = 0; i < switchData.windows.length; i++) {
        let w = switchData.windows[i];
        if (w.window.get_parent() == switchData.outGroup) {
            w.window.reparent(w.parent);
            w.window.hide();
        } else
            w.window.reparent(w.parent);
    }
    Tweener.removeTweens(switchData.inGroup);
    Tweener.removeTweens(switchData.outGroup);
    switchData.inGroup.destroy();
    switchData.outGroup.destroy();

    this._shellwm.completed_switch_workspace();
}

};
