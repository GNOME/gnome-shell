// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Signals = imports.signals;
const Clutter = imports.gi.Clutter;

//in milliseconds
let LONG_PRESS_TIMEOUT = 250;

let MOTION_THRESHOLD = 30;

const AppSwitchAction = new Lang.Class({
    Name: 'AppSwitchAction',
    Extends: Clutter.GestureAction,

    _init : function() {
	this.parent();
	this.set_n_touch_points (3);
	global.display.connect('grab-op-begin', Lang.bind(this, this.cancel));
	global.display.connect('grab-op-end', Lang.bind(this, this.cancel));
    },

    vfunc_gesture_prepare : function(action, actor) {
	return this.get_n_current_points() <= 4;
    },

    vfunc_gesture_begin : function(action, actor) {
	let nPoints = this.get_n_current_points();
	let event = this.get_last_event (nPoints - 1);

	if (nPoints == 3)
	    this._longPressStartTime = event.get_time();
	else if (nPoints == 4) {
	    // Check whether the 4th finger press happens after a 3-finger long press,
	    // this only needs to be checked on the first 4th finger press
	    if (this._longPressStartTime != null &&
		event.get_time() < this._longPressStartTime + LONG_PRESS_TIMEOUT)
		this.cancel();
	    else {
		this._longPressStartTime = null;
		this.emit('activated');
	    }
	}

	return this.get_n_current_points() <= 4;
    },

    vfunc_gesture_progress : function(action, actor) {
	if (this.get_n_current_points() == 3) {
	    for (let i = 0; i < this.get_n_current_points(); i++) {
		[startX, startY] = this.get_press_coords(i);
		[x, y] = this.get_motion_coords(i);

		if (Math.abs(x - startX) > MOTION_THRESHOLD ||
		    Math.abs(y - startY) > MOTION_THRESHOLD)
		    return false;
	    }

	}

	return true;
    }
});
Signals.addSignalMethods(AppSwitchAction.prototype);
