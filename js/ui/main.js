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
let recorder = null;
let inModal = false;

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
    panel.actor.connect('notify::visible', _panelVisibilityChanged);
    _panelVisibilityChanged();

    overlay = new Overlay.Overlay();
    wm = new WindowManager.WindowManager();
    
    let display = global.screen.get_display();
    let toggleOverlay = function(display) {
        if (overlay.visible) {
            hide_overlay();
        } else {
            show_overlay();
        }
    };

    global.screen.connect('toggle-recording', function() {
        if (recorder == null) {
            // We have to initialize GStreamer first. This isn't done
            // inside ShellRecorder to make it usable inside projects
            // with other usage of GStreamer.
            let Gst = imports.gi.Gst;
            Gst.init(null, null);
            recorder = new Shell.Recorder({ stage: global.stage });
        }

        if (recorder.is_recording()) {
            recorder.pause();
        } else {
            recorder.record();
        }
    });

    display.connect('overlay-key', toggleOverlay);
    global.connect('panel-main-menu', toggleOverlay);
    
    Mainloop.idle_add(_removeUnusedWorkspaces);
}

// metacity-clutter currently uses the same prefs as plain metacity,
// which probably means we'll be starting out with multiple workspaces;
// remove any unused ones. (We do this from an idle handler, because
// global.get_windows() still returns NULL at the point when start()
// is called.)
function _removeUnusedWorkspaces() {

    let global = Shell.Global.get();

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

    return false;
}

function _panelVisibilityChanged() {
    if (!inModal) {
        let global = Shell.Global.get();

        if (panel.actor.visible) {
            global.set_stage_input_area(0, 0,
                                        global.screen_width, Panel.PANEL_HEIGHT);
        } else
            global.set_stage_input_area(0, 0, 0, 0);
    }
}

// Used to go into a mode where all keyboard and mouse input goes to
// the stage. Returns true if we successfully grabbed the keyboard and
// went modal, false otherwise
function startModal() {
    let global = Shell.Global.get();

    if (!global.grab_keyboard())
        return false;

    inModal = true;
    global.set_stage_input_area(0, 0, global.screen_width, global.screen_height);

    return true;
}

function endModal() {
    let global = Shell.Global.get();

    global.ungrab_keyboard();
    inModal = false;
    _panelVisibilityChanged();
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
