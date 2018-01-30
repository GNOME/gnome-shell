// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

// the following is basically a copy of bolt/contrib/js/client.js

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Signals = imports.signals;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

/* Keep in sync with data/org.freedesktop.bolt.xml */

const BoltClientInterface = '<node> \
  <interface name="org.freedesktop.bolt1.Manager"> \
    <property name="Version" type="u" access="read"></property> \
    <property name="Probing" type="b" access="read"></property> \
    <method name="ListDevices"> \
      <arg name="devices" direction="out" type="ao"> </arg> \
    </method> \
    <method name="DeviceByUid"> \
      <arg type="s" name="uid" direction="in"> </arg> \
      <arg name="device" direction="out" type="o"> </arg> \
    </method> \
    <method name="EnrollDevice"> \
      <arg type="s" name="uid" direction="in"> </arg> \
      <arg type="u" name="policy" direction="in"> </arg> \
      <arg type="u" name="flags" direction="in"> </arg> \
      <arg name="device" direction="out" type="o"> </arg> \
    </method> \
    <method name="ForgetDevice">  \
      <arg type="s" name="uid" direction="in"> </arg> \
    </method> \
    <signal name="DeviceAdded"> \
      <arg name="device" type="o"> </arg> \
    </signal> \
    <signal name="DeviceRemoved"> \
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
    <method name="Authorize"> \
      <arg type="u" name="flags" direction="in"> </arg> \
    </method> \
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

    _init: function(readyCallback) {
	this._readyCallback = readyCallback;

	this._proxy = new BoltClientProxy(
	    Gio.DBus.system,
	    BOLT_DBUS_NAME,
	    BOLT_DBUS_PATH,
	    Lang.bind(this, this._onProxyReady)
	);

	this._signals = [];
	this.probing = false;
    },

    _onProxyReady: function(proxy, error) {
	if (error !== null) {
	    log(error.message);
	    return;
	}
	this._proxy = proxy;
	this._proxyConnect('g-properties-changed', Lang.bind(this, this._onPropertiesChanged));
	this._proxyConnect('DeviceAdded', Lang.bind(this, this._onDeviceAdded), true);

	this.probing = this._proxy.Probing;
	if (this.probing)
	    this.emit('probing-changed', this.probing);

	this._readyCallback(this);
    },

    _onPropertiesChanged: function(proxy, properties) {
        let unpacked = properties.deep_unpack();
        if (!('Probing' in unpacked))
	    return;

	this.probing = this._proxy.Probing;
	this.emit('probing-changed', this.probing);
    },

    _onDeviceAdded: function(proxy, emitter, params) {
	let [path] = params;
	let device = new BoltDeviceProxy(Gio.DBus.system,
					 BOLT_DBUS_NAME,
					 path);
	this.emit('device-added', device);
    },

    _proxyConnect: function(name, callback, dbus) {
	var signal_id;

	if (dbus === true)
	    signal_id = this._proxy.connectSignal(name, Lang.bind(this, callback));
	else
	    signal_id = this._proxy.connect(name, Lang.bind(this, callback));

	this._signals.push([dbus, signal_id]);
    },

    _proxyDisconnectAll: function() {
	while (this._signals.length) {
	    let [dbus, sid] = this._signals.shift();
	    if (dbus === true)
		this._proxy.disconnectSignal(sid);
	    else
		this._proxy.disconnect(sid);
	}
    },

    /* public methods */
    close: function() {
	this._proxyDisconnectAll();
	this._proxy = null;
    },

    listDevices: function(callback) {
	this._proxy.ListDevicesRemote(Lang.bind(this, function (res, error) {
	    if (error) {
		callback(null, error);
		return;
	    }

	    let [paths] = res;

	    let devices = [];
	    for (let i = 0; i < paths.length; i++) {
		let path = paths[i];
		let device = new BoltDeviceProxy(Gio.DBus.system,
						 BOLT_DBUS_NAME,
						 path);
		devices.push(device);
	    }
	    callback(devices, null);
	}));

    },

    enrollDevice: function(id, policy, callback) {
	this._proxy.EnrollDeviceRemote(id, policy, AuthFlags.NONE,
				       Lang.bind(this, function (res, error) {
	    if (error) {
		callback(null, error);
		return;
	    }

	    let [path] = res;
	    let device = new BoltDeviceProxy(Gio.DBus.system,
					     BOLT_DBUS_NAME,
					     path);
	    callback(device, null);
	}));
    },

    deviceGetParent: function(dev, callback) {
	let parentUid = dev.Parent;

	if (!parentUid) {
	    callback (null, dev, null);
	    return;
	}

	this._proxy.DeviceByUidRemote(dev.Parent, Lang.bind(this, function (res, error) {
	    if (error) {
		callback(null, dev, error);
		return;
	    }

	    let [path] = res;
	    let parent = new BoltDeviceProxy(Gio.DBus.system,
					     BOLT_DBUS_NAME,
					     path);
	    callback(parent, dev, null);
	}));
    },
});

Signals.addSignalMethods(Client.prototype);

/* helper class to automatically authorize new devices */
var AuthRobot = new Lang.Class({
    Name: 'BoltAuthRobot',

    _init: function(client) {

	this._client = client;

	this._devicesToEnroll = [];
	this._enrolling = false;

	this._client.connect('device-added', Lang.bind(this, this._onDeviceAdded));
    },

    close: function() {
	this.disconnectAll();
	this._client = null;
    },

    /* the "device-added" signal will be emitted by boltd for every
     * device that is not currently stored in the database. We are
     * only interested in those devices, because all known devices
     * will be handled by the user himself */
    _onDeviceAdded: function(cli, dev) {
	if (dev.Status !== Status.CONNECTED) {
	    return;
	}

	/* check if we should enroll the device */
	let res = [false];
	this.emit('enroll-device', dev, res);
	if (res[0] !== true) {
	    return;
	}

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
    _enrollDevices: function() {
	if (this._enrolling) {
	    return;
	}

	this.enrolling = true;
	GLib.idle_add(GLib.PRIORITY_DEFAULT,
		      Lang.bind(this, this._enrollDevicesIdle));
    },

    _onEnrollDone: function(device, error) {
	if (error) {
	    this.emit('enroll-failed', error, device);
	}

	/* TODO: scan the list of devices to be authorized for children
	 *  of this device and remove them (and their children and
	 *  their children and ....) from the device queue
	 */
	this._enrolling = this._devicesToEnroll.length > 0;

	if (this._enrolling) {
	    GLib.idle_add(GLib.PRIORITY_DEFAULT,
			  Lang.bind(this, this._enrollDevicesIdle));
	}
    },

    _enrollDevicesIdle: function() {
	let devices = this._devicesToEnroll;

	let dev = devices.shift();
	if (dev === undefined) {
	    return GLib.SOURCE_REMOVE;
	}

	this._client.enrollDevice(dev.Uid,
				  Policy.DEFAULT,
				  Lang.bind(this, this._onEnrollDone));
	return GLib.SOURCE_REMOVE;
    },


});

Signals.addSignalMethods(AuthRobot.prototype);

/* eof client.js  */

var Indicator = new Lang.Class({
    Name: 'ThunderboltIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init: function() {
        this.parent();

	this._indicator = this._addIndicator();
        this._indicator.icon_name = 'thunderbolt-symbolic';

	this._client = new Client(Lang.bind(this, this._onClientReady));
	this._client.connect('probing-changed', Lang.bind(this, this._onProbing));

	this._robot =  new AuthRobot(this._client);

	this._robot.connect('enroll-device', Lang.bind(this, this._onEnrollDevice));
	this._robot.connect('enroll-failed', Lang.bind(this, this._onEnrollFailed));

	//this.actor.connect('destroy', Lang.bind(this, this._onDestroy));

	Main.sessionMode.connect('updated', Lang.bind(this, this._sessionUpdated));
        this._sessionUpdated();

	this._source = null;
	log('Thunderbolt indicator ready!');
    },

    _onDestroy: function() {
	log('Destorying Thunderbolt indicator');
	this._client.close();
	this._robot.close();
	// disconnect updated signal
	//	this.disconnectAll();
    },

    _ensureSource: function() {
        if (!this._source) {
            this._source = new MessageTray.Source(_("Thunderbolt"),
                                                  'thunderbolt-symbolic');
            this._source.policy = new MessageTray.NotificationApplicationPolicy('gnome-thunderbolt-panel');
            this._source.connect('destroy', Lang.bind(this, function() {
                this._source = null;
            }));

            Main.messageTray.add(this._source);
        }

        return this._source;
    },

    _notify: function(title, body, urgency) {
        if (this._notification)
            this._notification.destroy();

        let source = this._ensureSource();

	this._notification = new MessageTray.Notification(source, title, body);
	this._notification.setUrgency(urgency);
        this._notification.setTransient(true);
        this._notification.connect('destroy', function() {
            this._notification = null;
        });
        this._source.notify(this._notification);
    },

    /* Session callbacks */
    _sessionUpdated: function() {
        let active = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
	this._indicator.visible = active && this._client.probing;
    },


    /* Bolt.Client callbacks */
    _onClientReady: function() {
	log('Bolt client ready');
    },

    _onProbing: function(cli, probing) {
	if (probing) {
	    this._indicator.icon_name = 'thunderbolt-acquiring-symbolic';
	} else {
	    this._indicator.icon_name = 'thunderbolt-symbolic';
	}

	let active = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
	this._indicator.visible = active && probing;
    },


    /* AuthRobot callbacks */
    _onEnrollDevice: function(obj, device, policy) {
	let auth = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
	policy[0] = auth;

	log("[%s] auto enrollment: %s".format(device.Uid, auth ? 'yes' : 'no'));
	if (auth) {
	    /* we are done */
	    return;
	}

	const title = _('%s Thunderbolt device'.format(device.Name));
	const body = _('New thunderbolt devices have been detected while you were away. Please disconnect and reconnect the device to start using it.');
	this._notify(title, body, MessageTray.Urgency.HIGH);
    },

    _onEnrollFailed: function (obj, device, error) {
	const title = _('Thunderbolt authorization error');
	const body = _('Could not authorize the thunderbolt device: %s'.format(error.message));

	this._notify(title, body, MessageTray.Urgency.HIGH);
    },

});
