// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;

const Main = imports.ui.main;
const Params = imports.misc.params;

const DEFAULT_MODE = 'user';

const _modes = {
    'gdm': { hasOverview: false,
             showCalendarEvents: false,
             allowSettings: false,
             allowExtensions: false,
             allowKeybindingsWhenModal: true,
             hasRunDialog: false,
             hasWorkspaces: false,
             createSession: Main.createGDMSession,
             createUnlockDialog: Main.createGDMLoginDialog,
             extraStylesheet: null,
             panel: {
                 left: [],
                 center: ['dateMenu'],
                 right: ['a11y', 'display', 'keyboard',
                         'volume', 'battery', 'lockScreen', 'powerMenu']
             }
           },

    'initial-setup': { hasOverview: false,
                       showCalendarEvents: false,
                       allowSettings: false,
                       allowExtensions: false,
                       allowKeybindingsWhenModal: false,
                       hasRunDialog: false,
                       hasWorkspaces: false,
                       createSession: Main.createInitialSetupSession,
                       extraStylesheet: null,
                       panel: {
                           left: [],
                           center: ['dateMenu'],
                           right: ['a11y', 'keyboard', 'volume', 'lockScreen']
                       }
                     },

    'user': { hasOverview: true,
              showCalendarEvents: true,
              allowSettings: true,
              allowExtensions: true,
              allowKeybindingsWhenModal: false,
              hasRunDialog: true,
              hasWorkspaces: true,
              createSession: Main.createUserSession,
              createUnlockDialog: Main.createSessionUnlockDialog,
              extraStylesheet: null,
              panel: {
                  left: ['activities', 'appMenu'],
                  center: ['dateMenu'],
                  right: ['a11y', 'keyboard', 'volume', 'bluetooth',
                          'network', 'battery', 'lockScreen', 'userMenu']
              }
            }
};

function listModes() {
    let modes = Object.getOwnPropertyNames(_modes);
    for (let i = 0; i < modes.length; i++)
        print(modes[i]);
}

const SessionMode = new Lang.Class({
    Name: 'SessionMode',

    _init: function() {
        let params = _modes[global.session_mode];

        params = Params.parse(params, _modes[DEFAULT_MODE]);

        this._createSession = params.createSession;
        delete params.createSession;
        this._createUnlockDialog = params.createUnlockDialog;
        delete params.createUnlockDialog;

        Lang.copyProperties(params, this);
    },

    createSession: function() {
        if (this._createSession)
            this._createSession();
    },

    createUnlockDialog: function() {
        if (this._createUnlockDialog)
            return this._createUnlockDialog.apply(this, arguments);
        else
            return null;
    },
});
