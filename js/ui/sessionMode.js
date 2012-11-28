// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Signals = imports.signals;

const Main = imports.ui.main;
const Params = imports.misc.params;

const DEFAULT_MODE = 'restrictive';

const _modes = {
    'restrictive': {
        hasOverview: false,
        showCalendarEvents: false,
        allowSettings: false,
        allowExtensions: false,
        hasRunDialog: false,
        hasWorkspaces: false,
        hasWindows: false,
        hasNotifications: false,
        isLocked: false,
        isGreeter: false,
        isPrimary: false,
        unlockDialog: null,
        components: [],
        panel: {
            left: [],
            center: [],
            right: []
        },
        panelStyle: null
    },

    'gdm': {
        hasNotifications: true,
        isGreeter: true,
        isPrimary: true,
        unlockDialog: imports.gdm.loginDialog.LoginDialog,
        components: ['polkitAgent'],
        panel: {
            left: ['logo'],
            center: ['dateMenu'],
            right: ['a11y', 'display', 'keyboard',
                    'volume', 'battery', 'powerMenu']
        },
        panelStyle: 'login-screen'
    },

    'lock-screen': {
        isLocked: true,
        isGreeter: undefined,
        unlockDialog: undefined,
        components: ['polkitAgent', 'telepathyClient'],
        panel: {
            left: ['userMenu'],
            center: [],
            right: ['lockScreen']
        },
        panelStyle: 'lock-screen'
    },

    'unlock-dialog': {
        isLocked: true,
        unlockDialog: undefined,
        components: ['polkitAgent', 'telepathyClient'],
        panel: {
            left: ['userMenu'],
            center: [],
            right: ['a11y', 'keyboard', 'lockScreen']
        },
        panelStyle: 'unlock-screen'
    },

    'initial-setup': {
        isPrimary: true,
        components: ['keyring'],
        panel: {
            left: [],
            center: ['dateMenu'],
            right: ['a11y', 'keyboard', 'volume']
        }
    },

    'user': {
        hasOverview: true,
        showCalendarEvents: true,
        allowSettings: true,
        allowExtensions: true,
        hasRunDialog: true,
        hasWorkspaces: true,
        hasWindows: true,
        hasNotifications: true,
        isLocked: false,
        isPrimary: true,
        unlockDialog: imports.ui.unlockDialog.UnlockDialog,
        components: ['networkAgent', 'polkitAgent', 'telepathyClient',
                     'keyring', 'recorder', 'autorunManager', 'automountManager'],
        panel: {
            left: ['activities', 'appMenu'],
            center: ['dateMenu'],
            right: ['a11y', 'keyboard', 'volume', 'bluetooth',
                    'network', 'battery', 'userMenu']
        }
    }
};

function _getModes() {
    let modes = _modes;
    let dataDirs = GLib.get_system_data_dirs();
    for (let i = 0; i < dataDirs.length; i++) {
        let path = GLib.build_filenamev([dataDirs[i], 'gnome-shell', 'modes']);
        let dir = Gio.file_new_for_path(path);

        try {
            dir.query_info('standard:type', Gio.FileQueryInfoFlags.NONE, null);
        } catch (e) {
            continue;
        }

        _getModesFromDir(dir, modes);
    }
    return modes;
}

function _getModesFromDir(dir, modes) {
    let fileEnum;
    try {
        fileEnum = dir.enumerate_children('standard::*',
                                          Gio.FileQueryInfoFlags.NONE, null);
    } catch(e) {
        return;
    }

    let info;
    while ((info = fileEnum.next_file(null)) != null) {
        let name = info.get_name();
        let suffix = name.indexOf('.json');
        let modeName = suffix == -1 ? name : name.slice(name, suffix);

        if (modes.hasOwnProperty(modeName))
            continue;

        let file = dir.get_child(name);
        let fileContent, success, tag, newMode;
        try {
            [success, fileContent, tag] = file.load_contents(null);
            newMode = JSON.parse(fileContent);
        } catch(e) {
            continue;
        }

        modes[modeName] = {};
        let propBlacklist = ['unlockDialog'];
        for (let prop in modes[DEFAULT_MODE]) {
            if (newMode[prop] !== undefined &&
                propBlacklist.indexOf(prop) == -1)
                modes[modeName][prop]= newMode[prop];
        }
        modes[modeName]['isPrimary'] = true;
    }
    fileEnum.close(null);
}

function listModes() {
    let modes = Object.getOwnPropertyNames(_getModes());
    for (let i = 0; i < modes.length; i++)
        if (_modes[modes[i]].isPrimary)
            print(modes[i]);
}

const SessionMode = new Lang.Class({
    Name: 'SessionMode',

    _init: function() {
        global.connect('notify::session-mode', Lang.bind(this, this._sync));
        this._modes = _getModes();
        let mode = this._modes[global.session_mode].isPrimary ? global.session_mode
                                                              : 'user';
        this._modeStack = [mode];
        this._sync();
    },

    pushMode: function(mode) {
        this._modeStack.push(mode);
        this._sync();
    },

    popMode: function(mode) {
        if (this.currentMode != mode || this._modeStack.length === 1)
            throw new Error("Invalid SessionMode.popMode");
        this._modeStack.pop();
        this._sync();
    },

    switchMode: function(to) {
        if (this.currentMode == to)
            return;
        this._modeStack[this._modeStack.length - 1] = to;
        this._sync();
    },

    get currentMode() {
        return this._modeStack[this._modeStack.length - 1];
    },

    _sync: function() {
        let params = this._modes[this.currentMode];
        params = Params.parse(params, this._modes[DEFAULT_MODE]);

        // A simplified version of Lang.copyProperties, handles
        // undefined as a special case for "no change / inherit from previous mode"
        for (let prop in params) {
            if (params[prop] !== undefined)
                this[prop] = params[prop];
        }

        this.emit('updated');
    }
});
Signals.addSignalMethods(SessionMode.prototype);
