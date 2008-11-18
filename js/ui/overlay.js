/* -*- mode: js2; js2-basic-offset: 4; -*- */

const Clutter = imports.gi.Clutter;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Tweener = imports.tweener.tweener;

const Main = imports.ui.main;
const Panel = imports.ui.panel;

const OVERLAY_BACKGROUND_COLOR = new Clutter.Color();
OVERLAY_BACKGROUND_COLOR.from_pixel(0x000000ff);

// Time for initial animation going into overlay mode
const ANIMATION_TIME = 0.3;

// How much to scale the desktop down by in overlay mode
const DESKTOP_SCALE = 0.75;

// Windows are slightly translucent in the overlay mode
const WINDOW_OPACITY = 0.9 * 255;

// Define a layout scheme for small window counts. For larger
// counts we fall back to an algorithm. We need more schemes here
// unless we have a really good algorithm.
//
// Each triplet is [x_center, y_center, scale] where the scale
// is relative to the width of the desktop.
const POSITIONS = {
    1: [[0.5, 0.5, 0.8]],
    2: [[0.25, 0.5, 0.4], [0.75, 0.5, 0.4]],
    3: [[0.2, 0.33, 0.3],  [0.5, 0.67, 0.3],  [0.8, 0.33, 0.3]],
    4: [[0.25, 0.25, 0.3],   [0.75, 0.25, 0.3], [0.75, 0.75, 0.3], [0.25, 0.75, 0.3]]
};

function Overlay() {
    this._init();
};

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

	    let global = Shell.global_get();
	    let windows = global.get_windows();
	    let desktop_window = null;

	    let screen_width = global.screen_width
	    let screen_height = global.screen_height

	    for (let i = 0; i < windows.length; i++)
		if (windows[i].get_window_type() == Meta.WindowType.DESKTOP)
		    desktop_window = windows[i];

	    // The desktop windows are shown on top of a scaled down version of the
	    // desktop. This is positioned at the right side of the screen
	    this._desktop_width = screen_width * DESKTOP_SCALE;
	    this._desktop_height = screen_height * DESKTOP_SCALE;
	    this._desktop_x = screen_width - this._desktop_width - 10;
	    this._desktop_y = Panel.PANEL_HEIGHT + (screen_height - this._desktop_height - Panel.PANEL_HEIGHT) / 2;

            // If a file manager is displaying desktop icons, there will be a desktop window.
            // This window will have the size of the whole desktop. When such window is not present 
            // (e.g. when the preference for showing icons on the desktop is disabled by the user 
            // or we are running inside a Xephyr window), we should create a desktop rectangle 
            // to serve as the background.
	    if (desktop_window)
		this._createDesktopClone(desktop_window);
            else 
                this._createDesktopRectangle();

	    // Count the total number of windows so we know what layout scheme to use
	    let n_windows = 0;
	    for (let i = 0; i < windows.length; i++) {
		let w = windows[i];
		if (w == desktop_window || w.is_override_redirect())
		    continue;

		n_windows++;
	    }

	    // Now create actors for all the desktop windows. Do it in
	    // reverse order so that the active actor ends up on top
	    let window_index = 0;
	    for (let i = windows.length - 1; i >= 0; i--) {
		let w = windows[i];
		if (w == desktop_window || w.is_override_redirect())
		    continue;
		this._createWindowClone(w, n_windows - window_index - 1, n_windows);

		window_index++;
	    }

	    // All the the actors in the window group are completely obscured,
	    // hiding the group holding them while the overlay is displayed greatly
	    // increases performance of the overlay especially when there are many
	    // windows visible.
	    //
	    // If we switched to displaying the actors in the overlay rather than
	    // clones of them, this would obviously no longer be necessary.
	    global.window_group.hide()
	    this._group.show();
	}
    },

    hide : function() {
	if (this.visible) {
	    let global = Shell.global_get();

	    this.visible = false;
	    global.window_group.show()
	    this._group.hide();

	    for (let i = 0; i < this._window_clones.length; i++) {
		this._window_clones[i].destroy();
	    }

	    this._window_clones = [];
	}
    },

    _createDesktopClone : function(w) {
	let clone = new Clutter.CloneTexture({ parent_texture: w.get_texture(),
					       reactive: true,
					       x: 0,
					       y: 0 });
        this._addDesktop(clone);
    },

    _createDesktopRectangle : function() {   
        let global = Shell.global_get();
        // In the case when we have a desktop window from the file manager, its height is
        // full-screen, i.e. it includes the height of the panel, so we should not subtract
        // the height of the panel from global.screen_height here either to have them show
        // up identically.
        // We are also using (0,0) coordinates in both cases which makes the background
        // window animate out from behind the panel. 
	let desktop_rectangle = new Clutter.Rectangle({ color: global.stage.color,
					                reactive: true,
					                x: 0,
					                y: 0,
                                                        width: global.screen_width,
                                                        height: global.screen_height });
        this._addDesktop(desktop_rectangle);
    },

    _addDesktop : function(desktop) {
	this._window_clones.push(desktop);
	this._group.add_actor(desktop);

	Tweener.addTween(desktop,
			 { x: this._desktop_x,
			   y: this._desktop_y,
			   scale_x: DESKTOP_SCALE,
			   scale_y: DESKTOP_SCALE,
			   time: ANIMATION_TIME,
			   transition: "linear"
			 });

	let me = this;
	desktop.connect("button-press-event",
		      function() {
			  me._deactivate();
		      });
    },

    // window_index == 0 => top in stacking order
    _computeWindowPosition : function(window_index, n_windows) {
	if (n_windows in POSITIONS)
	    return POSITIONS[n_windows][window_index];

	// If we don't have a predefined scheme for this window count, overlap the windows
	// along the diagonal of the desktop (improve this!)
	let fraction = Math.sqrt(1/n_windows);

	// The top window goes at the lower right - this is different from the
	// fixed position schemes where the windows are in "reading order"
	// and the top window goes at the upper left.
	let pos = (n_windows - window_index - 1) / (n_windows - 1);
	let x_center = (fraction / 2) + (1 - fraction) * pos;
	let y_center = x_center;

	return [x_center, y_center, fraction];
    },

    _createWindowClone : function(w, window_index, n_windows) {
	// We show the window using "clones" of the texture .. separate
	// actors that mirror the original actors for the window. For
	// animation purposes, it may be better to actually move the
	// original actors about instead.

	let clone = new Clutter.CloneTexture({ parent_texture: w.get_texture(),
					       reactive: true,
					       x: w.x,
					       y: w.y });

	let [x_center, y_center, fraction] = this._computeWindowPosition(window_index, n_windows);

	let desired_size = this._desktop_width * fraction;

	x_center = this._desktop_x + x_center * this._desktop_width;
	y_center = this._desktop_y + y_center * this._desktop_height;

	let size = clone.width;
	if (clone.height > size)
	    size = clone.height;

	// Never scale up
	let scale = desired_size / size;
	if (scale > 1)
	    scale = 1;

	this._group.add_actor(clone);
	this._window_clones.push(clone);

	Tweener.addTween(clone,
			 { x: x_center - 0.5 * scale * w.width,
			   y: y_center - 0.5 * scale * w.height,
			   scale_x: scale,
			   scale_y: scale,
			   time: ANIMATION_TIME,
			   opacity: WINDOW_OPACITY,
			   transition: "linear"
			  });

	let me = this;
	clone.connect("button-press-event",
		      function(clone, event) {
			  me._activateWindow(w, event.get_time());
		      });
    },

    _activateWindow : function(w, time) {
	this._deactivate();
	w.get_meta_window().activate(time);
    },

    _deactivate : function() {
	Main.hide_overlay();
    }
};
