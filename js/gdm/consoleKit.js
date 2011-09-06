// -*- mode: js2; indent-tabs-mode: nil; js2-basic-offset: 4 -*-

const DBus = imports.dbus;

const ConsoleKitManagerIface = {
    name: 'org.freedesktop.ConsoleKit.Manager',
    methods: [{ name: 'CanRestart',
                inSignature: '',
                outSignature: 'b' },
              { name: 'CanStop',
                inSignature: '',
                outSignature: 'b' },
              { name: 'Restart',
                inSignature: '',
                outSignature: '' },
              { name: 'Stop',
                inSignature: '',
                outSignature: '' }]
};

function ConsoleKitManager() {
    this._init();
};

ConsoleKitManager.prototype = {
    _init: function() {
        DBus.system.proxifyObject(this,
                                  'org.freedesktop.ConsoleKit',
                                  '/org/freedesktop/ConsoleKit/Manager');
    }
};
DBus.proxifyPrototype(ConsoleKitManager.prototype, ConsoleKitManagerIface);
