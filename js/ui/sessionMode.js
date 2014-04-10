// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Signals = imports.signals;

const FileUtils = imports.misc.fileUtils;
const Main = imports.ui.main;
const Params = imports.misc.params;

const Config = imports.misc.config;

const DEFAULT_MODE = 'restrictive';

const _modes = {
    'restrictive': {
        parentMode: null,
        stylesheetName: 'gnome-shell.css',
        overridesSchema: 'org.gnome.shell.overrides',
        hasOverview: false,
        showCalendarEvents: false,
        allowSettings: false,
        allowExtensions: false,
        allowScreencast: false,
        enabledExtensions: [],
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
            left: [],
            center: ['dateMenu'],
            right: ['a11yGreeter', 'keyboard', 'aggregateMenu'],
        },
        panelStyle: 'login-screen'
    },

    'lock-screen': {
        isLocked: true,
        isGreeter: undefined,
        unlockDialog: undefined,
        components: ['polkitAgent', 'telepathyClient'],
        panel: {
            left: [],
            center: [],
            right: ['aggregateMenu']
        },
        panelStyle: 'lock-screen'
    },

    'unlock-dialog': {
        isLocked: true,
        unlockDialog: undefined,
        components: ['polkitAgent', 'telepathyClient'],
        panel: {
            left: [],
            center: [],
            right: ['a11y', 'keyboard', 'aggregateMenu']
        },
        panelStyle: 'unlock-screen'
    },

    'user': {
        hasOverview: true,
        showCalendarEvents: true,
        allowSettings: true,
        allowExtensions: true,
        allowScreencast: true,
        hasRunDialog: true,
        hasWorkspaces: true,
        hasWindows: true,
        hasNotifications: true,
        isLocked: false,
        isPrimary: true,
        unlockDialog: imports.ui.unlockDialog.UnlockDialog,
        components: Config.HAVE_NETWORKMANAGER ?
                    ['networkAgent', 'polkitAgent', 'telepathyClient',
                     'keyring', 'autorunManager', 'automountManager'] :
                    ['polkitAgent', 'telepathyClient',
                     'keyring', 'autorunManager', 'automountManager'],

        panel: {
            left: ['activities', 'appMenu'],
            center: ['dateMenu'],
            right: ['a11y', 'keyboard', 'aggregateMenu']
        }
    }
};

function _loadMode(file, info) {
    let name = info.get_name();
    let suffix = name.indexOf('.json');
    let modeName = suffix == -1 ? name : name.slice(name, suffix);

    if (_modes.hasOwnProperty(modeName))
        return;

    let fileContent, success, tag, newMode;
    try {
        [success, fileContent, tag] = file.load_contents(null);
        newMode = JSON.parse(fileContent);
    } catch(e) {
        return;
    }

    _modes[modeName] = {};
    let propBlacklist = ['unlockDialog'];
    for (let prop in _modes[DEFAULT_MODE]) {
        if (newMode[prop] !== undefined &&
            propBlacklist.indexOf(prop) == -1)
            _modes[modeName][prop] = newMode[prop];
    }
    _modes[modeName]['isPrimary'] = true;
}

function _loadModes() {
    FileUtils.collectFromDatadirs('modes', false, _loadMode);
}

function listModes() {
    _loadModes();
    let id = Mainloop.idle_add(function() {
        let names = Object.getOwnPropertyNames(_modes);
        for (let i = 0; i < names.length; i++)
            if (_modes[names[i]].isPrimary)
                print(names[i]);
        Mainloop.quit('listModes');
    });
    GLib.Source.set_name_by_id(id, '[gnome-shell] listModes');
    Mainloop.run('listModes');
}

const SessionMode = new Lang.Class({
    Name: 'SessionMode',

    _init: function() {
        _loadModes();
        let isPrimary = (_modes[global.session_mode] &&
                         _modes[global.session_mode].isPrimary);
        let mode = isPrimary ? global.session_mode : 'user';
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
        let params = _modes[this.currentMode];
        let defaults;
        if (params.parentMode)
            defaults = Params.parse(_modes[params.parentMode],
                                    _modes[DEFAULT_MODE]);
        else
            defaults = _modes[DEFAULT_MODE];
        params = Params.parse(params, defaults);

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
