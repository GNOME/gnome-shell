// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const GnomeDesktop = imports.gi.GnomeDesktop;
const Shell = imports.gi.Shell;

let _pointerWatcher = null;
function getPointerWatcher() {
    if (_pointerWatcher == null)
        _pointerWatcher = new PointerWatcher();

    return _pointerWatcher;
}

const PointerWatch = new Lang.Class({
    Name: 'PointerWatch',

    _init: function(watcher, callback) {
        this.watcher = watcher;
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
        this._cursorTracker = Meta.CursorTracker.get_for_screen(global.screen);
        this._cursorTracker.connect('position-changed', Lang.bind(this, this._updatePointer));

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
        this._cursorTracker.enable_track_position();

        // Avoid unreliably calling the watch for the current position
        this._updatePointer();

        this._watches.push(callback);
        return watch;
    },

    _removeWatch: function(watch) {
        for (let i = 0; i < this._watches.length; i++) {
            if (this._watches[i] == watch) {
                this._cursorTracker.disable_track_position();
                this._watches.splice(i, 1);
                return;
            }
        }
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
