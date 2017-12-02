// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Gio = imports.gi.Gio;
const Gvc = imports.gi.Gvc;
const St = imports.gi.St;
const Signals = imports.signals;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const Slider = imports.ui.slider;

var VOLUME_NOTIFY_ID = 1;

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

var StreamSlider = new Lang.Class({
    Name: 'StreamSlider',

    _init(control) {
        this._control = control;

        this.item = new PopupMenu.PopupBaseMenuItem({ activate: false });

        this._slider = new Slider.Slider(0);
        this._slider.connect('value-changed', this._sliderChanged.bind(this));
        this._slider.connect('drag-end', this._notifyVolumeChange.bind(this));

        this._icon = new St.Icon({ style_class: 'popup-menu-icon' });
        this.item.actor.add(this._icon);
        this.item.actor.add(this._slider.actor, { expand: true });
        this.item.actor.connect('button-press-event', (actor, event) => {
            return this._slider.startDragging(event);
        });
        this.item.actor.connect('key-press-event', (actor, event) => {
            return this._slider.onKeyPressEvent(actor, event);
        });

        this._stream = null;
    },

    get stream() {
        return this._stream;
    },

    set stream(stream) {
        if (this._stream) {
            this._disconnectStream(this._stream);
        }

        this._stream = stream;

        if (this._stream) {
            this._connectStream(this._stream);
            this._updateVolume();
        } else {
            this.emit('stream-updated');
        }

        this._updateVisibility();
    },

    _disconnectStream(stream) {
        stream.disconnect(this._mutedChangedId);
        this._mutedChangedId = 0;
        stream.disconnect(this._volumeChangedId);
        this._volumeChangedId = 0;
    },

    _connectStream(stream) {
        this._mutedChangedId = stream.connect('notify::is-muted', this._updateVolume.bind(this));
        this._volumeChangedId = stream.connect('notify::volume', this._updateVolume.bind(this));
    },

    _shouldBeVisible() {
        return this._stream != null;
    },

    _updateVisibility() {
        let visible = this._shouldBeVisible();
        this.item.actor.visible = visible;
    },

    scroll(event) {
        return this._slider.scroll(event);
    },

    setValue(value) {
        // piggy-back off of sliderChanged
        this._slider.setValue(value);
    },

    _sliderChanged(slider, value, property) {
        if (!this._stream)
            return;

        let volume = value * this._control.get_vol_max_norm();
        let prevMuted = this._stream.is_muted;
        if (volume < 1) {
            this._stream.volume = 0;
            if (!prevMuted)
                this._stream.change_is_muted(true);
        } else {
            this._stream.volume = volume;
            if (prevMuted)
                this._stream.change_is_muted(false);
        }
        this._stream.push_volume();
    },

    _notifyVolumeChange() {
        global.cancel_theme_sound(VOLUME_NOTIFY_ID);
        global.play_theme_sound(VOLUME_NOTIFY_ID,
                                'audio-volume-change',
                                _("Volume changed"),
                                Clutter.get_current_event ());
    },

    _updateVolume() {
        let muted = this._stream.is_muted;
        this._slider.setValue(muted ? 0 : (this._stream.volume / this._control.get_vol_max_norm()));
        this.emit('stream-updated');
    },

    getIcon() {
        if (!this._stream)
            return null;

        let volume = this._stream.volume;
        if (this._stream.is_muted || volume <= 0) {
            return 'audio-volume-muted-symbolic';
        } else {
            let n = Math.floor(3 * volume / this._control.get_vol_max_norm()) + 1;
            if (n < 2)
                return 'audio-volume-low-symbolic';
            if (n >= 3)
                return 'audio-volume-high-symbolic';
            return 'audio-volume-medium-symbolic';
        }
    },

    getLevel() {
        if (!this._stream)
            return null;

        return 100 * this._stream.volume / this._control.get_vol_max_norm();
    }
});
Signals.addSignalMethods(StreamSlider.prototype);

var OutputStreamSlider = new Lang.Class({
    Name: 'OutputStreamSlider',
    Extends: StreamSlider,

    _init(control) {
        this.parent(control);
        this._slider.actor.accessible_name = _("Volume");
    },

    _connectStream(stream) {
        this.parent(stream);
        this._portChangedId = stream.connect('notify::port', this._portChanged.bind(this));
        this._portChanged();
    },

    _findHeadphones(sink) {
        // This only works for external headphones (e.g. bluetooth)
        if (sink.get_form_factor() == 'headset' ||
            sink.get_form_factor() == 'headphone')
            return true;

        // a bit hackish, but ALSA/PulseAudio have a number
        // of different identifiers for headphones, and I could
        // not find the complete list
        if (sink.get_ports().length > 0)
            return sink.get_port().port.indexOf('headphone') >= 0;

        return false;
    },

    _disconnectStream(stream) {
        this.parent(stream);
        stream.disconnect(this._portChangedId);
        this._portChangedId = 0;
    },

    _updateSliderIcon() {
        this._icon.icon_name = (this._hasHeadphones ?
                                'audio-headphones-symbolic' :
                                'audio-speakers-symbolic');
    },

    _portChanged() {
        let hasHeadphones = this._findHeadphones(this._stream);
        if (hasHeadphones != this._hasHeadphones) {
            this._hasHeadphones = hasHeadphones;
            this._updateSliderIcon();
        }
    }
});

var InputStreamSlider = new Lang.Class({
    Name: 'InputStreamSlider',
    Extends: StreamSlider,

    _init(control) {
        this.parent(control);
        this._slider.actor.accessible_name = _("Microphone");
        this._control.connect('stream-added', this._maybeShowInput.bind(this));
        this._control.connect('stream-removed', this._maybeShowInput.bind(this));
        this._icon.icon_name = 'audio-input-microphone-symbolic';
    },

    _connectStream(stream) {
        this.parent(stream);
        this._maybeShowInput();
    },

    _maybeShowInput() {
        // only show input widgets if any application is recording audio
        let showInput = false;
        let recordingApps = this._control.get_source_outputs();
        if (this._stream && recordingApps) {
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

        this._showInput = showInput;
        this._updateVisibility();
    },

    _shouldBeVisible() {
        return this.parent() && this._showInput;
    }
});

var VolumeMenu = new Lang.Class({
    Name: 'VolumeMenu',
    Extends: PopupMenu.PopupMenuSection,

    _init(control) {
        this.parent();

        this.hasHeadphones = false;

        this._control = control;
        this._control.connect('state-changed', this._onControlStateChanged.bind(this));
        this._control.connect('default-sink-changed', this._readOutput.bind(this));
        this._control.connect('default-source-changed', this._readInput.bind(this));

        this._output = new OutputStreamSlider(this._control);
        this._output.connect('stream-updated', () => {
            this.emit('icon-changed');
        });
        this.addMenuItem(this._output.item);

        this._input = new InputStreamSlider(this._control);
        this.addMenuItem(this._input.item);

        this.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        this._onControlStateChanged();
    },

    scroll(event) {
        return this._output.scroll(event);
    },

    _onControlStateChanged() {
        if (this._control.get_state() == Gvc.MixerControlState.READY) {
            this._readInput();
            this._readOutput();
        } else {
            this.emit('icon-changed');
        }
    },

    _readOutput() {
        this._output.stream = this._control.get_default_sink();
    },

    _readInput() {
        this._input.stream = this._control.get_default_source();
    },

    getIcon() {
        return this._output.getIcon();
    },

    getLevel() {
        return this._output.getLevel();
    }
});

var Indicator = new Lang.Class({
    Name: 'VolumeIndicator',
    Extends: PanelMenu.SystemIndicator,

    _init() {
        this.parent();

        this._primaryIndicator = this._addIndicator();

        this._control = getMixerControl();
        this._volumeMenu = new VolumeMenu(this._control);
        this._volumeMenu.connect('icon-changed', menu => {
            let icon = this._volumeMenu.getIcon();

            if (icon != null) {
                this.indicators.show();
                this._primaryIndicator.icon_name = icon;
            } else {
                this.indicators.hide();
            }
        });

        this.menu.addMenuItem(this._volumeMenu);

        this.indicators.connect('scroll-event', this._onScrollEvent.bind(this));
    },

    _onScrollEvent(actor, event) {
        let result = this._volumeMenu.scroll(event);
        if (result == Clutter.EVENT_PROPAGATE || this.menu.actor.mapped)
            return result;

        let gicon = new Gio.ThemedIcon({ name: this._volumeMenu.getIcon() });
        let level = this._volumeMenu.getLevel();
        Main.osdWindowManager.show(-1, gicon, null, level);
        return result;
    }
});
