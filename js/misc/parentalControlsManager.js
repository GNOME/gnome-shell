// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// Copyright (C) 2018 Endless Mobile, Inc.
//
// This is a GNOME Shell component to wrap the interactions over
// D-Bus with the eos-parental-controls library.
//
// Licensed under the GNU General Public License Version 2
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

const EosParentalControls = imports.gi.EosParentalControls;
const Gettext = imports.gettext;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject
const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Signals = imports.signals;

let _singleton = null;

function getDefault() {
    if (_singleton == null)
        _singleton = new ParentalControlsManager();

    return _singleton;
}

// A manager class which provides cached access to the constructing user’s
// parental controls settings. There are currently no notifications of changes
// to those settings, as it’s assumed that only an administrator can change a
// user’s parental controls, the administrator themselves doesn’t have any
// parental controls, and two users cannot be logged in at the same time (we
// don’t allow fast user switching).
var ParentalControlsManager = new Lang.Class({
    Name: 'ParentalControlsManager',
    Extends: GObject.Object,

    _init: function() {
        this.parent();

        log('Getting parental controls for user ' + Shell.util_get_uid ());
        try {
            this._appFilter = EosParentalControls.get_app_filter(null,
                                                                 Shell.util_get_uid (),
                                                                 false, null);
        } catch (e) {
            logError(e, 'Failed to get parental controls settings');
        }
    },

    // Calculate whether the given app (a Gio.DesktopAppInfo) should be shown
    // on the desktop, in search results, etc. The app should be shown if:
    //  - The .desktop file doesn’t say it should be hidden.
    //  - The executable from the .desktop file’s Exec line isn’t blacklisted in
    //    the user’s parental controls.
    //  - None of the flatpak app IDs from the X-Flatpak and the
    //    X-Flatpak-RenamedFrom lines are blacklisted in the user’s parental
    //    controls.
    shouldShowApp: function(appInfo) {
        // Quick decision?
        if (!appInfo.should_show())
            return false;

        // Have we finished initialising yet?
        if (!this._appFilter) {
            log('Warning: Hiding app because parental controls not yet initialised: ' + appInfo.get_id());
            return false;
        }

        return this._appFilter.is_appinfo_allowed(appInfo);
    },

});
