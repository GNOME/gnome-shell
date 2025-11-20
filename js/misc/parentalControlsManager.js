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
    Gio._promisify(Malcontent.Manager.prototype, 'get_session_limits_async');
}

// We require libmalcontent ≥ 0.14.0 for session limits
const HAVE_MALCONTENT_0_14 = Malcontent &&
    GObject.signal_lookup('session-limits-changed', Malcontent.Manager) > 0;

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
        'session-limits-changed': {},
    },
    Properties: {
        'any-parental-controls-enabled': GObject.ParamSpec.boolean(
            'any-parental-controls-enabled', null, null,
            GObject.ParamFlags.READABLE,
            false),
    },
}, class ParentalControlsManager extends GObject.Object {
    _init() {
        super._init();

        this._initialized = false;
        this._disabled = false;
        this._appFilter = null;
        this._sessionLimits = null;

        this._initializeManager();
    }

    async _initializeManager() {
        if (!HAVE_MALCONTENT) {
            console.debug('Skipping parental controls support, malcontent not found');
            this._initialized = true;
            this.emit('app-filter-changed');
            this.emit('session-limits-changed');
            this.notify('any-parental-controls-enabled');
            return;
        }

        try {
            const connection = await Gio.DBus.get(Gio.BusType.SYSTEM, null);
            this._manager = new Malcontent.Manager({connection});
            this._appFilter = await this._getAppFilter();

            if (HAVE_MALCONTENT_0_14)
                this._sessionLimits = await this._getSessionLimits();
        } catch (e) {
            logError(e, 'Failed to get parental controls settings');
            return;
        }

        this._manager.connect('app-filter-changed', this._onAppFilterChanged.bind(this));
        if (HAVE_MALCONTENT_0_14)
            this._manager.connect('session-limits-changed', this._onSessionLimitsChanged.bind(this));

        // Signal initialisation is complete.
        this._initialized = true;
        this.emit('app-filter-changed');
        this.emit('session-limits-changed');
        this.notify('any-parental-controls-enabled');
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
        const currentUid = Shell.util_get_uid();
        if (currentUid !== uid)
            return;

        try {
            this._appFilter = await this._getAppFilter();
            this.emit('app-filter-changed');
            this.notify('any-parental-controls-enabled');
        } catch (e) {
            // Log an error and keep the old app filter.
            logError(e, `Failed to get new MctAppFilter for uid ${Shell.util_get_uid()} on app-filter-changed`);
        }
    }

    async _getSessionLimits() {
        let sessionLimits = null;

        try {
            sessionLimits = await this._manager.get_session_limits_async(
                Shell.util_get_uid(),
                Malcontent.ManagerGetValueFlags.NONE,
                null);
        } catch (e) {
            if (!e.matches(Malcontent.ManagerError, Malcontent.ManagerError.DISABLED))
                throw e;

            console.debug('Parental controls globally disabled');
            this._disabled = true;
        }

        return sessionLimits;
    }

    async _onSessionLimitsChanged(manager, uid) {
        // Emit 'changed' signal only if session-limits are changed for currently logged-in user
        const currentUid = Shell.util_get_uid();
        if (currentUid !== uid)
            return;

        try {
            this._sessionLimits = await this._getSessionLimits();
            this.emit('session-limits-changed');
            this.notify('any-parental-controls-enabled');
        } catch (e) {
            // Keep the old session limits
            logError(e, `Failed to get new MctSessionLimits for uid ${Shell.util_get_uid()} on session-limits-changed`);
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

    /**
     * Check whether parental controls app filtering is enabled for the account,
     * and if the library supports the features required for the functionality.
     *
     * Typically you should check whether a certain app is filtered
     * (shouldShowApp()), rather than checking whether filtering is enabled at
     * all.
     *
     * @returns {bool} whether app filtering is supported and enabled.
     */
    appFilterEnabled() {
        // Are latest parental controls enabled (at configure or runtime)?
        if (!HAVE_MALCONTENT_0_14 || this._disabled)
            return false;

        // Have we finished initialising yet?
        if (!this.initialized) {
            console.debug('Ignoring app filter because parental controls not yet initialised');
            return false;
        }

        return this._appFilter.is_enabled();
    }

    /**
     * Check whether parental controls session limits are enabled for the account,
     * and if the library supports the features required for the functionality.
     *
     * @returns {bool} whether session limits are supported and enabled.
     */
    sessionLimitsEnabled() {
        // Are latest parental controls enabled (at configure or runtime)?
        if (!HAVE_MALCONTENT_0_14 || this._disabled)
            return false;

        // Have we finished initialising yet?
        if (!this.initialized) {
            console.debug('Ignoring session limits because parental controls not yet initialised');
            return false;
        }

        return this._sessionLimits.is_enabled();
    }

    /**
     * Whether any parental controls are supported and enabled for the current
     * user.
     *
     * @type {bool}
     */
    get anyParentalControlsEnabled() {
        return this.appFilterEnabled() || this.sessionLimitsEnabled();
    }
});
