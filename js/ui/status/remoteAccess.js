// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported RemoteAccessApplet, ScreenRecordingIndicator, ScreenSharingIndicator */

const { Atk, Clutter, GLib, GObject, Meta, St } = imports.gi;

const Main = imports.ui.main;
const PanelMenu = imports.ui.panelMenu;
const {SystemIndicator} = imports.ui.quickSettings;

// Minimum amount of time the shared indicator is visible (in micro seconds)
const MIN_SHARED_INDICATOR_VISIBLE_TIME_US = 5 * GLib.TIME_SPAN_SECOND;

var RemoteAccessApplet = GObject.registerClass(
class RemoteAccessApplet extends SystemIndicator {
    _init() {
        super._init();

        let controller = global.backend.get_remote_access_controller();

        if (!controller)
            return;

        this._handles = new Set();

        this._indicator = this._addIndicator();
        this._indicator.icon_name = 'media-record-symbolic';
        this._indicator.add_style_class_name('screencast-indicator');

        controller.connect('new-handle', (o, handle) => {
            this._onNewHandle(handle);
        });
        this._sync();
    }

    _isRecording() {
        // Screenshot UI screencasts have their own panel, so don't show this
        // indicator if there's only a screenshot UI screencast.
        if (Main.screenshotUI.screencast_in_progress)
            return this._handles.size > 1;

        return this._handles.size > 0;
    }

    _sync() {
        this._indicator.visible = this._isRecording();
    }

    _onStopped(handle) {
        this._handles.delete(handle);
        this._sync();
    }

    _onNewHandle(handle) {
        if (!handle.is_recording)
            return;

        this._handles.add(handle);
        handle.connect('stopped', this._onStopped.bind(this));

        this._sync();
    }
});

var ScreenRecordingIndicator = GObject.registerClass({
    Signals: { 'menu-set': {} },
}, class ScreenRecordingIndicator extends PanelMenu.ButtonBox {
    _init() {
        super._init({
            reactive: true,
            can_focus: true,
            track_hover: true,
            accessible_name: _('Stop Screencast'),
            accessible_role: Atk.Role.PUSH_BUTTON,
        });
        this.add_style_class_name('screen-recording-indicator');

        this._box = new St.BoxLayout();
        this.add_child(this._box);

        this._label = new St.Label({
            text: '0:00',
            y_align: Clutter.ActorAlign.CENTER,
        });
        this._box.add_child(this._label);

        this._icon = new St.Icon({ icon_name: 'stop-symbolic' });
        this._box.add_child(this._icon);

        this.hide();
        Main.screenshotUI.connect(
            'notify::screencast-in-progress',
            this._onScreencastInProgressChanged.bind(this));
    }

    vfunc_event(event) {
        if (event.type() === Clutter.EventType.TOUCH_BEGIN ||
            event.type() === Clutter.EventType.BUTTON_PRESS)
            Main.screenshotUI.stopScreencast();

        return Clutter.EVENT_PROPAGATE;
    }

    _updateLabel() {
        const minutes = this._secondsPassed / 60;
        const seconds = this._secondsPassed % 60;
        this._label.text = '%d:%02d'.format(minutes, seconds);
    }

    _onScreencastInProgressChanged() {
        if (Main.screenshotUI.screencast_in_progress) {
            this.show();

            this._secondsPassed = 0;
            this._updateLabel();

            this._timeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, 1000, () => {
                this._secondsPassed += 1;
                this._updateLabel();
                return GLib.SOURCE_CONTINUE;
            });
            GLib.Source.set_name_by_id(
                this._timeoutId, '[gnome-shell] screen recording indicator tick');
        } else {
            this.hide();

            GLib.source_remove(this._timeoutId);
            delete this._timeoutId;

            delete this._secondsPassed;
        }
    }
});

var ScreenSharingIndicator = GObject.registerClass({
    Signals: {'menu-set': {}},
}, class ScreenSharingIndicator extends PanelMenu.ButtonBox {
    _init() {
        super._init({
            reactive: true,
            can_focus: true,
            track_hover: true,
            accessible_name: _('Stop Screen Sharing'),
            accessible_role: Atk.Role.PUSH_BUTTON,
        });
        this.add_style_class_name('screen-sharing-indicator');

        this._box = new St.BoxLayout();
        this.add_child(this._box);

        let icon = new St.Icon({icon_name: 'screen-shared-symbolic'});
        this._box.add_child(icon);

        icon = new St.Icon({icon_name: 'window-close-symbolic'});
        this._box.add_child(icon);

        this._controller = global.backend.get_remote_access_controller();

        this._handles = new Set();

        this._controller?.connect('new-handle',
            (o, handle) => this._onNewHandle(handle));

        this._sync();
    }

    _onNewHandle(handle) {
        // We can't possibly know about all types of screen sharing on X11, so
        // showing these controls on X11 might give a false sense of security.
        // Thus, only enable these controls when using Wayland, where we are
        // in control of sharing.
        if (!Meta.is_wayland_compositor())
            return;

        if (handle.isRecording)
            return;

        this._handles.add(handle);
        handle.connect('stopped', () => {
            this._handles.delete(handle);
            this._sync();
        });
        this._sync();
    }

    vfunc_event(event) {
        if (event.type() === Clutter.EventType.TOUCH_BEGIN ||
            event.type() === Clutter.EventType.BUTTON_PRESS)
            this._stopSharing();

        return Clutter.EVENT_PROPAGATE;
    }

    _stopSharing() {
        for (const handle of this._handles)
            handle.stop();
    }

    _hideIndicator() {
        this.hide();
        delete this._hideIndicatorId;
        return GLib.SOURCE_REMOVE;
    }

    _sync() {
        if (this._hideIndicatorId) {
            GLib.source_remove(this._hideIndicatorId);
            delete this._hideIndicatorId;
        }

        if (this._handles.size > 0) {
            if (!this.visible)
                this._visibleTimeUs = GLib.get_monotonic_time();
            this.show();
        } else if (this.visible) {
            const currentTimeUs = GLib.get_monotonic_time();
            const timeSinceVisibleUs = currentTimeUs - this._visibleTimeUs;

            if (timeSinceVisibleUs >= MIN_SHARED_INDICATOR_VISIBLE_TIME_US) {
                this._hideIndicator();
            } else {
                const timeUntilHideUs =
                    MIN_SHARED_INDICATOR_VISIBLE_TIME_US - timeSinceVisibleUs;
                this._hideIndicatorId =
                    GLib.timeout_add(GLib.PRIORITY_DEFAULT,
                        timeUntilHideUs / GLib.TIME_SPAN_MILLISECOND,
                        () => this._hideIndicator());
            }
        }
    }
});
