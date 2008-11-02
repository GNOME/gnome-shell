/* -*- mode: js2; js2-basic-offset: 4; -*- */

const Shell = imports.gi.Shell;
const Clutter = imports.gi.Clutter;

const Panel = imports.ui.panel;

const OVERLAY_BACKGROUND_COLOR = new Clutter.Color();
OVERLAY_BACKGROUND_COLOR.from_pixel(0x000000ff);

function Overlay() {
    this._init();
}

Overlay.prototype = {
    _init : function() {
	let global = Shell.global_get();

	this._group = new Clutter.Group();
	this.visible = false;

	let background = new Clutter.Rectangle({ color: OVERLAY_BACKGROUND_COLOR,
						 reactive: true,
						 x: 0,
						 y: Panel.PANEL_HEIGHT,
					         width: global.screen_width,
					         height: global.screen_width - Panel.PANEL_HEIGHT });
	this._group.add_actor(background);

	this._group.hide();
	global.overlay_group.add_actor(this._group);

	this._window_clones = []
    },

    show : function() {
	if (!this.visible) {
	    this.visible = true;

	    // Very simple version of a window overview ... when we show the
	    // overlay, display all the user's windows in the overlay scaled
	    // down.
	    //
	    // We show the window using "clones" of the texture .. separate
	    // actors that mirror the original actors for the window. For
	    // animation purposes, it may be better to actually move the
	    // original actors about instead.

	    let global = Shell.global_get();
	    let windows = global.get_windows();
	
	    let screen_width = global.screen_width
	    let screen_height = global.screen_height

	    let x = screen_width / 4
	    let y = screen_height / 4
    
	    for (let i = 0; i < windows.length; i++)
		if (!windows[i].is_override_redirect()) {
		    let clone = new Clutter.CloneTexture({ parent_texture: windows[i].get_texture(),
							   x: x,
							   y: y });

		    // We scale each window down so that it is at most 300x300, but we 
		    // never want to scale a window up.
		    let size = clone.width;
		    if (clone.height > size)
			size = clone.height;

		    let scale = 300 / size;
		    if (scale > 1)
			scale = 1;
		    
		    clone.set_scale(scale, scale);
		    this._group.add_actor(clone);
		    this._window_clones.push(clone);

		    // Windows are overlapped diagonally. If there are too many, they'll
		    // end up off the screen
		    x += 50;
		    y += 50;
		}
	    
	    this._group.show();
	}
    },

    hide : function() {
	if (this.visible) {
	    this.visible = false;
	    this._group.hide();

	    for (let i = 0; i < this._window_clones.length; i++) {
		this._window_clones[i].destroy();
	    }

	    this._window_clones = [];
	}
    }
};
