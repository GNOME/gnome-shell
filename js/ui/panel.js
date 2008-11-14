/* -*- mode: js2; js2-basic-offset: 4; -*- */

const Mainloop = imports.mainloop;

const Shell = imports.gi.Shell;
const Clutter = imports.gi.Clutter;
const Tidy = imports.gi.Tidy;

const Main = imports.ui.main;

const PANEL_HEIGHT = 32;
const TRAY_HEIGHT = 24;
const PANEL_BACKGROUND_COLOR = new Clutter.Color();
PANEL_BACKGROUND_COLOR.from_pixel(0xeeddccff);

function Panel() {
    this._init();
}

Panel.prototype = {
    _init : function() {
	let global = Shell.global_get();

	this._group = new Clutter.Group();

	let background = new Clutter.Rectangle({ color: PANEL_BACKGROUND_COLOR,
						 reactive: true,
					         width: global.screen_width+2,
					         height: PANEL_HEIGHT+1,
	                                         border_width: 1});
	background.set_position(-1, -1);
	this._group.add_actor(background);

	let message = new Clutter.Label({ font_name: "Sans Bold 16px",
					  text: "Activities",
					  reactive: true });
	message.set_position(5, 5);
	this._group.add_actor(message);

	this._grid = new Tidy.Grid({ height: TRAY_HEIGHT,
				     valign: 0.5,
				     end_align: true,
				     column_gap: 2 })
	this._group.add_actor(this._grid);

	this._clock = new Clutter.Label({ font_name: "Sans Bold 16px",
					  text: "" });
	this._grid.add_actor(this._clock);

	this._grid.set_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
	this._grid.set_position(global.screen_width - 2, (PANEL_HEIGHT - TRAY_HEIGHT) / 2);

	this._traymanager = new Shell.TrayManager();
	let panel = this;
	this._traymanager.connect('tray-icon-added',
	    function(o, icon) {
		panel._grid.add_actor(icon);
		/* bump the clock back to the end */
		panel._grid.remove_actor(panel._clock);
		panel._grid.add_actor(panel._clock);
		panel._grid.move_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
	    });
	this._traymanager.connect('tray-icon-removed',
	    function(o, icon) {
		panel._grid.remove_actor(icon);
		panel._grid.move_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
	    });
	this._traymanager.manage_stage(global.stage);

	message.connect('button-press-event',
	    function(o, event) {
		if (Main.overlay.visible)
		    Main.hide_overlay();
		else
		    Main.show_overlay();

		return true;
            });

	global.stage.add_actor(this._group);

	this._updateClock();
	this._startClock();
    },

    _startClock: function() {
	let me = this;
	Mainloop.timeout_add_seconds(60,
	    function() {
		me._updateClock();
		return true;
	    });
    },

    _updateClock: function() {
	this._clock.set_text(new Date().toLocaleFormat("%H:%M"));
	return true;
    }
};
