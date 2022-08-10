// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

// the following is a modified version of bolt/contrib/js/client.js

const { Gio, GLib, GObject, Polkit, Shell } = imports.gi;
const Signals = imports.misc.signals;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const {SystemIndicator} = imports.ui.quickSettings;

const { loadInterfaceXML } = imports.misc.fileUtils;

/* Keep in sync with data/org.freedesktop.bolt.xml */

const BoltClientInterface = loadInterfaceXML('org.freedesktop.bolt1.Manager');
const BoltDeviceInterface = loadInterfaceXML('org.freedesktop.bolt1.Device');

const BoltDeviceProxy = Gio.DBusProxy.makeProxyWrapper(BoltDeviceInterface);

/*  */

var Status = {
    DISCONNECTED: 'disconnected',
    CONNECTING: 'connecting',
    CONNECTED: 'connected',
    AUTHORIZING: 'authorizing',
    AUTH_ERROR: 'auth-error',
    AUTHORIZED: 'authorized',
};

var Policy = {
    DEFAULT: 'default',
    MANUAL: 'manual',
    AUTO: 'auto',
};

var AuthCtrl = {
    NONE: 'none',
};

var AuthMode = {
    DISABLED: 'disabled',
    ENABLED: 'enabled',
};

const BOLT_DBUS_CLIENT_IFACE = 'org.freedesktop.bolt1.Manager';
const BOLT_DBUS_NAME = 'org.freedesktop.bolt';
const BOLT_DBUS_PATH = '/org/freedesktop/bolt';

var Client = class extends Signals.EventEmitter {
    constructor() {
        super();

        this._proxy = null;
        this.probing = false;
        this._getProxy();
    }

    async _getProxy() {
        let nodeInfo = Gio.DBusNodeInfo.new_for_xml(BoltClientInterface);
        try {
            this._proxy = await Gio.DBusProxy.new(
                Gio.DBus.system,
                Gio.DBusProxyFlags.DO_NOT_AUTO_START,
                nodeInfo.lookup_interface(BOLT_DBUS_CLIENT_IFACE),
                BOLT_DBUS_NAME,
                BOLT_DBUS_PATH,
                BOLT_DBUS_CLIENT_IFACE,
                null);
        } catch (e) {
            log(`error creating bolt proxy: ${e.message}`);
            return;
        }
        this._proxy.connectObject('g-properties-changed',
            this._onPropertiesChanged.bind(this), this);
        this._deviceAddedId = this._proxy.connectSignal('DeviceAdded', this._onDeviceAdded.bind(this));

        this.probing = this._proxy.Probing;
        if (this.probing)
            this.emit('probing-changed', this.probing);
    }

    _onPropertiesChanged(proxy, properties) {
        const probingChanged = !!properties.lookup_value('Probing', null);
        if (probingChanged) {
            this.probing = this._proxy.Probing;
            this.emit('probing-changed', this.probing);
        }
    }

    _onDeviceAdded(proxy, emitter, params) {
        let [path] = params;
        let device = new BoltDeviceProxy(Gio.DBus.system,
                                         BOLT_DBUS_NAME,
                                         path);
        this.emit('device-added', device);
    }

    /* public methods */
    close() {
        if (!this._proxy)
            return;

        this._proxy.disconnectSignal(this._deviceAddedId);
        this._proxy.disconnectObject(this);
        this._proxy = null;
    }

    async enrollDevice(id, policy) {
        try {
            const [path] = await this._proxy.EnrollDeviceAsync(id, policy, AuthCtrl.NONE);
            const device = new BoltDeviceProxy(Gio.DBus.system, BOLT_DBUS_NAME, path);
            return device;
        } catch (error) {
            Gio.DBusError.strip_remote_error(error);
            throw error;
        }
    }

    get authMode() {
        return this._proxy.AuthMode;
    }
};

/* helper class to automatically authorize new devices */
var AuthRobot = class extends Signals.EventEmitter {
    constructor(client) {
        super();

        this._client = client;

        this._devicesToEnroll = [];
        this._enrolling = false;

        this._client.connect('device-added', this._onDeviceAdded.bind(this));
    }

    close() {
        this.disconnectAll();
        this._client = null;
    }

    /* the "device-added" signal will be emitted by boltd for every
     * device that is not currently stored in the database. We are
     * only interested in those devices, because all known devices
     * will be handled by the user himself */
    _onDeviceAdded(cli, dev) {
        if (dev.Status !== Status.CONNECTED)
            return;

        /* check if authorization is enabled in the daemon. if not
         * we won't even bother authorizing, because we will only
         * get an error back. The exact contents of AuthMode might
         * change in the future, but must contain AuthMode.ENABLED
         * if it is enabled. */
        if (!cli.authMode.split('|').includes(AuthMode.ENABLED))
            return;

        /* check if we should enroll the device */
        let res = [false];
        this.emit('enroll-device', dev, res);
        if (res[0] !== true)
            return;

        /* ok, we should authorize the device, add it to the back
         * of the list  */
        this._devicesToEnroll.push(dev);
        this._enrollDevices();
    }

    /* The enrollment queue:
     *   - new devices will be added to the end of the array.
     *   - an idle callback will be scheduled that will keep
     *     calling itself as long as there a devices to be
     *     enrolled.
     */
    _enrollDevices() {
        if (this._enrolling)
            return;

        this._enrolling = true;
        GLib.idle_add(GLib.PRIORITY_DEFAULT,
                      this._enrollDevicesIdle.bind(this));
    }

    async _enrollDevicesIdle() {
        let devices = this._devicesToEnroll;

        let dev = devices.shift();
        if (dev === undefined)
            return GLib.SOURCE_REMOVE;

        try {
            await this._client.enrollDevice(dev.Uid, Policy.DEFAULT);

            /* TODO: scan the list of devices to be authorized for children
             *  of this device and remove them (and their children and
             *  their children and ....) from the device queue
             */
            this._enrolling = this._devicesToEnroll.length > 0;

            if (this._enrolling) {
                GLib.idle_add(GLib.PRIORITY_DEFAULT,
                    this._enrollDevicesIdle.bind(this));
            }
        } catch (error) {
            this.emit('enroll-failed', null, error);
        }
        return GLib.SOURCE_REMOVE;
    }
};

/* eof client.js  */

var Indicator = GObject.registerClass(
class Indicator extends SystemIndicator {
    _init() {
        super._init();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'thunderbolt-symbolic';

        this._client = new Client();
        this._client.connect('probing-changed', this._onProbing.bind(this));

        this._robot =  new AuthRobot(this._client);

        this._robot.connect('enroll-device', this._onEnrollDevice.bind(this));
        this._robot.connect('enroll-failed', this._onEnrollFailed.bind(this));

        Main.sessionMode.connect('updated', this._sync.bind(this));
        this._sync();

        this._source = null;
        this._perm = null;
        this._createPermission();
    }

    async _createPermission() {
        try {
            this._perm = await Polkit.Permission.new('org.freedesktop.bolt.enroll', null, null);
        } catch (e) {
            log(`Failed to get PolKit permission: ${e}`);
        }
    }

    _onDestroy() {
        this._robot.close();
        this._client.close();
    }

    _ensureSource() {
        if (!this._source) {
            this._source = new MessageTray.Source(_("Thunderbolt"),
                                                  'thunderbolt-symbolic');
            this._source.connect('destroy', () => (this._source = null));

            Main.messageTray.add(this._source);
        }

        return this._source;
    }

    _notify(title, body) {
        if (this._notification)
            this._notification.destroy();

        let source = this._ensureSource();

        this._notification = new MessageTray.Notification(source, title, body);
        this._notification.setUrgency(MessageTray.Urgency.HIGH);
        this._notification.connect('destroy', () => {
            this._notification = null;
        });
        this._notification.connect('activated', () => {
            let app = Shell.AppSystem.get_default().lookup_app('gnome-thunderbolt-panel.desktop');
            if (app)
                app.activate();
        });
        this._source.showNotification(this._notification);
    }

    /* Session callbacks */
    _sync() {
        let active = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        this._indicator.visible = active && this._client.probing;
    }

    /* Bolt.Client callbacks */
    _onProbing(cli, probing) {
        if (probing)
            this._indicator.icon_name = 'thunderbolt-acquiring-symbolic';
        else
            this._indicator.icon_name = 'thunderbolt-symbolic';

        this._sync();
    }

    /* AuthRobot callbacks */
    _onEnrollDevice(obj, device, policy) {
        /* only authorize new devices when in an unlocked user session */
        let unlocked = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        /* and if we have the permission to do so, otherwise we trigger a PolKit dialog */
        let allowed = this._perm && this._perm.allowed;

        let auth = unlocked && allowed;
        policy[0] = auth;

        log(`thunderbolt: [${device.Name}] auto enrollment: ${auth ? 'yes' : 'no'} (allowed: ${allowed ? 'yes' : 'no'})`);

        if (auth)
            return; /* we are done */

        if (!unlocked) {
            const title = _("Unknown Thunderbolt device");
            const body = _("New device has been detected while you were away. Please disconnect and reconnect the device to start using it.");
            this._notify(title, body);
        } else {
            const title = _("Unauthorized Thunderbolt device");
            const body = _("New device has been detected and needs to be authorized by an administrator.");
            this._notify(title, body);
        }
    }

    _onEnrollFailed(obj, device, error) {
        const title = _("Thunderbolt authorization error");
        const body = _("Could not authorize the Thunderbolt device: %s").format(error.message);
        this._notify(title, body);
    }
});
