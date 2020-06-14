// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* manages dynamic desktop areas */

const { GLib, GObject } = imports.gi;

var SafeDesktopArea = GObject.registerClass({
    Signals: {
        'margins-changed': {},
    },
}, class SafeDesktopArea extends GObject.Object {
    _init(layoutManager) {
        super._init();
        this._safeMargins = [];
        this._registered = [];
        this._idCounter = 0;
        this._sentSignal = false;

        this._layoutManager = layoutManager;
        this._monitorsChangedId = this._layoutManager.connect('monitors-changed', () => {
            this._updateMonitors();
        });
        this._updateMonitors();
    }

    _updateMonitors() {
        let monitors = this._layoutManager.monitors.length;

        if (this._safeMargins.length === monitors)
            return;

        if (this._safeMargins.length > monitors) {
            this._safeMargins = this._safeMargins.slice(0, monitors);
        } else {
            for (let i = this._safeMargins.length; i < monitors; i++)
                this._safeMargins.push({});
        }
    }

    _emitChanged() {
        if (this._sentSignal)
            return;

        this._sentSignal = true;

        this._sentSignalId = GLib.idle_add(GLib.PRIORITY_DEFAULT_IDLE, () => {
            this._sentSignal = false;
            this.emit('margins-changed');
            this._sentSignalId = 0;
            return false;
        });
    }

    /**
     * Registers an element/extension. It returns an ID that must be used to
     * register margins. Without a valid ID, an extension can't register
     * margins.
     *
     * @returns a numeric ID to identify this element/extension. It is never 0.
     */
    register() {
        this._idCounter += 1;
        this._registered.push(this._idCounter);
        return this._idCounter;
    }

    /**
     * Unregisters an element/extension and removes all the margins set by it.
     * @param {int} id The element/extension ID
     */
    unregister(id) {
        if (!this._registered.includes(id))
            throw new Error('Trying to unregister an inexistent ID');
        for (let monitor in this._safeMargins)
            this.unsetMargins(id, monitor);
        this._registered.splice(this._registered.indexOf(id));
    }

    /**
     * Utility function to get the physical monitor to which the specified
     * coordinates belong.
     * @param {int} x X coordinate
     * @param {int} y Y coordinate
     * @returns The monitor index (starting from 0), or -1 if it falls outside
     * all the monitors
     */
    getMonitorForCoordinates(x, y) {
        for (let monitorIndex = 0; monitorIndex < this._layoutManager.monitors.length; monitorIndex++) {
            let monitor = this._layoutManager.monitors[monitorIndex];
            if ((x >= monitor.x) && (y >= monitor.y) && (x < (monitor.x + monitor.width)) && (y < (monitor.y + monitor.height)))
                return monitorIndex;
        }
        return -1;
    }

    /**
     * Sets the used margins for an specific ID and monitor. If the extension
     * wants to register margin in several monitors, it must call this method
     * several times, one per monitor.
     *
     * @param {*} id the element/extension ID (obtained with `register()`)
     * @param {*} monitor the monitor to which these margins belongs
     * @param {*} top top margin for this monitor
     * @param {*} bottom bottom margin for this monitor
     * @param {*} left left margin for this monitor
     * @param {*} right right margin for this monitor
     */
    setSafeMargins(id, monitor, top, bottom, left, right) {
        if (!this._registered.includes(id))
            throw new Error('Using an inexistent ID to set desktop safe margins');
        if (monitor >= this._safeMargins.length)
            return;
        let margins = {};
        margins.top = top;
        margins.bottom = bottom;
        margins.left = left;
        margins.right = right;
        this._safeMargins[monitor][id] = margins;
        this._emitChanged();
    }

    /**
     * Removes all the margins set by an ID in the specific monitor
     * @param {*} id The element/extension ID
     * @param {*} monitor The monitor from which remove all the margins set by
     * ID (or -1 to remove all the margins set by ID)
     */
    unsetSafeMargins(id, monitor) {
        if (!this._registered.includes(id))
            throw new Error('Using an inexistent ID to unset desktop safe margins');
        if (monitor === -1) {
            for (monitor in this._safeMargins) {
                if (id in this._safeMargins[monitor])
                    delete this._safeMargins[monitor][id];
            }
        } else {
            if (monitor >= this._safeMargins.length)
                return;
            if (!(id in this._safeMargins[monitor]))
                return;
            delete this._safeMargins[monitor][id];
        }
        this._emitChanged();
    }

    /**
     * Returns an object with all the margins set for an specific monitor
     * @param {int} monitor The monitor from which get the margins
     * @returns an object with four elements: `top`, `bottom`, `left` and `right`,
     * each one containing an integer with the corresponding margin.
     * The caller must get the monitor resolution and add or substract the margins
     * to get the safe area.
     */
    getSafeMargins(monitor) {
        let margins = {};

        margins.top = 0;
        margins.bottom = 0;
        margins.left = 0;
        margins.right = 0;
        if (monitor < this._safeMargins.length) {
            for (let id in this._safeMargins[monitor]) {
                let idmargins = this._safeMargins[monitor][id];
                if (idmargins.top > margins.top)
                    margins.top = idmargins.top;
                if (idmargins.bottom > margins.bottom)
                    margins.bottom = idmargins.bottom;
                if (idmargins.left > margins.left)
                    margins.left = idmargins.left;
                if (idmargins.right > margins.right)
                    margins.right = idmargins.right;
            }
        }
        return margins;
    }
});
