// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported componentManager, notificationDaemon, windowAttentionHandler,
            ctrlAltTabManager, padOsdService, osdWindowManager,
            osdMonitorLabeler, shellMountOpDBusService, shellDBusService,
            shellAccessDialogDBusService, shellAudioSelectionDBusService,
            screenSaverDBus, uiGroup, magnifier, xdndHandler, keyboard,
            kbdA11yDialog, introspectService, start, pushModal, popModal,
            activateWindow, createLookingGlass, initializeDeferredWork,
            getThemeStylesheet, setThemeStylesheet */
// @ts-check

import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as AccessDialog from './accessDialog.js';
import * as AudioDeviceSelection from './audioDeviceSelection.js';
import * as Components from './components.js';
import * as CtrlAltTab from './ctrlAltTab.js';
import * as EndSessionDialog from './endSessionDialog.js';
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
import * as Params from '../misc/params.js';
import * as RunDialog from './runDialog.js';
import * as WelcomeDialog from './welcomeDialog.js';
import * as Layout from './layout.js';
import * as LoginManager from '../misc/loginManager.js';
import * as LookingGlass from './lookingGlass.js';
import * as NotificationDaemon from './notificationDaemon.js';
import * as WindowAttentionHandler from './windowAttentionHandler.js';
import * as ScreenShield from './screenShield.js';
import * as Scripting from './scripting.js';
import * as SessionMode from './sessionMode.js';
import * as ShellDBus from './shellDBus.js';
import * as ShellMountOperation from './shellMountOperation.js';
import * as WindowManager from './windowManager.js';
import * as Magnifier from './magnifier.js';
import * as XdndHandler from './xdndHandler.js';
import * as KbdA11yDialog from './kbdA11yDialog.js';
import * as LocatePointer from './locatePointer.js';
import * as PointerA11yTimeout from './pointerA11yTimeout.js';
import * as ParentalControlsManager from '../misc/parentalControlsManager.js';
const Config = imports.misc.config;
import * as Util from '../misc/util.js';
import * as ExtensionUtils from '../misc/extensionUtils.js';

const WELCOME_DIALOG_LAST_SHOWN_VERSION = 'welcome-dialog-last-shown-version';
// Make sure to mention the point release, otherwise it will show every time
// until this version is current
const WELCOME_DIALOG_LAST_TOUR_CHANGE = '40.beta';
const LOG_DOMAIN = 'GNOME Shell';
const GNOMESHELL_STARTED_MESSAGE_ID = 'f3ea493c22934e26811cd62abe8e203a';

export class Main {
    componentManager = null;
    extensionManager = null;
    panel = null;
    overview = null;
    runDialog = null;
    lookingGlass = null;
    welcomeDialog = null;
    /** @type {WindowManager.WindowManager} */
    wm = null;
    messageTray = null;
    screenShield = null;
    notificationDaemon = null;
    windowAttentionHandler = null;
    /** @type {CtrlAltTab.CtrlAltTabManager} */
    ctrlAltTabManager = null;
    padOsdService = null;
    osdWindowManager = null;
    osdMonitorLabeler = null;
    sessionMode = null;
    shellAccessDialogDBusService = null;
    shellAudioSelectionDBusService = null;
    shellDBusService = null;
    shellMountOpDBusService = null;
    screenSaverDBus = null;
    modalCount = 0;
    actionMode = Shell.ActionMode.NONE;
    modalActorFocusStack = [];
    uiGroup = null;
    /** @type {Magnifier.Magnifier} */
    magnifier = null;
    /** @type {XdndHandler.XdndHandler} */
    xdndHandler = null;
    keyboard = null;
    /** @type {Layout.LayoutManager["prototype"]} */
    layoutManager = null;
    kbdA11yDialog = null;
    inputMethod = null;
    introspectService = null;
    locatePointer = null;
    _startDate;
    _defaultCssStylesheet = null;
    _cssStylesheet = null;
    _themeResource = null;
    _oskResource = null;
    
    _remoteAccessInhibited = false;

    _sessionUpdated() {
        const { wm, sessionMode, welcomeDialog, overview, runDialog, lookingGlass } = this;
    
        if (sessionMode.isPrimary)
            this._loadDefaultStylesheet();
    
    
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
            sessionMode.hasRunDialog ? this.openRunDialog : null);
    
        if (!sessionMode.hasRunDialog) {
            if (runDialog)
                runDialog.close();
            if (lookingGlass)
                lookingGlass.close();
            if (welcomeDialog)
                welcomeDialog.close();
        }
    
        let remoteAccessController = global.backend.get_remote_access_controller();
        if (remoteAccessController) {
            if (sessionMode.allowScreencast && this._remoteAccessInhibited) {
                remoteAccessController.uninhibit_remote_access();
                this._remoteAccessInhibited = false;
            } else if (!sessionMode.allowScreencast && !this._remoteAccessInhibited) {
                remoteAccessController.inhibit_remote_access();
                this._remoteAccessInhibited = true;
            }
        }

        log('session updated...');
    }
    
    start() {
        // These are here so we don't break compatibility.
        global.logError = globalThis.log;
        global.log = globalThis.log;
    
        // Chain up async errors reported from C
        global.connect('notify-error', (global, msg, detail) => {
            this.notifyError(msg, detail);
        });
    
        let currentDesktop = GLib.getenv('XDG_CURRENT_DESKTOP');
        if (!currentDesktop || !currentDesktop.split(':').includes('GNOME'))
            Gio.DesktopAppInfo.set_desktop_env('GNOME');
    
        this.sessionMode = new SessionMode.SessionMode();
        this.sessionMode.connect('updated', this._sessionUpdated);
    
        St.Settings.get().connect('notify::gtk-theme', this._loadDefaultStylesheet);
    
        // Initialize ParentalControlsManager before the UI
        ParentalControlsManager.getDefault();
    
        this._initializeUI();
    
        this.shellAccessDialogDBusService = new AccessDialog.AccessDialogDBus();
        this.shellAudioSelectionDBusService = new AudioDeviceSelection.AudioDeviceSelectionDBus();
        this.shellDBusService = new ShellDBus.GnomeShell();
        this.shellMountOpDBusService = new ShellMountOperation.GnomeShellMountOpHandler();
    
        const watchId = Gio.DBus.session.watch_name('org.gnome.Shell.Notifications',
            Gio.BusNameWatcherFlags.AUTO_START,
            bus => bus.unwatch_name(watchId),
            bus => bus.unwatch_name(watchId));
    
        this._sessionUpdated();
    
        log("Started...");
    }
    
    _initializeUI() {
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
    
        this.reloadThemeResource();
        this._loadOskLayouts();
        this._loadDefaultStylesheet();
    
        new AnimationsSettings();
    
        // Setup the stage hierarchy early
        this.layoutManager = new Layout.LayoutManager();
    
        // Various parts of the codebase still refer to Main.uiGroup
        // instead of using the layoutManager. This keeps that code
        // working until it's updated.
        this.uiGroup = this.layoutManager.uiGroup;
    
        this.padOsdService = new PadOsd.PadOsdService();
        this.xdndHandler = new XdndHandler.XdndHandler();
        this.ctrlAltTabManager = new CtrlAltTab.CtrlAltTabManager();
        this.osdWindowManager = new OsdWindow.OsdWindowManager();
        this.osdMonitorLabeler = new OsdMonitorLabeler.OsdMonitorLabeler();
        this.overview = new Overview.Overview();
        this.kbdA11yDialog = new KbdA11yDialog.KbdA11yDialog();
        this.wm = new WindowManager.WindowManager();
        this.magnifier = new Magnifier.Magnifier();
        this.locatePointer = new LocatePointer.LocatePointer();
    
        if (LoginManager.canLock())
            this.screenShield = new ScreenShield.ScreenShield();
    
        this.inputMethod = new InputMethod.InputMethod();
        Clutter.get_default_backend().set_input_method(this.inputMethod);
    
        this.messageTray = new MessageTray.MessageTray();
        this.panel = new Panel.Panel();
        this.keyboard = new Keyboard.KeyboardManager();
        this.notificationDaemon = new NotificationDaemon.NotificationDaemon();
        this.windowAttentionHandler = new WindowAttentionHandler.WindowAttentionHandler();
        this.componentManager = new Components.ComponentManager();
    
        this.introspectService = new Introspect.IntrospectService();
    
        this.layoutManager.init();
        this.overview.init();
    
        new PointerA11yTimeout.PointerA11yTimeout();
    
        global.connect('locate-pointer', () => {
            this.locatePointer.show();
        });
    
        global.display.connect('show-restart-message', (display, message) => {
            showRestartMessage(message);
            return true;
        });
    
        global.display.connect('restart', () => {
            global.reexec_self();
            return true;
        });
    
        global.display.connect('gl-video-memory-purged', this.loadTheme);
    
        // Provide the bus object for gnome-session to
        // initiate logouts.
        EndSessionDialog.init();
    
        // We're ready for the session manager to move to the next phase
        GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
            log('>>>>');
            Shell.util_sd_notify();
            global.context.notify_ready();
            log('<<<<<')
            return GLib.SOURCE_REMOVE;
        });
    
        this._startDate = new Date();
    
        // ExtensionDownloader.init();
        // this.extensionManager = new ExtensionSystem.ExtensionManager();
        // this.extensionManager.init();
    
        if (this.sessionMode.isGreeter && this.screenShield) {
            this.layoutManager.connect('startup-prepared', () => {
                log('Startup Prepared!');
                this.screenShield.showDialog();
            });
        }
    
        this.layoutManager.connect('startup-complete', () => {
            log('Startup Complete!');
            if (this.actionMode == Shell.ActionMode.NONE)
                this.actionMode = Shell.ActionMode.NORMAL;
    
            if (this.screenShield)
                this.screenShield.lockIfWasLocked();
    
            if (this.sessionMode.currentMode != 'gdm' &&
                this.sessionMode.currentMode != 'initial-setup') {
                GLib.log_structured(LOG_DOMAIN, GLib.LogLevelFlags.LEVEL_MESSAGE, {
                    'MESSAGE': 'GNOME Shell started at %s'.format(this._startDate),
                    'MESSAGE_ID': GNOMESHELL_STARTED_MESSAGE_ID,
                });
            }
    
            let credentials = new Gio.Credentials();
            if (credentials.get_unix_user() === 0) {
                this.notify(_('Logged in as a privileged user'),
                       _('Running a session as a privileged user should be avoided for security reasons. If possible, you should log in as a normal user.'));
            } else if (this.sessionMode.showWelcomeDialog) {
                this._handleShowWelcomeScreen();
            }
    
            if (this.sessionMode.currentMode !== 'gdm' &&
                this.sessionMode.currentMode !== 'initial-setup')
                this._handleLockScreenWarning();
    
            LoginManager.registerSessionWithGDM();
    
            let perfModuleName = GLib.getenv("SHELL_PERF_MODULE");
            if (perfModuleName) {
                let perfOutput = GLib.getenv("SHELL_PERF_OUTPUT");
                let module = eval('imports.perf.%s;'.format(perfModuleName));
                Scripting.runPerfScript(module, perfOutput);
            }
        });
    
        log('done init...');
    }
    
    _handleShowWelcomeScreen() {
        const lastShownVersion = global.settings.get_string(WELCOME_DIALOG_LAST_SHOWN_VERSION);
        if (Util.GNOMEversionCompare(WELCOME_DIALOG_LAST_TOUR_CHANGE, lastShownVersion) > 0) {
            this.openWelcomeDialog();
            global.settings.set_string(WELCOME_DIALOG_LAST_SHOWN_VERSION, Config.PACKAGE_VERSION);
        }
    }

    async _handleLockScreenWarning() {
        const path = '%s/lock-warning-shown'.format(global.userdatadir);
        const file = Gio.File.new_for_path(path);

        const hasLockScreen = this.screenShield !== null;
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

            this.notify(
                _('Screen Lock disabled'),
                _('Screen Locking requires the GNOME display manager.'));
        }
    }
    _getStylesheet(name) {
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

    _getDefaultStylesheet() {
        let stylesheet = null;
        let name = this.sessionMode.stylesheetName;

        // Look for a high-contrast variant first when using GTK+'s HighContrast
        // theme
        if (St.Settings.get().gtk_theme == 'HighContrast')
            stylesheet = this._getStylesheet(name.replace('.css', '-high-contrast.css'));

        if (stylesheet == null)
            stylesheet = this._getStylesheet(this.sessionMode.stylesheetName);

        return stylesheet;
    }

    _loadDefaultStylesheet() {
        let stylesheet = this._getDefaultStylesheet();
        if (this._defaultCssStylesheet && this._defaultCssStylesheet.equal(stylesheet))
            return;

        this._defaultCssStylesheet = stylesheet;
        this.loadTheme();
    }

    /**
     * getThemeStylesheet:
     *
     * Get the theme CSS file that the shell will load
     *
     * @returns {?Gio.File}: A #GFile that contains the theme CSS,
     *          null if using the default
     */
    getThemeStylesheet() {
        return this._cssStylesheet;
    }


    /**
     * setThemeStylesheet:
 * @param {string=} cssStylesheet: A file path that contains the theme CSS,
 *     set it to null to use the default
 *
 * Set the theme CSS file that the shell will load
 */
    setThemeStylesheet(cssStylesheet) {
        this._cssStylesheet = cssStylesheet ? Gio.File.new_for_path(cssStylesheet) : null;
    }

    reloadThemeResource() {
        if (this._themeResource)
            this._themeResource._unregister();

        this._themeResource = Gio.Resource.load('%s/%s'.format(global.datadir,
            this.sessionMode.themeResourceName));
        this._themeResource._register();
    }

    _loadOskLayouts() {
        this._oskResource = Gio.Resource.load('%s/gnome-shell-osk-layouts.gresource'.format(global.datadir));
        this._oskResource._register();
    }


    /**
     * loadTheme:
     *
     * Reloads the theme CSS file
     */
    loadTheme() {
        let themeContext = St.ThemeContext.get_for_stage(global.stage);
        let previousTheme = themeContext.get_theme();

        let theme = new St.Theme({
            application_stylesheet: this._cssStylesheet,
            default_stylesheet: this._defaultCssStylesheet,
        });

        if (theme.default_stylesheet == null)
            throw new Error("No valid stylesheet found for '%s'".format(this.sessionMode.stylesheetName));

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
    notify(msg, details) {
        let source = new MessageTray.SystemNotificationSource();
        this.messageTray.add(source);
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
    notifyError(msg, details) {
        // Also print to stderr so it's logged somewhere
        if (details)
            log('error: %s: %s'.format(msg, details));
        else
            log('error: %s'.format(msg));

        this.notify(msg, details);
    }

    _findModal(actor) {
        for (let i = 0; i < this.modalActorFocusStack.length; i++) {
            if (this.modalActorFocusStack[i].actor == actor)
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
     * @returns {boolean}: true iff we successfully acquired a grab or already had one
     */
    pushModal(actor, params) {
        params = Params.parse(params, {
            timestamp: global.get_current_time(),
            options: 0,
            actionMode: Shell.ActionMode.NONE
        });

        if (this.modalCount == 0) {
            if (!global.begin_modal(params.timestamp, params.options)) {
                log('pushModal: invocation of begin_modal failed');
                return false;
            }
            Meta.disable_unredirect_for_display(global.display);
        }

        this.modalCount += 1;
        let actorDestroyId = actor.connect('destroy', () => {
            let index = this._findModal(actor);
            if (index >= 0)
                this.popModal(actor);
        });

        let prevFocus = global.stage.get_key_focus();
        let prevFocusDestroyId;
        if (prevFocus != null) {
            prevFocusDestroyId = prevFocus.connect('destroy', () => {
                const index = this.modalActorFocusStack.findIndex(
                    record => record.prevFocus === prevFocus);

                if (index >= 0)
                    this.modalActorFocusStack[index].prevFocus = null;
            });
        }
        this.modalActorFocusStack.push({
            actor,
            destroyId: actorDestroyId,
            prevFocus,
            prevFocusDestroyId,
            actionMode: this.actionMode
        });

        this.actionMode = params.actionMode;
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
    popModal(actor, timestamp) {
        if (timestamp == undefined)
            timestamp = global.get_current_time();

        let focusIndex = this._findModal(actor);
        if (focusIndex < 0) {
            global.stage.set_key_focus(null);
            global.end_modal(timestamp);
            this.actionMode = Shell.ActionMode.NORMAL;

            throw new Error('incorrect pop');
        }

        this.modalCount -= 1;

        let record = this.modalActorFocusStack[focusIndex];
        record.actor.disconnect(record.destroyId);

        const { modalActorFocusStack } = this;

        if (focusIndex == this.modalActorFocusStack.length - 1) {
            if (record.prevFocus)
                record.prevFocus.disconnect(record.prevFocusDestroyId);
            this.actionMode = record.actionMode;
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

        if (this.modalCount > 0)
            return;

        this.layoutManager.modalEnded();
        global.end_modal(timestamp);
        Meta.enable_unredirect_for_display(global.display);
        this.actionMode = Shell.ActionMode.NORMAL;
    }

    createLookingGlass() {
        if (this.lookingGlass == null)
            this.lookingGlass = new LookingGlass.LookingGlass();

        return this.lookingGlass;
    }

    openRunDialog() {
        if (this.runDialog == null)
            this.runDialog = new RunDialog.RunDialog();

        this.runDialog.open();
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
    activateWindow(window, time, workspaceNum) {
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

        this.overview.hide();
        this.panel.closeCalendar();
    }

    _deferredWorkData = {};

    // Work scheduled for some point in the future
    _deferredWorkQueue = [];
    // Work we need to process before the next redraw
    _beforeRedrawQueue = [];
    // Counter to assign work ids
    _deferredWorkSequence = 0;
    _deferredTimeoutId = 0;

    _runDeferredWork(workId) {
        if (!this._deferredWorkData[workId])
            return;
        let index = this._deferredWorkQueue.indexOf(workId);
        if (index < 0)
            return;

        this._deferredWorkQueue.splice(index, 1);
        this._deferredWorkData[workId].callback();
        if (this._deferredWorkQueue.length == 0 && this._deferredTimeoutId > 0) {
            GLib.source_remove(this._deferredTimeoutId);
            this._deferredTimeoutId = 0;
        }
    }

    _runAllDeferredWork() {
        while (this._deferredWorkQueue.length > 0)
            this._runDeferredWork(this._deferredWorkQueue[0]);
    }

    _runBeforeRedrawQueue() {
        for (let i = 0; i < this._beforeRedrawQueue.length; i++) {
            let workId = this._beforeRedrawQueue[i];
            this._runDeferredWork(workId);
        }
        this._beforeRedrawQueue = [];
    }

    _queueBeforeRedraw(workId) {
        this._beforeRedrawQueue.push(workId);
        if (this._beforeRedrawQueue.length == 1) {
            Meta.later_add(Meta.LaterType.BEFORE_REDRAW, () => {
                this._runBeforeRedrawQueue();
                return false;
            });
        }
    }

    /**
     * initializeDeferredWork:
     * @param {Clutter.Actor} actor: an actor
     * @param {(...args: any[]) => any} callback: Function to invoke to perform work
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
    initializeDeferredWork(actor, callback) {
        // Turn into a string so we can use as an object property
        let workId = (++this._deferredWorkSequence).toString();
        this._deferredWorkData[workId] = {
            actor,
            callback
        };
        actor.connect('notify::mapped', () => {
            if (!(actor.mapped && this._deferredWorkQueue.includes(workId)))
                return;
            this._queueBeforeRedraw(workId);
        });
        actor.connect('destroy', () => {
            let index = this._deferredWorkQueue.indexOf(workId);
            if (index >= 0)
                this._deferredWorkQueue.splice(index, 1);
            delete this._deferredWorkData[workId];
        });
        this.queueDeferredWork(workId);
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
    queueDeferredWork(workId) {
        let data = this._deferredWorkData[workId];
        if (!data) {
            let message = 'Invalid work id %s'.format(workId);
            logError(new Error(message), message);
            return;
        }
        if (!this._deferredWorkQueue.includes(workId))
            this._deferredWorkQueue.push(workId);
        if (data.actor.mapped) {
            this._queueBeforeRedraw(workId);
        } else if (this._deferredTimeoutId == 0) {
            this._deferredTimeoutId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT, DEFERRED_TIMEOUT_SECONDS, () => {
                this._runAllDeferredWork();
                this._deferredTimeoutId = 0;
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(this._deferredTimeoutId, '[gnome-shell] _runAllDeferredWork');
        }
    }
    
    openWelcomeDialog() {
        if (this.welcomeDialog === null)
            this.welcomeDialog = new WelcomeDialog.WelcomeDialog();
    
        this.welcomeDialog.open();
    }
};
    
export const main = new Main();
    
ExtensionUtils._setMain(main);

globalThis.getMain = function getMain() {
    return main;
}

export default main;

Gio._promisify(Gio._LocalFilePrototype, 'delete_async', 'delete_finish');
Gio._promisify(Gio._LocalFilePrototype, 'touch_async', 'touch_finish');

// TODO - replace this timeout with some system to guess when the user might
// be e.g. just reading the screen and not likely to interact.
export const DEFERRED_TIMEOUT_SECONDS = 20;

export const RestartMessage = GObject.registerClass(
    class RestartMessage extends ModalDialog.ModalDialog {
        _init(message) {
            super._init({
                shellReactive: true,
                styleClass: 'restart-message headline',
                shouldFadeIn: false,
                destroyOnClose: true
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

export class AnimationsSettings {
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
