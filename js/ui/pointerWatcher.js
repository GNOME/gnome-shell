// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Shell = imports.gi.Shell;

// We stop polling if the user is idle for more than this amount of time
const IDLE_TIME = 1000;

// This file implements a reasonably efficient system for tracking the position
// of the mouse pointer. We simply query the pointer from the X server in a loop,
// but we turn off the polling when the user is idle.

let _pointerWatcher = null;
function getPointerWatcher() {
    if (_pointerWatcher == null)
        _pointerWatcher = new PointerWatcher();

    return _pointerWatcher;
}

const PointerWatch = new Lang.Class({
    Name: 'PointerWatch',

    _init: function(watcher, interval, callback) {
        this.watcher = watcher;
        this.interval = interval;
        this.callback = callback;
    },

    // remove:
    // remove this watch. This function may safely be called
    // while the callback is executing.
    remove: function() {
        this.watcher._removeWatch(this);
    }
});

const PointerWatcher = new Lang.Class({
    Name: 'PointerWatcher',

    _init: function() {
        this._idleMonitor = Meta.IdleMonitor.get_core();
        this._idleMonitor.add_idle_watch(IDLE_TIME, Lang.bind(this, this._onIdleMonitorBecameIdle));
        this._idle = this._idleMonitor.get_idletime() > IDLE_TIME;
        this._watches = [];
        this.pointerX = null;
        this.pointerY = null;
    },

    // addWatch:
    // @interval: hint as to the time resolution needed. When the user is
    //   not idle, the position of the pointer will be queried at least
    //   once every this many milliseconds.
    // @callback: function to call when the pointer position changes - takes
    //   two arguments, X and Y.
    //
    // Set up a watch on the position of the mouse pointer. Returns a
    // PointerWatch object which has a remove() method to remove the watch.
    addWatch: function(interval, callback) {
        // Avoid unreliably calling the watch for the current position
        this._updatePointer();

        let watch = new PointerWatch(this, interval, callback);
        this._watches.push(watch);
        this._updateTimeout();
        return watch;
    },

    _removeWatch: function(watch) {
        for (let i = 0; i < this._watches.length; i++) {
            if (this._watches[i] == watch) {
                this._watches.splice(i, 1);
                this._updateTimeout();
                return;
            }
        }
    },

    _onIdleMonitorBecameActive: function(monitor) {
        this._idle = false;
        this._updatePointer();
        this._updateTimeout();
    },

    _onIdleMonitorBecameIdle: function(monitor) {
        this._idle = true;
        this._idleMonitor.add_user_active_watch(Lang.bind(this, this._onIdleMonitorBecameActive));
        this._updateTimeout();
    },

    _updateTimeout: function() {
        if (this._timeoutId) {
            Mainloop.source_remove(this._timeoutId);
            this._timeoutId = 0;
        }

        if (this._idle || this._watches.length == 0)
            return;

        let minInterval = this._watches[0].interval;
        for (let i = 1; i < this._watches.length; i++)
            minInterval = Math.min(this._watches[i].interval, minInterval);

        this._timeoutId = Mainloop.timeout_add(minInterval,
                                               Lang.bind(this, this._onTimeout));
        GLib.Source.set_name_by_id(this._timeoutId, '[gnome-shell] this._onTimeout');
    },

    _onTimeout: function() {
        this._updatePointer();
        return GLib.SOURCE_CONTINUE;
    },

    _updatePointer: function() {
        let [x, y, mods] = global.get_pointer();
        if (this.pointerX == x && this.pointerY == y)
            return;

        this.pointerX = x;
        this.pointerY = y;

        for (let i = 0; i < this._watches.length;) {
            let watch = this._watches[i];
            watch.callback(x, y);
            if (watch == this._watches[i]) // guard against self-removal
                i++;
        }
    }
});
