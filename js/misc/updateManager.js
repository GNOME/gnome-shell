// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// Copyright (C) 2019 Endless Mobile, Inc.
//
// This is a GNOME Shell component to wrap the interactions over
// D-Bus with the Mogwai system daemon.
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

const { Clutter, Gio, GLib,
        GObject, Gtk, NM, Shell, St } = imports.gi;

const NM_SETTING_AUTOMATIC_UPDATES_NOTIFICATION_TIME = "connection.automatic-updates-notification-time";
const NM_SETTING_ALLOW_DOWNLOADS = 'connection.allow-downloads';
const NM_SETTING_TARIFF_ENABLED = "connection.tariff-enabled";

const SchedulerInterface = '\
<node> \
  <interface name="com.endlessm.DownloadManager1.Scheduler"> \
    <property name="ActiveEntryCount" type="u" access="read" /> \
    <property name="EntryCount" type="u" access="read" /> \
  </interface> \
</node>';

const SchedulerProxy = Gio.DBusProxy.makeProxyWrapper(SchedulerInterface);

let _updateManager = null;

function getUpdateManager() {
    if (_updateManager == null)
        _updateManager = new UpdateManager();
    return _updateManager;
}

var State = {
    UNKNOWN: 0,
    DISCONNECTED: 1,
    DISABLED: 2,
    IDLE: 3,
    SCHEDULED: 4,
    DOWNLOADING: 5
};

function stateToIconName(state) {
    switch (state) {
    case State.UNKNOWN:
    case State.DISCONNECTED:
        return null;

    case State.DISABLED:
        return 'resource:///org/gnome/shell/theme/automatic-updates-off-symbolic.svg';

    case State.IDLE:
    case State.DOWNLOADING:
        return 'resource:///org/gnome/shell/theme/automatic-updates-on-symbolic.svg';

    case State.SCHEDULED:
        return 'resource:///org/gnome/shell/theme/automatic-updates-scheduled-symbolic.svg';
    }

    return null;
}

var UpdateManager = GObject.registerClass ({
    Properties: {
        'last-notification-time': GObject.ParamSpec.int('last-notification-time',
                                                        'last-notification-time',
                                                        'last-notification-time',
                                                        GObject.ParamFlags.READWRITE,
                                                        null),
        'icon': GObject.ParamSpec.object('icon', 'icon', 'icon',
                                         GObject.ParamFlags.READABLE,
                                         Gio.Icon.$gtype),
        'state': GObject.ParamSpec.uint('state', 'state', 'state',
                                        GObject.ParamFlags.READABLE,
                                        null),
    },
}, class UpdateManager extends GObject.Object {
    _init() {
        super._init();

        this._activeConnection = null;
        this._settingChangedSignalId = 0;
        this._updateTimeoutId = 0;

        this._state = State.UNKNOWN;

        NM.Client.new_async(null, this._clientGot.bind(this));
    }

    _clientGot(obj, result) {
        this._client = NM.Client.new_finish(result);

        this._client.connect('notify::primary-connection', this._sync.bind(this));
        this._client.connect('notify::state', this._sync.bind(this));

        // Start retrieving the Mogwai proxy
        this._proxy = new SchedulerProxy(Gio.DBus.system,
                                         'com.endlessm.MogwaiSchedule1',
                                         '/com/endlessm/DownloadManager1',
                                          (proxy, error) => {
                                              if (error) {
                                                  log(error.message);
                                                  return;
                                              }
                                              this._proxy.connect('g-properties-changed',
                                                                  this._sync.bind(this));
                                              this._updateStatus();
                                          });

        this._sync();
    }

    _sync() {
        if (!this._client || !this._proxy)
            return;

        if (this._updateTimeoutId > 0) {
            GLib.source_remove(this._updateTimeoutId);
            this._updateTimeoutId = 0;
        }

        // Intermediate states (connecting or disconnecting) must not trigger
        // any kind of state change.
        if (this._client.state == NM.State.CONNECTING || this._client.state == NM.State.DISCONNECTING)
            return;

        // Use a timeout to avoid instantly throwing the notification at
        // the user's face, and to avoid a series of unecessary updates
        // that happen when NetworkManager is still figuring out details.
        this._updateTimeoutId = GLib.timeout_add_seconds(GLib.PRIORITY_DEFAULT,
                                                         2,
                                                         () => {
                                                            this._updateStatus();
                                                            this._updateTimeoutId = 0;
                                                            return GLib.SOURCE_REMOVE;
                                                         });
        GLib.Source.set_name_by_id(this._updateTimeoutId, '[update] updateStatus');
    }

    _updateStatus() {
        // Update the current active connection. This will connect to the
        // NM.SettingUser signal to sync every time someone updates the
        // NM_SETTING_ALLOW_DOWNLOADS setting.
        this._updateActiveConnection();

        let state = this._getState();
        if (state != this._state) {
            this._state = state;
            this.notify('state');

            this._updateIcon();
        }
    }

    _updateActiveConnection() {
        let currentActiveConnection = this._getActiveConnection();

        if (this._activeConnection == currentActiveConnection)
            return;

        // Disconnect from the previous active connection
        if (this._settingChangedSignalId > 0) {
            this._activeConnection.disconnect(this._settingChangedSignalId);
            this._settingChangedSignalId = 0;
        }

        this._activeConnection = currentActiveConnection;

        // Connect from the current active connection
        if (currentActiveConnection)
            this._settingChangedSignalId = currentActiveConnection.connect('changed', this._updateStatus.bind(this));
    }

    _ensureUserSetting(connection) {
        let userSetting = connection.get_setting(NM.SettingUser.$gtype);
        if (!userSetting) {
            userSetting = new NM.SettingUser();
            connection.add_setting(userSetting);
        }
        return userSetting;
    }

    _getState() {
        if (!this._activeConnection)
            return State.DISCONNECTED;

        let userSetting = this._ensureUserSetting(this._activeConnection);

        // We only return true when:
        //  * Automatic Updates are on
        //  * A schedule was set
        //  * Something is being downloaded

        let allowDownloadsValue = userSetting.get_data(NM_SETTING_ALLOW_DOWNLOADS);
        if (allowDownloadsValue) {
            let allowDownloads = (allowDownloadsValue === '1');

            if (!allowDownloads)
                return State.DISABLED;
        } else {
            // Guess the default value from the metered state. Only return
            // if it's disabled - if it's not, we want to follow the regular
            // code paths and fetch the correct state
            let connectionSetting = this._activeConnection.get_setting_connection();

            if (!connectionSetting)
                return State.DISABLED;

            let metered = connectionSetting.get_metered();
            if (metered == NM.Metered.YES || metered == NM.Metered.GUESS_YES)
                return State.DISABLED;
        }

        // Without the proxy, we can't really know the state
        if (!this._proxy)
            return State.UNKNOWN;

        let scheduleSet = userSetting.get_data(NM_SETTING_TARIFF_ENABLED) === '1';
        if (!scheduleSet)
            return State.IDLE;

        let downloading = this._proxy.ActiveEntryCount > 0;
        if (downloading)
            return State.DOWNLOADING;

        // At this point we're not downloading anything, but something
        // might be queued
        let downloadsQueued = this._proxy.EntryCount > 0;
        if (downloadsQueued)
            return State.SCHEDULED;
        else
            return State.IDLE;
    }

    _getActiveConnection() {
        let activeConnection = this._client.get_primary_connection();
        return activeConnection ? activeConnection.get_connection() : null;
    }

    _updateIcon() {
        let state = this._state;
        let iconName = stateToIconName(state);

        if (iconName) {
            let iconFile = Gio.File.new_for_uri(iconName);
            this._icon = new Gio.FileIcon({ file: iconFile });
        } else {
            this._icon = null;
        }

        this.notify('icon');
    }

    get state() {
        return this._state;
    }

    get lastNotificationTime() {
        let connection = this._getActiveConnection();
        if (!connection)
            return -1;

        let userSetting = connection.get_setting(NM.SettingUser.$gtype);
        if (!userSetting)
            return -1;

        let time = userSetting.get_data(NM_SETTING_AUTOMATIC_UPDATES_NOTIFICATION_TIME);
        return time ? parseInt(time) : -1;
    }

    set lastNotificationTime(time) {
        if (!this._activeConnection)
            return;

        let userSetting = this._ensureUserSetting(this._activeConnection);
        userSetting.set_data(NM_SETTING_AUTOMATIC_UPDATES_NOTIFICATION_TIME,
                             '%s'.format(time));

        this._activeConnection.commit_changes(true, null);
    }


    get active() {
        return this._active;
    }

    set active(_active) {
        if (this._active == _active)
            return;

        this._active = _active;
        this.notify('active');
    }

    get icon() {
        return this._icon;
    }

    toggleAutomaticUpdates() {
        if (!this._activeConnection)
            return;

        let userSetting = this._ensureUserSetting(this._activeConnection);

        let state = this._getState();
        let value;

        if (state == State.IDLE ||
            state == State.SCHEDULED ||
            state == State.DOWNLOADING) {
            value = '0';
        } else {
            value = '1';
        }

        userSetting.set_data(NM_SETTING_ALLOW_DOWNLOADS, value);

        this._activeConnection.commit_changes_async(true, null, (con, res, data) => {
            this._activeConnection.commit_changes_finish(res);
            this._updateStatus();
        });
    }
});
