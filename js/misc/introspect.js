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

        let tracker = Shell.WindowTracker.get_default();
        tracker.connect('notify::focus-app',
                        () => {
                            this._activeApplicationDirty = true;
                            this._syncRunningApplications();
                        });

        this._syncRunningApplications();
    },

    _isStandaloneApp(app) {
        let windows = app.get_windows();

        return app.get_windows().some(w => w.transient_for == null);
    },

    _isIntrospectEnabled() {
       return this._settings.get_boolean(INTROSPECT_KEY);
    },

    _isSenderWhitelisted(sender) {
       return APP_WHITELIST.includes(sender);
    },

    _syncRunningApplications() {
        let tracker = Shell.WindowTracker.get_default();
        let apps = this._appSystem.get_running();
        let seatName = "seat0";
        let newRunningApplications = {};

        let newActiveApplication = null;
        let focusedApp = tracker.focus_app;

        for (let app of apps) {
            let appInfo = {};
            let isAppActive = (focusedApp == app);

            if (!this._isStandaloneApp(app))
                continue;

            if (isAppActive) {
                appInfo['active-on-seats'] = new GLib.Variant('as', [seatName]);
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

    GetRunningApplicationsAsync(params, invocation) {
        if (!this._isIntrospectEnabled() &&
            !this._isSenderWhitelisted(invocation.get_sender())) {
            invocation.return_error_literal(Gio.DBusError,
                                            Gio.DBusError.ACCESS_DENIED,
                                            'App introspection not allowed');
            return;
        }

        invocation.return_value(new GLib.Variant('(a{sa{sv}})', [this._runningApplications]));
    }
});
