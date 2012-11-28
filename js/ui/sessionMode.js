// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

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
        buttonLayout: ['button-layout', 'org.gnome.shell.overrides'],
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
    },

    'foo': {
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
        buttonLayout: ['button-layout', 'org.gnome.shell-foo.overrides'],
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

function listModes() {
    let modes = Object.getOwnPropertyNames(_modes);
    for (let i = 0; i < modes.length; i++)
        if (_modes[modes[i]].isPrimary)
            print(modes[i]);
}

const SessionMode = new Lang.Class({
    Name: 'SessionMode',

    _init: function() {
        global.connect('notify::session-mode', Lang.bind(this, this._sync));
        let mode = _modes[global.session_mode].isPrimary ? global.session_mode
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
        let params = _modes[this.currentMode];
        params = Params.parse(params, _modes[DEFAULT_MODE]);

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
