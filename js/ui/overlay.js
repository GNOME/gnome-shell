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
    },

    show : function() {
	if (!this.visible) {
	    this.visible = true;
	    this._group.show();
	}
    },

    hide : function() {
	if (this.visible) {
	    this.visible = false;
	    this._group.hide();
	}
    }
};
