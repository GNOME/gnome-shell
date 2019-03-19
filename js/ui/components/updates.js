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

const { Gio, GLib, Shell } = imports.gi;

const UpdateManager = imports.misc.updateManager;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

var UpdateComponent = class {
    constructor() {
        this._notification = null;
        this._state = UpdateManager.State.UNKNOWN;

        this._manager = UpdateManager.getUpdateManager();
        this._manager.connect('notify::state', this._updateState.bind(this));

        this._updateState();
    }

    enable() {
    }

    disable() {
    }

    _updateState() {
        let newState = this._manager.state;

        if (this._state == newState)
            return;

        this._updateNotification(newState);
        this._state = newState;
    }

    _updateNotification(newState) {
        // Don't notify when starting up
        if (this._manager.state == UpdateManager.State.UNKNOWN)
            return;

        let alreadySentNotification = this._manager.lastNotificationTime != -1;

        let wasDisconnected = this._state == UpdateManager.State.DISCONNECTED;
        let wasActive = this._state >= UpdateManager.State.IDLE;
        let isActive = newState >= UpdateManager.State.IDLE;

        // The criteria to notify about the Automatic Updates setting is:
        //   1. If the user was disconnected and connects to a new network; or
        //   2. If the user was connected and connects to a network with different status;
        if ((wasDisconnected && alreadySentNotification) || (!wasDisconnected && isActive == wasActive))
            return;

        if (this._notification)
            this._notification.destroy();

        if (newState == UpdateManager.State.DISCONNECTED)
            return;

        let source = new MessageTray.SystemNotificationSource();
        Main.messageTray.add(source);

        // Figure out the title, subtitle and icon
        let title, subtitle, iconFile;

        if (isActive) {
            title = _("Automatic updates on");
            subtitle = _("Your connection has unlimited data so automatic updates have been turned on.");
            iconFile = UpdateManager.stateToIconName(UpdateManager.State.IDLE);
        } else {
            title = _("Automatic updates are turned off to save your data");
            subtitle = _("You will need to choose which updates to apply when on this connection.");
            iconFile = UpdateManager.stateToIconName(UpdateManager.State.DISABLED);
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
        this._notification = new MessageTray.Notification(source, title, subtitle, { gicon: gicon });
        this._notification.setUrgency(alreadySentNotification ?
                                      MessageTray.Urgency.NORMAL : MessageTray.Urgency.CRITICAL);
        this._notification.setTransient(false);

        this._notification.addAction(_("Close"), () => {
            this._notification.destroy();
        });

        this._notification.addAction(_("Change Settings…"), () => {
            // FIXME: this requires the Automatic Updates panel in GNOME
            // Settings. Going with the Network panel for now…
            let app = Shell.AppSystem.get_default().lookup_app('gnome-network-panel.desktop');
            Main.overview.hide();
            app.activate();
        });

        source.notify(this._notification);

        this._notification.connect('destroy', () => {
            this._notification = null;
        });

        // Now that we first detected this connection, mark it as such
        this._manager.lastNotificationTime = GLib.get_real_time();
    }
};

var Component = UpdateComponent;
