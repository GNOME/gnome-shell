// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Shell = imports.gi.Shell;

const Main = imports.ui.main;
const Params = imports.misc.params;

const DEFAULT_MODE = 'user';

const _modes = {
    'gdm': { hasOverview: false,
             hasAppMenu: false,
             showCalendarEvents: false,
             allowSettings: false,
             allowExtensions: false,
             allowKeybindingsWhenModal: true,
             hasRunDialog: false,
             hasWorkspaces: false,
             createSession: Main.createGDMSession,
             sessionType: Shell.SessionType.GDM },

    'user': { hasOverview: true,
              hasAppMenu: true,
              showCalendarEvents: true,
              allowSettings: true,
              allowExtensions: true,
              allowKeybindingsWhenModal: false,
              hasRunDialog: true,
              hasWorkspaces: true,
              createSession: Main.createUserSession,
              sessionType: Shell.SessionType.USER }
};

function modeExists(mode) {
    let modes = Object.getOwnPropertyNames(_modes);
    return modes.indexOf(mode) != -1;
}

const SessionMode = new Lang.Class({
    Name: 'SessionMode',

    _init: function() {
        let params = _modes[global.session_mode];

        params = Params.parse(params, _modes[DEFAULT_MODE]);

        this._createSession = params.createSession;
        delete params.createSession;

        Lang.copyProperties(params, this);
    },

    createSession: function() {
        if (this._createSession)
            this._createSession();
    }
});
