// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const GLib = imports.gi.GLib;

// current desktop doesn't change unless we restart the shell or control
// the env variable. It's safe to cache matching result
let _currentDesktopsMatches = {};

// is:
// @name: desktop string you want to assert if it matches the current desktop env
//
// The function examples XDG_CURRENT_DESKTOP and return if the current desktop
// is part of that desktop string.
//
// Return value: if the environment isn't set or doesn't match, return False
// otherwise, return True.
function is(name) {

    if (_currentDesktopsMatches[name] !== undefined) {
        return _currentDesktopsMatches[name];
    }

    let desktopsEnv = GLib.getenv('XDG_CURRENT_DESKTOP');
    if (!desktopsEnv) {
        _currentDesktopsMatches[name] = false;
        return false;
    }

    let desktops = desktopsEnv.split(":");
    for (let i = 0; i < desktops.length; i++) {
        if (desktops[i] === name) {
            _currentDesktopsMatches[name] = true;
            return true;
        }
    }

    _currentDesktopsMatches[name] = false;
    return false;
}
