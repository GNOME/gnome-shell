// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

// the following is a modified version of bolt/contrib/js/client.js

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const PanelMenu = imports.ui.panelMenu;

/* Keep in sync with data/org.freedesktop.bolt.xml */

const BoltClientInterface = '<node> \
  <interface name="org.freedesktop.bolt1.Manager"> \
    <property name="Probing" type="b" access="read"></property> \
    <method name="EnrollDevice"> \
      <arg type="s" name="uid" direction="in"> </arg> \
      <arg type="u" name="policy" direction="in"> </arg> \
      <arg type="u" name="flags" direction="in"> </arg> \
      <arg name="device" direction="out" type="o"> </arg> \
    </method> \
    <signal name="DeviceAdded"> \
      <arg name="device" type="o"> </arg> \
    </signal> \
  </interface> \
</node>';

const BoltDeviceInterface = '<node> \
  <interface name="org.freedesktop.bolt1.Device"> \
    <property name="Uid" type="s" access="read"></property> \
    <property name="Name" type="s" access="read"></property> \
    <property name="Vendor" type="s" access="read"></property> \
    <property name="Status" type="u" access="read"></property> \
    <property name="SysfsPath" type="s" access="read"></property> \
    <property name="Security" type="u" access="read"></property> \
    <property name="Parent" type="s" access="read"></property> \
    <property name="Stored" type="b" access="read"></property> \
    <property name="Policy" type="u" access="read"></property> \
    <property name="Key" type="u" access="read"></property> \
  </interface> \
</node>';

const BoltClientProxy = Gio.DBusProxy.makeProxyWrapper(BoltClientInterface);
const BoltDeviceProxy = Gio.DBusProxy.makeProxyWrapper(BoltDeviceInterface);

/*  */

var Status = {
    DISCONNECTED: 0,
    CONNECTED: 1,
    AUTHORIZING: 2,
    AUTH_ERROR: 3,
    AUTHORIZED: 4,
    AUTHORIZED_SECURE: 5,
    AUTHORIZED_NEWKY: 6
};

var Policy = {
    DEFAULT: 0,
    MANUAL: 1,
    AUTO:2
};

var AuthFlags = {
    NONE: 0,
};

const BOLT_DBUS_NAME = 'org.freedesktop.bolt';
const BOLT_DBUS_PATH = '/org/freedesktop/bolt';

var Client = new Lang.Class({
    Name: 'BoltClient',

    _init() {

	this._proxy = null;
        new BoltClientProxy(
	    Gio.DBus.system,
	    BOLT_DBUS_NAME,
	    BOLT_DBUS_PATH,
	    this._onProxyReady.bind(this)
	);

	this.probing = false;
    },

    _onProxyReady(proxy, error) {
	if (error !== null) {
	    log('error creating bolt proxy: %s'.format(error.message));
	    return;
	}
	this._proxy = proxy;
	this._propsChangedId = this._proxy.connect('g-properties-changed', this._onPropertiesChanged.bind(this));
	this._deviceAddedId = this._proxy.connectSignal('DeviceAdded', this._onDeviceAdded.bind(this));

	this.probing = this._proxy.Probing;
	if (this.probing)
	    this.emit('probing-changed', this.probing);

    },

    _onPropertiesChanged(proxy, properties) {
        let unpacked = properties.deep_unpack();
        if (!('Probing' in unpacked))
	    return;

	this.probing = this._proxy.Probing;
	this.emit('probing-changed', this.probing);
    },

    _onDeviceAdded(proxy, emitter, params) {
	let [path] = params;
	let device = new BoltDeviceProxy(Gio.DBus.system,
					 BOLT_DBUS_NAME,
					 path);
	this.emit('device-added', device);
    },

    /* public methods */
    close() {
        if (!this._proxy)
            return;

	this._proxy.disconnectSignal(this._deviceAddedId);
	this._proxy.disconnect(this._propsChangedId);
	this._proxy = null;
    },

    enrollDevice(id, policy, callback) {
	this._proxy.EnrollDeviceRemote(id, policy, AuthFlags.NONE,
                                       (res, error) => {
	    if (error) {
		callback(null, error);
		return;
	    }

	    let [path] = res;
	    let device = new BoltDeviceProxy(Gio.DBus.system,
					     BOLT_DBUS_NAME,
					     path);
	    callback(device, null);
	});
    }

});

Signals.addSignalMethods(Client.prototype);

/* helper class to automatically authorize new devices */
var AuthRobot = new Lang.Class({
    Name: 'BoltAuthRobot',

    _init(client) {

	this._client = client;

	this._devicesToEnroll = [];
	this._enrolling = false;

	this._client.connect('device-added', this._onDeviceAdded.bind(this));
    },

    close() {
	this.disconnectAll();
	this._client = null;
    },

    /* the "device-added" signal will be emitted by boltd for every
     * device that is not currently stored in the database. We are
     * only interested in those devices, because all known devices
     * will be handled by the user himself */
    _onDeviceAdded(cli, dev) {
	if (dev.Status !== Status.CONNECTED)
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
    },

    /* The enrollment queue:
     *   - new devices will be added to the end of the array.
     *   - an idle callback will be scheduled that will keep
     *     calling itself as long as there a devices to be
     *     enrolled.
     */
    _enrollDevices() {
	if (this._enrolling)
	    return;

	this.enrolling = true;
	GLib.idle_add(GLib.PRIORITY_DEFAULT,
		      this._enrollDevicesIdle.bind(this));
    },

    _onEnrollDone(device, error) {
	if (error)
	    this.emit('enroll-failed', error, device);

	/* TODO: scan the list of devices to be authorized for children
	 *  of this device and remove them (and their children and
	 *  their children and ....) from the device queue
	 */
	this._enrolling = this._devicesToEnroll.length > 0;

	if (this._enrolling)
	    GLib.idle_add(GLib.PRIORITY_DEFAULT,
			  this._enrollDevicesIdle.bind(this));
    },

    _enrollDevicesIdle() {
	let devices = this._devicesToEnroll;

	let dev = devices.shift();
	if (dev === undefined)
	    return GLib.SOURCE_REMOVE;

	this._client.enrollDevice(dev.Uid,
				  Policy.DEFAULT,
				  this._onEnrollDone.bind(this));
	return GLib.SOURCE_REMOVE;
    }

});

Signals.addSignalMethods(AuthRobot.prototype);

/* eof client.js  */

var Indicator = new Lang.Class({
    Name: 'ThunderboltIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init() {
        this.parent();

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
    },

    _onDestroy() {
        this._robot.close();
	this._client.close();
    },

    _ensureSource() {
        if (!this._source) {
            this._source = new MessageTray.Source(_("Thunderbolt"),
                                                  'thunderbolt-symbolic');
            this._source.connect('destroy', () => { this._source = null; });

            Main.messageTray.add(this._source);
        }

        return this._source;
    },

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
        this._source.notify(this._notification);
    },

    /* Session callbacks */
    _sync() {
        let active = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
	this._indicator.visible = active && this._client.probing;
    },


    /* Bolt.Client callbacks */
    _onProbing(cli, probing) {
	if (probing)
	    this._indicator.icon_name = 'thunderbolt-acquiring-symbolic';
	else
	    this._indicator.icon_name = 'thunderbolt-symbolic';

        this._sync();
    },


    /* AuthRobot callbacks */
    _onEnrollDevice(obj, device, policy) {
	let auth = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
	policy[0] = auth;

	log("thunderbolt: [%s] auto enrollment: %s".format(device.Name, auth ? 'yes' : 'no'));
	if (auth)
	    return; /* we are done */

	const title = _('Unknown Thunderbolt device');
	const body = _('New device has been detected while you were away. Please disconnect and reconnect the device to start using it.');
	this._notify(title, body);
    },

    _onEnrollFailed(obj, device, error) {
	const title = _('Thunderbolt authorization error');
	const body = _('Could not authorize the thunderbolt device: %s'.format(error.message));
	this._notify(title, body);
    }

});
