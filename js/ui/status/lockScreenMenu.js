// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const GObject = imports.gi.GObject;
const Lang = imports.lang;
const St = imports.gi.St;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const VolumeMenu = imports.ui.status.volume;

const FakeStatusIcon = new Lang.Class({
    Name: 'FakeStatusIcon',

    _init: function(button) {
        this.actor = new St.BoxLayout({ style_class: 'panel-status-button-box' });
        this._button = button;
        this._button.connect('icons-updated', Lang.bind(this, this._reconstructIcons));
        this._button.actor.bind_property('visible', this.actor, 'visible',
                                         GObject.BindingFlags.SYNC_CREATE);
        this._reconstructIcons();
    },

    _reconstructIcons: function() {
        this.actor.destroy_all_children();
        this._button.icons.forEach(Lang.bind(this, function(icon) {
            let newIcon = new St.Icon({ style_class: 'system-status-icon' });
            icon.bind_property('gicon', newIcon, 'gicon',
                               GObject.BindingFlags.SYNC_CREATE);
            icon.bind_property('visible', newIcon, 'visible',
                               GObject.BindingFlags.SYNC_CREATE);
            this.actor.add_actor(newIcon);
        }));
    }
});

const Indicator = new Lang.Class({
    Name: 'LockScreenMenuIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init: function() {
        this.parent(null, _("Volume, network, battery"));
        this.indicators.style_class = 'lock-screen-status-button-box';

        this._volumeControl = VolumeMenu.getMixerControl();
        this._volumeMenu = new VolumeMenu.VolumeMenu(this._volumeControl);
        this.menu.addMenuItem(this._volumeMenu);

        this._volume = new FakeStatusIcon(Main.panel.statusArea.volume);
        this.indicators.add_child(this._volume.actor);

        // Network may not exist if the user doesn't have NetworkManager
        if (Main.panel.statusArea.network) {
            this._network = new FakeStatusIcon(Main.panel.statusArea.network);
            this.indicators.add_child(this._network.actor);
        }

        this._battery = new FakeStatusIcon(Main.panel.statusArea.battery);
        this.indicators.add_child(this._battery.actor);
    }
});
