/* exported DBusService, ServiceImplementation */

const { Gio, GLib } = imports.gi;

const Signals = imports.signals;

const IDLE_SHUTDOWN_TIME = 2; // s

const { programArgs } = imports.system;

var ServiceImplementation = class {
    constructor(info, objectPath) {
        this._objectPath = objectPath;
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(info, this);

        this._injectTracking('return_dbus_error');
        this._injectTracking('return_error_literal');
        this._injectTracking('return_gerror');
        this._injectTracking('return_value');
        this._injectTracking('return_value_with_unix_fd_list');

        this._senders = new Map();
        this._holdCount = 0;

        this._hasSignals = this._dbusImpl.get_info().signals.length > 0;
        this._shutdownTimeoutId = 0;

        // subclasses may override this to disable automatic shutdown
        this._autoShutdown = true;

        this._queueShutdownCheck();
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

    hold() {
        this._holdCount++;
    }

    release() {
        if (this._holdCount === 0) {
            logError(new Error('Unmatched call to release()'));
            return;
        }

        this._holdCount--;

        if (this._holdCount === 0)
            this._queueShutdownCheck();
    }

    /**
     * _handleError:
     * @param {Gio.DBusMethodInvocation}
     * @param {Error}
     *
     * Complete @invocation with an appropriate error if @error is set;
     * useful for implementing early returns from method implementations.
     *
     * @returns {bool} - true if @invocation was completed
     */

    _handleError(invocation, error) {
        if (error === null)
            return false;

        if (error instanceof GLib.Error) {
            invocation.return_gerror(error);
        } else {
            let name = error.name;
            if (!name.includes('.')) // likely a normal JS error
                name = `org.gnome.gjs.JSError.${name}`;
            invocation.return_dbus_error(name, error.message);
        }

        return true;
    }

    _maybeShutdown() {
        if (!this._autoShutdown)
            return;

        if (GLib.getenv('SHELL_DBUS_PERSIST'))
            return;

        if (this._holdCount > 0)
            return;

        this.emit('shutdown');
    }

    _queueShutdownCheck() {
        if (this._shutdownTimeoutId)
            GLib.source_remove(this._shutdownTimeoutId);

        this._shutdownTimeoutId = GLib.timeout_add_seconds(
            GLib.PRIORITY_DEFAULT, IDLE_SHUTDOWN_TIME,
            () => {
                this._shutdownTimeoutId = 0;
                this._maybeShutdown();

                return GLib.SOURCE_REMOVE;
            });
    }

    _trackSender(sender) {
        if (this._senders.has(sender))
            return;

        this.hold();
        this._senders.set(sender,
            this._dbusImpl.get_connection().watch_name(
                sender,
                Gio.BusNameWatcherFlags.NONE,
                null,
                () => this._untrackSender(sender)));
    }

    _untrackSender(sender) {
        const id = this._senders.get(sender);

        if (id)
            this._dbusImpl.get_connection().unwatch_name(id);

        if (this._senders.delete(sender))
            this.release();
    }

    _injectTracking(methodName) {
        const { prototype } = Gio.DBusMethodInvocation;
        const origMethod = prototype[methodName];
        const that = this;

        prototype[methodName] = function (...args) {
            origMethod.apply(this, args);

            if (that._hasSignals)
                that._trackSender(this.get_sender());

            that._queueShutdownCheck();
        };
    }
};
Signals.addSignalMethods(ServiceImplementation.prototype);

var DBusService = class {
    constructor(name, service) {
        this._name = name;
        this._service = service;
        this._loop = new GLib.MainLoop(null, false);

        this._service.connect('shutdown', () => this._loop.quit());
    }

    run() {
        // Bail out when not running under gnome-shell
        Gio.DBus.watch_name(Gio.BusType.SESSION,
            'org.gnome.Shell',
            Gio.BusNameWatcherFlags.NONE,
            null,
            () => this._loop.quit());

        this._service.register();

        let flags = Gio.BusNameOwnerFlags.ALLOW_REPLACEMENT;
        if (programArgs.includes('--replace'))
            flags |= Gio.BusNameOwnerFlags.REPLACE;

        Gio.DBus.own_name(Gio.BusType.SESSION,
            this._name,
            flags,
            () => this._service.export(),
            null,
            () => this._loop.quit());

        this._loop.run();
    }
};
