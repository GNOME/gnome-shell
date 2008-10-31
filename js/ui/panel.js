/* -*- mode: js2; js2-basic-offset: 4; -*- */

const Mainloop = imports.mainloop;

const Shell = imports.gi.Shell;
const Clutter = imports.gi.Clutter;

const PANEL_HEIGHT = 32;
const PANEL_BACKGROUND_COLOR = new Clutter.Color();
PANEL_BACKGROUND_COLOR.from_pixel(0xeeddccff);

function Panel() {
    this._init();
}

Panel.prototype = {
    _init : function() {
	let global = Shell.global_get();

	global.set_stage_input_area(0, 0, global.screen_width, PANEL_HEIGHT);

	this._group = new Clutter.Group();

	let background = new Clutter.Rectangle({ color: PANEL_BACKGROUND_COLOR,
						 reactive: true,
					         width: global.screen_width,
					         height: PANEL_HEIGHT });
	this._group.add_actor(background);

	let message = new Clutter.Label({ font_name: "Sans Bold 16px",
					  text: "Activities",
					  reactive: true });
	message.set_position(5, 5);
	this._group.add_actor(message);

	this._clock = new Clutter.Label({ font_name: "Sans Bold 16px",
					  text: "" });
	this._clock.set_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
	this._clock.set_position(global.screen_width - 5, 5);
	this._group.add_actor(this._clock);

	message.connect('button-press-event',
	    function(o, event) {
		log('Clicked!');
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
	this._clock.set_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);

	return true;
    }
};
