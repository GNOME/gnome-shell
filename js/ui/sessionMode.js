// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Shell = imports.gi.Shell;

const Params = imports.misc.params;

const DEFAULT_MODE = 'user';

const _modes = {
    'gdm': { sessionType: Shell.SessionType.GDM },

    'user': { sessionType: Shell.SessionType.USER }
};

const SessionMode = new Lang.Class({
    Name: 'SessionMode',

    _init: function() {
        let params = _modes[global.session_mode];

        params = Params.parse(params, _modes[DEFAULT_MODE]);
        Lang.copyProperties(params, this);
    }
});
