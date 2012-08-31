// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gdk = imports.gi.Gdk;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const AutomountManager = imports.ui.automountManager;
const AutorunManager = imports.ui.autorunManager;
const CtrlAltTab = imports.ui.ctrlAltTab;
const EndSessionDialog = imports.ui.endSessionDialog;
const PolkitAuthenticationAgent = imports.ui.polkitAuthenticationAgent;
const KeyringPrompt = imports.ui.keyringPrompt;
const Environment = imports.ui.environment;
const ExtensionSystem = imports.ui.extensionSystem;
const ExtensionDownloader = imports.ui.extensionDownloader;
const Keyboard = imports.ui.keyboard;
const MessageTray = imports.ui.messageTray;
const Overview = imports.ui.overview;
const Panel = imports.ui.panel;
const PlaceDisplay = imports.ui.placeDisplay;
const RunDialog = imports.ui.runDialog;
const Layout = imports.ui.layout;
const LookingGlass = imports.ui.lookingGlass;
const NetworkAgent = imports.ui.networkAgent;
const NotificationDaemon = imports.ui.notificationDaemon;
const WindowAttentionHandler = imports.ui.windowAttentionHandler;
const ScreenShield = imports.ui.screenShield;
const Scripting = imports.ui.scripting;
const SessionMode = imports.ui.sessionMode;
const ShellDBus = imports.ui.shellDBus;
const ShellMountOperation = imports.ui.shellMountOperation;
const TelepathyClient = imports.ui.telepathyClient;
const UnlockDialog = imports.ui.unlockDialog;
const WindowManager = imports.ui.windowManager;
const Magnifier = imports.ui.magnifier;
const XdndHandler = imports.ui.xdndHandler;
const Util = imports.misc.util;

const OVERRIDES_SCHEMA = 'org.gnome.shell.overrides';
const DEFAULT_BACKGROUND_COLOR = Clutter.Color.from_pixel(0x2266bbff);

let automountManager = null;
let autorunManager = null;
let panel = null;
let hotCorners = [];
let overview = null;
let runDialog = null;
let lookingGlass = null;
let wm = null;
let messageTray = null;
let screenShield = null;
let notificationDaemon = null;
let windowAttentionHandler = null;
let telepathyClient = null;
let ctrlAltTabManager = null;
let recorder = null;
let sessionMode = null;
let shellDBusService = null;
let shellMountOpDBusService = null;
let screenSaverDBus = null;
let modalCount = 0;
let modalActorFocusStack = [];
let uiGroup = null;
let magnifier = null;
let xdndHandler = null;
let keyboard = null;
let layoutManager = null;
let networkAgent = null;
let _startDate;
let _defaultCssStylesheet = null;
let _cssStylesheet = null;
let _overridesSettings = null;

let background = null;

function createUserSession() {
    // Load the calendar server. Note that we are careful about
    // not loading any events until the user presses the clock
    global.launch_calendar_server();

    telepathyClient = new TelepathyClient.Client();
    automountManager = new AutomountManager.AutomountManager();
    autorunManager = new AutorunManager.AutorunManager();
    networkAgent = new NetworkAgent.NetworkAgent();

    _initRecorder();
}

function createGDMSession() {
    screenShield.showDialog();
}

function createGDMLoginDialog(parentActor) {
    // We do this this here instead of at the top to prevent GDM
    // related code from getting loaded in normal user sessions
    const LoginDialog = imports.gdm.loginDialog;

    let loginDialog = new LoginDialog.LoginDialog(parentActor);
    return [loginDialog, true];
}

function createSessionUnlockDialog(parentActor) {
    let dialog = new UnlockDialog.UnlockDialog(parentActor);
    return [dialog, false];
}

function createInitialSetupSession() {
    networkAgent = new NetworkAgent.NetworkAgent();
}

function _initRecorder() {
    let recorderSettings = new Gio.Settings({ schema: 'org.gnome.shell.recorder' });
    let desktopLockdownSettings = new Gio.Settings({ schema: 'org.gnome.desktop.lockdown' });
    let bindingSettings = new Gio.Settings({ schema: 'org.gnome.shell.keybindings' });

    global.display.add_keybinding('toggle-recording',
                                  bindingSettings,
                                  Meta.KeyBindingFlags.NONE, function() {
        if (recorder == null) {
            recorder = new Shell.Recorder({ stage: global.stage });
        }

        if (recorder.is_recording()) {
            recorder.close();
            Meta.enable_unredirect_for_screen(global.screen);
        } else if (!desktopLockdownSettings.get_boolean('disable-save-to-disk')) {
            // read the parameters from GSettings always in case they have changed
            recorder.set_framerate(recorderSettings.get_int('framerate'));
            /* Translators: this is a filename used for screencast recording */
            // xgettext:no-c-format
            recorder.set_filename(_("Screencast from %d %t") + '.' + recorderSettings.get_string('file-extension'));
            let pipeline = recorderSettings.get_string('pipeline');

            if (!pipeline.match(/^\s*$/))
                recorder.set_pipeline(pipeline);
            else
                recorder.set_pipeline(null);

            Meta.disable_unredirect_for_screen(global.screen);
            recorder.record();
        }
    });
}

function start() {
    // These are here so we don't break compatibility.
    global.logError = window.log;
    global.log = window.log;

    // Chain up async errors reported from C
    global.connect('notify-error', function (global, msg, detail) { notifyError(msg, detail); });

    Gio.DesktopAppInfo.set_desktop_env('GNOME');

    sessionMode = new SessionMode.SessionMode();
    shellDBusService = new ShellDBus.GnomeShell();
    shellMountOpDBusService = new ShellMountOperation.GnomeShellMountOpHandler();

    // Ensure ShellWindowTracker and ShellAppUsage are initialized; this will
    // also initialize ShellAppSystem first.  ShellAppSystem
    // needs to load all the .desktop files, and ShellWindowTracker
    // will use those to associate with windows.  Right now
    // the Monitor doesn't listen for installed app changes
    // and recalculate application associations, so to avoid
    // races for now we initialize it here.  It's better to
    // be predictable anyways.
    let tracker = Shell.WindowTracker.get_default();
    Shell.AppUsage.get_default();

    tracker.connect('startup-sequence-changed', _queueCheckWorkspaces);

    // The stage is always covered so Clutter doesn't need to clear it; however
    // the color is used as the default contents for the Mutter root background
    // actor so set it anyways.
    global.stage.color = DEFAULT_BACKGROUND_COLOR;
    global.stage.no_clear_hint = true;

    _defaultCssStylesheet = global.datadir + '/theme/gnome-shell.css';
    loadTheme();

    // Set up stage hierarchy to group all UI actors under one container.
    uiGroup = new Shell.GenericContainer({ name: 'uiGroup' });
    uiGroup.connect('allocate',
                    function (actor, box, flags) {
                        let children = uiGroup.get_children();
                        for (let i = 0; i < children.length; i++)
                            children[i].allocate_preferred_size(flags);
                    });
    uiGroup.connect('get-preferred-width',
                    function(actor, forHeight, alloc) {
                        let width = global.stage.width;
                        [alloc.min_size, alloc.natural_size] = [width, width];
                    });
    uiGroup.connect('get-preferred-height',
                    function(actor, forWidth, alloc) {
                        let height = global.stage.height;
                        [alloc.min_size, alloc.natural_size] = [height, height];
                    });
    global.window_group.reparent(uiGroup);
    global.overlay_group.reparent(uiGroup);
    global.stage.add_actor(uiGroup);

    layoutManager = new Layout.LayoutManager();
    xdndHandler = new XdndHandler.XdndHandler();
    ctrlAltTabManager = new CtrlAltTab.CtrlAltTabManager();
    overview = new Overview.Overview();
    magnifier = new Magnifier.Magnifier();
    screenShield = new ScreenShield.ScreenShield();
    screenSaverDBus = new ShellDBus.ScreenSaverDBus();
    panel = new Panel.Panel();
    wm = new WindowManager.WindowManager();
    messageTray = new MessageTray.MessageTray();
    keyboard = new Keyboard.Keyboard();
    notificationDaemon = new NotificationDaemon.NotificationDaemon();
    windowAttentionHandler = new WindowAttentionHandler.WindowAttentionHandler();

    sessionMode.createSession();

    panel.startStatusArea();

    layoutManager.init();
    keyboard.init();
    overview.init();

    if (sessionMode.hasWorkspaces)
        global.screen.override_workspace_layout(Meta.ScreenCorner.TOPLEFT,
                                                false, -1, 1);

    if (sessionMode.hasRunDialog) {
        Meta.keybindings_set_custom_handler('panel-run-dialog', function() {
           getRunDialog().open();
        });
    }

    if (sessionMode.hasOverview) {
        Meta.keybindings_set_custom_handler('panel-main-menu', function () {
            overview.toggle();
        });

        global.display.connect('overlay-key',
                               Lang.bind(overview, overview.toggle));
    }

    // Provide the bus object for gnome-session to
    // initiate logouts.
    EndSessionDialog.init();

    // Attempt to become a PolicyKit authentication agent
    PolkitAuthenticationAgent.init()

    // Become a prompter for gnome keyring
    KeyringPrompt.init();

    _startDate = new Date();

    global.stage.connect('captured-event', _globalKeyPressHandler);

    log('GNOME Shell started at ' + _startDate);

    let perfModuleName = GLib.getenv("SHELL_PERF_MODULE");
    if (perfModuleName) {
        let perfOutput = GLib.getenv("SHELL_PERF_OUTPUT");
        let module = eval('imports.perf.' + perfModuleName + ';');
        Scripting.runPerfScript(module, perfOutput);
    }

    _overridesSettings = new Gio.Settings({ schema: OVERRIDES_SCHEMA });
    _overridesSettings.connect('changed::dynamic-workspaces', _queueCheckWorkspaces);

    global.screen.connect('notify::n-workspaces', _nWorkspacesChanged);

    global.screen.connect('window-entered-monitor', _windowEnteredMonitor);
    global.screen.connect('window-left-monitor', _windowLeftMonitor);
    global.screen.connect('restacked', _windowsRestacked);

    _nWorkspacesChanged();

    if (sessionMode.allowExtensions) {
        ExtensionDownloader.init();
        ExtensionSystem.loadExtensions();
    }
}

let _workspaces = [];
let _checkWorkspacesId = 0;

/*
 * When the last window closed on a workspace is a dialog or splash
 * screen, we assume that it might be an initial window shown before
 * the main window of an application, and give the app a grace period
 * where it can map another window before we remove the workspace.
 */
const LAST_WINDOW_GRACE_TIME = 1000;

function _checkWorkspaces() {
    let i;
    let emptyWorkspaces = [];

    if (!Meta.prefs_get_dynamic_workspaces()) {
        _checkWorkspacesId = 0;
        return false;
    }

    for (i = 0; i < _workspaces.length; i++) {
        let lastRemoved = _workspaces[i]._lastRemovedWindow;
        if ((lastRemoved &&
             (lastRemoved.get_window_type() == Meta.WindowType.SPLASHSCREEN ||
              lastRemoved.get_window_type() == Meta.WindowType.DIALOG ||
              lastRemoved.get_window_type() == Meta.WindowType.MODAL_DIALOG)) ||
            _workspaces[i]._keepAliveId)
                emptyWorkspaces[i] = false;
        else
            emptyWorkspaces[i] = true;
    }

    let sequences = Shell.WindowTracker.get_default().get_startup_sequences();
    for (i = 0; i < sequences.length; i++) {
        let index = sequences[i].get_workspace();
        if (index >= 0 && index <= global.screen.n_workspaces)
            emptyWorkspaces[index] = false;
    }

    let windows = global.get_window_actors();
    for (i = 0; i < windows.length; i++) {
        let win = windows[i];

        if (win.get_meta_window().is_on_all_workspaces())
            continue;

        let workspaceIndex = win.get_workspace();
        emptyWorkspaces[workspaceIndex] = false;
    }

    // If we don't have an empty workspace at the end, add one
    if (!emptyWorkspaces[emptyWorkspaces.length -1]) {
        global.screen.append_new_workspace(false, global.get_current_time());
        emptyWorkspaces.push(false);
    }

    let activeWorkspaceIndex = global.screen.get_active_workspace_index();
    let removingCurrentWorkspace = (emptyWorkspaces[activeWorkspaceIndex] &&
                                    activeWorkspaceIndex < emptyWorkspaces.length - 1);
    // Don't enter the overview when removing multiple empty workspaces at startup
    let showOverview  = (removingCurrentWorkspace &&
                         !emptyWorkspaces.every(function(x) { return x; }));

    if (removingCurrentWorkspace) {
        // "Merge" the empty workspace we are removing with the one at the end
        wm.blockAnimations();
    }

    // Delete other empty workspaces; do it from the end to avoid index changes
    for (i = emptyWorkspaces.length - 2; i >= 0; i--) {
        if (emptyWorkspaces[i])
            global.screen.remove_workspace(_workspaces[i], global.get_current_time());
    }

    if (removingCurrentWorkspace) {
        global.screen.get_workspace_by_index(global.screen.n_workspaces - 1).activate(global.get_current_time());
        wm.unblockAnimations();

        if (!overview.visible && showOverview)
            overview.show();
    }

    _checkWorkspacesId = 0;
    return false;
}

function keepWorkspaceAlive(workspace, duration) {
    if (workspace._keepAliveId)
        Mainloop.source_remove(workspace._keepAliveId);

    workspace._keepAliveId = Mainloop.timeout_add(duration, function() {
        workspace._keepAliveId = 0;
        _queueCheckWorkspaces();
        return false;
    });
}

function _windowRemoved(workspace, window) {
    workspace._lastRemovedWindow = window;
    _queueCheckWorkspaces();
    Mainloop.timeout_add(LAST_WINDOW_GRACE_TIME, function() {
        if (workspace._lastRemovedWindow == window) {
            workspace._lastRemovedWindow = null;
            _queueCheckWorkspaces();
        }
        return false;
    });
}

function _windowLeftMonitor(metaScreen, monitorIndex, metaWin) {
    // If the window left the primary monitor, that
    // might make that workspace empty
    if (monitorIndex == layoutManager.primaryIndex)
        _queueCheckWorkspaces();
}

function _windowEnteredMonitor(metaScreen, monitorIndex, metaWin) {
    // If the window entered the primary monitor, that
    // might make that workspace non-empty
    if (monitorIndex == layoutManager.primaryIndex)
        _queueCheckWorkspaces();
}

function _windowsRestacked() {
    // Figure out where the pointer is in case we lost track of
    // it during a grab. (In particular, if a trayicon popup menu
    // is dismissed, see if we need to close the message tray.)
    global.sync_pointer();
}

function _queueCheckWorkspaces() {
    if (_checkWorkspacesId == 0)
        _checkWorkspacesId = Meta.later_add(Meta.LaterType.BEFORE_REDRAW, _checkWorkspaces);
}

function _nWorkspacesChanged() {
    let oldNumWorkspaces = _workspaces.length;
    let newNumWorkspaces = global.screen.n_workspaces;

    if (oldNumWorkspaces == newNumWorkspaces)
        return false;

    let lostWorkspaces = [];
    if (newNumWorkspaces > oldNumWorkspaces) {
        let w;

        // Assume workspaces are only added at the end
        for (w = oldNumWorkspaces; w < newNumWorkspaces; w++)
            _workspaces[w] = global.screen.get_workspace_by_index(w);

        for (w = oldNumWorkspaces; w < newNumWorkspaces; w++) {
            let workspace = _workspaces[w];
            workspace._windowAddedId = workspace.connect('window-added', _queueCheckWorkspaces);
            workspace._windowRemovedId = workspace.connect('window-removed', _windowRemoved);
        }

    } else {
        // Assume workspaces are only removed sequentially
        // (e.g. 2,3,4 - not 2,4,7)
        let removedIndex;
        let removedNum = oldNumWorkspaces - newNumWorkspaces;
        for (let w = 0; w < oldNumWorkspaces; w++) {
            let workspace = global.screen.get_workspace_by_index(w);
            if (_workspaces[w] != workspace) {
                removedIndex = w;
                break;
            }
        }

        let lostWorkspaces = _workspaces.splice(removedIndex, removedNum);
        lostWorkspaces.forEach(function(workspace) {
                                   workspace.disconnect(workspace._windowAddedId);
                                   workspace.disconnect(workspace._windowRemovedId);
                               });
    }

    _queueCheckWorkspaces();

    return false;
}

/**
 * getThemeStylesheet:
 *
 * Get the theme CSS file that the shell will load
 *
 * Returns: A file path that contains the theme CSS,
 *          null if using the default
 */
function getThemeStylesheet()
{
    return _cssStylesheet;
}

/**
 * setThemeStylesheet:
 * @cssStylesheet: A file path that contains the theme CSS,
 *                  set it to null to use the default
 *
 * Set the theme CSS file that the shell will load
 */
function setThemeStylesheet(cssStylesheet)
{
    _cssStylesheet = cssStylesheet;
}

/**
 * loadTheme:
 *
 * Reloads the theme CSS file
 */
function loadTheme() {
    let themeContext = St.ThemeContext.get_for_stage (global.stage);
    let previousTheme = themeContext.get_theme();

    let cssStylesheet = _defaultCssStylesheet;
    if (_cssStylesheet != null)
        cssStylesheet = _cssStylesheet;

    let theme = new St.Theme ({ application_stylesheet: cssStylesheet });

    if (sessionMode.extraStylesheet)
        theme.load_stylesheet(sessionMode.extraStylesheet);

    if (previousTheme) {
        let customStylesheets = previousTheme.get_custom_stylesheets();

        for (let i = 0; i < customStylesheets.length; i++)
            theme.load_stylesheet(customStylesheets[i]);
    }

    themeContext.set_theme (theme);
}

/**
 * notify:
 * @msg: A message
 * @details: Additional information
 */
function notify(msg, details) {
    let source = new MessageTray.SystemNotificationSource();
    messageTray.add(source);
    let notification = new MessageTray.Notification(source, msg, details);
    notification.setTransient(true);
    notification.setShowWhenLocked(true);
    source.notify(notification);
}

/**
 * notifyError:
 * @msg: An error message
 * @details: Additional information
 *
 * See shell_global_notify_problem().
 */
function notifyError(msg, details) {
    // Also print to stderr so it's logged somewhere
    if (details)
        log('error: ' + msg + ': ' + details);
    else
        log('error: ' + msg);

    notify(msg, details);
}

function isWindowActorDisplayedOnWorkspace(win, workspaceIndex) {
    return win.get_workspace() == workspaceIndex ||
        (win.get_meta_window() && win.get_meta_window().is_on_all_workspaces());
}

function getWindowActorsForWorkspace(workspaceIndex) {
    return global.get_window_actors().filter(function (win) {
        return isWindowActorDisplayedOnWorkspace(win, workspaceIndex);
    });
}

// This function encapsulates hacks to make certain global keybindings
// work even when we are in one of our modes where global keybindings
// are disabled with a global grab. (When there is a global grab, then
// all key events will be delivered to the stage, so ::captured-event
// on the stage can be used for global keybindings.)
function _globalKeyPressHandler(actor, event) {
    if (modalCount == 0)
        return false;
    if (event.type() != Clutter.EventType.KEY_PRESS)
        return false;

    if (!sessionMode.allowKeybindingsWhenModal) {
        if (modalCount > (overview.visible ? 1 : 0))
            return false;
    }

    let symbol = event.get_key_symbol();
    let keyCode = event.get_key_code();
    let ignoredModifiers = global.display.get_ignored_modifier_mask();
    let modifierState = event.get_state() & ~ignoredModifiers;

    // This relies on the fact that Clutter.ModifierType is the same as Gdk.ModifierType
    let action = global.display.get_keybinding_action(keyCode, modifierState);

    if (action == Meta.KeyBindingAction.SWITCH_PANELS) {
        ctrlAltTabManager.popup(modifierState & Clutter.ModifierType.SHIFT_MASK,
                                modifierState);
        return true;
    }

    switch (action) {
        // left/right would effectively act as synonyms for up/down if we enabled them;
        // but that could be considered confusing; we also disable them in the main view.
        //
        // case Meta.KeyBindingAction.WORKSPACE_LEFT:
        //  if (!sessionMode.hasWorkspaces)
        //      return false;
        //
        //     wm.actionMoveWorkspaceLeft();
        //     return true;
        // case Meta.KeyBindingAction.WORKSPACE_RIGHT:
        //  if (!sessionMode.hasWorkspaces)
        //      return false;
        //
        //     wm.actionMoveWorkspaceRight();
        //     return true;
        case Meta.KeyBindingAction.WORKSPACE_UP:
            if (!sessionMode.hasWorkspaces)
                return false;

            wm.actionMoveWorkspace(Meta.MotionDirection.UP);
            return true;
        case Meta.KeyBindingAction.WORKSPACE_DOWN:
            if (!sessionMode.hasWorkspaces)
                return false;

            wm.actionMoveWorkspace(Meta.MotionDirection.DOWN);
            return true;
        case Meta.KeyBindingAction.PANEL_RUN_DIALOG:
        case Meta.KeyBindingAction.COMMAND_2:
            if (!sessionMode.hasRunDialog)
                return false;

            getRunDialog().open();
            return true;
        case Meta.KeyBindingAction.PANEL_MAIN_MENU:
        case Meta.KeyBindingAction.OVERLAY_KEY:
            overview.hide();
            return true;
    }

    return false;
}

function _findModal(actor) {
    for (let i = 0; i < modalActorFocusStack.length; i++) {
        if (modalActorFocusStack[i].actor == actor)
            return i;
    }
    return -1;
}

function isInModalStack(actor) {
    return _findModal(actor) != -1;
}

/**
 * pushModal:
 * @actor: #ClutterActor which will be given keyboard focus
 * @timestamp: optional timestamp
 *
 * Ensure we are in a mode where all keyboard and mouse input goes to
 * the stage, and focus @actor. Multiple calls to this function act in
 * a stacking fashion; the effect will be undone when an equal number
 * of popModal() invocations have been made.
 *
 * Next, record the current Clutter keyboard focus on a stack. If the
 * modal stack returns to this actor, reset the focus to the actor
 * which was focused at the time pushModal() was invoked.
 *
 * @timestamp is optionally used to associate the call with a specific user
 * initiated event.  If not provided then the value of
 * global.get_current_time() is assumed.
 *
 * @options: optional Meta.ModalOptions flags to indicate that the
 *           pointer is alrady grabbed
 *
 * Returns: true iff we successfully acquired a grab or already had one
 */
function pushModal(actor, timestamp, options) {
    if (timestamp == undefined)
        timestamp = global.get_current_time();

    if (modalCount == 0) {
        if (!global.begin_modal(timestamp, options ? options : 0)) {
            log('pushModal: invocation of begin_modal failed');
            return false;
        }
        Meta.disable_unredirect_for_screen(global.screen);
    }

    global.set_stage_input_mode(Shell.StageInputMode.FULLSCREEN);

    modalCount += 1;
    let actorDestroyId = actor.connect('destroy', function() {
        let index = _findModal(actor);
        if (index >= 0)
            popModal(actor);
    });
    let curFocus = global.stage.get_key_focus();
    let curFocusDestroyId;
    if (curFocus != null) {
        curFocusDestroyId = curFocus.connect('destroy', function() {
            let index = _findModal(actor);
            if (index >= 0)
                modalActorFocusStack[index].actor = null;
        });
    }
    modalActorFocusStack.push({ actor: actor,
                                focus: curFocus,
                                destroyId: actorDestroyId,
                                focusDestroyId: curFocusDestroyId });

    global.stage.set_key_focus(actor);
    return true;
}

/**
 * popModal:
 * @actor: #ClutterActor passed to original invocation of pushModal().
 * @timestamp: optional timestamp
 *
 * Reverse the effect of pushModal().  If this invocation is undoing
 * the topmost invocation, then the focus will be restored to the
 * previous focus at the time when pushModal() was invoked.
 *
 * @timestamp is optionally used to associate the call with a specific user
 * initiated event.  If not provided then the value of
 * global.get_current_time() is assumed.
 */
function popModal(actor, timestamp) {
    if (timestamp == undefined)
        timestamp = global.get_current_time();

    let focusIndex = _findModal(actor);
    if (focusIndex < 0) {
        global.stage.set_key_focus(null);
        global.end_modal(timestamp);
        global.set_stage_input_mode(Shell.StageInputMode.NORMAL);

        throw new Error('incorrect pop');
    }

    modalCount -= 1;

    let record = modalActorFocusStack[focusIndex];
    record.actor.disconnect(record.destroyId);

    if (focusIndex == modalActorFocusStack.length - 1) {
        if (record.focus)
            record.focus.disconnect(record.focusDestroyId);
        global.stage.set_key_focus(record.focus);
    } else {
        let t = modalActorFocusStack[modalActorFocusStack.length - 1];
        if (t.focus)
            t.focus.disconnect(t.focusDestroyId);
        // Remove from the middle, shift the focus chain up
        for (let i = modalActorFocusStack.length - 1; i > focusIndex; i--) {
            modalActorFocusStack[i].focus = modalActorFocusStack[i - 1].focus;
            modalActorFocusStack[i].focusDestroyId = modalActorFocusStack[i - 1].focusDestroyId;
        }
    }
    modalActorFocusStack.splice(focusIndex, 1);

    if (modalCount > 0)
        return;

    global.end_modal(timestamp);
    global.set_stage_input_mode(Shell.StageInputMode.NORMAL);
    Meta.enable_unredirect_for_screen(global.screen);
}

function createLookingGlass() {
    if (lookingGlass == null) {
        lookingGlass = new LookingGlass.LookingGlass();
    }
    return lookingGlass;
}

function getRunDialog() {
    if (runDialog == null) {
        runDialog = new RunDialog.RunDialog();
    }
    return runDialog;
}

/**
 * activateWindow:
 * @window: the Meta.Window to activate
 * @time: (optional) current event time
 * @workspaceNum: (optional) window's workspace number
 *
 * Activates @window, switching to its workspace first if necessary,
 * and switching out of the overview if it's currently active
 */
function activateWindow(window, time, workspaceNum) {
    let activeWorkspaceNum = global.screen.get_active_workspace_index();
    let windowWorkspaceNum = (workspaceNum !== undefined) ? workspaceNum : window.get_workspace().index();

    if (!time)
        time = global.get_current_time();

    if (windowWorkspaceNum != activeWorkspaceNum) {
        let workspace = global.screen.get_workspace_by_index(windowWorkspaceNum);
        workspace.activate_with_focus(window, time);
    } else {
        window.activate(time);
    }

    overview.hide();
}

// TODO - replace this timeout with some system to guess when the user might
// be e.g. just reading the screen and not likely to interact.
const DEFERRED_TIMEOUT_SECONDS = 20;
var _deferredWorkData = {};
// Work scheduled for some point in the future
var _deferredWorkQueue = [];
// Work we need to process before the next redraw
var _beforeRedrawQueue = [];
// Counter to assign work ids
var _deferredWorkSequence = 0;
var _deferredTimeoutId = 0;

function _runDeferredWork(workId) {
    if (!_deferredWorkData[workId])
        return;
    let index = _deferredWorkQueue.indexOf(workId);
    if (index < 0)
        return;

    _deferredWorkQueue.splice(index, 1);
    _deferredWorkData[workId].callback();
    if (_deferredWorkQueue.length == 0 && _deferredTimeoutId > 0) {
        Mainloop.source_remove(_deferredTimeoutId);
        _deferredTimeoutId = 0;
    }
}

function _runAllDeferredWork() {
    while (_deferredWorkQueue.length > 0)
        _runDeferredWork(_deferredWorkQueue[0]);
}

function _runBeforeRedrawQueue() {
    for (let i = 0; i < _beforeRedrawQueue.length; i++) {
        let workId = _beforeRedrawQueue[i];
        _runDeferredWork(workId);
    }
    _beforeRedrawQueue = [];
}

function _queueBeforeRedraw(workId) {
    _beforeRedrawQueue.push(workId);
    if (_beforeRedrawQueue.length == 1) {
        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, function () {
            _runBeforeRedrawQueue();
            return false;
        });
    }
}

/**
 * initializeDeferredWork:
 * @actor: A #ClutterActor
 * @callback: Function to invoke to perform work
 *
 * This function sets up a callback to be invoked when either the
 * given actor is mapped, or after some period of time when the machine
 * is idle.  This is useful if your actor isn't always visible on the
 * screen (for example, all actors in the overview), and you don't want
 * to consume resources updating if the actor isn't actually going to be
 * displaying to the user.
 *
 * Note that queueDeferredWork is called by default immediately on
 * initialization as well, under the assumption that new actors
 * will need it.
 *
 * Returns: A string work identifer
 */
function initializeDeferredWork(actor, callback, props) {
    // Turn into a string so we can use as an object property
    let workId = '' + (++_deferredWorkSequence);
    _deferredWorkData[workId] = { 'actor': actor,
                                  'callback': callback };
    actor.connect('notify::mapped', function () {
        if (!(actor.mapped && _deferredWorkQueue.indexOf(workId) >= 0))
            return;
        _queueBeforeRedraw(workId);
    });
    actor.connect('destroy', function() {
        let index = _deferredWorkQueue.indexOf(workId);
        if (index >= 0)
            _deferredWorkQueue.splice(index, 1);
        delete _deferredWorkData[workId];
    });
    queueDeferredWork(workId);
    return workId;
}

/**
 * queueDeferredWork:
 * @workId: work identifier
 *
 * Ensure that the work identified by @workId will be
 * run on map or timeout.  You should call this function
 * for example when data being displayed by the actor has
 * changed.
 */
function queueDeferredWork(workId) {
    let data = _deferredWorkData[workId];
    if (!data) {
        let message = 'Invalid work id %d'.format(workId);
        logError(new Error(message), message);
        return;
    }
    if (_deferredWorkQueue.indexOf(workId) < 0)
        _deferredWorkQueue.push(workId);
    if (data.actor.mapped) {
        _queueBeforeRedraw(workId);
        return;
    } else if (_deferredTimeoutId == 0) {
        _deferredTimeoutId = Mainloop.timeout_add_seconds(DEFERRED_TIMEOUT_SECONDS, function () {
            _runAllDeferredWork();
            _deferredTimeoutId = 0;
            return false;
        });
    }
}
