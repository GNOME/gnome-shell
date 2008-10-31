/* -*- mode: js2; js2-basic-offset: 4; -*- */

const Shell = imports.gi.Shell;
const Clutter = imports.gi.Clutter;

const Panel = imports.ui.panel;

const DEFAULT_BACKGROUND_COLOR = new Clutter.Color();
DEFAULT_BACKGROUND_COLOR.from_pixel(0x2266bbff);

let panel = null;

function start() {
    let global = Shell.global_get();

    // The background color really only matters if there is no desktop
    // window (say, nautilus) running. We set it mostly so things look good
    // when we are running inside Xephyr.
    global.stage.color = DEFAULT_BACKGROUND_COLOR;

    // Mutter currently hardcodes putting "Yessir. The compositor is running""
    // in the overlay. Clear that out.
    children = global.overlay_group.get_children();
    for (let i = 0; i < children.length; i++)
	children[i].destroy();

    panel = new Panel.Panel();
}
