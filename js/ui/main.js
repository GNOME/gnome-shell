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

const Components = imports.ui.components;
const CtrlAltTab = imports.ui.ctrlAltTab;
const EndSessionDialog = imports.ui.endSessionDialog;
const Environment = imports.ui.environment;
const ExtensionSystem = imports.ui.extensionSystem;
const ExtensionDownloader = imports.ui.extensionDownloader;
const Keyboard = imports.ui.keyboard;
const MessageTray = imports.ui.messageTray;
const OsdWindow = imports.ui.osdWindow;
const Overview = imports.ui.overview;
const Panel = imports.ui.panel;
const Params = imports.misc.params;
const RunDialog = imports.ui.runDialog;
const Layout = imports.ui.layout;
const LoginManager = imports.misc.loginManager;
const LookingGlass = imports.ui.lookingGlass;
const NotificationDaemon = imports.ui.notificationDaemon;
const WindowAttentionHandler = imports.ui.windowAttentionHandler;
const ScreenShield = imports.ui.screenShield;
const Scripting = imports.ui.scripting;
const SessionMode = imports.ui.sessionMode;
const ShellDBus = imports.ui.shellDBus;
const ShellMountOperation = imports.ui.shellMountOperation;
const WindowManager = imports.ui.windowManager;
const Magnifier = imports.ui.magnifier;
const XdndHandler = imports.ui.xdndHandler;
const Util = imports.misc.util;

const DEFAULT_BACKGROUND_COLOR = Clutter.Color.from_pixel(0x2e3436ff);

let componentManager = null;
let panel = null;
let overview = null;
let runDialog = null;
let lookingGlass = null;
let wm = null;
let messageTray = null;
let screenShield = null;
let notificationDaemon = null;
let windowAttentionHandler = null;
let ctrlAltTabManager = null;
let osdWindow = null;
let sessionMode = null;
let shellDBusService = null;
let shellMountOpDBusService = null;
let screenSaverDBus = null;
let modalCount = 0;
let keybindingMode = Shell.KeyBindingMode.NONE;
let modalActorFocusStack = [];
let uiGroup = null;
let magnifier = null;
let xdndHandler = null;
let keyboard = null;
let layoutManager = null;
let _startDate;
let _defaultCssStylesheet = null;
let _cssStylesheet = null;
let _workspacesSettings = null;

function _sessionUpdated() {
    _loadDefaultStylesheet();

    wm.setCustomKeybindingHandler('panel-main-menu',
                                  Shell.KeyBindingMode.NORMAL |
                                  Shell.KeyBindingMode.OVERVIEW,
                                  sessionMode.hasOverview ? Lang.bind(overview, overview.toggle) : null);
    wm.allowKeybinding('overlay-key', Shell.KeyBindingMode.NORMAL |
                                      Shell.KeyBindingMode.OVERVIEW);

    wm.setCustomKeybindingHandler('panel-run-dialog',
                                  Shell.KeyBindingMode.NORMAL |
                                  Shell.KeyBindingMode.OVERVIEW,
                                  sessionMode.hasRunDialog ? openRunDialog : null);

    if (!sessionMode.hasRunDialog && lookingGlass)
        lookingGlass.close();
}

function start() {
    // These are here so we don't break compatibility.
    global.logError = window.log;
    global.log = window.log;

    // Chain up async errors reported from C
    global.connect('notify-error', function (global, msg, detail) { notifyError(msg, detail); });

    Gio.DesktopAppInfo.set_desktop_env('GNOME');

    sessionMode = new SessionMode.SessionMode();
    sessionMode.connect('sessions-loaded', _sessionsLoaded);
    sessionMode.init();
}

function _sessionsLoaded() {
    sessionMode.connect('updated', _sessionUpdated);
    _initializePrefs();
    _initializeUI();

    shellDBusService = new ShellDBus.GnomeShell();
    shellMountOpDBusService = new ShellMountOperation.GnomeShellMountOpHandler();

    _sessionUpdated();
}

function _initializePrefs() {
    let keys = new Gio.Settings({ schema: sessionMode.overridesSchema }).list_keys();
    for (let i = 0; i < keys.length; i++)
        Meta.prefs_override_preference_schema (keys[i], sessionMode.overridesSchema);

    let workspacesSchema;
    if (keys.indexOf('dynamic-workspaces') > -1)
        workspacesSchema = sessionMode.overridesSchema;
    else
        workspacesSchema = 'org.gnome.mutter';

     _workspacesSettings = new Gio.Settings({ schema: workspacesSchema });
     _workspacesSettings.connect('changed::dynamic-workspaces', _queueCheckWorkspaces);
}

function _initializeUI() {
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

    _loadDefaultStylesheet();

    // Setup the stage hierarchy early
    layoutManager = new Layout.LayoutManager();

    // Various parts of the codebase still refers to Main.uiGroup
    // instead using the layoutManager.  This keeps that code
    // working until it's updated.
    uiGroup = layoutManager.uiGroup;

    xdndHandler = new XdndHandler.XdndHandler();
    ctrlAltTabManager = new CtrlAltTab.CtrlAltTabManager();
    osdWindow = new OsdWindow.OsdWindow();
    overview = new Overview.Overview();
    wm = new WindowManager.WindowManager();
    magnifier = new Magnifier.Magnifier();
    if (LoginManager.canLock())
        screenShield = new ScreenShield.ScreenShield();

    panel = new Panel.Panel();
    messageTray = new MessageTray.MessageTray();
    keyboard = new Keyboard.Keyboard();
    notificationDaemon = new NotificationDaemon.NotificationDaemon();
    windowAttentionHandler = new WindowAttentionHandler.WindowAttentionHandler();
    componentManager = new Components.ComponentManager();

    layoutManager.init();
    overview.init();

    global.screen.override_workspace_layout(Meta.ScreenCorner.TOPLEFT,
                                            false, -1, 1);
    global.display.connect('overlay-key', Lang.bind(overview, overview.toggle));

    // Provide the bus object for gnome-session to
    // initiate logouts.
    EndSessionDialog.init();

    // We're ready for the session manager to move to the next phase
    Meta.register_with_session();

    _startDate = new Date();

    log('GNOME Shell started at ' + _startDate);

    let perfModuleName = GLib.getenv("SHELL_PERF_MODULE");
    if (perfModuleName) {
        let perfOutput = GLib.getenv("SHELL_PERF_OUTPUT");
        let module = eval('imports.perf.' + perfModuleName + ';');
        Scripting.runPerfScript(module, perfOutput);
    }

    global.screen.connect('notify::n-workspaces', _nWorkspacesChanged);

    global.screen.connect('window-entered-monitor', _windowEnteredMonitor);
    global.screen.connect('window-left-monitor', _windowLeftMonitor);
    global.screen.connect('restacked', _windowsRestacked);

    _nWorkspacesChanged();

    ExtensionDownloader.init();
    ExtensionSystem.init();

    if (sessionMode.isGreeter && screenShield) {
        layoutManager.connect('startup-prepared', function() {
            screenShield.showDialog();
        });
    }

    layoutManager.connect('startup-complete', function() {
                              if (keybindingMode == Shell.KeyBindingMode.NONE) {
                                  keybindingMode = Shell.KeyBindingMode.NORMAL;
                              }
                          });
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

function _loadDefaultStylesheet() {
    if (!sessionMode.isPrimary)
        return;

    let stylesheet = global.datadir + '/theme/' + sessionMode.stylesheetName;
    if (_defaultCssStylesheet == stylesheet)
        return;

    _defaultCssStylesheet = stylesheet;
    loadTheme();
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

function _findModal(actor) {
    for (let i = 0; i < modalActorFocusStack.length; i++) {
        if (modalActorFocusStack[i].actor == actor)
            return i;
    }
    return -1;
}

/**
 * pushModal:
 * @actor: #ClutterActor which will be given keyboard focus
 * @params: optional parameters
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
 * @params may be used to provide the following parameters:
 *  - timestamp: used to associate the call with a specific user initiated
 *               event.  If not provided then the value of
 *               global.get_current_time() is assumed.
 *
 *  - options: Meta.ModalOptions flags to indicate that the pointer is
 *             already grabbed
 *
 *  - keybindingMode: used to set the current Shell.KeyBindingMode to filter
 *                    global keybindings; the default of NONE will filter
 *                    out all keybindings
 *
 * Returns: true iff we successfully acquired a grab or already had one
 */
function pushModal(actor, params) {
    params = Params.parse(params, { timestamp: global.get_current_time(),
                                    options: 0,
                                    keybindingMode: Shell.KeyBindingMode.NONE });

    if (modalCount == 0) {
        if (!global.begin_modal(params.timestamp, params.options)) {
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

    let prevFocus = global.stage.get_key_focus();
    let prevFocusDestroyId;
    if (prevFocus != null) {
        prevFocusDestroyId = prevFocus.connect('destroy', function() {
            let index = _findModal(actor);
            if (index >= 0)
                modalActorFocusStack[index].prevFocus = null;
        });
    }
    modalActorFocusStack.push({ actor: actor,
                                destroyId: actorDestroyId,
                                prevFocus: prevFocus,
                                prevFocusDestroyId: prevFocusDestroyId,
                                keybindingMode: keybindingMode });

    keybindingMode = params.keybindingMode;
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
        keybindingMode = Shell.KeyBindingMode.NORMAL;

        throw new Error('incorrect pop');
    }

    modalCount -= 1;

    let record = modalActorFocusStack[focusIndex];
    record.actor.disconnect(record.destroyId);

    if (focusIndex == modalActorFocusStack.length - 1) {
        if (record.prevFocus)
            record.prevFocus.disconnect(record.prevFocusDestroyId);
        keybindingMode = record.keybindingMode;
        global.stage.set_key_focus(record.prevFocus);
    } else {
        // If we have:
        //     global.stage.set_focus(a);
        //     Main.pushModal(b);
        //     Main.pushModal(c);
        //     Main.pushModal(d);
        //
        // then we have the stack:
        //     [{ prevFocus: a, actor: b },
        //      { prevFocus: b, actor: c },
        //      { prevFocus: c, actor: d }]
        //
        // When actor c is destroyed/popped, if we only simply remove the
        // record, then the focus stack will be [a, c], rather than the correct
        // [a, b]. Shift the focus stack up before removing the record to ensure
        // that we get the correct result.
        let t = modalActorFocusStack[modalActorFocusStack.length - 1];
        if (t.prevFocus)
            t.prevFocus.disconnect(t.prevFocusDestroyId);
        // Remove from the middle, shift the focus chain up
        for (let i = modalActorFocusStack.length - 1; i > focusIndex; i--) {
            modalActorFocusStack[i].prevFocus = modalActorFocusStack[i - 1].prevFocus;
            modalActorFocusStack[i].prevFocusDestroyId = modalActorFocusStack[i - 1].prevFocusDestroyId;
            modalActorFocusStack[i].keybindingMode = modalActorFocusStack[i - 1].keybindingMode;
        }
    }
    modalActorFocusStack.splice(focusIndex, 1);

    if (modalCount > 0)
        return;

    global.end_modal(timestamp);
    global.set_stage_input_mode(Shell.StageInputMode.NORMAL);
    Meta.enable_unredirect_for_screen(global.screen);
    keybindingMode = Shell.KeyBindingMode.NORMAL;
}

function createLookingGlass() {
    if (lookingGlass == null) {
        lookingGlass = new LookingGlass.LookingGlass();
    }
    return lookingGlass;
}

function openRunDialog() {
    if (runDialog == null) {
        runDialog = new RunDialog.RunDialog();
    }
    runDialog.open();
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
