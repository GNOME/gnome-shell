import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Gvc from 'gi://Gvc';

import * as Main from '../main.js';
import * as PopupMenu from '../popupMenu.js';

import {QuickSlider, SystemIndicator} from '../quickSettings.js';

const ALLOW_AMPLIFIED_VOLUME_KEY = 'allow-volume-above-100-percent';
const UNMUTE_DEFAULT_VOLUME = 0.25;

// Each Gvc.MixerControl is a connection to PulseAudio,
// so it's better to make it a singleton
let _mixerControl;

/**
 * @returns {Gvc.MixerControl} - the mixer control singleton
 */
export function getMixerControl() {
    if (_mixerControl)
        return _mixerControl;

    _mixerControl = new Gvc.MixerControl({name: 'GNOME Shell Volume Control'});
    _mixerControl.open();

    return _mixerControl;
}

const StreamSlider = GObject.registerClass({
    Signals: {
        'stream-updated': {},
    },
}, class StreamSlider extends QuickSlider {
    _init(control) {
        super._init({
            icon_reactive: true,
        });

        this._control = control;

        this._inDrag = false;
        this._notifyVolumeChangeId = 0;

        this._soundSettings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.sound',
        });
        this._soundSettings.connect(`changed::${ALLOW_AMPLIFIED_VOLUME_KEY}`,
            () => this._amplifySettingsChanged());
        this._amplifySettingsChanged();

        this._sliderChangedId = this.slider.connect('notify::value',
            () => this._sliderChanged());
        this.slider.connect('drag-begin', () => (this._inDrag = true));
        this.slider.connect('drag-end', () => {
            this._inDrag = false;
            this._notifyVolumeChange();
        });

        this.connect('icon-clicked', () => {
            if (!this._stream)
                return;

            const {isMuted} = this._stream;
            if (isMuted && this._stream.volume === 0) {
                this._stream.volume =
                    UNMUTE_DEFAULT_VOLUME * this._control.get_vol_max_norm();
                this._stream.push_volume();
            }
            this._stream.change_is_muted(!isMuted);
        });

        this._deviceItems = new Map();

        this._deviceSection = new PopupMenu.PopupMenuSection();
        this.menu.addMenuItem(this._deviceSection);

        this.menu.addMenuItem(new PopupMenu.PopupSeparatorMenuItem());
        this.menu.addSettingsAction(_('Sound Settings'),
            'gnome-sound-panel.desktop');

        this._stream = null;
        this._volumeCancellable = null;
        this._icons = [];

        this._sync();
    }

    get stream() {
        return this._stream;
    }

    set stream(stream) {
        this._stream?.disconnectObject(this);

        this._stream = stream;

        if (this._stream) {
            this._connectStream(this._stream);
            this._updateVolume();
        } else {
            this.emit('stream-updated');
        }

        this._sync();
    }

    _connectStream(stream) {
        stream.connectObject(
            'notify::is-muted', this._updateVolume.bind(this),
            'notify::volume', this._updateVolume.bind(this), this);
    }

    _lookupDevice(_id) {
        throw new GObject.NotImplementedError(
            `_lookupDevice in ${this.constructor.name}`);
    }

    _activateDevice(_device) {
        throw new GObject.NotImplementedError(
            `_activateDevice in ${this.constructor.name}`);
    }

    _addDevice(id) {
        if (this._deviceItems.has(id))
            return;

        const device = this._lookupDevice(id);
        if (!device)
            return;

        const {description, origin} = device;
        const name = origin
            ? `${description} – ${origin}`
            : description;
        const item = new PopupMenu.PopupImageMenuItem(name, device.get_gicon());
        item.connect('activate', () => {
            const dev = this._lookupDevice(id);
            if (dev)
                this._activateDevice(dev);
            else
                console.warn(`Trying to activate invalid device ${id}`);
        });

        this._deviceSection.addMenuItem(item);
        this._deviceItems.set(id, item);

        this._sync();
    }

    _removeDevice(id) {
        this._deviceItems.get(id)?.destroy();
        if (this._deviceItems.delete(id))
            this._sync();
    }

    _setActiveDevice(activeId) {
        for (const [id, item] of this._deviceItems) {
            item.setOrnament(id === activeId
                ? PopupMenu.Ornament.CHECK
                : PopupMenu.Ornament.NONE);
        }
    }

    _shouldBeVisible() {
        return this._stream != null;
    }

    _sync() {
        this.visible = this._shouldBeVisible();
        this.menuEnabled = this._deviceItems.size > 1;
    }

    _sliderChanged() {
        if (!this._stream)
            return;

        let value = this.slider.value;
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
            _('Volume changed'), this._volumeCancellable);
    }

    _changeSlider(value) {
        this.slider.block_signal_handler(this._sliderChangedId);
        this.slider.value = value;
        this.slider.unblock_signal_handler(this._sliderChangedId);
    }

    _updateVolume() {
        let muted = this._stream.is_muted;
        this._changeSlider(muted
            ? 0 : this._stream.volume / this._control.get_vol_max_norm());
        this.iconLabel = muted ? _('Unmute') : _('Mute');
        this._updateIcon();
        this.emit('stream-updated');
    }

    _amplifySettingsChanged() {
        this._allowAmplified = this._soundSettings.get_boolean(ALLOW_AMPLIFIED_VOLUME_KEY);

        this.slider.maximum_value = this._allowAmplified
            ? this.getMaxLevel() : 1;

        if (this._stream)
            this._updateVolume();
    }

    _updateIcon() {
        this.iconName = this.getIcon();
    }

    getIcon() {
        if (!this._stream)
            return null;

        let volume = this._stream.volume;
        let n;
        if (this._stream.is_muted || volume <= 0) {
            n = 0;
        } else {
            n = Math.ceil(3 * volume / this._control.get_vol_max_norm());
            n = Math.clamp(n, 1, this._icons.length - 1);
        }
        return this._icons[n];
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

    showOSD() {
        const gicon = new Gio.ThemedIcon({name: this.getIcon()});
        const level = this.getLevel();
        const maxLevel = this.getMaxLevel();
        Main.osdWindowManager.showAll(gicon, null, level, maxLevel);
    }
});

const OutputStreamSlider = GObject.registerClass(
class OutputStreamSlider extends StreamSlider {
    _init(control) {
        super._init(control);

        this.slider.accessible_name = _('Volume');

        this._control.connectObject(
            'output-added', (c, id) => this._addDevice(id),
            'output-removed', (c, id) => this._removeDevice(id),
            'active-output-update', (c, id) => this._setActiveDevice(id),
            this);

        this._icons = [
            'audio-volume-muted-symbolic',
            'audio-volume-low-symbolic',
            'audio-volume-medium-symbolic',
            'audio-volume-high-symbolic',
            'audio-volume-overamplified-symbolic',
        ];

        this.menu.setHeader('audio-headphones-symbolic', _('Sound Output'));
        this.menuButtonAccessibleName = _('Open sound output menu');
    }

    _connectStream(stream) {
        super._connectStream(stream);
        stream.connectObject('notify::port',
            this._portChanged.bind(this), this);
        this._portChanged();
    }

    _lookupDevice(id) {
        return this._control.lookup_output_id(id);
    }

    _activateDevice(device) {
        this._control.change_output(device);
    }

    _findHeadphones(sink) {
        // This only works for external headphones (e.g. bluetooth)
        if (sink.get_form_factor() === 'headset' ||
            sink.get_form_factor() === 'headphone')
            return true;

        // a bit hackish, but ALSA/PulseAudio have a number
        // of different identifiers for headphones, and I could
        // not find the complete list
        if (sink.get_ports().length > 0)
            return sink.get_port().port.toLowerCase().includes('headphone');

        return false;
    }

    _portChanged() {
        const hasHeadphones = this._findHeadphones(this._stream);
        if (hasHeadphones === this._hasHeadphones)
            return;

        const initializing = this._hasHeadphones === undefined;
        this._hasHeadphones = hasHeadphones;
        this._updateIcon();
        if (!initializing)
            this.showOSD();
    }

    _updateIcon() {
        this.iconName = this._hasHeadphones
            ? 'audio-headphones-symbolic'
            : this.getIcon();
    }
});

const InputStreamSlider = GObject.registerClass(
class InputStreamSlider extends StreamSlider {
    _init(control) {
        super._init(control);

        this.slider.accessible_name = _('Microphone');

        this._control.connectObject(
            'input-added', (c, id) => this._addDevice(id),
            'input-removed', (c, id) => this._removeDevice(id),
            'active-input-update', (c, id) => this._setActiveDevice(id),
            'stream-added', () => this._maybeShowInput(),
            'stream-removed', () => this._maybeShowInput(),
            this);

        this.iconName = 'audio-input-microphone-symbolic';
        this._icons = [
            'microphone-sensitivity-muted-symbolic',
            'microphone-sensitivity-low-symbolic',
            'microphone-sensitivity-medium-symbolic',
            'microphone-sensitivity-high-symbolic',
        ];

        this.menu.setHeader('audio-input-microphone-symbolic', _('Sound Input'));
        this.menuButtonAccessibleName = _('Open sound input menu');
    }

    _connectStream(stream) {
        super._connectStream(stream);
        this._maybeShowInput();
    }

    _lookupDevice(id) {
        return this._control.lookup_input_id(id);
    }

    _activateDevice(device) {
        this._control.change_input(device);
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

            showInput = this._control.get_source_outputs().some(
                output => !skippedApps.includes(output.get_application_id()));
        }

        this._showInput = showInput;
        this._sync();
    }

    _shouldBeVisible() {
        return super._shouldBeVisible() && this._showInput;
    }
});

let VolumeIndicator = GObject.registerClass(
class VolumeIndicator extends SystemIndicator {
    constructor() {
        super();

        this._indicator = this._addIndicator();
        this._indicator.reactive = true;
    }

    _handleScrollEvent(item, event) {
        if (event.get_flags() & Clutter.EventFlags.FLAG_POINTER_EMULATED)
            return Clutter.EVENT_PROPAGATE;

        let direction = event.get_scroll_direction();
        let nSteps = 0;
        if (direction === Clutter.ScrollDirection.DOWN) {
            nSteps = -1;
        } else if (direction === Clutter.ScrollDirection.UP) {
            nSteps = 1;
        } else if (direction === Clutter.ScrollDirection.SMOOTH) {
            let [, dy] = event.get_scroll_delta();
            nSteps = -dy;
            // Match physical direction
            if (event.get_scroll_flags() & Clutter.ScrollFlags.INVERTED)
                nSteps *= -1;
        }

        if (item.mapped || item.slider.step(nSteps))
            item.showOSD();

        return Clutter.EVENT_STOP;
    }
});

export const OutputIndicator = GObject.registerClass(
class OutputIndicator extends VolumeIndicator {
    constructor() {
        super();

        this._indicator.connect('scroll-event',
            (actor, event) => this._handleScrollEvent(this._output, event));

        this._control = getMixerControl();
        this._control.connectObject(
            'state-changed', () => this._onControlStateChanged(),
            'default-sink-changed', () => this._readOutput(),
            this);

        this._output = new OutputStreamSlider(this._control);
        this._output.connect('stream-updated', () => {
            const icon = this._output.getIcon();

            if (icon)
                this._indicator.icon_name = icon;
            this._indicator.visible = icon !== null;
        });

        this.quickSettingsItems.push(this._output);

        this._onControlStateChanged();
    }

    _onControlStateChanged() {
        if (this._control.get_state() === Gvc.MixerControlState.READY)
            this._readOutput();
        else
            this._indicator.hide();
    }

    _readOutput() {
        this._output.stream = this._control.get_default_sink();
    }
});

export const InputIndicator = GObject.registerClass(
class InputIndicator extends VolumeIndicator {
    constructor() {
        super();

        this._indicator.add_style_class_name('privacy-indicator');

        this._indicator.connect('scroll-event',
            (actor, event) => this._handleScrollEvent(this._input, event));

        this._control = getMixerControl();
        this._control.connectObject(
            'state-changed', () => this._onControlStateChanged(),
            'default-source-changed', () => this._readInput(),
            this);

        this._input = new InputStreamSlider(this._control);
        this._input.connect('stream-updated', () => {
            const icon = this._input.getIcon();

            if (icon)
                this._indicator.icon_name = icon;
        });

        this._input.bind_property('visible',
            this._indicator, 'visible',
            GObject.BindingFlags.SYNC_CREATE);

        this.quickSettingsItems.push(this._input);

        this._onControlStateChanged();
    }

    _onControlStateChanged() {
        if (this._control.get_state() === Gvc.MixerControlState.READY)
            this._readInput();
    }

    _readInput() {
        this._input.stream = this._control.get_default_source();
    }
});
