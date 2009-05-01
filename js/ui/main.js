/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gdk = imports.gi.Gdk;
const Gio = imports.gi.Gio;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
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
    
    // Need to update struts on new workspaces when they are added
    global.screen.connect('notify::n-workspaces', _setStageArea);

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

// Used to go into a mode where all keyboard and mouse input goes to
// the stage. Returns true if we successfully grabbed the keyboard and
// went modal, false otherwise
function startModal() {
    let global = Shell.Global.get();

    if (!global.grab_keyboard())
        return false;
    global.set_stage_input_mode(Shell.StageInputMode.FULLSCREEN);

    inModal = true;

    return true;
}

function endModal() {
    let global = Shell.Global.get();

    global.ungrab_keyboard();
    global.set_stage_input_mode(Shell.StageInputMode.NORMAL);
    inModal = false;
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

function create_app_launch_context() {
    let global = Shell.Global.get();
    let screen = global.screen;
    let display = screen.get_display();

    let context = new Gdk.AppLaunchContext();
    context.set_timestamp(display.get_current_time());

    // Make sure that the app is opened on the current workspace even if
    // the user switches before it starts
    context.set_desktop(screen.get_active_workspace_index());

    return context;
}

let _shellActors = [];

// For adding an actor that is part of the shell in the normal desktop view
function addShellActor(actor) {
    let global = Shell.Global.get();

    _shellActors.push(actor);

    actor.connect('notify::visible', _setStageArea);
    actor.connect('destroy', function(actor) {
                      let i = _shellActors.indexOf(actor);
                      if (i != -1)
                          _shellActors.splice(i, 1);
                      _setStageArea();
                  });

    while (actor != global.stage) {
        actor.connect('notify::allocation', _setStageArea);
        actor = actor.get_parent();
    }

    _setStageArea();
}

function _setStageArea() {
    let global = Shell.Global.get();
    let rects = [], struts = [];

    for (let i = 0; i < _shellActors.length; i++) {
        if (!_shellActors[i].visible)
            continue;

        let [x, y] = _shellActors[i].get_transformed_position();
        let [w, h] = _shellActors[i].get_transformed_size();

        let rect = new Meta.Rectangle({ x: x, y: y, width: w, height: h});
        rects.push(rect);

        // Metacity wants to know what side of the screen the strut is
        // considered to be attached to. If the actor is only touching
        // one edge, or is touching the entire width/height of one
        // edge, then it's obvious which side to call it. If it's in a
        // corner, we pick a side arbitrarily. If it doesn't touch any
        // edges, or it spans the width/height across the middle of
        // the screen, then we don't create a strut for it at all.
        let side;
        if (w >= global.screen_width) {
            if (y <= 0)
                side = Meta.Side.TOP;
            else if (y + h >= global.screen_height)
                side = Meta.Side.BOTTOM;
            else
                continue;
        } else if (h >= global.screen_height) {
            if (x <= 0)
                side = Meta.Side.LEFT;
            else if (x + w >= global.screen_width)
                side = Meta.Side.RIGHT;
            else
                continue;
        } else if (x <= 0)
            side = Meta.Side.LEFT;
        else if (y <= 0)
            side = Meta.Side.TOP;
        else if (x + w >= global.screen_width)
            side = Meta.Side.RIGHT;
        else if (y + h >= global.screen_height)
            side = Meta.Side.BOTTOM;
        else
            continue;

        let strut = new Meta.Strut({ rect: rect, side: side });
        struts.push(strut);
    }

    let screen = global.screen;
    for (let w = 0; w < screen.n_workspaces; w++) {
        let workspace = screen.get_workspace_by_index(w);
        workspace.set_builtin_struts(struts);
    }

    global.set_stage_input_region(rects);
}
