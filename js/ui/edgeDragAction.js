// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported EdgeDragAction */

const { Clutter, GObject, Meta, St } = imports.gi;

const Main = imports.ui.main;

var EDGE_THRESHOLD = 20;
var DRAG_DISTANCE = 80;

var EdgeDragAction = GObject.registerClass({
    Signals: {
        'activated': {},
        'progress': { param_types: [GObject.TYPE_DOUBLE] },
    },
}, class EdgeDragAction extends Clutter.GestureAction {
    _init(side, allowedModes) {
        super._init();
        this._side = side;
        this._allowedModes = allowedModes;
        this.set_n_touch_points(1);
        this.set_threshold_trigger_edge(Clutter.GestureTriggerEdge.AFTER);
    }

    _getMonitorRect(x, y) {
        let rect = new Meta.Rectangle({ x: x - 1, y: y - 1, width: 1, height: 1 });
        let monitorIndex = global.display.get_monitor_index_for_rect(rect);

        return global.display.get_monitor_geometry(monitorIndex);
    }

    vfunc_gesture_prepare(_actor) {
        if (this.get_n_current_points() == 0)
            return false;

        if (!(this._allowedModes & Main.actionMode))
            return false;

        let [x, y] = this.get_press_coords(0);
        let monitorRect = this._getMonitorRect(x, y);

        return (this._side == St.Side.LEFT && x < monitorRect.x + EDGE_THRESHOLD) ||
                (this._side == St.Side.RIGHT && x > monitorRect.x + monitorRect.width - EDGE_THRESHOLD) ||
                (this._side == St.Side.TOP && y < monitorRect.y + EDGE_THRESHOLD) ||
                (this._side == St.Side.BOTTOM && y > monitorRect.y + monitorRect.height - EDGE_THRESHOLD);
    }

    vfunc_gesture_progress(_actor) {
        let [startX, startY] = this.get_press_coords(0);
        let [x, y] = this.get_motion_coords(0);
        let offsetX = Math.abs(x - startX);
        let offsetY = Math.abs(y - startY);

        if (offsetX < EDGE_THRESHOLD && offsetY < EDGE_THRESHOLD)
            return true;

        if ((offsetX > offsetY &&
             (this._side == St.Side.TOP || this._side == St.Side.BOTTOM)) ||
            (offsetY > offsetX &&
             (this._side == St.Side.LEFT || this._side == St.Side.RIGHT))) {
            this.cancel();
            return false;
        }

        if (this._side === St.Side.TOP ||
            this._side === St.Side.BOTTOM)
            this.emit('progress', offsetY);
        else
            this.emit('progress', offsetX);

        return true;
    }

    vfunc_gesture_end(_actor) {
        let [startX, startY] = this.get_press_coords(0);
        let [x, y] = this.get_motion_coords(0);
        let monitorRect = this._getMonitorRect(startX, startY);

        if ((this._side == St.Side.TOP && y > monitorRect.y + DRAG_DISTANCE) ||
            (this._side == St.Side.BOTTOM && y < monitorRect.y + monitorRect.height - DRAG_DISTANCE) ||
            (this._side == St.Side.LEFT && x > monitorRect.x + DRAG_DISTANCE) ||
            (this._side == St.Side.RIGHT && x < monitorRect.x + monitorRect.width - DRAG_DISTANCE))
            this.emit('activated');
        else
            this.cancel();
    }
});
