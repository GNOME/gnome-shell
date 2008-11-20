/* -*- mode: js2; js2-basic-offset: 4; -*- */

const Shell = imports.gi.Shell;
const Clutter = imports.gi.Clutter;

const Panel = imports.ui.panel;
const Overlay = imports.ui.overlay;
const RunDialog = imports.ui.run_dialog;

const DEFAULT_BACKGROUND_COLOR = new Clutter.Color();
DEFAULT_BACKGROUND_COLOR.from_pixel(0x2266bbff);

let panel = null;
let overlay = null;
let run_dialog = null;

function start() {
    let global = Shell.global_get();

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
	    let handler = function() {
                run_dialog.destroy();
                run_dialog = null;
            };
            run_dialog.connect('run', handler);
            run_dialog.connect('cancel', handler);
            run_dialog.show();
        }
    });

    panel = new Panel.Panel();
    overlay = new Overlay.Overlay();
    global.set_stage_input_area(0, 0, global.screen_width, Panel.PANEL_HEIGHT);
}

function show_overlay() {
    let global = Shell.global_get();

    overlay.show();
    global.set_stage_input_area(0, 0, global.screen_width, global.screen_height);
}

function hide_overlay() {
    let global = Shell.global_get();

    overlay.hide();
    panel.overlayHidden();
    global.set_stage_input_area(0, 0, global.screen_width, Panel.PANEL_HEIGHT);
}
