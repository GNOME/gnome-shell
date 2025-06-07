import GLib from 'gi://GLib';
import * as Signals from '../misc/signals.js';

import * as FileUtils from '../misc/fileUtils.js';

import {LoginDialog}  from '../gdm/loginDialog.js';
import {UnlockDialog} from '../ui/unlockDialog.js';

import * as Config from '../misc/config.js';

const DEFAULT_MODE = 'restrictive';

const USER_SESSION_COMPONENTS = [
    'polkitAgent', 'keyring',
    'autorunManager', 'automountManager',
];

if (Config.HAVE_NETWORKMANAGER)
    USER_SESSION_COMPONENTS.push('networkAgent');

const _modes = {
    'restrictive': {
        parentMode: null,
        stylesheetName: 'gnome-shell.css',
        colorScheme: 'prefer-dark',
        themeResourceName: 'gnome-shell-theme.gresource',
        hasOverview: false,
        showCalendarEvents: false,
        showWelcomeDialog: false,
        allowSettings: false,
        allowScreencast: false,
        enabledExtensions: [],
        hasRunDialog: false,
        hasWorkspaces: false,
        hasWindows: false,
        hasNotifications: false,
        hasWmMenus: false,
        isLocked: false,
        isGreeter: false,
        isPrimary: false,
        unlockDialog: null,
        components: [],
        panel: {
            left: [],
            center: [],
            right: [],
        },
        panelStyle: null,
    },

    'gdm': {
        hasNotifications: true,
        isGreeter: true,
        isPrimary: true,
        unlockDialog: LoginDialog,
        components: Config.HAVE_NETWORKMANAGER
            ? ['networkAgent', 'polkitAgent']
            : ['polkitAgent'],
        panel: {
            left: [],
            center: ['dateMenu'],
            right: ['dwellClick', 'keyboard', 'quickSettings'],
        },
        panelStyle: 'login-screen',
    },

    'unlock-dialog': {
        isLocked: true,
        unlockDialog: undefined,
        components: ['polkitAgent'],
        panel: {
            left: [],
            center: [],
            right: ['dwellClick', 'a11y', 'keyboard', 'quickSettings'],
        },
        panelStyle: 'unlock-screen',
    },

    'user': {
        hasOverview: true,
        showCalendarEvents: true,
        showWelcomeDialog: true,
        allowSettings: true,
        allowScreencast: true,
        hasRunDialog: true,
        hasWorkspaces: true,
        hasWindows: true,
        hasWmMenus: true,
        hasNotifications: true,
        isLocked: false,
        isPrimary: true,
        unlockDialog: UnlockDialog,
        components: USER_SESSION_COMPONENTS,
        panel: {
            left: ['activities'],
            center: ['dateMenu'],
            right: ['screenRecording', 'screenSharing', 'dwellClick', 'a11y', 'keyboard', 'quickSettings'],
        },
    },
};

function _loadMode(file, info) {
    let name = info.get_name();
    let suffix = name.indexOf('.json');
    let modeName = suffix === -1 ? name : name.slice(name, suffix);

    if (Object.prototype.hasOwnProperty.call(_modes, modeName))
        return;

    let fileContent, success_, newMode;
    try {
        [success_, fileContent] = file.load_contents(null);
        const decoder = new TextDecoder();
        newMode = JSON.parse(decoder.decode(fileContent));
    } catch {
        return;
    }

    _modes[modeName] = {};
    const  excludedProps = ['unlockDialog'];
    for (let prop in _modes[DEFAULT_MODE]) {
        if (newMode[prop] !== undefined &&
            !excludedProps.includes(prop))
            _modes[modeName][prop] = newMode[prop];
    }
    _modes[modeName]['isPrimary'] = true;
}

/**
 * Loads external session modes from the system data directories.
 */
function _loadModes() {
    for (const {file, info} of FileUtils.collectFromDatadirs('modes', false))
        _loadMode(file, info);
}

export function listModes() {
    _loadModes();
    let loop = new GLib.MainLoop(null, false);
    let id = GLib.idle_add(GLib.PRIORITY_DEFAULT, () => {
        let names = Object.getOwnPropertyNames(_modes);
        for (let i = 0; i < names.length; i++) {
            if (_modes[names[i]].isPrimary)
                print(names[i]);
        }
        loop.quit();
    });
    GLib.Source.set_name_by_id(id, '[gnome-shell] listModes');
    loop.run();
}

export class SessionMode extends Signals.EventEmitter {
    constructor() {
        super();

        _loadModes();
        let isPrimary = _modes[global.session_mode] &&
                         _modes[global.session_mode].isPrimary;
        let mode = isPrimary ? global.session_mode : 'user';
        this._modeStack = [mode];
        this._sync();
    }

    pushMode(mode) {
        console.debug(`sessionMode: Pushing mode ${mode}`);
        this._modeStack.push(mode);
        this._sync();
    }

    popMode(mode) {
        if (this.currentMode !== mode || this._modeStack.length === 1)
            throw new Error('Invalid SessionMode.popMode');

        console.debug(`sessionMode: Popping mode ${mode}`);
        this._modeStack.pop();
        this._sync();
    }

    switchMode(to) {
        if (this.currentMode === to)
            return;
        this._modeStack[this._modeStack.length - 1] = to;
        this._sync();
    }

    get currentMode() {
        return this._modeStack[this._modeStack.length - 1];
    }

    _sync() {
        const current = _modes[this.currentMode];
        const parent = current.parentMode
            ? {..._modes[DEFAULT_MODE], ..._modes[current.parentMode]}
            : {..._modes[DEFAULT_MODE]};

        const params = {...parent, ...current};

        // A simplified version of Lang.copyProperties, handles
        // undefined as a special case for "no change / inherit from previous mode"
        for (let prop in params) {
            if (params[prop] !== undefined)
                this[prop] = params[prop];
        }

        this.emit('updated');
    }
}
