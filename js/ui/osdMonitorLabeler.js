import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import St from 'gi://St';


import * as Main from './main.js';

const OsdMonitorLabel = GObject.registerClass(
class OsdMonitorLabel extends St.Widget {
    _init(monitor, label) {
        super._init({x_expand: true, y_expand: true});

        this._monitor = monitor;

        this._box = new St.BoxLayout({
            orientation: Clutter.Orientation.VERTICAL,
        });
        this.add_child(this._box);

        this._label = new St.Label({
            style_class: 'osd-monitor-label',
            text: label,
        });
        this._box.add_child(this._label);

        Main.uiGroup.add_child(this);
        Main.uiGroup.set_child_above_sibling(this, null);
        this._position();

        global.compositor.disable_unredirect();
        this.connect('destroy', () => {
            global.compositor.enable_unredirect();
        });
    }

    _position() {
        let workArea = Main.layoutManager.getWorkAreaForMonitor(this._monitor);

        if (Clutter.get_default_text_direction() === Clutter.TextDirection.RTL)
            this._box.x = workArea.x + (workArea.width - this._box.width);
        else
            this._box.x = workArea.x;

        this._box.y = workArea.y;
    }
});

export class OsdMonitorLabeler {
    constructor() {
        this._monitorManager = global.backend.get_monitor_manager();
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
            return this._client === client;

        this._client = client;
        this._clientWatchId = Gio.bus_watch_name(Gio.BusType.SESSION,
            client, 0, null,
            (c, name) => {
                this.hide(name);
            });
        return true;
    }

    _untrackClient(client) {
        if (!this._client || this._client !== client)
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

        for (let connector in params) {
            let monitor = this._monitorManager.get_monitor_for_connector(connector);
            if (monitor === -1)
                continue;
            this._monitorLabels.get(monitor).push(params[connector].deepUnpack());
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
}
