// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Signals = imports.signals;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Clutter = imports.gi.Clutter;

let MOTION_THRESHOLD = 50;

const WorkspaceSwitchAction = new Lang.Class({
    Name: 'WorkspaceSwitchAction',
    Extends: Clutter.GestureAction,

    _init : function() {
        this.parent();
        this.set_n_touch_points (4);
        global.display.connect('grab-op-begin', Lang.bind(this, this.cancel));
        global.display.connect('grab-op-end', Lang.bind(this, this.cancel));
    },

    vfunc_gesture_prepare : function(action, actor) {
        return this.get_n_current_points() == this.get_n_touch_points();
    },

    vfunc_gesture_end : function(action, actor) {
        // Just check one touchpoint here
        let [startX, startY] = this.get_press_coords(0);
        let [x, y] = this.get_motion_coords(0);
        let offsetX = x - startX;
        let offsetY = y - startY;
        let direction;

        if (Math.abs(offsetX) < MOTION_THRESHOLD &&
            Math.abs(offsetY) < MOTION_THRESHOLD)
            return;

        if (Math.abs(offsetY) > Math.abs(offsetX)) {
            if (offsetY > 0)
                direction = Meta.MotionDirection.UP;
            else
                direction = Meta.MotionDirection.DOWN;
        } else {
            if (offsetX > 0)
                direction = Meta.MotionDirection.LEFT;
            else
                direction = Meta.MotionDirection.RIGHT;
        }

        this.emit('activated', direction);
    }
});
Signals.addSignalMethods(WorkspaceSwitchAction.prototype);
