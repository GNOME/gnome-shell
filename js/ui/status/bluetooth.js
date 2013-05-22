// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GLib = imports.gi.GLib;
const GnomeBluetoothApplet = imports.gi.GnomeBluetoothApplet;
const GnomeBluetooth = imports.gi.GnomeBluetooth;
const Lang = imports.lang;
const St = imports.gi.St;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const NotificationDaemon = imports.ui.notificationDaemon;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const ConnectionState = {
    DISCONNECTED: 0,
    CONNECTED: 1,
    DISCONNECTING: 2,
    CONNECTING: 3
}

const Indicator = new Lang.Class({
    Name: 'BTIndicator',
    Extends: PanelMenu.SystemStatusButton,

    _init: function() {
        this.parent('bluetooth-disabled-symbolic', _("Bluetooth"));

        this._applet = new GnomeBluetoothApplet.Applet();

        this._killswitch = new PopupMenu.PopupSwitchMenuItem(_("Bluetooth"), false);
        this._applet.connect('notify::killswitch-state', Lang.bind(this, this._updateKillswitch));
        this._killswitch.connect('toggled', Lang.bind(this, function() {
            let current_state = this._applet.killswitch_state;
            if (current_state != GnomeBluetooth.KillswitchState.HARD_BLOCKED &&
                current_state != GnomeBluetooth.KillswitchState.NO_ADAPTER) {
                this._applet.killswitch_state = this._killswitch.state ?
                    GnomeBluetooth.KillswitchState.UNBLOCKED:
                    GnomeBluetooth.KillswitchState.SOFT_BLOCKED;
            } else
                this._killswitch.setToggleState(false);
        }));

        this._discoverable = new PopupMenu.PopupSwitchMenuItem(_("Visibility"), this._applet.discoverable);
        this._applet.connect('notify::discoverable', Lang.bind(this, function() {
            this._discoverable.setToggleState(this._applet.discoverable);
        }));
        this._discoverable.connect('toggled', Lang.bind(this, function() {
            this._applet.discoverable = this._discoverable.state;
        }));

        this._updateKillswitch();
        this.menu.addMenuItem(this._killswitch);
        this.menu.addMenuItem(this._discoverable);
        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        this._fullMenuItems = [new PopupMenu.PopupSeparatorMenuItem(),
                               new PopupMenu.PopupMenuItem(_("Send Files to Device…")),
                               new PopupMenu.PopupMenuItem(_("Set Up a New Device…")),
                               new PopupMenu.PopupSeparatorMenuItem()];
        this._hasDevices = false;

        this._fullMenuItems[1].connect('activate', function() {
            GLib.spawn_command_line_async('bluetooth-sendto');
        });
        this._fullMenuItems[2].connect('activate', function() {
            GLib.spawn_command_line_async('bluetooth-wizard');
        });

        for (let i = 0; i < this._fullMenuItems.length; i++) {
            let item = this._fullMenuItems[i];
            this.menu.addMenuItem(item);
        }

        this._deviceItemPosition = 3;
        this._deviceItems = [];
        this._applet.connect('devices-changed', Lang.bind(this, this._updateDevices));
        this._updateDevices();

        this._applet.connect('notify::show-full-menu', Lang.bind(this, this._updateFullMenu));
        this._updateFullMenu();

        this.menu.addSettingsAction(_("Bluetooth Settings"), 'gnome-bluetooth-panel.desktop');

        this._applet.connect('pincode-request', Lang.bind(this, this._pinRequest));
        this._applet.connect('confirm-request', Lang.bind(this, this._confirmRequest));
        this._applet.connect('auth-request', Lang.bind(this, this._authRequest));
        this._applet.connect('auth-service-request', Lang.bind(this, this._authServiceRequest));
        this._applet.connect('cancel-request', Lang.bind(this, this._cancelRequest));
    },

    _updateKillswitch: function() {
        let current_state = this._applet.killswitch_state;
        let on = current_state == GnomeBluetooth.KillswitchState.UNBLOCKED;
        let has_adapter = current_state != GnomeBluetooth.KillswitchState.NO_ADAPTER;
        let can_toggle = current_state != GnomeBluetooth.KillswitchState.NO_ADAPTER &&
                         current_state != GnomeBluetooth.KillswitchState.HARD_BLOCKED;

        this._killswitch.setToggleState(on);
        if (can_toggle)
            this._killswitch.setStatus(null);
        else
            /* TRANSLATORS: this means that bluetooth was disabled by hardware rfkill */
            this._killswitch.setStatus(_("hardware disabled"));

        this.actor.visible = has_adapter;

        if (on) {
            this._discoverable.actor.show();
            this.setIcon('bluetooth-active-symbolic');
        } else {
            this._discoverable.actor.hide();
            this.setIcon('bluetooth-disabled-symbolic');
        }
    },

    _updateDevices: function() {
        let devices = this._applet.get_devices();

        let newlist = [ ];
        for (let i = 0; i < this._deviceItems.length; i++) {
            let item = this._deviceItems[i];
            let destroy = true;
            for (let j = 0; j < devices.length; j++) {
                if (item._device.device_path == devices[j].device_path) {
                    this._updateDeviceItem(item, devices[j]);
                    destroy = false;
                    break;
                }
            }
            if (destroy)
                item.destroy();
            else
                newlist.push(item);
        }

        this._deviceItems = newlist;
        this._hasDevices = newlist.length > 0;
        for (let i = 0; i < devices.length; i++) {
            let d = devices[i];
            if (d._item)
                continue;
            let item = this._createDeviceItem(d);
            if (item) {
                this.menu.addMenuItem(item, this._deviceItemPosition + this._deviceItems.length);
                this._deviceItems.push(item);
                this._hasDevices = true;
            }
        }
    },

    _updateDeviceItem: function(item, device) {
        if (!device.can_connect && device.capabilities == GnomeBluetoothApplet.Capabilities.NONE) {
            item.destroy();
            return;
        }

        let prevDevice = item._device;
        let prevCapabilities = prevDevice.capabilities;
        let prevCanConnect = prevDevice.can_connect;

        // adopt the new device object
        item._device = device;
        device._item = item;

        // update properties
        item.label.text = device.alias;

        if (prevCapabilities != device.capabilities ||
            prevCanConnect != device.can_connect) {
            // need to rebuild the submenu
            item.menu.removeAll();
            this._buildDeviceSubMenu(item, device);
        }

        // update connected property
        if (device.can_connect)
            item._connectedMenuItem.setToggleState(device.connected);
    },

    _createDeviceItem: function(device) {
        if (!device.can_connect && device.capabilities == GnomeBluetoothApplet.Capabilities.NONE)
            return null;
        let item = new PopupMenu.PopupSubMenuMenuItem(device.alias);

        // adopt the device object, and add a back link
        item._device = device;
        device._item = item;

        this._buildDeviceSubMenu(item, device);

        return item;
    },

    _buildDeviceSubMenu: function(item, device) {
        if (device.can_connect) {
            let menuitem = new PopupMenu.PopupSwitchMenuItem(_("Connection"), device.connected);
            item._connected = device.connected;
            item._connectedMenuItem = menuitem;
            menuitem.connect('toggled', Lang.bind(this, function() {
                if (item._connected > ConnectionState.CONNECTED) {
                    // operation already in progress, revert
                    // (should not happen anyway)
                    menuitem.setToggleState(menuitem.state);
                }
                if (item._connected) {
                    item._connected = ConnectionState.DISCONNECTING;
                    menuitem.setStatus(_("disconnecting..."));
                    this._applet.disconnect_device(item._device.device_path, function(applet, success) {
                        if (success) { // apply
                            item._connected = ConnectionState.DISCONNECTED;
                            menuitem.setToggleState(false);
                        } else { // revert
                            item._connected = ConnectionState.CONNECTED;
                            menuitem.setToggleState(true);
                        }
                        menuitem.setStatus(null);
                    });
                } else {
                    item._connected = ConnectionState.CONNECTING;
                    menuitem.setStatus(_("connecting..."));
                    this._applet.connect_device(item._device.device_path, function(applet, success) {
                        if (success) { // apply
                            item._connected = ConnectionState.CONNECTED;
                            menuitem.setToggleState(true);
                        } else { // revert
                            item._connected = ConnectionState.DISCONNECTED;
                            menuitem.setToggleState(false);
                        }
                        menuitem.setStatus(null);
                    });
                }
            }));

            item.menu.addMenuItem(menuitem);
        }

        if (device.capabilities & GnomeBluetoothApplet.Capabilities.OBEX_PUSH) {
            item.menu.addAction(_("Send Files…"), Lang.bind(this, function() {
                this._applet.send_to_address(device.bdaddr, device.alias);
            }));
        }

        switch (device.type) {
        case GnomeBluetoothApplet.Type.KEYBOARD:
            item.menu.addSettingsAction(_("Keyboard Settings"), 'gnome-keyboard-panel.desktop');
            break;
        case GnomeBluetoothApplet.Type.MOUSE:
            item.menu.addSettingsAction(_("Mouse Settings"), 'gnome-mouse-panel.desktop');
            break;
        case GnomeBluetoothApplet.Type.HEADSET:
        case GnomeBluetoothApplet.Type.HEADPHONES:
        case GnomeBluetoothApplet.Type.OTHER_AUDIO:
            item.menu.addSettingsAction(_("Sound Settings"), 'gnome-sound-panel.desktop');
            break;
        default:
            break;
        }
    },

    _updateFullMenu: function() {
        if (this._applet.show_full_menu) {
            this._showAll(this._fullMenuItems);
            if (this._hasDevices)
                this._showAll(this._deviceItems);
        } else {
            this._hideAll(this._fullMenuItems);
            this._hideAll(this._deviceItems);
        }
    },

    _showAll: function(items) {
        for (let i = 0; i < items.length; i++)
            items[i].actor.show();
    },

    _hideAll: function(items) {
        for (let i = 0; i < items.length; i++)
            items[i].actor.hide();
    },

    _destroyAll: function(items) {
        for (let i = 0; i < items.length; i++)
            items[i].destroy();
    },

    _ensureSource: function() {
        if (!this._source) {
            this._source = new MessageTray.Source(_("Bluetooth"), 'bluetooth-active');
            this._source.policy = new NotificationDaemon.NotificationApplicationPolicy('gnome-bluetooth-panel');
            Main.messageTray.add(this._source);
        }
    },

    _authRequest: function(applet, device_path, name, long_name) {
        this._ensureSource();
        this._source.notify(new AuthNotification(this._source, this._applet, device_path, name, long_name));
    },

    _authServiceRequest: function(applet, device_path, name, long_name, uuid) {
        this._ensureSource();
        this._source.notify(new AuthServiceNotification(this._source, this._applet, device_path, name, long_name, uuid));
    },

    _confirmRequest: function(applet, device_path, name, long_name, pin) {
        this._ensureSource();
        this._source.notify(new ConfirmNotification(this._source, this._applet, device_path, name, long_name, pin));
    },

    _pinRequest: function(applet, device_path, name, long_name, numeric) {
        this._ensureSource();
        this._source.notify(new PinNotification(this._source, this._applet, device_path, name, long_name, numeric));
    },

    _cancelRequest: function() {
        this._source.destroy();
    }
});

const AuthNotification = new Lang.Class({
    Name: 'AuthNotification',
    Extends: MessageTray.Notification,

    _init: function(source, applet, device_path, name, long_name) {
        this.parent(source,
                    _("Bluetooth"),
                    _("Authorization request from %s").format(name),
                    { customContent: true });
        this.setResident(true);

        this._applet = applet;
        this._devicePath = device_path;
        this.addBody(_("Device %s wants to pair with this computer").format(long_name));

        this.addButton('allow', _("Allow"));
        this.addButton('deny', _("Deny"));

        this.connect('action-invoked', Lang.bind(this, function(self, action) {
            if (action == 'allow')
                this._applet.agent_reply_confirm(this._devicePath, true);
            else
                this._applet.agent_reply_confirm(this._devicePath, false);
            this.destroy();
        }));
    }
});

const AuthServiceNotification = new Lang.Class({
    Name: 'AuthServiceNotification',
    Extends: MessageTray.Notification,

    _init: function(source, applet, device_path, name, long_name, uuid) {
        this.parent(source,
                    _("Bluetooth"),
                    _("Authorization request from %s").format(name),
                    { customContent: true });
        this.setResident(true);

        this._applet = applet;
        this._devicePath = device_path;
        this.addBody(_("Device %s wants access to the service '%s'").format(long_name, uuid));

        this.addButton('always-grant', _("Always grant access"));
        this.addButton('grant', _("Grant this time only"));
        this.addButton('reject', _("Reject"));

        this.connect('action-invoked', Lang.bind(this, function(self, action) {
            switch (action) {
            case 'always-grant':
                this._applet.agent_reply_auth_service(this._devicePath, true, true);
                break;
            case 'grant':
                this._applet.agent_reply_auth_service(this._devicePath, true, false);
                break;
            case 'reject':
            default:
                this._applet.agent_reply_auth_service(this._devicePath, false, false);
            }
            this.destroy();
        }));
    }
});

const ConfirmNotification = new Lang.Class({
    Name: 'ConfirmNotification',
    Extends: MessageTray.Notification,

    _init: function(source, applet, device_path, name, long_name, pin) {
        this.parent(source,
                    _("Bluetooth"),
                    /* Translators: argument is the device short name */
                    _("Pairing confirmation for %s").format(name),
                    { customContent: true });
        this.setResident(true);

        this._applet = applet;
        this._devicePath = device_path;
        this.addBody(_("Device %s wants to pair with this computer").format(long_name));
        this.addBody(_("Please confirm whether the Passkey '%06d' matches the one on the device.").format(pin));

        /* Translators: this is the verb, not the noun */
        this.addButton('matches', _("Matches"));
        this.addButton('does-not-match', _("Does not match"));

        this.connect('action-invoked', Lang.bind(this, function(self, action) {
            if (action == 'matches')
                this._applet.agent_reply_confirm(this._devicePath, true);
            else
                this._applet.agent_reply_confirm(this._devicePath, false);
            this.destroy();
        }));
    }
});

const PinNotification = new Lang.Class({
    Name: 'PinNotification',
    Extends: MessageTray.Notification,

    _init: function(source, applet, device_path, name, long_name, numeric) {
        this.parent(source,
                    _("Bluetooth"),
                    _("Pairing request for %s").format(name),
                    { customContent: true });
        this.setResident(true);

        this._applet = applet;
        this._devicePath = device_path;
        this._numeric = numeric;
        this.addBody(_("Device %s wants to pair with this computer").format(long_name));
        this.addBody(_("Please enter the PIN mentioned on the device."));

        this._entry = new St.Entry();
        this._entry.connect('key-release-event', Lang.bind(this, function(entry, event) {
            let key = event.get_key_symbol();
            if (key == Clutter.KEY_Return) {
                if (this._canActivateOkButton())
                    this.emit('action-invoked', 'ok');
                return true;
            } else if (key == Clutter.KEY_Escape) {
                this.emit('action-invoked', 'cancel');
                return true;
            }
            return false;
        }));
        this.addActor(this._entry);

        this.addButton('ok', _("OK"));
        this.addButton('cancel', _("Cancel"));

        this.setButtonSensitive('ok', this._canActivateOkButton());
        this._entry.clutter_text.connect('text-changed', Lang.bind(this,
            function() {
                this.setButtonSensitive('ok', this._canActivateOkButton());
            }));

        this.connect('action-invoked', Lang.bind(this, function(self, action) {
            if (action == 'ok') {
                if (this._numeric) {
                    let num = parseInt(this._entry.text);
                    if (isNaN(num)) {
                        // user reply was empty, or was invalid
                        // cancel the operation
                        num = -1;
                    }
                    this._applet.agent_reply_passkey(this._devicePath, num);
                } else
                    this._applet.agent_reply_pincode(this._devicePath, this._entry.text);
            } else {
                if (this._numeric)
                    this._applet.agent_reply_passkey(this._devicePath, -1);
                else
                    this._applet.agent_reply_pincode(this._devicePath, null);
            }
            this.destroy();
        }));
    },

    _canActivateOkButton: function() {
        // PINs have a fixed length of 6
        if (this._numeric)
            return this._entry.clutter_text.text.length == 6;
        else
            return true;
    }
});
