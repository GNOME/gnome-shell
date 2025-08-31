//
// Copyright (C) 2018, 2019, 2020 Endless Mobile, Inc.
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

import Gio from 'gi://Gio';
import GObject from 'gi://GObject';
import Shell from 'gi://Shell';

// We require libmalcontent ≥ 0.6.0
const HAVE_MALCONTENT = imports.package.checkSymbol(
    'Malcontent', '0', 'ManagerGetValueFlags');

let Malcontent = null;
if (HAVE_MALCONTENT) {
    ({default: Malcontent} = await import('gi://Malcontent?version=0'));
    Gio._promisify(Malcontent.Manager.prototype, 'get_app_filter_async');
}

let _singleton = null;

/**
 * @returns {ParentalControlsManager}
 */
export function getDefault() {
    if (_singleton === null)
        _singleton = new ParentalControlsManager();

    return _singleton;
}

// A manager class which provides cached access to the constructing user’s
// parental controls settings. It’s possible for the user’s parental controls
// to change at runtime if the Parental Controls application is used by an
// administrator from within the user’s session.
const ParentalControlsManager = GObject.registerClass({
    Signals: {
        'app-filter-changed': {},
    },
}, class ParentalControlsManager extends GObject.Object {
    _init() {
        super._init();

        this._initialized = false;
        this._disabled = false;
        this._appFilter = null;

        this._initializeManager();
    }

    async _initializeManager() {
        if (!HAVE_MALCONTENT) {
            console.debug('Skipping parental controls support, malcontent not found');
            this._initialized = true;
            this.emit('app-filter-changed');
            return;
        }

        try {
            const connection = await Gio.DBus.get(Gio.BusType.SYSTEM, null);
            this._manager = new Malcontent.Manager({connection});
            this._appFilter = await this._getAppFilter();
        } catch (e) {
            logError(e, 'Failed to get parental controls settings');
            return;
        }

        this._manager.connect('app-filter-changed', this._onAppFilterChanged.bind(this));

        // Signal initialisation is complete.
        this._initialized = true;
        this.emit('app-filter-changed');
    }

    async _getAppFilter() {
        let appFilter = null;

        try {
            appFilter = await this._manager.get_app_filter_async(
                Shell.util_get_uid(),
                Malcontent.ManagerGetValueFlags.NONE,
                null);
        } catch (e) {
            if (!e.matches(Malcontent.ManagerError, Malcontent.ManagerError.DISABLED))
                throw e;

            console.debug('Parental controls globally disabled');
            this._disabled = true;
        }

        return appFilter;
    }

    async _onAppFilterChanged(manager, uid) {
        // Emit 'changed' signal only if app-filter is changed for currently logged-in user.
        let currentUid = Shell.util_get_uid();
        if (currentUid !== uid)
            return;

        try {
            this._appFilter = await this._getAppFilter();
            this.emit('app-filter-changed');
        } catch (e) {
            // Log an error and keep the old app filter.
            logError(e, `Failed to get new MctAppFilter for uid ${Shell.util_get_uid()} on app-filter-changed`);
        }
    }

    get initialized() {
        return this._initialized;
    }

    // Calculate whether the given app (a GioUnix.DesktopAppInfo) should be shown
    // on the desktop, in search results, etc. The app should be shown if:
    //  - The .desktop file doesn’t say it should be hidden.
    //  - The executable from the .desktop file’s Exec line isn’t denied in
    //    the user’s parental controls.
    //  - None of the flatpak app IDs from the X-Flatpak and the
    //    X-Flatpak-RenamedFrom lines are denied in the user’s parental
    //    controls.
    shouldShowApp(appInfo) {
        // Quick decision?
        if (!appInfo.should_show())
            return false;

        // Are parental controls enabled (at configure time or runtime)?
        if (!HAVE_MALCONTENT || this._disabled)
            return true;

        // Have we finished initialising yet?
        if (!this.initialized) {
            console.debug(`Hiding app because parental controls not yet initialised: ${appInfo.get_id()}`);
            return false;
        }

        return this._appFilter.is_appinfo_allowed(appInfo);
    }
});
