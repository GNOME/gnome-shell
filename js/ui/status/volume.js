// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Gvc = imports.gi.Gvc;
const St = imports.gi.St;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

const VOLUME_ADJUSTMENT_STEP = 0.05; /* Volume adjustment step in % */

const VOLUME_NOTIFY_ID = 1;

// Each Gvc.MixerControl is a connection to PulseAudio,
// so it's better to make it a singleton
let _mixerControl;
function getMixerControl() {
    if (_mixerControl)
        return _mixerControl;

    _mixerControl = new Gvc.MixerControl({ name: 'GNOME Shell Volume Control' });
    _mixerControl.open();

    return _mixerControl;
}

const VolumeMenu = new Lang.Class({
    Name: 'VolumeMenu',
    Extends: PopupMenu.PopupMenuSection,

    _init: function(control) {
        this.parent();

        this._control = control;
        this._control.connect('state-changed', Lang.bind(this, this._onControlStateChanged));
        this._control.connect('default-sink-changed', Lang.bind(this, this._readOutput));
        this._control.connect('default-source-changed', Lang.bind(this, this._readInput));
        this._control.connect('stream-added', Lang.bind(this, this._maybeShowInput));
        this._control.connect('stream-removed', Lang.bind(this, this._maybeShowInput));
        this._volumeMax = this._control.get_vol_max_norm();

        this._output = null;
        this._outputVolumeId = 0;
        this._outputMutedId = 0;
        /* Translators: This is the label for audio volume */
        this._outputTitle = new PopupMenu.PopupMenuItem(_("Volume"), { reactive: false });
        this._outputSlider = new PopupMenu.PopupSliderMenuItem(0);
        this._outputSlider.connect('value-changed', Lang.bind(this, this._sliderChanged, '_output'));
        this._outputSlider.connect('drag-end', Lang.bind(this, this._notifyVolumeChange));
        this.addMenuItem(this._outputTitle);
        this.addMenuItem(this._outputSlider);

        this.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        this._input = null;
        this._inputVolumeId = 0;
        this._inputMutedId = 0;
        this._inputTitle = new PopupMenu.PopupMenuItem(_("Microphone"), { reactive: false });
        this._inputSlider = new PopupMenu.PopupSliderMenuItem(0);
        this._inputSlider.connect('value-changed', Lang.bind(this, this._sliderChanged, '_input'));
        this._inputSlider.connect('drag-end', Lang.bind(this, this._notifyVolumeChange));
        this.addMenuItem(this._inputTitle);
        this.addMenuItem(this._inputSlider);

        this._onControlStateChanged();
    },

    toggleMute: function(quiet) {
        let muted = this._output.is_muted;
        this._output.change_is_muted(!muted);

        if (muted && !quiet)
            this._notifyVolumeChange();

        if (!muted)
            return ['audio-volume-muted-symbolic', 0];
        else
            return [this._volumeToIcon(this._output.volume),
                    this._output.volume / this._volumeMax];
    },

    scroll: function(direction, quiet) {
        let currentVolume = this._output.volume;

        if (direction == Clutter.ScrollDirection.DOWN) {
            let prev_muted = this._output.is_muted;
            this._output.volume = Math.max(0, currentVolume - this._volumeMax * VOLUME_ADJUSTMENT_STEP);
            if (this._output.volume < 1) {
                this._output.volume = 0;
                if (!prev_muted)
                    this._output.change_is_muted(true);
            }
            this._output.push_volume();
        }
        else if (direction == Clutter.ScrollDirection.UP) {
            this._output.volume = Math.min(this._volumeMax, currentVolume + this._volumeMax * VOLUME_ADJUSTMENT_STEP);
            this._output.change_is_muted(false);
            this._output.push_volume();
        }

        if (!quiet)
            this._notifyVolumeChange();

        if (this._output.is_muted)
            return ['audio-volume-muted-symbolic', 0];
        else
            return [this._volumeToIcon(this._output.volume),
                    this._output.volume / this._volumeMax];
    },

    _onControlStateChanged: function() {
        if (this._control.get_state() == Gvc.MixerControlState.READY) {
            this._readOutput();
            this._readInput();
            this._maybeShowInput();
        } else {
            this.emit('icon-changed', null);
        }
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
            this._outputSlider.setValue(0);
            this.emit('icon-changed', 'audio-volume-muted-symbolic');
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
            this._inputTitle.actor.hide();
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

        this._inputTitle.actor.visible = showInput;
        this._inputSlider.actor.visible = showInput;
    },

    _volumeToIcon: function(volume) {
        if (volume <= 0) {
            return 'audio-volume-muted-symbolic';
        } else {
            let n = Math.floor(3 * volume / this._volumeMax) + 1;
            if (n < 2)
                return 'audio-volume-low-symbolic';
            if (n >= 3)
                return 'audio-volume-high-symbolic';
            return 'audio-volume-medium-symbolic';
        }
    },

    _sliderChanged: function(slider, value, property) {
        if (this[property] == null) {
            log ('Volume slider changed for %s, but %s does not exist'.format(property, property));
            return;
        }
        let volume = value * this._volumeMax;
        let prev_muted = this[property].is_muted;
        if (volume < 1) {
            this[property].volume = 0;
            if (!prev_muted)
                this[property].change_is_muted(true);
        } else {
            this[property].volume = volume;
            if (prev_muted)
                this[property].change_is_muted(false);
        }
        this[property].push_volume();
    },

    _notifyVolumeChange: function() {
        global.cancel_theme_sound(VOLUME_NOTIFY_ID);
        global.play_theme_sound(VOLUME_NOTIFY_ID, 'audio-volume-change');
    },

    _mutedChanged: function(object, param_spec, property) {
        let muted = this[property].is_muted;
        let slider = this[property+'Slider'];
        slider.setValue(muted ? 0 : (this[property].volume / this._volumeMax));
        if (property == '_output') {
            if (muted)
                this.emit('icon-changed', 'audio-volume-muted-symbolic');
            else
                this.emit('icon-changed', this._volumeToIcon(this._output.volume));
        }
    },

    _volumeChanged: function(object, param_spec, property) {
        this[property+'Slider'].setValue(this[property].volume / this._volumeMax);
        if (property == '_output' && !this._output.is_muted)
            this.emit('icon-changed', this._volumeToIcon(this._output.volume));
    }
});

const Indicator = new Lang.Class({
    Name: 'VolumeIndicator',
    Extends: PanelMenu.SystemStatusButton,

    _init: function() {
        this.parent('audio-volume-muted-symbolic', _("Volume"));

        this._control = getMixerControl();
        this.volumeMenu = new VolumeMenu(this._control);
        this.volumeMenu.connect('icon-changed', Lang.bind(this, function(menu, icon) {
            this._hasPulseAudio = (icon != null);
            this.setIcon(icon);
            this._syncVisibility();
        }));

        this.menu.addMenuItem(this.volumeMenu);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.menu.addSettingsAction(_("Sound Settings"), 'gnome-sound-panel.desktop');

        this.actor.connect('scroll-event', Lang.bind(this, this._onScrollEvent));
    },

    _syncVisibility: function() {
        this.actor.visible = this._hasPulseAudio;
        this.mainIcon.visible = this._hasPulseAudio;
    },

    _onScrollEvent: function(actor, event) {
        this.volumeMenu.scroll(event.get_scroll_direction(), false);
    },
});
