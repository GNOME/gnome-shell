// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// Copyright (C) 2018, 2019 Endless Mobile, Inc.
//
// This is a GNOME Shell component to wrap the interactions over
// D-Bus with the malcontent library.
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

const { Gio, GLib, GObject, Shell } = imports.gi;
const Config = imports.misc.config;
const Signals = imports.signals;

if (Config.HAVE_MALCONTENT) {
    const Malcontent = imports.gi.Malcontent;
}

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
// parental controls, and two users cannot be logged in at the same time.
var ParentalControlsManager = class {
    constructor() {
        this._initialized = false;
        this._disabled = false;
        this._appFilter = null;

        if (!Config.HAVE_MALCONTENT) {
            log('Skipping parental controls support as it’s disabled');
            return;
        }

        log('Getting parental controls for user ' + Shell.util_get_uid ());
        let connection = Gio.bus_get_sync(Gio.BusType.SYSTEM, null);
        this._manager = new Malcontent.Manager({connection: connection});
        manager.get_app_filter_async(Shell.util_get_uid (),
                                     Malcontent.GetAppFilterFlags.NONE,
                                     null, this._onGetAppFilter.bind(this));
    }

    _onGetAppFilter(object, res) {
        try {
            this._appFilter = this._manager.get_app_filter_finish(res);
        } catch (e) {
            if (e.matches(Malcontent.AppFilterError, Malcontent.AppFilterError.DISABLED)) {
                log('Parental controls globally disabled');
                this._disabled = true;
            } else {
                logError(e, 'Failed to get parental controls settings');
            }
        }

        this._manager.connect('app-filter-changed', () => {
            this.emit('changed');
        });

        this._initialized = true;
        this.emit('initialized');
    }

    get initialized() {
        return this._initialized;
    }

    // Calculate whether the given app (a Gio.DesktopAppInfo) should be shown
    // on the desktop, in search results, etc. The app should be shown if:
    //  - The .desktop file doesn’t say it should be hidden.
    //  - The executable from the .desktop file’s Exec line isn’t blacklisted in
    //    the user’s parental controls.
    //  - None of the flatpak app IDs from the X-Flatpak and the
    //    X-Flatpak-RenamedFrom lines are blacklisted in the user’s parental
    //    controls.
    shouldShowApp(appInfo) {
        // Quick decision?
        if (!appInfo.should_show())
            return false;

        // Are parental controls enabled (at configure time or runtime)?
        if (!Config.HAVE_MALCONTENT || this._disabled)
            return true;

        // Have we finished initialising yet?
        if (!this.initialized) {
            log('Warning: Hiding app because parental controls not yet initialised: ' + appInfo.get_id());
            return false;
        }

        return this._appFilter.is_appinfo_allowed(appInfo);
    }
}
Signals.addSignalMethods(ParentalControlsManager.prototype);
