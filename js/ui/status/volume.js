/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Clutter = imports.gi.Clutter;
const DBus = imports.dbus;
const Lang = imports.lang;
const Mainloop = imports.mainloop;
const Shell = imports.gi.Shell;
const Gvc = imports.gi.Gvc;
const Signals = imports.signals;
const St = imports.gi.St;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

const VOLUME_MAX = 65536.0; /* PA_VOLUME_NORM */
const VOLUME_ADJUSTMENT_STEP = 0.05; /* Volume adjustment step in % */

function Indicator() {
    this._init.apply(this, arguments);
}

Indicator.prototype = {
    __proto__: PanelMenu.SystemStatusButton.prototype,

    _init: function() {
        PanelMenu.SystemStatusButton.prototype._init.call(this, 'audio-volume-muted', null);

        this._control = new Gvc.MixerControl({ name: 'GNOME Shell Volume Control' });
        this._control.connect('ready', Lang.bind(this, this._onControlReady));
        this._control.connect('default-sink-changed', Lang.bind(this, this._readOutput));
        this._control.connect('default-source-changed', Lang.bind(this, this._readInput));
        this._control.connect('stream-added', Lang.bind(this, this._maybeShowInput));
        this._control.connect('stream-removed', Lang.bind(this, this._maybeShowInput));

        this._output = null;
        this._outputVolumeId = 0;
        this._outputMutedId = 0;
        this._outputSwitch = new PopupMenu.PopupSwitchMenuItem(_("Output: Muted"), false);
        this._outputSwitch.connect('toggled', Lang.bind(this, this._switchToggled, '_output'));
        this._outputSlider = new PopupMenu.PopupSliderMenuItem(0);
        this._outputSlider.connect('value-changed', Lang.bind(this, this._sliderChanged, '_output'));
        this._outputSlider.connect('drag-end', Lang.bind(this, this._notifyVolumeChange));
        this.menu.addMenuItem(this._outputSwitch);
        this.menu.addMenuItem(this._outputSlider);

        this._separator = new PopupMenu.PopupSeparatorMenuItem();
        this.menu.addMenuItem(this._separator);

        this._input = null;
        this._inputVolumeId = 0;
        this._inputMutedId = 0;
        this._inputSwitch = new PopupMenu.PopupSwitchMenuItem(_("Input: Muted"), false);
        this._inputSwitch.connect('toggled', Lang.bind(this, this._switchToggled, '_input'));
        this._inputSlider = new PopupMenu.PopupSliderMenuItem(0);
        this._inputSlider.connect('value-changed', Lang.bind(this, this._sliderChanged, '_input'));
        this._inputSlider.connect('drag-end', Lang.bind(this, this._notifyVolumeChange));
        this.menu.addMenuItem(this._inputSwitch);
        this.menu.addMenuItem(this._inputSlider);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.menu.addAction(_("Sound Preferences"), function() {
            let p = new Shell.Process({ args: ['gnome-control-center', 'sound'] });
            p.run();
        });

        this.actor.connect('scroll-event', Lang.bind(this, this._onScrollEvent));
        this._control.open();
    },

    _onScrollEvent: function(actor, event) {
        let direction = event.get_scroll_direction();
        let currentVolume = this._output.volume;

        if (direction == Clutter.ScrollDirection.DOWN) {
            this._output.volume = Math.max(0, currentVolume - VOLUME_MAX * VOLUME_ADJUSTMENT_STEP);
            this._output.push_volume();
        }
        else if (direction == Clutter.ScrollDirection.UP) {
            this._output.volume = Math.min(VOLUME_MAX, currentVolume + VOLUME_MAX * VOLUME_ADJUSTMENT_STEP);
            this._output.push_volume();
        }
    },

    _onControlReady: function() {
        this._readOutput();
        this._readInput();
    },

    _readOutput: function() {
        if (this._outputVolumeId) {
            this._output.disconnect(this._outputVolumeId);
            this._output.disconnect(this._outputMutedId);
            this._outputVolumeId = 0;
            this._outputMutedId = 0;
        }
        this._output = this._control.get_default_sink();
        if (this._output) {
            this._outputMutedId = this._output.connect('notify::is-muted', Lang.bind(this, this._mutedChanged, '_output'));
            this._outputVolumeId = this._output.connect('notify::volume', Lang.bind(this, this._volumeChanged, '_output'));
            this._mutedChanged (null, null, '_output');
            this._volumeChanged (null, null, '_output');
        } else {
            this._outputSwitch.label.text = _("Output: Muted");
            this._outputSwitch.setToggleState(false);
            this.setIcon('audio-volume-muted-symbolic');
        }
    },

    _readInput: function() {
        if (this._inputVolumeId) {
            this._input.disconnect(this._inputVolumeId);
            this._input.disconnect(this._inputMutedId);
            this._inputVolumeId = 0;
            this._inputMutedId = 0;
        }
        this._input = this._control.get_default_source();
        if (this._input) {
            this._inputMutedId = this._input.connect('notify::is-muted', Lang.bind(this, this._mutedChanged, '_input'));
            this._inputVolumeId = this._input.connect('notify::volume', Lang.bind(this, this._volumeChanged, '_input'));
            this._mutedChanged (null, null, '_input');
            this._volumeChanged (null, null, '_input');
        } else {
            this._separator.actor.hide();
            this._inputSwitch.actor.hide();
            this._inputSlider.actor.hide();
        }
    },

    _maybeShowInput: function() {
        // only show input widgets if any application is recording audio
        let showInput = false;
        let recordingApps = this._control.get_source_outputs();
        if (this._input && recordingApps) {
            for (let i = 0; i < recordingApps.length; i++) {
                let outputStream = recordingApps[i];
                let id = outputStream.get_application_id();
                // but skip gnome-volume-control and pavucontrol
                // (that appear as recording because they show the input level)
                if (!id || (id != 'org.gnome.VolumeControl' && id != 'org.PulseAudio.pavucontrol')) {
                    showInput = true;
                    break;
                }
            }
        }
        if (showInput) {
            this._separator.actor.show();
            this._inputSwitch.actor.show();
            this._inputSlider.actor.show();
        } else {
            this._separator.actor.hide();
            this._inputSwitch.actor.hide();
            this._inputSlider.actor.hide();
        }
    },

    _volumeToIcon: function(volume) {
        if (volume <= 0) {
            return 'audio-volume-muted';
        } else {
            let v = volume / VOLUME_MAX;
            if (v < 0.33)
                return 'audio-volume-low';
            if (v > 0.8)
                return 'audio-volume-high';
            return 'audio-volume-medium';
        }
    },

    _sliderChanged: function(slider, value, property) {
        if (this[property] == null) {
            log ('Volume slider changed for %s, but %s does not exist'.format(property, property));
            return;
        }
        this[property].volume = value * VOLUME_MAX;
        this[property].push_volume();
    },

    _notifyVolumeChange: function() {
        global.play_theme_sound('audio-volume-change');
    },

    _switchToggled: function(switchItem, state, property) {
        if (this[property] == null) {
            log ('Volume mute switch toggled for %s, but %s does not exist'.format(property, property));
            return;
        }
        this[property].change_is_muted(!state);
        this._notifyVolumeChange();
    },

    _mutedChanged: function(object, param_spec, property) {
        let muted = this[property].is_muted;
        let toggleSwitch = this[property+'Switch'];
        toggleSwitch.setToggleState(!muted);
        this._updateLabel(property);
        if (property == '_output') {
            if (muted)
                this.setIcon('audio-volume-muted');
            else
                this.setIcon(this._volumeToIcon(this._output.volume));
        }
    },

    _volumeChanged: function(object, param_spec, property) {
        this[property+'Slider'].setValue(this[property].volume / VOLUME_MAX);
        this._updateLabel(property);
        if (property == '_output' && !this._output.is_muted)
            this.setIcon(this._volumeToIcon(this._output.volume));
    },

    _updateLabel: function(property) {
        let label;
        if (this[property].is_muted)
            label = (property == '_output' ? _("Output: Muted") : _("Input: Muted"));
        else
            label = (property == '_output' ? _("Output: %3.0f%%") : _("Input: %3.0f%%")).format(this[property].volume / VOLUME_MAX * 100);
        this[property+'Switch'].label.text = label;
    }
};
