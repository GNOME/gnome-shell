const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;

const INTROSPECT_SCHEMA = 'org.gnome.shell';
const INTROSPECT_KEY = 'introspect';
const APP_WHITELIST = ['org.freedesktop.impl.portal.desktop.gtk'];

const { loadInterfaceXML } = imports.misc.fileUtils;

const IntrospectDBusIface = loadInterfaceXML('org.gnome.Shell.Introspect');

var IntrospectService = new Lang.Class({
    Name: 'IntrospectService',

    _init() {
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

        this._appSystem = Shell.AppSystem.get_default();
        this._appSystem.connect('app-state-changed',
                                () => {
                                    this._runningApplicationsDirty = true;
                                    this._syncRunningApplications();
                                });

        this._settings = new Gio.Settings({ schema_id: INTROSPECT_SCHEMA });

        let display = global.display;
        display.connect('notify::focus-window',
                        () => {
                            this._activeApplicationDirty = true;
                            this._syncRunningApplications();
                        });

        this._syncRunningApplications();
    },

    _isStandaloneApp(app) {
        let windows = app.get_windows();

        for (var window of windows) {
            let toplevel = window.find_root_ancestor();
            if (windows.includes(toplevel))
                return true;
        }

        return false;
    },

    _isIntrospectEnabled() {
       return this._settings.get_boolean(INTROSPECT_KEY);
    },

    _isSenderWhitelisted(sender) {
       if (APP_WHITELIST.indexOf(sender) != -1)
           return true;

       return false;
    },

    _syncRunningApplications() {
        let display = global.display;
        let apps = this._appSystem.get_running();
        let seat_name = "seat0";
        let newRunningApplications = {};

        let newActiveApplication = null;

        for (var app of apps) {
            let appInfo = {};
            let isAppActive = false;

            if (!this._isStandaloneApp(app))
                continue;

            let focusWindow = display.get_focus_window();
            if (focusWindow) {
                let focusToplevel = focusWindow.find_root_ancestor();

                if (focusToplevel) {
                    if (app.get_windows().includes(focusToplevel))
                        isAppActive = true;
                }
            }

            if (isAppActive) {
                appInfo['active-on-seats'] = new GLib.Variant('as', [seat_name]);
                newActiveApplication = app.get_id();
            }

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
    },

    _isEligibleWindow(window) {
        if (window.is_override_redirect())
            return false;

        let type = window.get_window_type();
        return (type == Meta.WindowType.NORMAL ||
                type == Meta.WindowType.DIALOG ||
                type == Meta.WindowType.MODAL_DIALOG ||
                type == Meta.WindowType.UTILITY);
    },

    GetRunningApplicationsAsync(params, invocation) {
        if (!this._isIntrospectEnabled() &&
            !this._isSenderWhitelisted(invocation.get_sender())) {
            invocation.return_error_literal(Gio.DBusError,
                                            Gio.DBusError.ACCESS_DENIED,
                                            'App introspection not allowed');
            return;
        }

        invocation.return_value(new GLib.Variant('(a{sa{sv}})', [this._runningApplications]));
    },

    GetWindowsAsync(params, invocation) {
        let focusWindow = global.display.get_focus_window();
        let apps = this._appSystem.get_running();
        let windowsList = {};

        if (!this._isIntrospectEnabled()) {
            invocation.return_error_literal(Gio.DBusError,
                                            Gio.DBusError.ACCESS_DENIED,
                                            'App introspection not allowed');
            return;
        }

        for (var app of apps) {
            let windows = app.get_windows();
            for (var window of windows) {

                if (!this._isEligibleWindow(window))
                    continue;

                let window_id = window.get_id();
                let frameRect = window.get_frame_rect();
                let title = window.get_title();
                let wm_class = window.get_wm_class();

                windowsList[window_id] = {
                    'app-id': GLib.Variant.new('s', app.get_id()),
                    'client-type': GLib.Variant.new('u', window.get_client_type()),
                    'is-hidden': GLib.Variant.new('b', window.is_hidden()),
                    'has-focus': GLib.Variant.new('b', (window == focusWindow)),
                    'width': GLib.Variant.new('u', frameRect.width),
                    'height': GLib.Variant.new('u', frameRect.height)
                };

                // These properties may not be available for all windows:
                if (title != null)
                    windowsList[window_id]['title'] = GLib.Variant.new('s', title);

                if (wm_class != null)
                    windowsList[window_id]['wm-class'] = GLib.Variant.new('s', wm_class);
            }
        }
        invocation.return_value(new GLib.Variant('(a{ta{sv}})', [windowsList]));
    }
});
