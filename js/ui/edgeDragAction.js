// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported EdgeDragAction */

const { Clutter, GObject, GLib, Meta, St } = imports.gi;

const Main = imports.ui.main;

var EDGE_THRESHOLD = 20;
var DRAG_DISTANCE = 80;
var CANCEL_THRESHOLD = 120;

var CANCEL_TIMEOUT = 300;

var EdgeDragAction = GObject.registerClass({
    Signals: { 'activated': {} },
}, class EdgeDragAction extends Clutter.GestureAction {
    _init(side, allowedModes) {
        super._init();
        this._side = side;
        this._allowedModes = allowedModes;
        this.set_n_touch_points(1);
        this.set_replay_events(true);
        this.set_max_replay_delay(320);
    }

    _getMonitorForPoint(x, y) {
        let rect = new Meta.Rectangle({ x: x - 1, y: y - 1, width: 1, height: 1 });
        let monitorIndex = global.display.get_monitor_index_for_rect(rect);

        return Main.layoutManager.monitors[monitorIndex];
    }

    _isNearMonitorEdge(x, y) {
        let monitor = this._getMonitorForPoint(x, y);

        switch (this._side) {
        case St.Side.LEFT:
            return x < monitor.x + EDGE_THRESHOLD;
        case St.Side.RIGHT:
            return x > monitor.x + monitor.width - EDGE_THRESHOLD;
        case St.Side.TOP:
            return y < monitor.y + EDGE_THRESHOLD;
        case St.Side.BOTTOM:
            return y > monitor.y + monitor.height - EDGE_THRESHOLD;
        default:
            return;
        }
    }

    _exceedsCancelThreshold(x, y) {
        let [startX, startY] = this.get_press_coords(0);
        let offsetX = Math.abs(x - startX);
        let offsetY = Math.abs(y - startY);

        switch (this._side) {
        case St.Side.LEFT:
        case St.Side.RIGHT:
            return offsetY > CANCEL_THRESHOLD;
        case St.Side.TOP:
        case St.Side.BOTTOM:
            return offsetX > CANCEL_THRESHOLD;
        default:
            return;
        }
    }

    _passesDistanceNeeded(x, y) {
        let [startX, startY] = this.get_press_coords(0);
        let monitor = this._getMonitorForPoint(startX, startY);

        switch (this._side) {
        case St.Side.LEFT:
            return x > monitor.x + DRAG_DISTANCE;
        case St.Side.RIGHT:
            return x < monitor.x + monitor.width - DRAG_DISTANCE;
        case St.Side.TOP:
            return y > monitor.y + DRAG_DISTANCE;
        case St.Side.BOTTOM:
            return y < monitor.y + monitor.height - DRAG_DISTANCE;
        default:
            return;
        }
    }


    vfunc_gesture_begin(_actor, _point) {
        if (!(this._allowedModes & Main.actionMode))
            return false;

        let [x, y] = this.get_press_coords(0);
        if (!this._isNearMonitorEdge(x, y))
            return false;

        this._successful = false;

        this._cancelTimeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, CANCEL_TIMEOUT,
            () => {
                let [x, y] = this.get_press_coords(0);
                if (this._isNearMonitorEdge(x, y))
                    this.cancel();

                this._cancelTimeoutId = 0;
                return GLib.SOURCE_REMOVE;
            });
        GLib.Source.set_name_by_id(this._cancelTimeoutId, '[gnome-shell] this.cancel');

        return true;
    }

    vfunc_gesture_progress(_actor, _point) {
        let [x, y] = this.get_motion_coords(0);

        if (this._exceedsCancelThreshold(x, y)) {
            if (this._cancelTimeoutId != 0) {
                GLib.source_remove(this._cancelTimeoutId);
                this._cancelTimeoutId = 0;
            }

            this.cancel();
        } else if (this._passesDistanceNeeded(x, y)) {
            if (this._cancelTimeoutId != 0) {
                GLib.source_remove(this._cancelTimeoutId);
                this._cancelTimeoutId = 0;
            }

            this._successful = true;
            this.emit('activated');
            this.end();
        }

    }

    vfunc_gesture_end(_actor, _point) {
        if (!this._successful)
            this.cancel();
    }
});
