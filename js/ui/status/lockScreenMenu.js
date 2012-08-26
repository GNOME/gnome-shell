// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GObject = imports.gi.GObject;
const Lang = imports.lang;
const St = imports.gi.St;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const VolumeMenu = imports.ui.status.volume;

const Indicator = new Lang.Class({
    Name: 'LockScreenMenuIndicator',
    Extends: PanelMenu.SystemStatusButton,

    _init: function() {
        this.parent(null, _("Volume, network, battery"));
        this.actor.hide();

        this._volume = Main.panel.statusArea.volume;
        if (this._volume) {
            this._volumeIcon = this.addIcon(null);
            this._volume.mainIcon.bind_property('gicon', this._volumeIcon, 'gicon',
                                                GObject.BindingFlags.SYNC_CREATE);
            this._volume.mainIcon.bind_property('visible', this._volumeIcon, 'visible',
                                                GObject.BindingFlags.SYNC_CREATE);

            this._volumeControl = VolumeMenu.getMixerControl();
            this._volumeMenu = new VolumeMenu.VolumeMenu(this._volumeControl);
            this.menu.addMenuItem(this._volumeMenu);
        }

        this._network = Main.panel.statusArea.network;
        if (this._network) {
            this._networkIcon = this.addIcon(null);
            this._network.mainIcon.bind_property('gicon', this._networkIcon, 'gicon',
                                                 GObject.BindingFlags.SYNC_CREATE);
            this._network.mainIcon.bind_property('visible', this._networkIcon, 'visible',
                                                 GObject.BindingFlags.SYNC_CREATE);

            this._networkSecondaryIcon = this.addIcon(null);
            this._network.secondaryIcon.bind_property('gicon', this._networkSecondaryIcon, 'gicon',
                                                      GObject.BindingFlags.SYNC_CREATE);
            this._network.secondaryIcon.bind_property('visible', this._networkSecondaryIcon, 'visible',
                                                      GObject.BindingFlags.SYNC_CREATE);
        }

        this._battery = Main.panel.statusArea.battery;
        if (this._battery) {
            this._batteryIcon = this.addIcon(null);
            this._battery.mainIcon.bind_property('gicon', this._batteryIcon, 'gicon',
                                                 GObject.BindingFlags.SYNC_CREATE);
            this._battery.mainIcon.bind_property('visible', this._batteryIcon, 'visible',
                                                 GObject.BindingFlags.SYNC_CREATE);
        }
    },

    setLockedState: function(locked) {
        this.actor.visible = locked;
    }
});
