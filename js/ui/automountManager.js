/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Lang = imports.lang;
const DBus = imports.dbus;
const Mainloop = imports.mainloop;
const Gio = imports.gi.Gio;
const Params = imports.misc.params;

const Main = imports.ui.main;
const ScreenSaver = imports.misc.screenSaver;

// GSettings keys
const SETTINGS_SCHEMA = 'org.gnome.desktop.media-handling';
const SETTING_ENABLE_AUTOMOUNT = 'automount';

const ConsoleKitSessionIface = {
    name: 'org.freedesktop.ConsoleKit.Session',
    methods: [{ name: 'IsActive',
                inSignature: '',
                outSignature: 'b' }],
    signals: [{ name: 'ActiveChanged',
                inSignature: 'b' }]
};

const ConsoleKitSessionProxy = DBus.makeProxyClass(ConsoleKitSessionIface);

const ConsoleKitManagerIface = {
    name: 'org.freedesktop.ConsoleKit.Manager',
    methods: [{ name: 'GetCurrentSession',
                inSignature: '',
                outSignature: 'o' }]
};

function ConsoleKitManager() {
    this._init();
};

ConsoleKitManager.prototype = {
    _init: function() {
        this.sessionActive = true;

        DBus.system.proxifyObject(this,
                                  'org.freedesktop.ConsoleKit',
                                  '/org/freedesktop/ConsoleKit/Manager');

        DBus.system.watch_name('org.freedesktop.ConsoleKit',
                               false, // do not launch a name-owner if none exists
                               Lang.bind(this, this._onManagerAppeared),
                               Lang.bind(this, this._onManagerVanished));
    },

    _onManagerAppeared: function(owner) {
        this.GetCurrentSessionRemote(Lang.bind(this, this._onCurrentSession));
    },

    _onManagerVanished: function(oldOwner) {
        this.sessionActive = true;
    },

    _onCurrentSession: function(session) {
        this._ckSession = new ConsoleKitSessionProxy(DBus.system, 'org.freedesktop.ConsoleKit', session);

        this._ckSession.connect
            ('ActiveChanged', Lang.bind(this, function(object, isActive) {
                this.sessionActive = isActive;            
            }));
        this._ckSession.IsActiveRemote(Lang.bind(this, function(isActive) {
            this.sessionActive = isActive;            
        }));
    }
};
DBus.proxifyPrototype(ConsoleKitManager.prototype, ConsoleKitManagerIface);

function AutomountManager() {
    this._init();
}

AutomountManager.prototype = {
    _init: function() {
        this._settings = new Gio.Settings({ schema: SETTINGS_SCHEMA });
        this._volumeQueue = [];

        this.ckListener = new ConsoleKitManager();

        this._ssProxy = new ScreenSaver.ScreenSaverProxy();
        this._ssProxy.connect('active-changed',
                              Lang.bind(this,
                                        this._screenSaverActiveChanged));

        this._volumeMonitor = Gio.VolumeMonitor.get();

        this._volumeMonitor.connect('volume-added',
                                    Lang.bind(this,
                                              this._onVolumeAdded));
        this._volumeMonitor.connect('volume-removed',
                                    Lang.bind(this,
                                              this._onVolumeRemoved));

        Mainloop.idle_add(Lang.bind(this, this._startupMountAll));
    },

    _screenSaverActiveChanged: function(object, isActive) {
        if (!isActive) {
            this._volumeQueue.forEach(Lang.bind(this, function(volume) {
                this._checkAndMountVolume(volume);
            }));
        }

        // clear the queue anyway
        this._volumeQueue = [];
    },

    _startupMountAll: function() {
        let volumes = this._volumeMonitor.get_volumes();
        volumes.forEach(Lang.bind(this, function(volume) {
            this._checkAndMountVolume(volume, { checkSession: false,
                                                useMountOp: false });
        }));

        return false;
    },

    _onVolumeAdded: function(monitor, volume) {
        this._checkAndMountVolume(volume);
    },

    _checkAndMountVolume: function(volume, params) {
        params = Params.parse(params, { checkSession: true,
                                        useMountOp: true });

        if (!this._settings.get_boolean(SETTING_ENABLE_AUTOMOUNT))
            return;

        if (!volume.should_automount() ||
            !volume.can_mount())
            return;

        if (params.checkSession) {
            // if we're not in the current ConsoleKit session,
            // don't attempt automount
            if (!this.ckListener.sessionActive)
                return;

            if (this._ssProxy.getActive()) {
                if (this._volumeQueue.indexOf(volume) == -1)
                    this._volumeQueue.push(volume);

                return;
            }
        }

        // TODO: mount op
        this._mountVolume(volume, null);
    },

    _mountVolume: function(volume, operation) {
        volume.mount(0, operation, null,
                     Lang.bind(this, this._onVolumeMounted));
    },

    _onVolumeMounted: function(volume, res) {
        try {
            volume.mount_finish(res);
        } catch (e) {
            log('Unable to mount volume ' + volume.get_name() + ': ' +
                e.toString());
        }
    },

    _onVolumeRemoved: function(monitor, volume) {
        this._volumeQueue = 
            this._volumeQueue.filter(function(element) {
                return (element != volume);
            });
    }
}
