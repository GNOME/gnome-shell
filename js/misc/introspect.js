const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;

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

            let runningApplicationsVariant =
                new GLib.Variant('a{sa{sv}}', this._runningApplications);
            this._dbusImpl.emit_property_changed('RunningApplications',
                                                 runningApplicationsVariant);
        }
        this._runningApplicationsDirty = false;
        this._activeApplicationDirty = false;
    },

    get RunningApplications() {
        return this._runningApplications;
    },

    GetWindows() {
        let focusWindow = global.display.get_focus_window();
        let apps = this._appSystem.get_running();
        let windowsList = {};

        for (var app of apps) {
            let windows = app.get_windows();
            for (var window of windows) {
                let frameRect = window.get_frame_rect();

                windowsList[window.get_id()] =
                    { 'title': GLib.Variant.new('s', window.get_title()),
                      'app-id': GLib.Variant.new('s', app.get_id()),
                      'wm-class': GLib.Variant.new('s', window.get_wm_class()),
                      'client-type': GLib.Variant.new('u', window.get_client_type()),
                      'is-hidden': GLib.Variant.new('b', window.is_hidden()),
                      'has-focus': GLib.Variant.new('b', (window == focusWindow)),
                      'width': GLib.Variant.new('u', frameRect.width),
                      'height': GLib.Variant.new('u', frameRect.height) };
            }
        }

        return windowsList;
    }
});
