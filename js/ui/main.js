/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const Overlay = imports.ui.overlay;
const Panel = imports.ui.panel;
const RunDialog = imports.ui.runDialog;
const Tweener = imports.ui.tweener;
const WindowManager = imports.ui.windowManager;

const DEFAULT_BACKGROUND_COLOR = new Clutter.Color();
DEFAULT_BACKGROUND_COLOR.from_pixel(0x2266bbff);

let panel = null;
let overlay = null;
let overlayActive = false;
let runDialog = null;
let wm = null;

function start() {
    let global = Shell.Global.get();

    Gio.DesktopAppInfo.set_desktop_env("GNOME");

    global.grab_dbus_service();
    global.start_task_panel();

    Tweener.init();

    // The background color really only matters if there is no desktop
    // window (say, nautilus) running. We set it mostly so things look good
    // when we are running inside Xephyr.
    global.stage.color = DEFAULT_BACKGROUND_COLOR;

    // Mutter currently hardcodes putting "Yessir. The compositor is running""
    // in the overlay. Clear that out.
    let children = global.overlay_group.get_children();
    for (let i = 0; i < children.length; i++)
        children[i].destroy();

    // metacity-clutter currently uses the same prefs as plain metacity,
    // which probably means we'll be starting out with multiple workspaces;
    // remove any unused ones
    let windows = global.get_windows();
    let maxWorkspace = 0;
    for (let i = 0; i < windows.length; i++) {
        let win = windows[i];

        if (!win.get_meta_window().is_on_all_workspaces() &&
            win.get_workspace() > maxWorkspace) {
            maxWorkspace = win.get_workspace();
        }
    }
    let screen = global.screen;
    if (screen.n_workspaces > maxWorkspace) {
        for (let w = screen.n_workspaces - 1; w > maxWorkspace; w--) {
            let workspace = screen.get_workspace_by_index(w);
            screen.remove_workspace(workspace, 0);
        }
    }

    global.connect('panel-run-dialog', function(panel) {
        // Make sure not more than one run dialog is shown.
        if (!runDialog) {
            runDialog = new RunDialog.RunDialog();
            let end_handler = function() {
                runDialog.destroy();
                runDialog = null;
            };
            runDialog.connect('run', end_handler);
            runDialog.connect('cancel', end_handler);
            if (!runDialog.show())
                end_handler();
        }
    });

    panel = new Panel.Panel();
    global.set_stage_input_area(0, 0, global.screen_width, Panel.PANEL_HEIGHT);

    overlay = new Overlay.Overlay();
    wm = new WindowManager.WindowManager();
    
    let display = global.screen.get_display();
    display.connect('overlay-key', function(display) {
        if (overlay.visible) {
            hide_overlay();
        } else {
            show_overlay();
        }
    });
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
    if (startModal()) {
        overlayActive = true;
        overlay.show();
    }
}

function hide_overlay() {
    overlay.hide();
    overlayActive = false;
    endModal();
}
