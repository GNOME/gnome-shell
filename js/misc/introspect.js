/* exported IntrospectService */
const { Gio, GLib, Meta, Shell, St } = imports.gi;

const INTROSPECT_SCHEMA = 'org.gnome.shell';
const INTROSPECT_KEY = 'introspect';
const APP_WHITELIST = ['org.freedesktop.impl.portal.desktop.gtk'];

const INTROSPECT_DBUS_API_VERSION = 2;

const { loadInterfaceXML } = imports.misc.fileUtils;

const IntrospectDBusIface = loadInterfaceXML('org.gnome.Shell.Introspect');

var IntrospectService = class {
    constructor() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(IntrospectDBusIface,
                                                             this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gnome/Shell/Introspect');
        Gio.DBus.session.own_name('org.gnome.Shell.Introspect',
                                  Gio.BusNameOwnerFlags.REPLACE,
                                  null, null);

        this._runningApplications = {};
        this._runningApplicationsDirty = true;
        this._activeApplication = null;
        this._activeApplicationDirty = true;
        this._animationsEnabled = true;

        this._appSystem = Shell.AppSystem.get_default();
        this._appSystem.connect('app-state-changed',
                                () => {
                                    this._runningApplicationsDirty = true;
                                    this._syncRunningApplications();
                                });

        this._introspectSettings = new Gio.Settings({
            schema_id: INTROSPECT_SCHEMA,
        });

        let tracker = Shell.WindowTracker.get_default();
        tracker.connect('notify::focus-app',
                        () => {
                            this._activeApplicationDirty = true;
                            this._syncRunningApplications();
                        });

        this._syncRunningApplications();

        this._whitelistMap = new Map();
        APP_WHITELIST.forEach(appName => {
            Gio.DBus.watch_name(Gio.BusType.SESSION,
                appName,
                Gio.BusNameWatcherFlags.NONE,
                (conn, name, owner) => this._whitelistMap.set(name, owner),
                (conn, name) => this._whitelistMap.delete(name));
        });

        this._settings = St.Settings.get();
        this._settings.connect('notify::enable-animations',
            this._syncAnimationsEnabled.bind(this));
        this._syncAnimationsEnabled();
    }

    _isStandaloneApp(app) {
        return app.get_windows().some(w => w.transient_for == null);
    }

    _isIntrospectEnabled() {
        return this._introspectSettings.get_boolean(INTROSPECT_KEY);
    }

    _isSenderWhitelisted(sender) {
        return [...this._whitelistMap.values()].includes(sender);
    }

    _getSandboxedAppId(app) {
        let ids = app.get_windows().map(w => w.get_sandboxed_app_id());
        return ids.find(id => id != null);
    }

    _syncRunningApplications() {
        let tracker = Shell.WindowTracker.get_default();
        let apps = this._appSystem.get_running();
        let seatName = "seat0";
        let newRunningApplications = {};

        let newActiveApplication = null;
        let focusedApp = tracker.focus_app;

        for (let app of apps) {
            let appInfo = {};
            let isAppActive = focusedApp == app;

            if (!this._isStandaloneApp(app))
                continue;

            if (isAppActive) {
                appInfo['active-on-seats'] = new GLib.Variant('as', [seatName]);
                newActiveApplication = app.get_id();
            }

            let sandboxedAppId = this._getSandboxedAppId(app);
            if (sandboxedAppId)
                appInfo['sandboxed-app-id'] = new GLib.Variant('s', sandboxedAppId);

            newRunningApplications[app.get_id()] = appInfo;
        }

        if (this._runningApplicationsDirty ||
            (this._activeApplicationDirty &&
             this._activeApplication != newActiveApplication)) {
            this._runningApplications = newRunningApplications;
            this._activeApplication = newActiveApplication;

            this._dbusImpl.emit_signal('RunningApplicationsChanged', null);
        }
        this._runningApplicationsDirty = false;
        this._activeApplicationDirty = false;
    }

    _isEligibleWindow(window) {
        if (window.is_override_redirect())
            return false;

        let type = window.get_window_type();
        return type == Meta.WindowType.NORMAL ||
                type == Meta.WindowType.DIALOG ||
                type == Meta.WindowType.MODAL_DIALOG ||
                type == Meta.WindowType.UTILITY;
    }

    _isInvocationAllowed(invocation) {
        if (this._isIntrospectEnabled())
            return true;

        if (this._isSenderWhitelisted(invocation.get_sender()))
            return true;

        return false;
    }

    GetRunningApplicationsAsync(params, invocation) {
        if (!this._isInvocationAllowed(invocation)) {
            invocation.return_error_literal(Gio.DBusError,
                                            Gio.DBusError.ACCESS_DENIED,
                                            'App introspection not allowed');
            return;
        }

        invocation.return_value(new GLib.Variant('(a{sa{sv}})', [this._runningApplications]));
    }

    GetWindowsAsync(params, invocation) {
        let focusWindow = global.display.get_focus_window();
        let apps = this._appSystem.get_running();
        let windowsList = {};

        if (!this._isInvocationAllowed(invocation)) {
            invocation.return_error_literal(Gio.DBusError,
                                            Gio.DBusError.ACCESS_DENIED,
                                            'App introspection not allowed');
            return;
        }

        for (let app of apps) {
            let windows = app.get_windows();
            for (let window of windows) {

                if (!this._isEligibleWindow(window))
                    continue;

                let windowId = window.get_id();
                let frameRect = window.get_frame_rect();
                let title = window.get_title();
                let wmClass = window.get_wm_class();
                let sandboxedAppId = window.get_sandboxed_app_id();

                windowsList[windowId] = {
                    'app-id': GLib.Variant.new('s', app.get_id()),
                    'client-type': GLib.Variant.new('u', window.get_client_type()),
                    'is-hidden': GLib.Variant.new('b', window.is_hidden()),
                    'has-focus': GLib.Variant.new('b', window == focusWindow),
                    'width': GLib.Variant.new('u', frameRect.width),
                    'height': GLib.Variant.new('u', frameRect.height),
                };

                // These properties may not be available for all windows:
                if (title != null)
                    windowsList[windowId]['title'] = GLib.Variant.new('s', title);

                if (wmClass != null)
                    windowsList[windowId]['wm-class'] = GLib.Variant.new('s', wmClass);

                if (sandboxedAppId != null) {
                    windowsList[windowId]['sandboxed-app-id'] =
                        GLib.Variant.new('s', sandboxedAppId);
                }
            }
        }
        invocation.return_value(new GLib.Variant('(a{ta{sv}})', [windowsList]));
    }

    _syncAnimationsEnabled() {
        let wasAnimationsEnabled = this._animationsEnabled;
        this._animationsEnabled = this._settings.enable_animations;
        if (wasAnimationsEnabled !== this._animationsEnabled) {
            let variant = new GLib.Variant('b', this._animationsEnabled);
            this._dbusImpl.emit_property_changed('AnimationsEnabled', variant);
        }
    }

    get AnimationsEnabled() {
        return this._animationsEnabled;
    }

    get version() {
        return INTROSPECT_DBUS_API_VERSION;
    }
};
