// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported componentManager, notificationDaemon, windowAttentionHandler,
            ctrlAltTabManager, padOsdService, osdWindowManager,
            osdMonitorLabeler, shellMountOpDBusService, shellDBusService,
            shellAccessDialogDBusService, shellAudioSelectionDBusService,
            screenSaverDBus, uiGroup, magnifier, xdndHandler, keyboard,
            kbdA11yDialog, introspectService, start, pushModal, popModal,
            activateWindow, createLookingGlass, initializeDeferredWork,
            getThemeStylesheet, setThemeStylesheet */

const { Clutter, Gio, GLib, GObject, Meta, Shell, St } = imports.gi;

const AccessDialog = imports.ui.accessDialog;
const AudioDeviceSelection = imports.ui.audioDeviceSelection;
const Components = imports.ui.components;
const CtrlAltTab = imports.ui.ctrlAltTab;
const EndSessionDialog = imports.ui.endSessionDialog;
const ExtensionSystem = imports.ui.extensionSystem;
const ExtensionDownloader = imports.ui.extensionDownloader;
const InputMethod = imports.misc.inputMethod;
const Introspect = imports.misc.introspect;
const Keyboard = imports.ui.keyboard;
const MessageTray = imports.ui.messageTray;
const ModalDialog = imports.ui.modalDialog;
const OsdWindow = imports.ui.osdWindow;
const OsdMonitorLabeler = imports.ui.osdMonitorLabeler;
const Overview = imports.ui.overview;
const PadOsd = imports.ui.padOsd;
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
const KbdA11yDialog = imports.ui.kbdA11yDialog;
const LocatePointer = imports.ui.locatePointer;
const PointerA11yTimeout = imports.ui.pointerA11yTimeout;
const ParentalControlsManager = imports.misc.parentalControlsManager;

const A11Y_SCHEMA = 'org.gnome.desktop.a11y.keyboard';
const STICKY_KEYS_ENABLE = 'stickykeys-enable';
const LOG_DOMAIN = 'GNOME Shell';
const GNOMESHELL_STARTED_MESSAGE_ID = 'f3ea493c22934e26811cd62abe8e203a';

var componentManager = null;
var extensionManager = null;
var panel = null;
var overview = null;
var runDialog = null;
var lookingGlass = null;
var wm = null;
var messageTray = null;
var screenShield = null;
var notificationDaemon = null;
var windowAttentionHandler = null;
var ctrlAltTabManager = null;
var padOsdService = null;
var osdWindowManager = null;
var osdMonitorLabeler = null;
var sessionMode = null;
var shellAccessDialogDBusService = null;
var shellAudioSelectionDBusService = null;
var shellDBusService = null;
var shellMountOpDBusService = null;
var screenSaverDBus = null;
var modalCount = 0;
var actionMode = Shell.ActionMode.NONE;
var modalActorFocusStack = [];
var uiGroup = null;
var magnifier = null;
var xdndHandler = null;
var keyboard = null;
var layoutManager = null;
var kbdA11yDialog = null;
var inputMethod = null;
var introspectService = null;
var locatePointer = null;
let _startDate;
let _defaultCssStylesheet = null;
let _cssStylesheet = null;
let _a11ySettings = null;
let _themeResource = null;
let _oskResource = null;

Gio._promisify(Gio._LocalFilePrototype, 'delete_async', 'delete_finish');
Gio._promisify(Gio._LocalFilePrototype, 'touch_async', 'touch_finish');

let _remoteAccessInhibited = false;

function _sessionUpdated() {
    if (sessionMode.isPrimary)
        _loadDefaultStylesheet();

    wm.setCustomKeybindingHandler('panel-main-menu',
                                  Shell.ActionMode.NORMAL |
                                  Shell.ActionMode.OVERVIEW,
                                  sessionMode.hasOverview ? overview.toggle.bind(overview) : null);
    wm.allowKeybinding('overlay-key', Shell.ActionMode.NORMAL |
                                      Shell.ActionMode.OVERVIEW);

    wm.allowKeybinding('locate-pointer-key', Shell.ActionMode.ALL);

    wm.setCustomKeybindingHandler('panel-run-dialog',
                                  Shell.ActionMode.NORMAL |
                                  Shell.ActionMode.OVERVIEW,
                                  sessionMode.hasRunDialog ? openRunDialog : null);

    if (!sessionMode.hasRunDialog) {
        if (runDialog)
            runDialog.close();
        if (lookingGlass)
            lookingGlass.close();
    }

    let remoteAccessController = global.backend.get_remote_access_controller();
    if (remoteAccessController) {
        if (sessionMode.allowScreencast && _remoteAccessInhibited) {
            remoteAccessController.uninhibit_remote_access();
            _remoteAccessInhibited = false;
        } else if (!sessionMode.allowScreencast && !_remoteAccessInhibited) {
            remoteAccessController.inhibit_remote_access();
            _remoteAccessInhibited = true;
        }
    }
}

function start() {
    // These are here so we don't break compatibility.
    global.logError = globalThis.log;
    global.log = globalThis.log;

    // Chain up async errors reported from C
    global.connect('notify-error', (global, msg, detail) => {
        notifyError(msg, detail);
    });

    let currentDesktop = GLib.getenv('XDG_CURRENT_DESKTOP');
    if (!currentDesktop || !currentDesktop.split(':').includes('GNOME'))
        Gio.DesktopAppInfo.set_desktop_env('GNOME');

    sessionMode = new SessionMode.SessionMode();
    sessionMode.connect('updated', _sessionUpdated);

    St.Settings.get().connect('notify::gtk-theme', _loadDefaultStylesheet);

    // Initialize ParentalControlsManager before the UI
    ParentalControlsManager.getDefault();

    _initializeUI();

    shellAccessDialogDBusService = new AccessDialog.AccessDialogDBus();
    shellAudioSelectionDBusService = new AudioDeviceSelection.AudioDeviceSelectionDBus();
    shellDBusService = new ShellDBus.GnomeShell();
    shellMountOpDBusService = new ShellMountOperation.GnomeShellMountOpHandler();

    const watchId = Gio.DBus.session.watch_name('org.gnome.Shell.Notifications',
        Gio.BusNameWatcherFlags.AUTO_START,
        bus => bus.unwatch_name(watchId),
        bus => bus.unwatch_name(watchId));

    _sessionUpdated();
}

function _initializeUI() {
    // Ensure ShellWindowTracker and ShellAppUsage are initialized; this will
    // also initialize ShellAppSystem first. ShellAppSystem
    // needs to load all the .desktop files, and ShellWindowTracker
    // will use those to associate with windows. Right now
    // the Monitor doesn't listen for installed app changes
    // and recalculate application associations, so to avoid
    // races for now we initialize it here. It's better to
    // be predictable anyways.
    Shell.WindowTracker.get_default();
    Shell.AppUsage.get_default();

    reloadThemeResource();
    _loadOskLayouts();
    _loadDefaultStylesheet();

    new AnimationsSettings();

    // Setup the stage hierarchy early
    layoutManager = new Layout.LayoutManager();

    // Various parts of the codebase still refer to Main.uiGroup
    // instead of using the layoutManager. This keeps that code
    // working until it's updated.
    uiGroup = layoutManager.uiGroup;

    padOsdService = new PadOsd.PadOsdService();
    xdndHandler = new XdndHandler.XdndHandler();
    ctrlAltTabManager = new CtrlAltTab.CtrlAltTabManager();
    osdWindowManager = new OsdWindow.OsdWindowManager();
    osdMonitorLabeler = new OsdMonitorLabeler.OsdMonitorLabeler();
    overview = new Overview.Overview();
    kbdA11yDialog = new KbdA11yDialog.KbdA11yDialog();
    wm = new WindowManager.WindowManager();
    magnifier = new Magnifier.Magnifier();
    locatePointer = new LocatePointer.LocatePointer();

    if (LoginManager.canLock())
        screenShield = new ScreenShield.ScreenShield();

    inputMethod = new InputMethod.InputMethod();
    Clutter.get_default_backend().set_input_method(inputMethod);

    messageTray = new MessageTray.MessageTray();
    panel = new Panel.Panel();
    keyboard = new Keyboard.KeyboardManager();
    notificationDaemon = new NotificationDaemon.NotificationDaemon();
    windowAttentionHandler = new WindowAttentionHandler.WindowAttentionHandler();
    componentManager = new Components.ComponentManager();

    introspectService = new Introspect.IntrospectService();

    layoutManager.init();
    overview.init();

    new PointerA11yTimeout.PointerA11yTimeout();

    _a11ySettings = new Gio.Settings({ schema_id: A11Y_SCHEMA });

    global.display.connect('overlay-key', () => {
        if (!_a11ySettings.get_boolean(STICKY_KEYS_ENABLE))
            overview.toggle();
    });

    global.connect('locate-pointer', () => {
        locatePointer.show();
    });

    global.display.connect('show-restart-message', (display, message) => {
        showRestartMessage(message);
        return true;
    });

    global.display.connect('restart', () => {
        global.reexec_self();
        return true;
    });

    global.display.connect('gl-video-memory-purged', loadTheme);

    // Provide the bus object for gnome-session to
    // initiate logouts.
    EndSessionDialog.init();

    // We're ready for the session manager to move to the next phase
    GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
        Shell.util_sd_notify();
        Meta.register_with_session();
        return GLib.SOURCE_REMOVE;
    });

    _startDate = new Date();

    ExtensionDownloader.init();
    extensionManager = new ExtensionSystem.ExtensionManager();
    extensionManager.init();

    if (sessionMode.isGreeter && screenShield) {
        layoutManager.connect('startup-prepared', () => {
            screenShield.showDialog();
        });
    }

    layoutManager.connect('startup-complete', () => {
        if (actionMode == Shell.ActionMode.NONE)
            actionMode = Shell.ActionMode.NORMAL;

        if (screenShield)
            screenShield.lockIfWasLocked();

        if (sessionMode.currentMode != 'gdm' &&
            sessionMode.currentMode != 'initial-setup') {
            GLib.log_structured(LOG_DOMAIN, GLib.LogLevelFlags.LEVEL_MESSAGE, {
                'MESSAGE': 'GNOME Shell started at %s'.format(_startDate),
                'MESSAGE_ID': GNOMESHELL_STARTED_MESSAGE_ID,
            });
        }

        let credentials = new Gio.Credentials();
        if (credentials.get_unix_user() === 0) {
            notify(_('Logged in as a privileged user'),
                   _('Running a session as a privileged user should be avoided for security reasons. If possible, you should log in as a normal user.'));
        }

        if (sessionMode.currentMode !== 'gdm' &&
            sessionMode.currentMode !== 'initial-setup')
            _handleLockScreenWarning();

        LoginManager.registerSessionWithGDM();

        let perfModuleName = GLib.getenv("SHELL_PERF_MODULE");
        if (perfModuleName) {
            let perfOutput = GLib.getenv("SHELL_PERF_OUTPUT");
            let module = eval('imports.perf.%s;'.format(perfModuleName));
            Scripting.runPerfScript(module, perfOutput);
        }
    });
}

async function _handleLockScreenWarning() {
    const path = '%s/lock-warning-shown'.format(global.userdatadir);
    const file = Gio.File.new_for_path(path);

    const hasLockScreen = screenShield !== null;
    if (hasLockScreen) {
        try {
            await file.delete_async(0, null);
        } catch (e) {
            if (!e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.NOT_FOUND))
                logError(e);
        }
    } else {
        try {
            if (!await file.touch_async())
                return;
        } catch (e) {
            logError(e);
        }

        notify(
            _('Screen Lock disabled'),
            _('Screen Locking requires the GNOME display manager.'));
    }
}

function _getStylesheet(name) {
    let stylesheet;

    stylesheet = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/%s'.format(name));
    if (stylesheet.query_exists(null))
        return stylesheet;

    let dataDirs = GLib.get_system_data_dirs();
    for (let i = 0; i < dataDirs.length; i++) {
        let path = GLib.build_filenamev([dataDirs[i], 'gnome-shell', 'theme', name]);
        stylesheet = Gio.file_new_for_path(path);
        if (stylesheet.query_exists(null))
            return stylesheet;
    }

    stylesheet = Gio.File.new_for_path('%s/theme/%s'.format(global.datadir, name));
    if (stylesheet.query_exists(null))
        return stylesheet;

    return null;
}

function _getDefaultStylesheet() {
    let stylesheet = null;
    let name = sessionMode.stylesheetName;

    // Look for a high-contrast variant first when using GTK+'s HighContrast
    // theme
    if (St.Settings.get().gtk_theme == 'HighContrast')
        stylesheet = _getStylesheet(name.replace('.css', '-high-contrast.css'));

    if (stylesheet == null)
        stylesheet = _getStylesheet(sessionMode.stylesheetName);

    return stylesheet;
}

function _loadDefaultStylesheet() {
    let stylesheet = _getDefaultStylesheet();
    if (_defaultCssStylesheet && _defaultCssStylesheet.equal(stylesheet))
        return;

    _defaultCssStylesheet = stylesheet;
    loadTheme();
}

/**
 * getThemeStylesheet:
 *
 * Get the theme CSS file that the shell will load
 *
 * @returns {?Gio.File}: A #GFile that contains the theme CSS,
 *          null if using the default
 */
function getThemeStylesheet() {
    return _cssStylesheet;
}

/**
 * setThemeStylesheet:
 * @param {string=} cssStylesheet: A file path that contains the theme CSS,
 *     set it to null to use the default
 *
 * Set the theme CSS file that the shell will load
 */
function setThemeStylesheet(cssStylesheet) {
    _cssStylesheet = cssStylesheet ? Gio.File.new_for_path(cssStylesheet) : null;
}

function reloadThemeResource() {
    if (_themeResource)
        _themeResource._unregister();

    _themeResource = Gio.Resource.load('%s/%s'.format(global.datadir,
        sessionMode.themeResourceName));
    _themeResource._register();
}

function _loadOskLayouts() {
    _oskResource = Gio.Resource.load('%s/gnome-shell-osk-layouts.gresource'.format(global.datadir));
    _oskResource._register();
}

/**
 * loadTheme:
 *
 * Reloads the theme CSS file
 */
function loadTheme() {
    let themeContext = St.ThemeContext.get_for_stage(global.stage);
    let previousTheme = themeContext.get_theme();

    let theme = new St.Theme({
        application_stylesheet: _cssStylesheet,
        default_stylesheet: _defaultCssStylesheet,
    });

    if (theme.default_stylesheet == null)
        throw new Error("No valid stylesheet found for '%s'".format(sessionMode.stylesheetName));

    if (previousTheme) {
        let customStylesheets = previousTheme.get_custom_stylesheets();

        for (let i = 0; i < customStylesheets.length; i++)
            theme.load_stylesheet(customStylesheets[i]);
    }

    themeContext.set_theme(theme);
}

/**
 * notify:
 * @param {string} msg: A message
 * @param {string} details: Additional information
 */
function notify(msg, details) {
    let source = new MessageTray.SystemNotificationSource();
    messageTray.add(source);
    let notification = new MessageTray.Notification(source, msg, details);
    notification.setTransient(true);
    source.showNotification(notification);
}

/**
 * notifyError:
 * @param {string} msg: An error message
 * @param {string} details: Additional information
 *
 * See shell_global_notify_problem().
 */
function notifyError(msg, details) {
    // Also print to stderr so it's logged somewhere
    if (details)
        log('error: %s: %s'.format(msg, details));
    else
        log('error: %s'.format(msg));

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
 * @param {Clutter.Actor} actor: actor which will be given keyboard focus
 * @param {Object=} params: optional parameters
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
 *               event. If not provided then the value of
 *               global.get_current_time() is assumed.
 *
 *  - options: Meta.ModalOptions flags to indicate that the pointer is
 *             already grabbed
 *
 *  - actionMode: used to set the current Shell.ActionMode to filter
 *                global keybindings; the default of NONE will filter
 *                out all keybindings
 *
 * @returns {bool}: true iff we successfully acquired a grab or already had one
 */
function pushModal(actor, params) {
    params = Params.parse(params, { timestamp: global.get_current_time(),
                                    options: 0,
                                    actionMode: Shell.ActionMode.NONE });

    if (modalCount == 0) {
        if (!global.begin_modal(params.timestamp, params.options)) {
            log('pushModal: invocation of begin_modal failed');
            return false;
        }
        Meta.disable_unredirect_for_display(global.display);
    }

    modalCount += 1;
    let actorDestroyId = actor.connect('destroy', () => {
        let index = _findModal(actor);
        if (index >= 0)
            popModal(actor);
    });

    let prevFocus = global.stage.get_key_focus();
    let prevFocusDestroyId;
    if (prevFocus != null) {
        prevFocusDestroyId = prevFocus.connect('destroy', () => {
            const index = modalActorFocusStack.findIndex(
                record => record.prevFocus === prevFocus);

            if (index >= 0)
                modalActorFocusStack[index].prevFocus = null;
        });
    }
    modalActorFocusStack.push({ actor,
                                destroyId: actorDestroyId,
                                prevFocus,
                                prevFocusDestroyId,
                                actionMode });

    actionMode = params.actionMode;
    global.stage.set_key_focus(actor);
    return true;
}

/**
 * popModal:
 * @param {Clutter.Actor} actor: the actor passed to original invocation
 *     of pushModal()
 * @param {number=} timestamp: optional timestamp
 *
 * Reverse the effect of pushModal(). If this invocation is undoing
 * the topmost invocation, then the focus will be restored to the
 * previous focus at the time when pushModal() was invoked.
 *
 * @timestamp is optionally used to associate the call with a specific user
 * initiated event. If not provided then the value of
 * global.get_current_time() is assumed.
 */
function popModal(actor, timestamp) {
    if (timestamp == undefined)
        timestamp = global.get_current_time();

    let focusIndex = _findModal(actor);
    if (focusIndex < 0) {
        global.stage.set_key_focus(null);
        global.end_modal(timestamp);
        actionMode = Shell.ActionMode.NORMAL;

        throw new Error('incorrect pop');
    }

    modalCount -= 1;

    let record = modalActorFocusStack[focusIndex];
    record.actor.disconnect(record.destroyId);

    if (focusIndex == modalActorFocusStack.length - 1) {
        if (record.prevFocus)
            record.prevFocus.disconnect(record.prevFocusDestroyId);
        actionMode = record.actionMode;
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
            modalActorFocusStack[i].actionMode = modalActorFocusStack[i - 1].actionMode;
        }
    }
    modalActorFocusStack.splice(focusIndex, 1);

    if (modalCount > 0)
        return;

    layoutManager.modalEnded();
    global.end_modal(timestamp);
    Meta.enable_unredirect_for_display(global.display);
    actionMode = Shell.ActionMode.NORMAL;
}

function createLookingGlass() {
    if (lookingGlass == null)
        lookingGlass = new LookingGlass.LookingGlass();

    return lookingGlass;
}

function openRunDialog() {
    if (runDialog == null)
        runDialog = new RunDialog.RunDialog();

    runDialog.open();
}

/**
 * activateWindow:
 * @param {Meta.Window} window: the window to activate
 * @param {number=} time: current event time
 * @param {number=} workspaceNum:  window's workspace number
 *
 * Activates @window, switching to its workspace first if necessary,
 * and switching out of the overview if it's currently active
 */
function activateWindow(window, time, workspaceNum) {
    let workspaceManager = global.workspace_manager;
    let activeWorkspaceNum = workspaceManager.get_active_workspace_index();
    let windowWorkspaceNum = workspaceNum !== undefined ? workspaceNum : window.get_workspace().index();

    if (!time)
        time = global.get_current_time();

    if (windowWorkspaceNum != activeWorkspaceNum) {
        let workspace = workspaceManager.get_workspace_by_index(windowWorkspaceNum);
        workspace.activate_with_focus(window, time);
    } else {
        window.activate(time);
    }

    overview.hide();
    panel.closeCalendar();
}

// TODO - replace this timeout with some system to guess when the user might
// be e.g. just reading the screen and not likely to interact.
var DEFERRED_TIMEOUT_SECONDS = 20;
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
        GLib.source_remove(_deferredTimeoutId);
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
        Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
            _runBeforeRedrawQueue();
            return false;
        });
    }
}

/**
 * initializeDeferredWork:
 * @param {Clutter.Actor} actor: an actor
 * @param {callback} callback: Function to invoke to perform work
 *
 * This function sets up a callback to be invoked when either the
 * given actor is mapped, or after some period of time when the machine
 * is idle. This is useful if your actor isn't always visible on the
 * screen (for example, all actors in the overview), and you don't want
 * to consume resources updating if the actor isn't actually going to be
 * displaying to the user.
 *
 * Note that queueDeferredWork is called by default immediately on
 * initialization as well, under the assumption that new actors
 * will need it.
 *
 * @returns {string}: A string work identifier
 */
function initializeDeferredWork(actor, callback) {
    // Turn into a string so we can use as an object property
    let workId = (++_deferredWorkSequence).toString();
    _deferredWorkData[workId] = { actor,
                                  callback };
    actor.connect('notify::mapped', () => {
        if (!(actor.mapped && _deferredWorkQueue.includes(workId)))
            return;
        _queueBeforeRedraw(workId);
    });
    actor.connect('destroy', () => {
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
 * @param {string} workId: work identifier
 *
 * Ensure that the work identified by @workId will be
 * run on map or timeout. You should call this function
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
    if (!_deferredWorkQueue.includes(workId))
        _deferredWorkQueue.push(workId);
    if (data.actor.mapped) {
        _queueBeforeRedraw(workId);
    } else if (_deferredTimeoutId == 0) {
        _deferredTimeoutId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, DEFERRED_TIMEOUT_SECONDS, () => {
            _runAllDeferredWork();
            _deferredTimeoutId = 0;
            return GLib.SOURCE_REMOVE;
        });
        GLib.Source.set_name_by_id(_deferredTimeoutId, '[gnome-shell] _runAllDeferredWork');
    }
}

var RestartMessage = GObject.registerClass(
class RestartMessage extends ModalDialog.ModalDialog {
    _init(message) {
        super._init({ shellReactive: true,
                      styleClass: 'restart-message headline',
                      shouldFadeIn: false,
                      destroyOnClose: true });

        let label = new St.Label({
            text: message,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
        });

        this.contentLayout.add_child(label);
        this.buttonLayout.hide();
    }
});

function showRestartMessage(message) {
    let restartMessage = new RestartMessage(message);
    restartMessage.open();
}

var AnimationsSettings = class {
    constructor() {
        let backend = global.backend;
        if (!backend.is_rendering_hardware_accelerated()) {
            St.Settings.get().inhibit_animations();
            return;
        }

        let isXvnc = Shell.util_has_x11_display_extension(
            global.display, 'VNC-EXTENSION');
        if (isXvnc) {
            St.Settings.get().inhibit_animations();
            return;
        }

        let remoteAccessController = backend.get_remote_access_controller();
        if (!remoteAccessController)
            return;

        this._handles = new Set();
        remoteAccessController.connect('new-handle',
            (_, handle) => this._onNewRemoteAccessHandle(handle));
    }

    _onRemoteAccessHandleStopped(handle) {
        let settings = St.Settings.get();

        settings.uninhibit_animations();
        this._handles.delete(handle);
    }

    _onNewRemoteAccessHandle(handle) {
        if (!handle.get_disable_animations())
            return;

        let settings = St.Settings.get();

        settings.inhibit_animations();
        this._handles.add(handle);
        handle.connect('stopped', this._onRemoteAccessHandleStopped.bind(this));
    }
};
