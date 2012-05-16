// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Shell = imports.gi.Shell;

const Params = imports.misc.params;

const DEFAULT_MODE = 'user';

const _modes = {
    'gdm': { hasOverview: false,
             sessionType: Shell.SessionType.GDM },

    'user': { hasOverview: true,
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
        Lang.copyProperties(params, this);
    }
});
