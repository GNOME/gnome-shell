// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const { Clutter, Gio, GLib, GObject, Gvc, St } = imports.gi;
const Signals = imports.signals;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const Slider = imports.ui.slider;

const ALLOW_AMPLIFIED_VOLUME_KEY = 'allow-volume-above-100-percent';

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

var StreamSlider = class {
    constructor(control) {
        this._control = control;

        this.item = new PopupMenu.PopupBaseMenuItem({ activate: false });

        this._inDrag = false;
        this._notifyVolumeChangeId = 0;

        this._slider = new Slider.Slider(0);

        this._soundSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.sound' });
        this._soundSettings.connect('changed::%s'.format(ALLOW_AMPLIFIED_VOLUME_KEY), this._amplifySettingsChanged.bind(this));
        this._amplifySettingsChanged();

        this._sliderChangedId = this._slider.connect('notify::value',
                                                     this._sliderChanged.bind(this));
        this._slider.connect('drag-begin', () => (this._inDrag = true));
        this._slider.connect('drag-end', () => {
            this._inDrag = false;
            this._notifyVolumeChange();
        });

        this._icon = new St.Icon({ style_class: 'popup-menu-icon' });
        this.item.add(this._icon);
        this.item.add_child(this._slider);
        this.item.connect('button-press-event', (actor, event) => {
            return this._slider.startDragging(event);
        });
        this.item.connect('key-press-event', (actor, event) => {
            return this._slider.emit('key-press-event', event);
        });
        this.item.connect('scroll-event', (actor, event) => {
            return this._slider.emit('scroll-event', event);
        });

        this._stream = null;
        this._volumeCancellable = null;
    }

    get stream() {
        return this._stream;
    }

    set stream(stream) {
        if (this._stream)
            this._disconnectStream(this._stream);

        this._stream = stream;

        if (this._stream) {
            this._connectStream(this._stream);
            this._updateVolume();
        } else {
            this.emit('stream-updated');
        }

        this._updateVisibility();
    }

    _disconnectStream(stream) {
        stream.disconnect(this._mutedChangedId);
        this._mutedChangedId = 0;
        stream.disconnect(this._volumeChangedId);
        this._volumeChangedId = 0;
    }

    _connectStream(stream) {
        this._mutedChangedId = stream.connect('notify::is-muted', this._updateVolume.bind(this));
        this._volumeChangedId = stream.connect('notify::volume', this._updateVolume.bind(this));
    }

    _shouldBeVisible() {
        return this._stream != null;
    }

    _updateVisibility() {
        let visible = this._shouldBeVisible();
        this.item.visible = visible;
    }

    scroll(event) {
        return this._slider.scroll(event);
    }

    _sliderChanged() {
        if (!this._stream)
            return;

        let value = this._slider.value;
        let volume = value * this._control.get_vol_max_norm();
        let prevMuted = this._stream.is_muted;
        let prevVolume = this._stream.volume;
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

        let volumeChanged = this._stream.volume !== prevVolume;
        if (volumeChanged && !this._notifyVolumeChangeId && !this._inDrag) {
            this._notifyVolumeChangeId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 30, () => {
                this._notifyVolumeChange();
                this._notifyVolumeChangeId = 0;
                return GLib.SOURCE_REMOVE;
            });
            GLib.Source.set_name_by_id(this._notifyVolumeChangeId,
                '[gnome-shell] this._notifyVolumeChangeId');
        }
    }

    _notifyVolumeChange() {
        if (this._volumeCancellable)
            this._volumeCancellable.cancel();
        this._volumeCancellable = null;

        if (this._stream.state === Gvc.MixerStreamState.RUNNING)
            return; // feedback not necessary while playing

        this._volumeCancellable = new Gio.Cancellable();
        let player = global.display.get_sound_player();
        player.play_from_theme('audio-volume-change',
                               _("Volume changed"),
                               this._volumeCancellable);
    }

    _changeSlider(value) {
        this._slider.block_signal_handler(this._sliderChangedId);
        this._slider.value = value;
        this._slider.unblock_signal_handler(this._sliderChangedId);
    }

    _updateVolume() {
        let muted = this._stream.is_muted;
        this._changeSlider(muted
            ? 0 : this._stream.volume / this._control.get_vol_max_norm());
        this.emit('stream-updated');
    }

    _amplifySettingsChanged() {
        this._allowAmplified = this._soundSettings.get_boolean(ALLOW_AMPLIFIED_VOLUME_KEY);

        this._slider.maximum_value = this._allowAmplified
            ? this.getMaxLevel() : 1;

        if (this._stream)
            this._updateVolume();
    }

    getIcon() {
        if (!this._stream)
            return null;

        let icons = ["audio-volume-muted-symbolic",
                     "audio-volume-low-symbolic",
                     "audio-volume-medium-symbolic",
                     "audio-volume-high-symbolic",
                     "audio-volume-overamplified-symbolic"];

        let volume = this._stream.volume;
        let n;
        if (this._stream.is_muted || volume <= 0) {
            n = 0;
        } else {
            n = Math.ceil(3 * volume / this._control.get_vol_max_norm());
            if (n < 1)
                n = 1;
            else if (n > 3)
                n = 4;
        }
        return icons[n];
    }

    getLevel() {
        if (!this._stream)
            return null;

        return this._stream.volume / this._control.get_vol_max_norm();
    }

    getMaxLevel() {
        let maxVolume = this._control.get_vol_max_norm();
        if (this._allowAmplified)
            maxVolume = this._control.get_vol_max_amplified();

        return maxVolume / this._control.get_vol_max_norm();
    }
};
Signals.addSignalMethods(StreamSlider.prototype);

var OutputStreamSlider = class extends StreamSlider {
    constructor(control) {
        super(control);
        this._slider.accessible_name = _("Volume");
    }

    _connectStream(stream) {
        super._connectStream(stream);
        this._portChangedId = stream.connect('notify::port', this._portChanged.bind(this));
        this._portChanged();
    }

    _findHeadphones(sink) {
        // This only works for external headphones (e.g. bluetooth)
        if (sink.get_form_factor() == 'headset' ||
            sink.get_form_factor() == 'headphone')
            return true;

        // a bit hackish, but ALSA/PulseAudio have a number
        // of different identifiers for headphones, and I could
        // not find the complete list
        if (sink.get_ports().length > 0)
            return sink.get_port().port.includes('headphone');

        return false;
    }

    _disconnectStream(stream) {
        super._disconnectStream(stream);
        stream.disconnect(this._portChangedId);
        this._portChangedId = 0;
    }

    _updateSliderIcon() {
        this._icon.icon_name = this._hasHeadphones
            ? 'audio-headphones-symbolic'
            : 'audio-speakers-symbolic';
    }

    _portChanged() {
        let hasHeadphones = this._findHeadphones(this._stream);
        if (hasHeadphones != this._hasHeadphones) {
            this._hasHeadphones = hasHeadphones;
            this._updateSliderIcon();
        }
    }
};

var InputStreamSlider = class extends StreamSlider {
    constructor(control) {
        super(control);
        this._slider.accessible_name = _("Microphone");
        this._control.connect('stream-added', this._maybeShowInput.bind(this));
        this._control.connect('stream-removed', this._maybeShowInput.bind(this));
        this._icon.icon_name = 'audio-input-microphone-symbolic';
    }

    _connectStream(stream) {
        super._connectStream(stream);
        this._maybeShowInput();
    }

    _maybeShowInput() {
        // only show input widgets if any application is recording audio
        let showInput = false;
        if (this._stream) {
            // skip gnome-volume-control and pavucontrol which appear
            // as recording because they show the input level
            let skippedApps = [
                'org.gnome.VolumeControl',
                'org.PulseAudio.pavucontrol',
            ];

            showInput = this._control.get_source_outputs().some(output => {
                return !skippedApps.includes(output.get_application_id());
            });
        }

        this._showInput = showInput;
        this._updateVisibility();
    }

    _shouldBeVisible() {
        return super._shouldBeVisible() && this._showInput;
    }
};

var VolumeMenu = class extends PopupMenu.PopupMenuSection {
    constructor(control) {
        super();

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
        this._input.item.connect('notify::visible', () => {
            this.emit('input-visible-changed');
        });
        this.addMenuItem(this._input.item);

        this.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());

        this._onControlStateChanged();
    }

    scroll(event) {
        return this._output.scroll(event);
    }

    _onControlStateChanged() {
        if (this._control.get_state() == Gvc.MixerControlState.READY) {
            this._readInput();
            this._readOutput();
        } else {
            this.emit('icon-changed');
        }
    }

    _readOutput() {
        this._output.stream = this._control.get_default_sink();
    }

    _readInput() {
        this._input.stream = this._control.get_default_source();
    }

    getIcon() {
        return this._output.getIcon();
    }

    getLevel() {
        return this._output.getLevel();
    }

    getMaxLevel() {
        return this._output.getMaxLevel();
    }

    getInputVisible() {
        return this._input.item.visible;
    }
};

var Indicator = GObject.registerClass(
class Indicator extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        this._primaryIndicator = this._addIndicator();
        this._inputIndicator = this._addIndicator();

        this._control = getMixerControl();
        this._volumeMenu = new VolumeMenu(this._control);
        this._volumeMenu.connect('icon-changed', () => {
            let icon = this._volumeMenu.getIcon();

            if (icon != null)
                this._primaryIndicator.icon_name = icon;
            this._primaryIndicator.visible = icon !== null;
        });

        this._inputIndicator.set({
            icon_name: 'audio-input-microphone-symbolic',
            visible: this._volumeMenu.getInputVisible(),
        });
        this._volumeMenu.connect('input-visible-changed', () => {
            this._inputIndicator.visible = this._volumeMenu.getInputVisible();
        });

        this.menu.addMenuItem(this._volumeMenu);
    }

    vfunc_scroll_event() {
        let result = this._volumeMenu.scroll(Clutter.get_current_event());
        if (result == Clutter.EVENT_PROPAGATE || this.menu.actor.mapped)
            return result;

        let gicon = new Gio.ThemedIcon({ name: this._volumeMenu.getIcon() });
        let level = this._volumeMenu.getLevel();
        let maxLevel = this._volumeMenu.getMaxLevel();
        Main.osdWindowManager.show(-1, gicon, null, level, maxLevel);
        return result;
    }
});
