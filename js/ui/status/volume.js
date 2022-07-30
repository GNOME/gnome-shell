// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported Indicator */

const { Clutter, Gio, GLib, GObject, Gvc, St } = imports.gi;
const Signals = imports.misc.signals;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;
const Slider = imports.ui.slider;

const ALLOW_AMPLIFIED_VOLUME_KEY = 'allow-volume-above-100-percent';

// Each Gvc.MixerControl is a connection to PulseAudio,
// so it's better to make it a singleton
let _mixerControl;
/**
 * @returns {Gvc.MixerControl} - the mixer control singleton
 */
function getMixerControl() {
    if (_mixerControl)
        return _mixerControl;

    _mixerControl = new Gvc.MixerControl({ name: 'GNOME Shell Volume Control' });
    _mixerControl.open();

    return _mixerControl;
}

var StreamSlider = class extends Signals.EventEmitter {
    constructor(control) {
        super();

        this._control = control;

        this.item = new PopupMenu.PopupMenuSection();

        const sliderItem = new PopupMenu.PopupBaseMenuItem({activate: false});
        this.item.addMenuItem(sliderItem);

        const submenuItem = new PopupMenu.PopupSubMenuMenuItem('');
        this.item.addMenuItem(submenuItem);

        // HACK: Hide the submenu item, its menu is controlled from sliderItem
        submenuItem.hide();

        this.menu = submenuItem.menu;

        this._inDrag = false;
        this._notifyVolumeChangeId = 0;

        this._slider = new Slider.Slider(0);

        this._soundSettings = new Gio.Settings({
            schema_id: 'org.gnome.desktop.sound',
        });
        this._soundSettings.connect(`changed::${ALLOW_AMPLIFIED_VOLUME_KEY}`,
            () => this._amplifySettingsChanged());
        this._amplifySettingsChanged();

        this._sliderChangedId = this._slider.connect('notify::value',
            () => this._sliderChanged());
        this._slider.connect('drag-begin', () => (this._inDrag = true));
        this._slider.connect('drag-end', () => {
            this._inDrag = false;
            this._notifyVolumeChange();
        });

        this._icon = new St.Icon({ style_class: 'popup-menu-icon' });
        sliderItem.add(this._icon);
        sliderItem.add_child(this._slider);
        sliderItem.connect('button-press-event',
            (actor, event) => this._slider.startDragging(event));
        sliderItem.connect('key-press-event',
            (actor, event) => this._slider.emit('key-press-event', event));
        sliderItem.connect('scroll-event',
            (actor, event) => this._slider.emit('scroll-event', event));

        this._menuButton = new St.Button({
            child: new St.Icon({
                iconName: 'pan-end-symbolic',
                style_class: 'popup-menu-arrow',
            }),
            y_expand: true,
        });
        sliderItem.add_child(this._menuButton);

        this._menuButton.connect('clicked', () => this.menu.toggle());

        // In order to keep sliders aligned, do not hide
        // the menu button, but make it fully transparent
        this._menuButton.bind_property_full('reactive',
            this._menuButton, 'opacity',
            GObject.BindingFlags.DEFAULT,
            (bind, source) => [true, source ? 255 : 0],
            null);

        this._deviceItems = new Map();

        this._deviceSection = new PopupMenu.PopupMenuSection();
        this.menu.addMenuItem(this._deviceSection);

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

        const item = new PopupMenu.PopupImageMenuItem(
            device.get_description(), device.get_gicon());
        item.connect('activate', () => this._activateDevice(device));

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
        this.item.actor.visible = this._shouldBeVisible();
        this._menuButton.reactive = this._deviceItems.size > 1;
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
            _('Volume changed'), this._volumeCancellable);
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
};

var OutputStreamSlider = class extends StreamSlider {
    constructor(control) {
        super(control);

        this._slider.accessible_name = _('Volume');

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

    _portChanged() {
        const hasHeadphones = this._findHeadphones(this._stream);
        if (hasHeadphones === this._hasHeadphones)
            return;

        this._hasHeadphones = hasHeadphones;
        this._icon.icon_name = this._hasHeadphones
            ? 'audio-headphones-symbolic'
            : 'audio-speakers-symbolic';
    }
};

var InputStreamSlider = class extends StreamSlider {
    constructor(control) {
        super(control);

        this._slider.accessible_name = _('Microphone');

        this._control.connectObject(
            'input-added', (c, id) => this._addDevice(id),
            'input-removed', (c, id) => this._removeDevice(id),
            'active-input-update', (c, id) => this._setActiveDevice(id),
            'stream-added', () => this._maybeShowInput(),
            'stream-removed', () => this._maybeShowInput(),
            this);

        this._icon.icon_name = 'audio-input-microphone-symbolic';
        this._icons = [
            'microphone-sensitivity-muted-symbolic',
            'microphone-sensitivity-low-symbolic',
            'microphone-sensitivity-medium-symbolic',
            'microphone-sensitivity-high-symbolic',
        ];
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
};

var Indicator = GObject.registerClass(
class Indicator extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        this._primaryIndicator = this._addIndicator();
        this._inputIndicator = this._addIndicator();

        this._primaryIndicator.reactive = true;
        this._inputIndicator.reactive = true;

        this._primaryIndicator.connect('scroll-event',
            (actor, event) => this._handleScrollEvent(this._output, event));
        this._inputIndicator.connect('scroll-event',
            (actor, event) => this._handleScrollEvent(this._input, event));

        const volumeMenu = new PopupMenu.PopupMenuSection();

        this._control = getMixerControl();
        this._control.connectObject(
            'state-changed', () => this._onControlStateChanged(),
            'default-sink-changed', () => this._readOutput(),
            'default-source-changed', () => this._readInput(),
            this);

        this._output = new OutputStreamSlider(this._control);
        this._output.connect('stream-updated', () => {
            const icon = this._output.getIcon();

            if (icon)
                this._primaryIndicator.icon_name = icon;
            this._primaryIndicator.visible = icon !== null;
        });
        volumeMenu.addMenuItem(this._output.item);

        this._input = new InputStreamSlider(this._control);
        this._input.connect('stream-updated', () => {
            const icon = this._input.getIcon();

            if (icon)
                this._inputIndicator.icon_name = icon;
        });
        volumeMenu.addMenuItem(this._input.item);

        this._input.item.actor.bind_property('visible',
            this._inputIndicator, 'visible',
            GObject.BindingFlags.SYNC_CREATE);

        this.menu.addMenuItem(volumeMenu);

        this._onControlStateChanged();
    }

    _onControlStateChanged() {
        if (this._control.get_state() === Gvc.MixerControlState.READY) {
            this._readInput();
            this._readOutput();
        } else {
            this._primaryIndicator.hide();
        }
    }

    _readOutput() {
        this._output.stream = this._control.get_default_sink();
    }

    _readInput() {
        this._input.stream = this._control.get_default_source();
    }

    _handleScrollEvent(item, event) {
        const result = item.scroll(event);
        if (result === Clutter.EVENT_PROPAGATE || this.menu.actor.mapped)
            return result;

        const gicon = new Gio.ThemedIcon({name: item.getIcon()});
        const level = item.getLevel();
        const maxLevel = item.getMaxLevel();
        Main.osdWindowManager.show(-1, gicon, null, level, maxLevel);
        return result;
    }
});
