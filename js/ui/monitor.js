// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, GObject } = imports.gi;

const Main = imports.ui.main;
const Params = imports.misc.params;

var MonitorConstraint = GObject.registerClass({
    Properties: {
        'primary': GObject.ParamSpec.boolean('primary',
                                             'Primary', 'Track primary monitor',
                                             GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE,
                                             false),
        'index': GObject.ParamSpec.int('index',
                                       'Monitor index', 'Track specific monitor',
                                       GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE,
                                       -1, 64, -1),
        'work-area': GObject.ParamSpec.boolean('work-area',
                                               'Work-area', 'Track monitor\'s work-area',
                                               GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE,
                                               false)
    },
}, class MonitorConstraint extends Clutter.Constraint {
    _init(props) {
        this._primary = false;
        this._index = -1;
        this._workArea = false;

        super._init(props);
    }

    get primary() {
        return this._primary;
    }

    set primary(v) {
        if (v)
            this._index = -1;
        this._primary = v;
        if (this.actor)
            this.actor.queue_relayout();
        this.notify('primary');
    }

    get index() {
        return this._index;
    }

    set index(v) {
        this._primary = false;
        this._index = v;
        if (this.actor)
            this.actor.queue_relayout();
        this.notify('index');
    }

    // eslint-disable-next-line camelcase
    get work_area() {
        return this._workArea;
    }

    // eslint-disable-next-line camelcase
    set work_area(v) {
        if (v == this._workArea)
            return;
        this._workArea = v;
        if (this.actor)
            this.actor.queue_relayout();
        this.notify('work-area');
    }

    vfunc_set_actor(actor) {
        if (actor) {
            if (!this._monitorsChangedId) {
                this._monitorsChangedId =
                    Main.layoutManager.connect('monitors-changed', () => {
                        this.actor.queue_relayout();
                    });
            }

            if (!this._workareasChangedId) {
                this._workareasChangedId =
                    global.display.connect('workareas-changed', () => {
                        if (this._workArea)
                            this.actor.queue_relayout();
                    });
            }
        } else {
            if (this._monitorsChangedId)
                Main.layoutManager.disconnect(this._monitorsChangedId);
            this._monitorsChangedId = 0;

            if (this._workareasChangedId)
                global.display.disconnect(this._workareasChangedId);
            this._workareasChangedId = 0;
        }

        super.vfunc_set_actor(actor);
    }

    vfunc_update_allocation(actor, actorBox) {
        if (!this._primary && this._index < 0)
            return;

        if (!Main.layoutManager.primaryMonitor)
            return;

        let index;
        if (this._primary)
            index = Main.layoutManager.primaryIndex;
        else
            index = Math.min(this._index, Main.layoutManager.monitors.length - 1);

        let rect;
        if (this._workArea) {
            let workspaceManager = global.workspace_manager;
            let ws = workspaceManager.get_workspace_by_index(0);
            rect = ws.get_work_area_for_monitor(index);
        } else {
            rect = Main.layoutManager.monitors[index];
        }

        actorBox.init_rect(rect.x, rect.y, rect.width, rect.height);
    }
});

var Monitor = class Monitor {
    constructor(index, geometry, geometryScale) {
        this.index = index;
        this.x = geometry.x;
        this.y = geometry.y;
        this.width = geometry.width;
        this.height = geometry.height;
        this.geometry_scale = geometryScale;
    }

    get inFullscreen() {
        return global.display.get_monitor_in_fullscreen(this.index);
    }
};
