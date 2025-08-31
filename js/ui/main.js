import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GioUnix from 'gi://GioUnix';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as AccessDialog from './accessDialog.js';
import * as AudioDeviceSelection from './audioDeviceSelection.js';
import * as BreakManager from '../misc/breakManager.js';
import * as BrightnessManager from '../misc/brightnessManager.js';
import * as Config from '../misc/config.js';
import * as Components from './components.js';
import * as CtrlAltTab from './ctrlAltTab.js';
import * as EndSessionDialog from './endSessionDialog.js';
import * as ExtensionSystem from './extensionSystem.js';
import * as ExtensionDownloader from './extensionDownloader.js';
import * as InputMethod from '../misc/inputMethod.js';
import * as Introspect from '../misc/introspect.js';
import * as Keyboard from './keyboard.js';
import * as MessageTray from './messageTray.js';
import * as ModalDialog from './modalDialog.js';
import * as OsdWindow from './osdWindow.js';
import * as OsdMonitorLabeler from './osdMonitorLabeler.js';
import * as Overview from './overview.js';
import * as PadOsd from './padOsd.js';
import * as Panel from './panel.js';
import * as RunDialog from './runDialog.js';
import * as WelcomeDialog from './welcomeDialog.js';
import * as Layout from './layout.js';
import * as LoginManager from '../misc/loginManager.js';
import * as LookingGlass from './lookingGlass.js';
import * as NotificationDaemon from './notificationDaemon.js';
import * as WindowAttentionHandler from './windowAttentionHandler.js';
import * as Screenshot from './screenshot.js';
import * as ScreenShield from './screenShield.js';
import * as SessionMode from './sessionMode.js';
import * as ShellDBus from './shellDBus.js';
import * as ShellMountOperation from './shellMountOperation.js';
import * as TimeLimitsManager from '../misc/timeLimitsManager.js';
import * as WindowManager from './windowManager.js';
import * as Magnifier from './magnifier.js';
import * as XdndHandler from './xdndHandler.js';
import * as KbdA11yDialog from './kbdA11yDialog.js';
import * as LocatePointer from './locatePointer.js';
import * as PointerA11yTimeout from './pointerA11yTimeout.js';
import {formatError} from '../misc/errorUtils.js';
import * as ParentalControlsManager from '../misc/parentalControlsManager.js';
import * as Util from '../misc/util.js';

const WELCOME_DIALOG_LAST_SHOWN_VERSION = 'welcome-dialog-last-shown-version';
// Make sure to mention the point release, otherwise it will show every time
// until this version is current
const WELCOME_DIALOG_LAST_TOUR_CHANGE = '40.beta';
const LOG_DOMAIN = 'GNOME Shell';
const GNOMESHELL_STARTED_MESSAGE_ID = 'f3ea493c22934e26811cd62abe8e203a';

export let componentManager = null;
export let extensionManager = null;
export let panel = null;
export let overview = null;
export let runDialog = null;
export let lookingGlass = null;
export let welcomeDialog = null;
export let wm = null;
export let messageTray = null;
export let screenShield = null;
export let notificationDaemon = null;
export let windowAttentionHandler = null;
export let ctrlAltTabManager = null;
export let padOsdService = null;
export let osdWindowManager = null;
export let osdMonitorLabeler = null;
export let sessionMode = null;
export let screenshotUI = null;
export let shellAccessDialogDBusService = null;
export let shellAudioSelectionDBusService = null;
export let shellDBusService = null;
export let shellMountOpDBusService = null;
export let screenSaverDBus = null;
export let modalCount = 0;
export let actionMode = Shell.ActionMode.NONE;
export let modalActorFocusStack = [];
export let uiGroup = null;
export let magnifier = null;
export let xdndHandler = null;
export let keyboard = null;
export let layoutManager = null;
export let kbdA11yDialog = null;
export let inputMethod = null;
export let introspectService = null;
export let locatePointer = null;
export let endSessionDialog = null;
export let breakManager = null;
export let screenTimeDBus = null;
export let breakManagerDispatcher = null;
export let timeLimitsManager = null;
export let timeLimitsDispatcher = null;
export let brightnessManager = null;
export let brightnessDBus = null;

let _startDate;
let _defaultCssStylesheet = null;
let _cssStylesheet = null;
let _themeResource = null;
let _oskResource = null;
let _iconResource = null;
let _workspacesAdjustment = null;
let _workspaceAdjustmentRegistry = null;

Gio._promisify(Gio.File.prototype, 'delete_async');
Gio._promisify(Gio.File.prototype, 'touch_async');

let _remoteAccessInhibited = false;

function _sessionUpdated() {
    if (sessionMode.isPrimary)
        _loadDefaultStylesheet();

    wm.allowKeybinding('overlay-key',
        Shell.ActionMode.NORMAL | Shell.ActionMode.OVERVIEW);

    wm.allowKeybinding('locate-pointer-key', Shell.ActionMode.ALL);

    wm.setCustomKeybindingHandler('panel-run-dialog',
        Shell.ActionMode.NORMAL | Shell.ActionMode.OVERVIEW,
        sessionMode.hasRunDialog ? openRunDialog : null);

    if (!sessionMode.hasRunDialog) {
        if (runDialog)
            runDialog.close();
        if (lookingGlass)
            lookingGlass.close();
        if (welcomeDialog)
            welcomeDialog.close();
    }

    let remoteAccessController = global.backend.get_remote_access_controller();
    if (remoteAccessController && !global.backend.is_headless()) {
        if (sessionMode.allowScreencast && _remoteAccessInhibited) {
            remoteAccessController.uninhibit_remote_access();
            _remoteAccessInhibited = false;
        } else if (!sessionMode.allowScreencast && !_remoteAccessInhibited) {
            remoteAccessController.inhibit_remote_access();
            _remoteAccessInhibited = true;
        }
    }
}

/** @returns {void} */
export async function start() {
    globalThis.log = console.log;
    globalThis.logError = function (err, msg) {
        const args = [formatError(err)];
        try {
            // toString() can throw
            if (msg)
                args.unshift(`${msg}:`);
        } catch {}

        console.error(...args);
    };

    // Chain up async errors reported from C
    global.connect('notify-error', (global, msg, detail) => {
        notifyError(msg, detail);
    });

    let currentDesktop = GLib.getenv('XDG_CURRENT_DESKTOP');
    if (!currentDesktop || !currentDesktop.split(':').includes('GNOME'))
        GioUnix.DesktopAppInfo.set_desktop_env('GNOME');

    sessionMode = new SessionMode.SessionMode();
    sessionMode.connect('updated', _sessionUpdated);

    St.Settings.get().connect('notify::high-contrast', _loadDefaultStylesheet);
    St.Settings.get().connect('notify::color-scheme', _loadDefaultStylesheet);

    // Initialize ParentalControlsManager before the UI
    ParentalControlsManager.getDefault();

    await _initializeUI();

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

/** @private */
async function _initializeUI() {
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
    _loadIcons();
    _loadOskLayouts();
    _loadDefaultStylesheet();
    _loadWorkspacesAdjustment();

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
    global.stage.context.get_backend().set_input_method(inputMethod);
    global.connect('shutdown',
        () => global.stage.context.get_backend().set_input_method(null));

    screenshotUI = new Screenshot.ScreenshotUI();

    messageTray = new MessageTray.MessageTray();
    panel = new Panel.Panel();
    keyboard = new Keyboard.KeyboardManager();
    notificationDaemon = new NotificationDaemon.NotificationDaemon();
    windowAttentionHandler = new WindowAttentionHandler.WindowAttentionHandler();
    componentManager = new Components.ComponentManager();

    introspectService = new Introspect.IntrospectService();

    // Set up the global default break reminder manager and its D-Bus interface
    breakManager = new BreakManager.BreakManager();
    timeLimitsManager = new TimeLimitsManager.TimeLimitsManager();
    screenTimeDBus = new ShellDBus.ScreenTimeDBus(breakManager);
    breakManagerDispatcher = new BreakManager.BreakDispatcher(breakManager);
    timeLimitsDispatcher = new TimeLimitsManager.TimeLimitsDispatcher(timeLimitsManager);

    brightnessManager = new BrightnessManager.BrightnessManager();
    brightnessDBus = new ShellDBus.BrightnessDBus(brightnessManager);

    global.connect('shutdown', () => {
        // Block shutdown until the session history file has been written
        const loop = new GLib.MainLoop(null, false);
        const source = GLib.idle_source_new();
        source.set_callback(() => {
            timeLimitsManager.shutdown()
                .catch(e => console.warn(`Failed to stop time limits manager: ${e.message}`))
                .finally(() => loop.quit());
            return GLib.SOURCE_REMOVE;
        });
        source.attach(loop.get_context());
        loop.run();
    });

    layoutManager.init();
    overview.init();

    new PointerA11yTimeout.PointerA11yTimeout();

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

    global.context.connect('notify::unsafe-mode', () => {
        if (!global.context.unsafe_mode)
            return; // we're safe
        if (lookingGlass?.isOpen)
            return; // assume user action

        const source = MessageTray.getSystemSource();
        const notification = new MessageTray.Notification({
            source,
            title: _('System was put in unsafe mode'),
            body: _('Apps now have unrestricted access'),
            isTransient: true,
        });
        notification.addAction(_('Undo'),
            () => (global.context.unsafe_mode = false));
        source.addNotification(notification);
    });

    // Provide the bus object for gnome-session to
    // initiate logouts.
    endSessionDialog = new EndSessionDialog.EndSessionDialog();

    // We're ready for the session manager to move to the next phase
    GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
        Shell.util_sd_notify();
        global.context.notify_ready();
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

    let Scripting;
    let perfModule;
    const {automationScript} = global;
    if (automationScript) {
        Scripting = await import('./scripting.js');
        perfModule = await import(automationScript.get_uri());
        if (perfModule.init)
            perfModule.init();
    }

    layoutManager.connect('startup-complete', () => {
        if (actionMode === Shell.ActionMode.NONE)
            actionMode = Shell.ActionMode.NORMAL;

        if (screenShield)
            screenShield.lockIfWasLocked();

        if (sessionMode.currentMode !== 'gdm' &&
            sessionMode.currentMode !== 'initial-setup') {
            GLib.log_structured(LOG_DOMAIN, GLib.LogLevelFlags.LEVEL_MESSAGE, {
                'MESSAGE': `GNOME Shell started at ${_startDate}`,
                'MESSAGE_ID': GNOMESHELL_STARTED_MESSAGE_ID,
            });
        }

        if (!perfModule) {
            let credentials = new Gio.Credentials();
            if (credentials.get_unix_user() === 0) {
                notify(
                    _('Logged in as a privileged user'),
                    _('Running a session as a privileged user should be avoided for security reasons. If possible, you should log in as a normal user.'));
            } else if (sessionMode.showWelcomeDialog) {
                _handleShowWelcomeScreen();
            }
        }

        if (sessionMode.currentMode !== 'gdm' &&
            sessionMode.currentMode !== 'initial-setup')
            _handleLockScreenWarning();

        LoginManager.registerSessionWithGDM();

        if (perfModule) {
            let perfOutput = GLib.getenv('SHELL_PERF_OUTPUT');
            Scripting.runPerfScript(perfModule, perfOutput);
        }
    });
}

function _handleShowWelcomeScreen() {
    const lastShownVersion = global.settings.get_string(WELCOME_DIALOG_LAST_SHOWN_VERSION);
    if (Util.GNOMEversionCompare(WELCOME_DIALOG_LAST_TOUR_CHANGE, lastShownVersion) > 0) {
        openWelcomeDialog();
        global.settings.set_string(WELCOME_DIALOG_LAST_SHOWN_VERSION, Config.PACKAGE_VERSION);
    }
}

async function _handleLockScreenWarning() {
    const path = `${global.userdatadir}/lock-warning-shown`;
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
            _('Screen Locking requires the GNOME display manager'));
    }
}

function _getStylesheet(name) {
    let stylesheet;

    stylesheet = Gio.File.new_for_uri(`resource:///org/gnome/shell/theme/${name}`);
    if (stylesheet.query_exists(null))
        return stylesheet;

    let dataDirs = GLib.get_system_data_dirs();
    for (let i = 0; i < dataDirs.length; i++) {
        let path = GLib.build_filenamev([dataDirs[i], 'gnome-shell', 'theme', name]);
        stylesheet = Gio.file_new_for_path(path);
        if (stylesheet.query_exists(null))
            return stylesheet;
    }

    stylesheet = Gio.File.new_for_path(`${global.datadir}/theme/${name}`);
    if (stylesheet.query_exists(null))
        return stylesheet;

    return null;
}

/** @returns {string} */
export function getStyleVariant() {
    const {colorScheme} = St.Settings.get();
    switch (sessionMode.colorScheme) {
    case 'force-dark':
        return 'dark';
    case 'force-light':
        return 'light';
    case 'prefer-dark':
        return colorScheme === St.SystemColorScheme.PREFER_LIGHT
            ? 'light' : 'dark';
    case 'prefer-light':
        return colorScheme === St.SystemColorScheme.PREFER_DARK
            ? 'dark' : 'light';
    default:
        return '';
    }
}

function _getDefaultStylesheet() {
    let stylesheet = null;
    let name = sessionMode.stylesheetName;

    // Look for a high-contrast variant first
    if (St.Settings.get().high_contrast)
        stylesheet = _getStylesheet(name.replace('.css', '-high-contrast.css'));

    if (stylesheet === null)
        stylesheet = _getStylesheet(name.replace('.css', `-${getStyleVariant()}.css`));

    if (stylesheet == null)
        stylesheet = _getStylesheet(name);

    return stylesheet;
}

function _loadDefaultStylesheet() {
    let stylesheet = _getDefaultStylesheet();
    if (_defaultCssStylesheet && _defaultCssStylesheet.equal(stylesheet))
        return;

    _defaultCssStylesheet = stylesheet;
    loadTheme();
}

class AdjustmentRegistry {
    #count = 0;
    #adjustments = new Map();
    #registry = new FinalizationRegistry(key => {
        this.#adjustments.delete(key);
    });

    register(adj) {
        const key = this.#count++;
        this.#adjustments.set(key, new WeakRef(adj));
        this.#registry.register(adj, key);
    }

    forEach(callback) {
        this.#adjustments.forEach((ref, key) => {
            const adj = ref.deref();
            if (adj)
                callback(adj);
            else
                this.#adjustments.delete(key);
        });
    }
}

function _loadWorkspacesAdjustment() {
    const {workspaceManager} = global;
    const activeWorkspaceIndex = workspaceManager.get_active_workspace_index();

    _workspacesAdjustment = new St.Adjustment({
        value: activeWorkspaceIndex,
        lower: 0,
        page_increment: 1,
        page_size: 1,
        step_increment: 0,
        upper: workspaceManager.n_workspaces,
    });

    workspaceManager.bind_property('n-workspaces',
        _workspacesAdjustment, 'upper',
        GObject.BindingFlags.SYNC_CREATE);

    _workspacesAdjustment.connect('notify::upper', () => {
        const newActiveIndex = workspaceManager.get_active_workspace_index();

        // A workspace might have been inserted or removed before the active
        // one, causing the adjustment to go out of sync, so update the value
        _workspaceAdjustmentRegistry.forEach(c => c.remove_transition('value'));
        _workspacesAdjustment.remove_transition('value');
        _workspacesAdjustment.value = newActiveIndex;
    });

    _workspaceAdjustmentRegistry = new AdjustmentRegistry();
}

/**
 * Creates an adjustment that has its lower, upper, and value
 * properties set for the number of available workspaces. Consumers
 * of the returned adjustment must only change the 'value' property,
 * and only that.
 *
 * @param {Clutter.Actor} actor
 *
 * @returns {St.Adjustment} - an adjustment representing the
 * current workspaces layout
 */
export function createWorkspacesAdjustment(actor) {
    const adjustment = new St.Adjustment({actor});

    const properties = [
        ['lower', GObject.BindingFlags.SYNC_CREATE],
        ['page-increment', GObject.BindingFlags.SYNC_CREATE],
        ['page-size', GObject.BindingFlags.SYNC_CREATE],
        ['step-increment', GObject.BindingFlags.SYNC_CREATE],
        ['upper', GObject.BindingFlags.SYNC_CREATE],
        ['value', GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL],
    ];

    for (const [propName, flags] of properties)
        _workspacesAdjustment.bind_property(propName, adjustment, propName, flags);

    _workspaceAdjustmentRegistry.register(adjustment);

    return adjustment;
}

/**
 * Get the theme CSS file that the shell will load
 *
 * @returns {?Gio.File}: A #GFile that contains the theme CSS,
 *          null if using the default
 */
export function getThemeStylesheet() {
    return _cssStylesheet;
}

/**
 * Set the theme CSS file that the shell will load
 *
 * @param {string=} cssStylesheet - A file path that contains the theme CSS,
 *     set it to null to use the default
 */
export function setThemeStylesheet(cssStylesheet) {
    _cssStylesheet = cssStylesheet ? Gio.File.new_for_path(cssStylesheet) : null;
}

export function reloadThemeResource() {
    if (_themeResource)
        _themeResource._unregister();

    _themeResource = Gio.Resource.load(
        `${global.datadir}/${sessionMode.themeResourceName}`);
    _themeResource._register();
}

/** @private */
function _loadIcons() {
    _iconResource = Gio.Resource.load(`${global.datadir}/gnome-shell-icons.gresource`);
    _iconResource._register();
}

function _loadOskLayouts() {
    _oskResource = Gio.Resource.load(`${global.datadir}/gnome-shell-osk-layouts.gresource`);
    _oskResource._register();
}

/**
 * loadTheme:
 *
 * Reloads the theme CSS file
 */
export function loadTheme() {
    let themeContext = St.ThemeContext.get_for_stage(global.stage);
    let previousTheme = themeContext.get_theme();

    let theme = new St.Theme({
        application_stylesheet: _cssStylesheet,
        default_stylesheet: _defaultCssStylesheet,
    });

    if (theme.default_stylesheet == null)
        throw new Error(`No valid stylesheet found for '${sessionMode.stylesheetName}'`);

    if (previousTheme) {
        let customStylesheets = previousTheme.get_custom_stylesheets();

        for (let i = 0; i < customStylesheets.length; i++)
            theme.load_stylesheet(customStylesheets[i]);
    }

    themeContext.set_theme(theme);
}

/**
 * @param {string} msg A message
 * @param {string=} details Additional information
 */
export function notify(msg, details = null) {
    const source = MessageTray.getSystemSource();
    const notification = new MessageTray.Notification({
        source,
        title: msg,
        body: details,
        isTransient: true,
    });
    source.addNotification(notification);
}

/**
 * See shell_global_notify_problem().
 *
 * @param {string} msg - An error message
 * @param {string} details - Additional information
 */
export function notifyError(msg, details) {
    // Also print to stderr so it's logged somewhere
    if (details)
        console.warn(`error: ${msg}: ${details}`);
    else
        console.warn(`error: ${msg}`);

    notify(msg, details);
}

/**
 * @private
 * @param {Clutter.Grab} grab - grab
 */
function _findModal(grab) {
    for (let i = 0; i < modalActorFocusStack.length; i++) {
        if (modalActorFocusStack[i].grab === grab)
            return i;
    }
    return -1;
}

/**
 * Ensure we are in a mode where all keyboard and mouse input goes to
 * the stage, and focus @actor. Multiple calls to this function act in
 * a stacking fashion; the effect will be undone when an equal number
 * of popModal() invocations have been made.
 *
 * Next, record the current Clutter keyboard focus on a stack. If the
 * modal stack returns to this actor, reset the focus to the actor
 * which was focused at the time pushModal() was invoked.
 *
 * `params` may be used to provide the following parameters:
 *  - actionMode: used to set the current Shell.ActionMode to filter
 *                global keybindings; the default of NONE will filter
 *                out all keybindings
 *
 * @param {Clutter.Actor} actor - actor which will be given keyboard focus
 * @param {object=} params - optional parameters
 * @returns {Clutter.Grab} - the grab handle created
 */
export function pushModal(actor, params = {}) {
    const {actionMode: newActionMode} = {
        actionMode: Shell.ActionMode.NONE,
        ...params,
    };

    let grab = global.stage.grab(actor);

    if (modalCount === 0)
        global.compositor.disable_unredirect();

    modalCount += 1;
    let actorDestroyId = actor.connect('destroy', () => {
        let index = _findModal(grab);
        if (index >= 0)
            popModal(grab);
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
    modalActorFocusStack.push({
        actor,
        grab,
        destroyId: actorDestroyId,
        prevFocus,
        prevFocusDestroyId,
        actionMode,
    });

    actionMode = newActionMode;
    const newFocus = actor === global.stage ? null : actor;
    global.stage.set_key_focus(newFocus);
    return grab;
}

/**
 * Reverse the effect of pushModal(). If this invocation is undoing
 * the topmost invocation, then the focus will be restored to the
 * previous focus at the time when pushModal() was invoked.
 *
 * @param {Clutter.Grab} grab - the grab given by pushModal()
 */
export function popModal(grab) {
    let focusIndex = _findModal(grab);
    if (focusIndex < 0) {
        global.stage.set_key_focus(null);
        actionMode = Shell.ActionMode.NORMAL;

        throw new Error('incorrect pop');
    }

    modalCount -= 1;

    let record = modalActorFocusStack[focusIndex];
    record.actor.disconnect(record.destroyId);

    record.grab.dismiss();

    if (focusIndex === modalActorFocusStack.length - 1) {
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
    global.compositor.enable_unredirect();
    actionMode = Shell.ActionMode.NORMAL;
}

/**
 * Creates the looking glass panel
 *
 * @returns {LookingGlass.LookingGlass}
 */
export function createLookingGlass() {
    if (lookingGlass == null)
        lookingGlass = new LookingGlass.LookingGlass();

    return lookingGlass;
}

/**
 * Opens the run dialog
 */
export function openRunDialog() {
    if (runDialog == null)
        runDialog = new RunDialog.RunDialog();

    runDialog.open();
}

export function openWelcomeDialog() {
    if (welcomeDialog === null)
        welcomeDialog = new WelcomeDialog.WelcomeDialog();

    welcomeDialog.open();
}

/**
 * activateWindow:
 *
 * @param {Meta.Window} window the window to activate
 * @param {number=} time current event time
 * @param {number=} workspaceNum  window's workspace number
 *
 * Activates @window, switching to its workspace first if necessary,
 * and switching out of the overview if it's currently active
 */
export function activateWindow(window, time, workspaceNum) {
    let workspaceManager = global.workspace_manager;
    let activeWorkspaceNum = workspaceManager.get_active_workspace_index();
    let windowWorkspaceNum = workspaceNum !== undefined ? workspaceNum : window.get_workspace().index();

    if (!time)
        time = global.get_current_time();

    if (windowWorkspaceNum !== activeWorkspaceNum) {
        let workspace = workspaceManager.get_workspace_by_index(windowWorkspaceNum);
        workspace.activate_with_focus(window, time);
    } else {
        window.activate(time);
    }

    overview.hide();
    panel.closeCalendar();
}

/**
 * Move @window to the specified monitor and workspace.
 *
 * @param {Meta.Window} window - the window to move
 * @param {number} monitorIndex - the requested monitor
 * @param {number} workspaceIndex - the requested workspace
 * @param {bool} append - create workspace if it doesn't exist
 */
export function moveWindowToMonitorAndWorkspace(window, monitorIndex, workspaceIndex, append = false) {
    // We need to move the window before changing the workspace, because
    // the move itself could cause a workspace change if the window enters
    // the primary monitor
    if (window.get_monitor() !== monitorIndex) {
        // Wait for the monitor change to take effect
        const id = global.display.connect('window-entered-monitor',
            (dsp, num, w) => {
                if (w !== window)
                    return;
                window.change_workspace_by_index(workspaceIndex, append);
                global.display.disconnect(id);
            });
        window.move_to_monitor(monitorIndex);
    } else {
        window.change_workspace_by_index(workspaceIndex, append);
    }
}

// TODO - replace this timeout with some system to guess when the user might
// be e.g. just reading the screen and not likely to interact.
const DEFERRED_TIMEOUT_SECONDS = 20;
let _deferredWorkData = {};
// Work scheduled for some point in the future
let _deferredWorkQueue = [];
// Work we need to process before the next redraw
let _beforeRedrawQueue = [];
// Counter to assign work ids
let _deferredWorkSequence = 0;
let _deferredTimeoutId = 0;

function _runDeferredWork(workId) {
    if (!_deferredWorkData[workId])
        return;
    let index = _deferredWorkQueue.indexOf(workId);
    if (index < 0)
        return;

    _deferredWorkQueue.splice(index, 1);
    _deferredWorkData[workId].callback();
    if (_deferredWorkQueue.length === 0 && _deferredTimeoutId > 0) {
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
    if (_beforeRedrawQueue.length === 1) {
        const laters = global.compositor.get_laters();
        laters.add(Meta.LaterType.BEFORE_REDRAW, () => {
            _runBeforeRedrawQueue();
            return false;
        });
    }
}

/**
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
 * @param {Clutter.Actor} actor - an actor
 * @param {callback} callback - Function to invoke to perform work
 *
 * @returns {string} - A string work identifier
 */
export function initializeDeferredWork(actor, callback) {
    // Turn into a string so we can use as an object property
    let workId = `${++_deferredWorkSequence}`;
    _deferredWorkData[workId] = {
        actor,
        callback,
    };
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
 *
 * @param {string} workId work identifier
 *
 * Ensure that the work identified by @workId will be
 * run on map or timeout. You should call this function
 * for example when data being displayed by the actor has
 * changed.
 */
export function queueDeferredWork(workId) {
    let data = _deferredWorkData[workId];
    if (!data) {
        let message = `Invalid work id ${workId}`;
        logError(new Error(message), message);
        return;
    }
    if (!_deferredWorkQueue.includes(workId))
        _deferredWorkQueue.push(workId);
    if (data.actor.mapped) {
        _queueBeforeRedraw(workId);
    } else if (_deferredTimeoutId === 0) {
        _deferredTimeoutId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, DEFERRED_TIMEOUT_SECONDS, () => {
            _runAllDeferredWork();
            _deferredTimeoutId = 0;
            return GLib.SOURCE_REMOVE;
        });
        GLib.Source.set_name_by_id(_deferredTimeoutId, '[gnome-shell] _runAllDeferredWork');
    }
}

const RestartMessage = GObject.registerClass(
class RestartMessage extends ModalDialog.ModalDialog {
    _init(message) {
        super._init({
            shellReactive: true,
            styleClass: 'restart-message',
            shouldFadeIn: false,
            destroyOnClose: true,
        });

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

class AnimationsSettings {
    constructor() {
        this._animationsEnabled = true;
        this._handles = new Set();

        global.connect('notify::force-animations',
            this._syncAnimationsEnabled.bind(this));
        this._syncAnimationsEnabled();

        const backend = global.backend;
        const remoteAccessController = backend.get_remote_access_controller();
        if (remoteAccessController) {
            remoteAccessController.connect('new-handle',
                (_, handle) => this._onNewRemoteAccessHandle(handle));
        }
    }

    _shouldEnableAnimations() {
        if (this._handles.size > 0)
            return false;

        if (global.force_animations)
            return true;

        const backend = global.backend;
        if (!backend.is_rendering_hardware_accelerated())
            return false;

        if (Shell.util_has_x11_display_extension(
            global.display, 'VNC-EXTENSION'))
            return false;

        return true;
    }

    _syncAnimationsEnabled() {
        const shouldEnableAnimations = this._shouldEnableAnimations();
        if (this._animationsEnabled === shouldEnableAnimations)
            return;
        this._animationsEnabled = shouldEnableAnimations;

        const settings = St.Settings.get();
        if (shouldEnableAnimations)
            settings.uninhibit_animations();
        else
            settings.inhibit_animations();
    }

    _onRemoteAccessHandleStopped(handle) {
        this._handles.delete(handle);
        this._syncAnimationsEnabled();
    }

    _onNewRemoteAccessHandle(handle) {
        if (!handle.get_disable_animations())
            return;

        this._handles.add(handle);
        this._syncAnimationsEnabled();
        handle.connect('stopped', this._onRemoteAccessHandleStopped.bind(this));
    }
}
