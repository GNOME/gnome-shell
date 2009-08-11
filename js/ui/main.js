/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const Gdk = imports.gi.Gdk;
const Gio = imports.gi.Gio;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const Chrome = imports.ui.chrome;
const Overview = imports.ui.overview;
const Panel = imports.ui.panel;
const RunDialog = imports.ui.runDialog;
const LookingGlass = imports.ui.lookingGlass;
const Sidebar = imports.ui.sidebar;
const Tweener = imports.ui.tweener;
const WindowManager = imports.ui.windowManager;

const DEFAULT_BACKGROUND_COLOR = new Clutter.Color();
DEFAULT_BACKGROUND_COLOR.from_pixel(0x2266bbff);

let chrome = null;
let panel = null;
let sidebar = null;
let overview = null;
let runDialog = null;
let lookingGlass = null;
let wm = null;
let recorder = null;
let inModal = false;

function start() {
    let global = Shell.Global.get();

    Gio.DesktopAppInfo.set_desktop_env("GNOME");

    global.grab_dbus_service();

    Tweener.init();

    // The background color really only matters if there is no desktop
    // window (say, nautilus) running. We set it mostly so things look good
    // when we are running inside Xephyr.
    global.stage.color = DEFAULT_BACKGROUND_COLOR;

    // Mutter currently hardcodes putting "Yessir. The compositor is running""
    // in the Overview. Clear that out.
    let children = global.overlay_group.get_children();
    for (let i = 0; i < children.length; i++)
        children[i].destroy();

    global.connect('panel-run-dialog', function(panel) {
        // Make sure not more than one run dialog is shown.
        if (runDialog == null) {
            runDialog = new RunDialog.RunDialog();
        }
        runDialog.open();
    });

    overview = new Overview.Overview();
    chrome = new Chrome.Chrome();
    panel = new Panel.Panel();
    sidebar = new Sidebar.Sidebar();
    wm = new WindowManager.WindowManager();
    
    global.screen.connect('toggle-recording', function() {
        if (recorder == null) {
            recorder = new Shell.Recorder({ stage: global.stage });
        }

        if (recorder.is_recording()) {
            recorder.pause();
        } else {
            recorder.record();
        }
    });

    panel.startupAnimation();

    let display = global.screen.get_display();
    display.connect('overlay-key', Lang.bind(overview, overview.toggle));
    global.connect('panel-main-menu', Lang.bind(overview, overview.toggle));
    
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

function createLookingGlass() {
    if (lookingGlass == null) {
        lookingGlass = new LookingGlass.LookingGlass();
        lookingGlass.slaveTo(panel.actor);
    }
    return lookingGlass;
}

function createAppLaunchContext() {
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
