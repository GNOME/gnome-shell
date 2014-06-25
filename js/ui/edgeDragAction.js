// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Signals = imports.signals;
const Meta = imports.gi.Meta;
const Clutter = imports.gi.Clutter;
const St = imports.gi.St;

let EDGE_THRESHOLD = 20;
let DRAG_DISTANCE = 80;

const EdgeDragAction = new Lang.Class({
    Name: 'EdgeDragAction',
    Extends: Clutter.GestureAction,

    _init : function(side) {
        this.parent();
        this._side = side;
        this.set_n_touch_points (1);
        global.display.connect('grab-op-begin', Lang.bind(this, this.cancel));
        global.display.connect('grab-op-end', Lang.bind(this, this.cancel));
    },

    _getMonitorRect : function (x, y) {
        let rect = new Meta.Rectangle({ x: x - 1, y: y - 1, width: 1, height: 1 });
        let monitorIndex = global.screen.get_monitor_index_for_rect(rect);

        return global.screen.get_monitor_geometry(monitorIndex);
    },

    vfunc_gesture_prepare : function(action, actor) {
        if (this.get_n_current_points() == 0)
            return false;

        let [x, y] = this.get_press_coords(0);
        let monitorRect = this._getMonitorRect(x, y);

        return ((this._side == St.Side.LEFT && x < monitorRect.x + EDGE_THRESHOLD) ||
                (this._side == St.Side.RIGHT && x > monitorRect.x + monitorRect.width - EDGE_THRESHOLD) ||
                (this._side == St.Side.TOP && y < monitorRect.y + EDGE_THRESHOLD) ||
                (this._side == St.Side.BOTTOM && y > monitorRect.y + monitorRect.height - EDGE_THRESHOLD));
    },

    vfunc_gesture_progress : function (action, actor) {
        let [startX, startY] = this.get_press_coords(0);
        let [x, y] = this.get_motion_coords(0);
        let offsetX = Math.abs (x - startX);
        let offsetY = Math.abs (y - startY);

        if (offsetX < EDGE_THRESHOLD && offsetY < EDGE_THRESHOLD)
            return true;

        if ((offsetX > offsetY &&
             (this._side == St.Side.TOP || this._side == St.Side.BOTTOM)) ||
            (offsetY > offsetX &&
             (this._side == St.Side.LEFT || this._side == St.Side.RIGHT))) {
            this.cancel();
            return false;
        }

        return true;
    },

    vfunc_gesture_end : function (action, actor) {
        let [startX, startY] = this.get_press_coords(0);
        let [x, y] = this.get_motion_coords(0);
        let monitorRect = this._getMonitorRect(startX, startY);

        if ((this._side == St.Side.TOP && y > monitorRect.y + DRAG_DISTANCE) ||
            (this._side == St.Side.BOTTOM && y < monitorRect.y + monitorRect.height - DRAG_DISTANCE) ||
            (this._side == St.Side.LEFT && x > monitorRect.x + DRAG_DISTANCE) ||
            (this._side == St.Side.RIGHT && x < monitorRect.x + monitorRect.width - DRAG_DISTANCE))
            this.emit('activated');
    }
});
Signals.addSignalMethods(EdgeDragAction.prototype);
