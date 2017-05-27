// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported WorkspaceMonitor */

const { Shell } = imports.gi;

const Main = imports.ui.main;
const ViewSelector = imports.ui.viewSelector;

var WorkspaceMonitor = class {
    constructor() {
        this._shellwm = global.window_manager;
        this._shellwm.connect('minimize-completed', this._windowDisappeared.bind(this));
        this._shellwm.connect('destroy-completed', this._windowDisappeared.bind(this));

        this._windowTracker = Shell.WindowTracker.get_default();
        this._windowTracker.connect('tracked-windows-changed', this._trackedWindowsChanged.bind(this));

        global.display.connect('in-fullscreen-changed', this._fullscreenChanged.bind(this));

        let primaryMonitor = Main.layoutManager.primaryMonitor;
        this._inFullscreen = primaryMonitor && primaryMonitor.inFullscreen;

        this._appSystem = Shell.AppSystem.get_default();
    }

    _fullscreenChanged() {
        let primaryMonitor = Main.layoutManager.primaryMonitor;
        let inFullscreen = primaryMonitor && primaryMonitor.inFullscreen;

        if (this._inFullscreen !== inFullscreen) {
            this._inFullscreen = inFullscreen;
            this._updateOverview();
        }
    }

    _updateOverview() {
        let visibleApps = this._getVisibleApps();
        if (visibleApps.length !== 0 && this._inFullscreen)
            Main.overview.hide();
    }

    _windowDisappeared() {
        this._updateOverview();
    }

    _trackedWindowsChanged() {
        let visibleApps = this._getVisibleApps();
        let isShowingAppsGrid = Main.overview.visible &&
            Main.overview.getActivePage() === ViewSelector.ViewPage.APPS;

        if (visibleApps.length > 0 && isShowingAppsGrid) {
            // Make sure to hide the apps grid so that running apps whose
            // windows are becoming visible are shown to the user.
            Main.overview.hide();
        } else {
            // Fallback to the default logic used for dissapearing windows.
            this._updateOverview();
        }
    }

    _getVisibleApps() {
        let runningApps = this._appSystem.get_running();
        return runningApps.filter(app => {
            let windows = app.get_windows();
            for (let window of windows) {
                // We do not count transient windows because of an issue with Audacity
                // where a transient window was always being counted as visible even
                // though it was minimized
                if (window.get_transient_for())
                    continue;

                if (!window.minimized)
                    return true;
            }

            return false;
        });
    }

    get hasActiveWindows() {
        // Count anything fullscreen as an extra window
        if (this._inFullscreen)
            return true;

        let apps = this._appSystem.get_running();
        return apps.length > 0;
    }

    get hasVisibleWindows() {
        // Count anything fullscreen as an extra window
        if (this._inFullscreen)
            return true;

        let visibleApps = this._getVisibleApps();
        return visibleApps.length > 0;
    }
};
