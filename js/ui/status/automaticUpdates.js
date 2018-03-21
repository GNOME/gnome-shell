// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// Copyright (C) 2018 Endless Mobile, Inc.
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

/* exported Indicator */

const { Gio, GLib, GObject, NM, Shell } = imports.gi;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const { loadInterfaceXML } = imports.misc.fileUtils;

const NM_SETTING_AUTOMATIC_UPDATES_NOTIFICATION_TIME = 'connection.automatic-updates-notification-time';
const NM_SETTING_ALLOW_DOWNLOADS = 'connection.allow-downloads';
const NM_SETTING_TARIFF_ENABLED = 'connection.tariff-enabled';

const SchedulerInterface = loadInterfaceXML('com.endlessm.DownloadManager1.Scheduler');
const SchedulerProxy = Gio.DBusProxy.makeProxyWrapper(SchedulerInterface);

var AutomaticUpdatesState = {
    UNKNOWN: 0,
    DISCONNECTED: 1,
    DISABLED: 2,
    IDLE: 3,
    SCHEDULED: 4,
    DOWNLOADING: 5,
};

function automaticUpdatesStateToString(state) {
    switch (state) {
    case AutomaticUpdatesState.UNKNOWN:
    case AutomaticUpdatesState.DISCONNECTED:
        return null;

    case AutomaticUpdatesState.DISABLED:
        return 'resource:///org/gnome/shell/theme/endless-auto-updates-off-symbolic.svg';

    case AutomaticUpdatesState.IDLE:
    case AutomaticUpdatesState.DOWNLOADING:
        return 'resource:///org/gnome/shell/theme/endless-auto-updates-on-symbolic.svg';

    case AutomaticUpdatesState.SCHEDULED:
        return 'resource:///org/gnome/shell/theme/endless-auto-updates-scheduled-symbolic.svg';
    }

    return null;
}

var Indicator = GObject.registerClass(
class AutomaticUpdatesIndicator extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        this.visible = false;

        this._item = new PopupMenu.PopupSubMenuMenuItem('', true);
        this._toggleItem = this._item.menu.addAction('', this._toggleAutomaticUpdates.bind(this));
        this._item.menu.addAction(_('Updates Queue'),
            () => {
                const params = new GLib.Variant('(sava{sv})', [
                    'set-mode', [new GLib.Variant('s', 'updates')],
                    {},
                ]);

                Gio.DBus.session.call(
                    'org.gnome.Software',
                    '/org/gnome/Software',
                    'org.gtk.Actions',
                    'Activate',
                    params,
                    null,
                    Gio.DBusCallFlags.NONE,
                    5000,
                    null,
                    (conn, result) => {
                        try {
                            conn.call_finish(result);
                        } catch (e) {
                            logError(e, 'Failed to start gnome-software');
                        }
                    });
            });
        this._item.menu.addSettingsAction(_('Set a Schedule'), 'gnome-updates-panel.desktop');
        this.menu.addMenuItem(this._item);

        this._activeConnection = null;
        this._settingChangedSignalId = 0;
        this._updateTimeoutId = 0;
        this._notification = null;

        this._state = AutomaticUpdatesState.UNKNOWN;

        NM.Client.new_async(null, this._clientGot.bind(this));
    }

    _clientGot(obj, result) {
        this._client = NM.Client.new_finish(result);

        this._client.connect('notify::primary-connection', this._sync.bind(this));
        this._client.connect('notify::state', this._sync.bind(this));

        Main.sessionMode.connect('updated', this._sessionUpdated.bind(this));
        this._sessionUpdated();

        // Start retrieving the Mogwai proxy
        this._proxy = new SchedulerProxy(
            Gio.DBus.system,
            'com.endlessm.MogwaiSchedule1',
            '/com/endlessm/DownloadManager1',
            (proxy, error) => {
                if (error) {
                    log(error.message);
                    return;
                }
                this._proxy.connect('g-properties-changed', this._sync.bind(this));
                this._updateStatus();
            });

        this._sync();
    }

    _sessionUpdated() {
        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        this.menu.setSensitive(sensitive);
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
        if (this._client.state === NM.State.CONNECTING ||
            this._client.state === NM.State.DISCONNECTING)
            return;

        // Use a timeout to avoid instantly throwing the notification at
        // the user's face, and to avoid a series of unecessary updates
        // that happen when NetworkManager is still figuring out details.
        this._updateTimeoutId = GLib.timeout_add_seconds(
            GLib.PRIORITY_DEFAULT,
            2,
            () => {
                this._updateStatus();
                this._updateTimeoutId = 0;
                return GLib.SOURCE_REMOVE;
            });
        GLib.Source.set_name_by_id(this._updateTimeoutId, '[automaticUpdates] updateStatus');
    }

    _updateStatus() {
        // Update the current active connection. This will connect to the
        // NM.SettingUser signal to sync every time someone updates the
        // NM_SETTING_ALLOW_DOWNLOADS setting.
        this._updateActiveConnection();

        // Toggle item name
        this._updateAutomaticUpdatesItem();

        // Icons
        let icon = this._getIcon();

        this._item.icon.gicon = icon;
        this.gicon = icon;

        // Only show the Automatic Updates icon at the bottom bar when it is
        // both enabled, and there are updates being downloaded or installed.
        this._updateVisibility();

        // The status label
        this._item.label.text = _('Automatic Updates');

        this._state = this._getState();
    }

    _updateActiveConnection() {
        let currentActiveConnection = this._getActiveConnection();

        if (this._activeConnection === currentActiveConnection)
            return;

        // Disconnect from the previous active connection
        if (this._settingChangedSignalId > 0) {
            this._activeConnection.disconnect(this._settingChangedSignalId);
            this._settingChangedSignalId = 0;
        }

        this._activeConnection = currentActiveConnection;

        // Maybe send a notification if the connection changed and
        // with it, the automatic updates setting
        this._updateNotification();

        // Connect from the current active connection
        if (currentActiveConnection)
            this._settingChangedSignalId = currentActiveConnection.connect('changed', this._updateStatus.bind(this));
    }

    _updateNotification() {
        // Only notify when in an regular session, not in GDM or initial-setup.
        if (Main.sessionMode.currentMode !== 'user' &&
            Main.sessionMode.currentMode !== 'user-coding' &&
            Main.sessionMode.currentMode !== 'endless')
            return;

        // Also don't notify when starting up
        if (this._state === AutomaticUpdatesState.UNKNOWN)
            return;

        let alreadySentNotification = this._alreadySentNotification();
        let newState = this._getState();

        let wasDisconnected = this._state === AutomaticUpdatesState.DISCONNECTED;
        let wasActive = this._state >= AutomaticUpdatesState.IDLE;
        let isActive = newState >= AutomaticUpdatesState.IDLE;

        // The criteria to notify about the Automatic Updates setting is:
        //   1. If the user was disconnected and connects to a new network; or
        //   2. If the user was connected and connects to a network with different status;
        if ((wasDisconnected && alreadySentNotification) || (!wasDisconnected && isActive === wasActive))
            return;

        if (this._notification)
            this._notification.destroy();

        if (newState === AutomaticUpdatesState.DISCONNECTED)
            return;

        let source = new MessageTray.SystemNotificationSource();
        Main.messageTray.add(source);

        // Figure out the title, subtitle and icon
        let title, subtitle, iconFile;

        if (isActive) {
            title = _('Automatic updates on');
            subtitle = _('You have Unlimited Data so automatic updates have been turned on for this connection.');
            iconFile = automaticUpdatesStateToString(AutomaticUpdatesState.IDLE);
        } else {
            title = _('Automatic updates are turned off to save your data');
            subtitle = _('You will need to choose which Endless updates to apply when on this connection.');
            iconFile = automaticUpdatesStateToString(AutomaticUpdatesState.DISABLED);
        }

        let gicon = new Gio.FileIcon({ file: Gio.File.new_for_uri(iconFile) });

        // Create the notification.
        // The first time we notify the user for a given connection,
        // we set the urgency to critical so that we make sure the
        // user understands how we may be changing their settings.
        // On subsequent notifications for the given connection,
        // for instance if the user regularly switches between
        // metered and unmetered connections, we set the urgency
        // to normal so as not to be too obtrusive.
        this._notification = new MessageTray.Notification(source, title, subtitle, { gicon });
        this._notification.setUrgency(alreadySentNotification ? MessageTray.Urgency.NORMAL : MessageTray.Urgency.CRITICAL);
        this._notification.setTransient(false);

        this._notification.addAction(_('Close'), () => {
            this._notification.destroy();
        });

        this._notification.addAction(_('Change Settings…'), () => {
            let app = Shell.AppSystem.get_default().lookup_app('gnome-updates-panel.desktop');
            Main.overview.hide();
            app.activate();
        });

        source.notify(this._notification);

        this._notification.connect('destroy', () => {
            this._notification = null;
        });

        // Now that we first detected this connection, mark it as such
        let userSetting = this._ensureUserSetting(this._activeConnection);
        userSetting.set_data(
            NM_SETTING_AUTOMATIC_UPDATES_NOTIFICATION_TIME,
            '%s'.format(GLib.get_real_time()));

        this._activeConnection.commit_changes(true, null);
    }

    _updateAutomaticUpdatesItem() {
        let state = this._getState();

        if (state === AutomaticUpdatesState.DISABLED)
            this._toggleItem.label.text = _('Turn On');
        else
            this._toggleItem.label.text = _('Turn Off');
    }

    _toggleAutomaticUpdates() {
        if (!this._activeConnection)
            return;

        let userSetting = this._ensureUserSetting(this._activeConnection);

        let state = this._getState();
        let value;

        if (state === AutomaticUpdatesState.IDLE ||
            state === AutomaticUpdatesState.SCHEDULED ||
            state === AutomaticUpdatesState.DOWNLOADING)
            value = '0';
        else
            value = '1';

        userSetting.set_data(NM_SETTING_ALLOW_DOWNLOADS, value);

        this._activeConnection.commit_changes_async(true, null, (con, res) => {
            this._activeConnection.commit_changes_finish(res);
            this._updateStatus();
        });
    }

    _ensureUserSetting(connection) {
        let userSetting = connection.get_setting(NM.SettingUser.$gtype);
        if (!userSetting) {
            userSetting = new NM.SettingUser();
            connection.add_setting(userSetting);
        }
        return userSetting;
    }

    _getIcon() {
        let state = this._getState();
        let iconName = automaticUpdatesStateToString(state);

        if (!iconName)
            return null;

        let iconFile = Gio.File.new_for_uri(iconName);
        let gicon = new Gio.FileIcon({ file: iconFile });

        return gicon;
    }

    _updateVisibility() {
        let state = this._getState();

        this._item.visible = state !== AutomaticUpdatesState.DISCONNECTED;
        this.visible = state === AutomaticUpdatesState.DOWNLOADING;
    }

    _getState() {
        if (!this._activeConnection)
            return AutomaticUpdatesState.DISCONNECTED;

        let userSetting = this._ensureUserSetting(this._activeConnection);

        // We only return true when:
        //  * Automatic Updates are on
        //  * A schedule was set
        //  * Something is being downloaded

        let allowDownloadsValue = userSetting.get_data(NM_SETTING_ALLOW_DOWNLOADS);
        if (allowDownloadsValue) {
            let allowDownloads = allowDownloadsValue === '1';

            if (!allowDownloads)
                return AutomaticUpdatesState.DISABLED;
        } else {
            // Guess the default value from the metered state. Only return
            // if it's disabled - if it's not, we want to follow the regular
            // code paths and fetch the correct state
            let connectionSetting = this._activeConnection.get_setting_connection();

            if (!connectionSetting)
                return AutomaticUpdatesState.DISABLED;

            let metered = connectionSetting.get_metered();
            if (metered === NM.Metered.YES || metered === NM.Metered.GUESS_YES)
                return AutomaticUpdatesState.DISABLED;
        }

        // Without the proxy, we can't really know the state
        if (!this._proxy)
            return AutomaticUpdatesState.UNKNOWN;

        let scheduleSet = userSetting.get_data(NM_SETTING_TARIFF_ENABLED) === '1';
        if (!scheduleSet)
            return AutomaticUpdatesState.IDLE;

        let downloading = this._proxy.ActiveEntryCount > 0;
        if (downloading)
            return AutomaticUpdatesState.DOWNLOADING;

        // At this point we're not downloading anything, but something
        // might be queued
        let downloadsQueued = this._proxy.EntryCount > 0;
        if (downloadsQueued)
            return AutomaticUpdatesState.SCHEDULED;
        else
            return AutomaticUpdatesState.IDLE;
    }

    _alreadySentNotification() {
        let connection = this._getActiveConnection();

        if (!connection)
            return false;

        let userSetting = connection.get_setting(NM.SettingUser.$gtype);

        if (!userSetting)
            return false;

        return userSetting.get_data(NM_SETTING_AUTOMATIC_UPDATES_NOTIFICATION_TIME) !== null;
    }

    _getActiveConnection() {
        let activeConnection = this._client.get_primary_connection();
        return activeConnection ? activeConnection.get_connection() : null;
    }
});
