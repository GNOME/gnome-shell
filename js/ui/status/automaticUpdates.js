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

const { Gio, GLib, Shell, St } = imports.gi;

const UpdateManager = imports.misc.updateManager;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

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

var Indicator = class extends PanelMenu.SystemIndicator {
    constructor() {
        super();

        this._indicator = this._addIndicator();
        this._indicator.visible = false;

        this._item = new PopupMenu.PopupSubMenuMenuItem("", true);
        this._toggleItem = this._item.menu.addAction("", this._toggleAutomaticUpdates.bind(this));
        this._item.menu.addAction(_("Updates Queue"), () => {
            let params = new GLib.Variant('(sava{sv})', [ 'set-mode', [ new GLib.Variant('s', 'updates') ], {} ]);
            Gio.DBus.session.call('org.gnome.Software',
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
        this._item.menu.addSettingsAction(_("Set a Schedule"), 'gnome-updates-panel.desktop');
        this.menu.addMenuItem(this._item);

        this._manager = UpdateManager.getUpdateManager();
        this._manager.connect('notify::state', this._updateState.bind(this));

        this._updateState();
    }

    _updateState() {
        this._updateStatus();
    }

    _sessionUpdated() {
        let sensitive = !Main.sessionMode.isLocked && !Main.sessionMode.isGreeter;
        this.menu.setSensitive(sensitive);
    }

    _updateStatus() {
        // Toggle item name
        this._updateItem();

        // Icons
        let icon = this._getIcon()

        this._item.icon.gicon = icon;
        this._indicator.gicon = icon;

        // Only show the Automatic Updates icon at the bottom bar when it is
        // both enabled, and there are updates being downloaded or installed.
        this._updateVisibility();

        // The status label
        this._item.label.text = _("Automatic Updates");
    }

    _updateItem() {
        let state = this._manager.state;

        if (state == UpdateManager.State.DISABLED)
            this._toggleItem.label.text = _("Turn On");
        else
            this._toggleItem.label.text = _("Turn Off");
    }

    _toggleAutomaticUpdates() {
        this._manager.toggleAutomaticUpdates();
    }

    _getIcon() {
        let state = this._manager.state;
        let iconName = UpdateManager.stateToIconName(state);

        if (!iconName)
            return null;

        let iconFile = Gio.File.new_for_uri(iconName);
        let gicon = new Gio.FileIcon({ file: iconFile });

        return gicon;
    }

    _updateVisibility() {
        let state = this._manager.state;

        this._item.actor.visible = (state != UpdateManager.State.DISCONNECTED);
        this._indicator.visible = (state == UpdateManager.State.DOWNLOADING);
    }
};
