// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

/* exported DesktopAppClient */

const { Clutter, Gio, GLib, Meta, Shell, St } = imports.gi;

const { loadInterfaceXML } = imports.misc.fileUtils;

const Main = imports.ui.main;
const ParentalControlsManager = imports.misc.parentalControlsManager;
const Util = imports.misc.util;

const SPLASH_SCREEN_TIMEOUT = 700; // ms

// By default, maximized windows are 75% of the workarea
// of the monitor they're on when unmaximized.
const DEFAULT_MAXIMIZED_WINDOW_SIZE = 0.75;
const LAUNCH_MAXIMIZED_DESKTOP_KEY = 'X-Endless-LaunchMaximized';

// GSettings that controls whether apps start maximized (default).
const NO_DEFAULT_MAXIMIZE_KEY = 'no-default-maximize';

// Determine if a splash screen should be shown for the provided
// ShellApp and other global settings
function _shouldShowSplash(app) {
    let info = app.get_app_info();

    // Don't show the splash screen if the app is already running.
    if (app.state === Shell.AppState.RUNNING)
        return false;

    if (!(info && info.has_key(LAUNCH_MAXIMIZED_DESKTOP_KEY) &&
          info.get_boolean(LAUNCH_MAXIMIZED_DESKTOP_KEY)))
        return false;

    // Don't show splash screen if default maximize is disabled
    if (global.settings.get_boolean(NO_DEFAULT_MAXIMIZE_KEY))
        return false;

    // Don't show splash screen if this is a link and the browser is
    // running. We can't rely on any signal being emitted in that
    // case, as links open in browser tabs.
    if (app.get_id().indexOf('eos-link-') !== -1 &&
        Util.getBrowserApp().state !== Shell.AppState.STOPPED)
        return false;

    let parentalControlsManager = ParentalControlsManager.getDefault();
    return parentalControlsManager.shouldShowApp(app.get_app_info());
}

var AppActivationContext = class {
    constructor(app) {
        this._app = app;

        this._splash = null;

        this._appStateId = 0;
        this._timeoutId = 0;

        this._appActivationTime = 0;

        this._hiddenWindows = [];

        this._appSystem = Shell.AppSystem.get_default();

        this._tracker = Shell.WindowTracker.get_default();
        this._tracker.connect('notify::focus-app', this._onFocusAppChanged.bind(this));
    }

    _doActivate(showSplash, timestamp) {
        if (!timestamp)
            timestamp = global.get_current_time();

        try {
            this._app.activate_full(-1, timestamp);
        } catch (e) {
            logError(e, `error while activating: ${this._app.get_id()}`);
            return;
        }

        if (showSplash)
            this.showSplash();
    }

    activate(event, timestamp) {
        let button = event ? event.get_button() : 0;
        let modifiers = event ? event.get_state() : 0;
        let isMiddleButton = button && button === Clutter.BUTTON_MIDDLE;
        let isCtrlPressed = (modifiers & Clutter.ModifierType.CONTROL_MASK) !== 0;
        let openNewWindow = this._app.can_open_new_window() &&
                            this._app.state === Shell.AppState.RUNNING &&
                            (isCtrlPressed || isMiddleButton);

        if (this._app.state === Shell.AppState.RUNNING) {
            if (openNewWindow)
                this._app.open_new_window(-1);
            else
                this._doActivate(false, timestamp);
        } else {
            this._doActivate(true, timestamp);
        }

        Main.overview.hide();
    }

    showSplash() {
        if (!_shouldShowSplash(this._app))
            return;

        // Prevent windows from being shown when the overview is hidden so it does
        // not affect the speedwagon's animation
        if (Main.overview.visible)
            this._hideWindows();

        this._splash = new SpeedwagonSplash(this._app);
        this._splash.show();

        // Scale the timeout by the slow down factor, because otherwise
        // we could be trying to destroy the splash screen window before
        // the map animation has finished.
        // This buffer time ensures that the user can never destroy the
        // splash before the animation is completed.
        this._timeoutId = GLib.timeout_add(
            GLib.PRIORITY_DEFAULT,
            SPLASH_SCREEN_TIMEOUT * St.Settings.get().slow_down_factor,
            this._splashTimeout.bind(this));

        // We can't fully trust windows-changed to be emitted with the
        // same ShellApp we called activate() on, as WMClass matching might
        // fail. For this reason, just pick to the first application that
        // will flip its state to running
        this._appStateId =
            this._appSystem.connect('app-state-changed', this._onAppStateChanged.bind(this));
        this._appActivationTime = GLib.get_monotonic_time();
    }

    _clearSplash() {
        this._resetWindowsVisibility();

        if (this._splash) {
            this._splash.rampOut();
            this._splash = null;
        }
    }

    _maybeClearSplash() {
        // Clear the splash only when we've waited at least 700ms,
        // and when the app has transitioned to the running state...
        if (this._appStateId === 0 && this._timeoutId === 0)
            this._clearSplash();
    }

    _splashTimeout() {
        this._timeoutId = 0;
        this._maybeClearSplash();

        return false;
    }

    _resetWindowsVisibility() {
        for (let actor of this._hiddenWindows)
            actor.visible = true;

        this._hiddenWindows = [];
    }

    _hideWindows() {
        let windows = global.get_window_actors();

        for (let actor of windows) {
            if (!actor.visible)
                continue;

            this._hiddenWindows.push(actor);
            actor.visible = false;
        }
    }

    _recordLaunchTime() {
        let activationTime = this._appActivationTime;
        this._appActivationTime = 0;

        if (activationTime === 0)
            return;

        if (!GLib.getenv('SHELL_DEBUG_LAUNCH_TIME'))
            return;

        let currentTime = GLib.get_monotonic_time();
        let elapsedTime = currentTime - activationTime;

        log(`Application ${this._app.get_name()} took ${elapsedTime / 1000000} seconds to launch`);
    }

    _isBogusWindow(app) {
        let launchedAppId = this._app.get_id();
        let appId = app.get_id();

        // When the application IDs match, the window is not bogus
        if (appId === launchedAppId)
            return false;

        // Special case for Libreoffice splash screen; we will get a non-matching
        // app with 'Soffice' as its name when the recovery screen comes up,
        // so special case that too
        if (launchedAppId.indexOf('libreoffice') !== -1 &&
            app.get_name() !== 'Soffice')
            return true;

        return false;
    }

    _onAppStateChanged(appSystem, app) {
        if (!(app.state === Shell.AppState.RUNNING ||
              app.state === Shell.AppState.STOPPED))
            return;

        if (this._isBogusWindow(app))
            return;

        appSystem.disconnect(this._appStateId);
        this._appStateId = 0;

        if (app.state === Shell.AppState.STOPPED) {
            this._clearSplash();
        } else {
            this._recordLaunchTime();
            this._maybeClearSplash();
        }
    }

    _onFocusAppChanged(tracker) {
        if (this._splash === null)
            return;

        let app = tracker.focus_app;
        if (!app || app.get_id() === this._app.get_id())
            return;

        // The focused application changed and it is not the one that we are showing
        // the splash for, so clear the splash after it times out (because we don't
        // want to risk hiding too early)
        this._appSystem.disconnect(this._appStateId);
        this._appStateId = 0;
        this._maybeClearSplash();
    }
};

const SpeedwagonIface = loadInterfaceXML('com.endlessm.Speedwagon');
const SpeedwagonProxy = Gio.DBusProxy.makeProxyWrapper(SpeedwagonIface);

var SpeedwagonSplash =  class {
    constructor(app) {
        this._app = app;

        this._proxy = new SpeedwagonProxy(
            Gio.DBus.session,
            'com.endlessm.Speedwagon',
            '/com/endlessm/Speedwagon');
    }

    show() {
        this._proxy.ShowSplashRemote(this._app.get_id());
    }

    rampOut() {
        this._proxy.HideSplashRemote(this._app.get_id());
    }
};

var DesktopAppClient = class DesktopAppClient {
    constructor() {
        this._lastDesktopApp = null;
        this._subscription =
            Gio.DBus.session.signal_subscribe(
                null,
                'org.gtk.gio.DesktopAppInfo',
                'Launched',
                '/org/gtk/gio/DesktopAppInfo',
                null, 0,
                this._onLaunched.bind(this));

        global.display.connect('window-created', this._windowCreated.bind(this));
    }

    _onLaunched(connection, senderName, objectPath, interfaceName, signalName, parameters) {
        let [desktopIdByteString] = parameters.deep_unpack();

        let desktopIdPath = imports.byteArray.toString(desktopIdByteString);
        let desktopIdFile = Gio.File.new_for_path(desktopIdPath);
        let desktopDirs = GLib.get_system_data_dirs();
        desktopDirs.push(GLib.get_user_data_dir());

        let desktopId = GLib.path_get_basename(desktopIdPath);

        // Convert subdirectories to app ID prefixes like GIO does
        desktopDirs.some(desktopDir => {
            let path = GLib.build_filenamev([desktopDir, 'applications']);
            let file = Gio.File.new_for_path(path);

            if (desktopIdFile.has_prefix(file)) {
                let relPath = file.get_relative_path(desktopIdFile);
                desktopId = relPath.replace(/\//g, '-');
                return true;
            }

            return false;
        });

        this._lastDesktopApp = Shell.AppSystem.get_default().lookup_app(desktopId);

        // Show the splash page if we didn't launch this ourselves, since in that case
        // we already explicitly control when the splash screen should be used
        let launchedByShell = senderName === Gio.DBus.session.get_unique_name();
        let showSplash =
            (this._lastDesktopApp !== null) &&
            (this._lastDesktopApp.state !== Shell.AppState.RUNNING) &&
            this._lastDesktopApp.get_app_info().should_show() &&
            !launchedByShell;

        if (showSplash) {
            let context = new AppActivationContext(this._lastDesktopApp);
            context.showSplash();
        }
    }

    _windowCreated(metaDisplay, metaWindow) {
        // Ignore splash screens, which will already be maximized.
        if (Shell.WindowTracker.is_speedwagon_window(metaWindow))
            return;

        // Don't maximize if key to disable default maximize is set
        if (global.settings.get_boolean(NO_DEFAULT_MAXIMIZE_KEY))
            return;

        // Don't maximize windows in non-overview sessions (e.g. initial setup)
        if (!Main.sessionMode.hasOverview)
            return;

        // Skip unknown applications
        let tracker = Shell.WindowTracker.get_default();
        let app = tracker.get_window_app(metaWindow);
        if (!app)
            return;

        // Skip applications we are not aware of
        if (!this._lastDesktopApp)
            return;

        // Don't maximize if the launch maximized key is false
        let info = app.get_app_info();
        if (info && info.has_key(LAUNCH_MAXIMIZED_DESKTOP_KEY) &&
            !info.get_boolean(LAUNCH_MAXIMIZED_DESKTOP_KEY))
            return;

        // Skip if the window does not belong to the launched app, but
        // special case eos-link launchers if we detect a browser window
        if (app !== this._lastDesktopApp &&
            !(this._lastDesktopApp.get_id().indexOf('eos-link-') !== -1 &&
              app === Util.getBrowserApp()))
            return;

        this._lastDesktopApp = null;

        if (metaWindow.is_skip_taskbar() || !metaWindow.resizeable)
            return;

        // Position the window so it's where we want it to be if the user
        // unmaximizes the window.
        let workArea = Main.layoutManager.getWorkAreaForMonitor(metaWindow.get_monitor());
        let width = workArea.width * DEFAULT_MAXIMIZED_WINDOW_SIZE;
        let height = workArea.height * DEFAULT_MAXIMIZED_WINDOW_SIZE;
        let x = workArea.x + (workArea.width - width) / 2;
        let y = workArea.y + (workArea.height - height) / 2;
        metaWindow.move_resize_frame(false, x, y, width, height);

        metaWindow.maximize(Meta.MaximizeFlags.HORIZONTAL |
                            Meta.MaximizeFlags.VERTICAL);
    }
};
