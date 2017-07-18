const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Main = imports.ui.main;
const ModalDialog = imports.ui.modalDialog;

var AudioDevice = {
    HEADPHONES: 1 << 0,
    HEADSET:    1 << 1,
    MICROPHONE: 1 << 2
};

const AudioDeviceSelectionIface = '<node> \
<interface name="org.gnome.Shell.AudioDeviceSelection"> \
<method name="Open"> \
    <arg name="devices" direction="in" type="as" /> \
</method> \
<method name="Close"> \
</method> \
<signal name="DeviceSelected"> \
    <arg name="device" type="s" /> \
</signal> \
</interface> \
</node>';

var AudioDeviceSelectionDialog = new Lang.Class({
    Name: 'AudioDeviceSelectionDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function(devices) {
        this.parent({ styleClass: 'audio-device-selection-dialog' });

        this._deviceItems = {};

        this._buildLayout();

        if (devices & AudioDevice.HEADPHONES)
            this._addDevice(AudioDevice.HEADPHONES);
        if (devices & AudioDevice.HEADSET)
            this._addDevice(AudioDevice.HEADSET);
        if (devices & AudioDevice.MICROPHONE)
            this._addDevice(AudioDevice.MICROPHONE);

        if (this._selectionBox.get_n_children() < 2)
            throw new Error('Too few devices for a selection');
    },

    destroy: function() {
        this.parent();
    },

    _buildLayout: function(devices) {
        let title = new St.Label({ style_class: 'audio-selection-title',
                                   text: _("Select Audio Device"),
                                   x_align: Clutter.ActorAlign.CENTER });

        this.contentLayout.style_class = 'audio-selection-content';
        this.contentLayout.add(title);

        this._selectionBox = new St.BoxLayout({ style_class: 'audio-selection-box' });
        this.contentLayout.add(this._selectionBox, { expand: true });

        this.addButton({ action: Lang.bind(this, this._openSettings),
                         label: _("Sound Settings") });
        this.addButton({ action: Lang.bind(this, this.close),
                         label: _("Cancel"),
                         key: Clutter.Escape });
    },

    _getDeviceLabel: function(device) {
        switch(device) {
            case AudioDevice.HEADPHONES:
                return _("Headphones");
            case AudioDevice.HEADSET:
                return _("Headset");
            case AudioDevice.MICROPHONE:
                return _("Microphone");
            default:
                return null;
        }
    },

    _getDeviceIcon: function(device) {
        switch(device) {
            case AudioDevice.HEADPHONES:
                return 'audio-headphones-symbolic';
            case AudioDevice.HEADSET:
                return 'audio-headset-symbolic';
            case AudioDevice.MICROPHONE:
                return 'audio-input-microphone-symbolic';
            default:
                return null;
        }
    },

    _addDevice: function(device) {
        let box = new St.BoxLayout({ style_class: 'audio-selection-device-box',
                                     vertical: true });
        box.connect('notify::height',
            function() {
                Meta.later_add(Meta.LaterType.BEFORE_REDRAW,
                    function() {
                        box.width = box.height;
                    });
            });

        let icon = new St.Icon({ style_class: 'audio-selection-device-icon',
                                 icon_name: this._getDeviceIcon(device) });
        box.add(icon);

        let label = new St.Label({ style_class: 'audio-selection-device-label',
                                   text: this._getDeviceLabel(device),
                                   x_align: Clutter.ActorAlign.CENTER });
        box.add(label);

        let button = new St.Button({ style_class: 'audio-selection-device',
                                     can_focus: true,
                                     child: box });
        this._selectionBox.add(button);

        button.connect('clicked', Lang.bind(this,
            function() {
                this.emit('device-selected', device);
                this.close();
                Main.overview.hide();
            }));
    },

    _openSettings: function() {
        let desktopFile = 'gnome-sound-panel.desktop'
        let app = Shell.AppSystem.get_default().lookup_app(desktopFile);

        if (!app) {
            log('Settings panel for desktop file ' + desktopFile + ' could not be loaded!');
            return;
        }

        this.close();
        Main.overview.hide();
        app.activate();
    }
});

var AudioDeviceSelectionDBus = new Lang.Class({
    Name: 'AudioDeviceSelectionDBus',

    _init: function() {
        this._audioSelectionDialog = null;

        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(AudioDeviceSelectionIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gnome/Shell/AudioDeviceSelection');

        Gio.DBus.session.own_name('org.gnome.Shell.AudioDeviceSelection', Gio.BusNameOwnerFlags.REPLACE, null, null);
    },

    _onDialogClosed: function() {
        this._audioSelectionDialog = null;
    },

    _onDeviceSelected: function(dialog, device) {
        let connection = this._dbusImpl.get_connection();
        let info = this._dbusImpl.get_info();
        let deviceName = Object.keys(AudioDevice).filter(
            function(dev) {
                return AudioDevice[dev] == device;
            })[0].toLowerCase();
        connection.emit_signal(this._audioSelectionDialog._sender,
                               this._dbusImpl.get_object_path(),
                               info ? info.name : null,
                               'DeviceSelected',
                               GLib.Variant.new('(s)', [deviceName]));
    },

    OpenAsync: function(params, invocation) {
        if (this._audioSelectionDialog) {
            invocation.return_value(null);
            return;
        }

        let [deviceNames] = params;
        let devices = 0;
        deviceNames.forEach(function(n) {
            devices |= AudioDevice[n.toUpperCase()];
        });

        let dialog;
        try {
            dialog = new AudioDeviceSelectionDialog(devices);
        } catch(e) {
            invocation.return_value(null);
            return;
        }
        dialog._sender = invocation.get_sender();

        dialog.connect('closed', Lang.bind(this, this._onDialogClosed));
        dialog.connect('device-selected',
                       Lang.bind(this, this._onDeviceSelected));
        dialog.open();

        this._audioSelectionDialog = dialog;
        invocation.return_value(null);
    },

    CloseAsync: function(params, invocation) {
        if (this._audioSelectionDialog &&
            this._audioSelectionDialog._sender == invocation.get_sender())
            this._audioSelectionDialog.close();

        invocation.return_value(null);
    }
});
