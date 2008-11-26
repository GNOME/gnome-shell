/* -*- mode: js2; js2-basic-offset: 4; -*- */

const Clutter = imports.gi.Clutter;
const Shell = imports.gi.Shell;
const Signals = imports.signals;
const Tweener = imports.tweener.tweener;

const Panel = imports.ui.panel;
const Overlay = imports.ui.overlay;
const RunDialog = imports.ui.run_dialog;
const WindowManager = imports.ui.windowmanager;

const DEFAULT_BACKGROUND_COLOR = new Clutter.Color();
DEFAULT_BACKGROUND_COLOR.from_pixel(0x2266bbff);

let panel = null;
let overlay = null;
let run_dialog = null;
let wm = null;

// The "FrameTicker" object is an object used to feed new frames to Tweener
// so it can update values and redraw. The default frame ticker for
// Tweener just uses a simple timeout at a fixed frame rate and has no idea
// of "catching up" by dropping frames.
//
// We substitute it with custom frame ticker here that connects Tweener to
// a Clutter.TimeLine. Now, Clutter.Timeline itself isn't a whole lot more
// sophisticated than a simple timeout at a fixed frame rate, but at least
// it knows how to drop frames. (See HippoAnimationManager for a more
// sophisticated view of continous time updates; even better is to pay
// attention to the vertical vblank and sync to that when possible.)
//
function ClutterFrameTicker() {
    this._init();
}

ClutterFrameTicker.prototype = {
    TARGET_FRAME_RATE : 60,

    _init : function() {
	// We don't have a finite duration; tweener will tell us to stop
	// when we need to stop, so use 1000 seconds as "infinity"
	this._timeline = new Clutter.Timeline({ fps: this.TARGET_FRAME_RATE,
	                                        duration: 1000*1000 });
	this._frame = 0;

	let me = this;
	this._timeline.connect('new-frame',
	    function(timeline, frame) {
		me._onNewFrame(frame);
	    });
    },

    _onNewFrame : function(frame) {
	// Unfortunately the interface to to send a new frame to tweener
	// is a simple "next frame" and there is no provision for signaling
	// that frames have been skipped or just telling it the new time.
	// But what it actually does internally is just:
	//
	//  _currentTime += 1000/_ticker.FRAME_RATE;
	//
	// So by dynamically adjusting the value of FRAME_RATE we can trick
	// it into dealing with dropped frames.

	let delta = frame - this._frame;
	if (delta == 0)
	    this.FRAME_RATE = this.TARGET_FRAME_RATE;
	else
	    this.FRAME_RATE = this.TARGET_FRAME_RATE / delta;
	this._frame = frame;
	this.emit('prepare-frame');
    },

    start : function() {
	this._timeline.start();
    },

    stop : function() {
	this._timeline.stop();
	this._frame = 0;
    }
};

Signals.addSignalMethods(ClutterFrameTicker.prototype);

function start() {
    let global = Shell.Global.get();

    Tweener.setFrameTicker(new ClutterFrameTicker());

    // The background color really only matters if there is no desktop
    // window (say, nautilus) running. We set it mostly so things look good
    // when we are running inside Xephyr.
    global.stage.color = DEFAULT_BACKGROUND_COLOR;

    // Mutter currently hardcodes putting "Yessir. The compositor is running""
    // in the overlay. Clear that out.
    let children = global.overlay_group.get_children();
    for (let i = 0; i < children.length; i++)
	children[i].destroy();

    global.connect('panel-run-dialog', function(panel) {
        // Make sure not more than one run dialog is shown.
        if (!run_dialog) {
            run_dialog = new RunDialog.RunDialog();
	    let end_handler = function() {
                run_dialog.destroy();
                run_dialog = null;
            };
            run_dialog.connect('run', end_handler);
            run_dialog.connect('cancel', end_handler);
            if (!run_dialog.show())
		end_handler();
        }
    });

    panel = new Panel.Panel();
    global.set_stage_input_area(0, 0, global.screen_width, Panel.PANEL_HEIGHT);

    overlay = new Overlay.Overlay();
    wm = new WindowManager.WindowManager();
}

// Used to go into a mode where all keyboard and mouse input goes to
// the stage. Returns true if we successfully grabbed the keyboard and
// went modal, false otherwise
function startModal() {
    let global = Shell.Global.get();

    if (!global.grab_keyboard())
	return false;

    global.set_stage_input_area(0, 0, global.screen_width, global.screen_height);

    return true;
}

function endModal() {
    let global = Shell.Global.get();

    global.ungrab_keyboard();
    global.set_stage_input_area(0, 0, global.screen_width, Panel.PANEL_HEIGHT);
}

function show_overlay() {
    if (startModal())
	overlay.show();
}

function hide_overlay() {
    overlay.hide();
    endModal();
}
