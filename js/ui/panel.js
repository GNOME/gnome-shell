/* -*- mode: js2; js2-basic-offset: 4; -*- */

const Clutter = imports.gi.Clutter;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Tidy = imports.gi.Tidy;

const Button = imports.ui.button;
const Main = imports.ui.main;

const PANEL_HEIGHT = 32;
const TRAY_HEIGHT = 24;
const PANEL_BACKGROUND_COLOR = new Clutter.Color();
PANEL_BACKGROUND_COLOR.from_pixel(0xeeddccff);
const PRESSED_BUTTON_BACKGROUND_COLOR = new Clutter.Color();
PRESSED_BUTTON_BACKGROUND_COLOR.from_pixel(0xccbbaaff);

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

 	this.button = new Button.Button("Activities", PANEL_BACKGROUND_COLOR, PRESSED_BUTTON_BACKGROUND_COLOR, true, null, PANEL_HEIGHT-1);

 	this._group.add_actor(this.button.button);

	this._grid = new Tidy.Grid({ height: TRAY_HEIGHT,
				     valign: 0.5,
				     end_align: true,
				     column_gap: 2 })
	this._group.add_actor(this._grid);

	this._clock = new Clutter.Label({ font_name: "Sans Bold 16px",
					  text: "" });
	this._grid.add_actor(this._clock);

        // Setting the anchor point at top right (north east) makes that portion of the 
        // grid positioned at the position specified below. 	
	this._grid.set_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
	this._grid.set_position(global.screen_width - 2, (PANEL_HEIGHT - TRAY_HEIGHT) / 2);

	this._traymanager = new Shell.TrayManager();
	let me = this;
        // the anchor point needs to be updated each time the height/width of the content might have changed, because 
        // it doesn't get updated on its own
        this._traymanager.connect('tray-icon-added',
	    function(o, icon) {
		me._grid.add_actor(icon);
		/* bump the clock back to the end */
		me._grid.remove_actor(me._clock);
		me._grid.add_actor(me._clock);
		me._grid.move_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
	    });
	this._traymanager.connect('tray-icon-removed',
	    function(o, icon) {
		me._grid.remove_actor(icon);
		me._grid.move_anchor_point_from_gravity(Clutter.Gravity.NORTH_EAST);
	    });
	this._traymanager.manage_stage(global.stage);

        // TODO: decide what to do with the rest of the panel in the overlay mode (make it fade-out, become non-reactive, etc.)
        // We get into the overlay mode on button-press-event as opposed to button-release-event because eventually we'll probably
        // have the overlay act like a menu that allows the user to release the mouse on the activity the user wants 
        // to switch to.
 	this.button.button.connect('button-press-event',
	    function(o, event) {
		if (Main.overlay.visible)
		    Main.hide_overlay();
		else
		    Main.show_overlay();

		return true;
            });

	this._setStruts();
	global.screen.connect('notify::n-workspaces',
	    function() {
		me._setStruts();
	    });

	global.stage.add_actor(this._group);

	this._updateClock();
	this._startClock();
    },

    // Struts determine the area along each side of the screen that is reserved
    // and not available to applications
    _setStruts: function() {
	let global = Shell.global_get();

	let struts = [
            new Meta.Strut({
		rect: {
		    x: 0,
		    y: 0,
		    width: global.screen_width,
		    height: PANEL_HEIGHT
		},
		side: Meta.Direction.TOP
	    })
	];

	let screen = global.screen;
	for (let i = 0; i < screen.n_workspaces; i++) {
	    let workspace = screen.get_workspace_by_index(i);
	    workspace.set_builtin_struts(struts);
	}
    },

    _startClock: function() {
	let me = this;
        // TODO: this makes the clock updated every 60 seconds, but not necessarily on the minute, so it is inaccurate
	Mainloop.timeout_add_seconds(60,
	    function() {
		me._updateClock();
		return true;
	    });
    },

    _updateClock: function() {
	this._clock.set_text(new Date().toLocaleFormat("%H:%M"));
	return true;
    },
 
    overlayHidden: function() { 
        this.button.release();
    }
};
