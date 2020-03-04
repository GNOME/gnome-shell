/* exported DBusService, ServiceImplementation */

const { Gio, GLib } = imports.gi;

var ServiceImplementation = class {
    constructor(info, objectPath) {
        this._objectPath = objectPath;
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(info, this);
    }

    // subclasses may override this to own additional names
    register() {
    }

    export() {
        this._dbusImpl.export(Gio.DBus.session, this._objectPath);
    }

    unexport() {
        this._dbusImpl.unexport();
    }
};

var DBusService = class {
    constructor(name, service) {
        this._name = name;
        this._service = service;
        this._loop = new GLib.MainLoop(null, false);
    }

    run() {
        // Bail out when not running under gnome-shell
        Gio.DBus.watch_name(Gio.BusType.SESSION,
            'org.gnome.Shell',
            Gio.BusNameWatcherFlags.NONE,
            null,
            () => this._loop.quit());

        this._service.register();

        Gio.DBus.own_name(Gio.BusType.SESSION,
            this._name,
            Gio.BusNameOwnerFlags.REPLACE,
            () => this._service.export(),
            null,
            () => this._loop.quit());

        this._loop.run();
    }
};

