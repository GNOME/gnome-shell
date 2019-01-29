// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const St = imports.gi.St;

const Main = imports.ui.main;
const Meta = imports.gi.Meta;

var FADE_TIME = 0.1;

var OsdMonitorLabel = class {
    constructor(monitor, label) {
        this._actor = new St.Widget({ x_expand: true,
                                      y_expand: true });

        this._monitor = monitor;

        this._box = new St.BoxLayout({ style_class: 'osd-window',
                                       vertical: true });
        this._actor.add_actor(this._box);

        this._label = new St.Label({ style_class: 'osd-monitor-label',
                                     text: label });
        this._box.add(this._label);

        Main.uiGroup.add_child(this._actor);
        Main.uiGroup.set_child_above_sibling(this._actor, null);
        this._position();

        Meta.disable_unredirect_for_display(global.display);
    }

    _position() {
        let workArea = Main.layoutManager.getWorkAreaForMonitor(this._monitor);

        if (Clutter.get_default_text_direction() == Clutter.TextDirection.RTL)
            this._box.x = workArea.x + (workArea.width - this._box.width);
        else
            this._box.x = workArea.x;

        this._box.y = workArea.y;
    }

    destroy() {
        this._actor.destroy();
        Meta.enable_unredirect_for_display(global.display);
    }
};

var OsdMonitorLabeler = class {
    constructor() {
        this._monitorManager = Meta.MonitorManager.get();
        this._client = null;
        this._clientWatchId = 0;
        this._osdLabels = [];
        this._monitorLabels = null;
        Main.layoutManager.connect('monitors-changed',
                                    this._reset.bind(this));
        this._reset();
    }

    _reset() {
        for (let i in this._osdLabels)
            this._osdLabels[i].destroy();
        this._osdLabels = [];
        this._monitorLabels = new Map();
        let monitors = Main.layoutManager.monitors;
        for (let i in monitors)
            this._monitorLabels.set(monitors[i].index, []);
    }

    _trackClient(client) {
        if (this._client)
            return (this._client == client);

        this._client = client;
        this._clientWatchId = Gio.bus_watch_name(Gio.BusType.SESSION, client, 0, null,
                                                 (c, name) => {
                                                     this.hide(name);
                                                 });
        return true;
    }

    _untrackClient(client) {
        if (!this._client || this._client != client)
            return false;

        Gio.bus_unwatch_name(this._clientWatchId);
        this._clientWatchId = 0;
        this._client = null;
        return true;
    }

    show(client, params) {
        if (!this._trackClient(client))
            return;

        this._reset();

        for (let id in params) {
            let monitor = this._monitorManager.get_monitor_for_output(id);
            if (monitor == -1)
                continue;
            this._monitorLabels.get(monitor).push(params[id].deep_unpack());
        }

        // In mirrored display setups, more than one physical outputs
        // might be showing the same logical monitor. In that case, we
        // join each output's labels on the same OSD widget.
        for (let [monitor, labels] of this._monitorLabels.entries()) {
            labels.sort();
            this._osdLabels.push(new OsdMonitorLabel(monitor, labels.join(' ')));
        }
    }

    show2(client, params) {
        if (!this._trackClient(client))
            return;

        this._reset();

        for (let connector in params) {
            let monitor = this._monitorManager.get_monitor_for_connector(connector);
            if (monitor == -1)
                continue;
            this._monitorLabels.get(monitor).push(params[connector].deep_unpack());
        }

        for (let [monitor, labels] of this._monitorLabels.entries()) {
            labels.sort();
            this._osdLabels.push(new OsdMonitorLabel(monitor, labels.join(' ')));
        }
    }

    hide(client) {
        if (!this._untrackClient(client))
            return;

        this._reset();
    }
};
