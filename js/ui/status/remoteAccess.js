// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported RemoteAccessApplet */

const { GObject, Meta } = imports.gi;

const PanelMenu = imports.ui.panelMenu;
const PopupMenu = imports.ui.popupMenu;

var RemoteAccessApplet = GObject.registerClass(
class RemoteAccessApplet extends PanelMenu.SystemIndicator {
    _init() {
        super._init();

        let controller = global.backend.get_remote_access_controller();

        if (!controller)
            return;

        this._handles = new Set();
        this._sharedIndicator = null;
        this._recordingIndicator = null;
        this._menuSection = null;

        controller.connect('new-handle', (o, handle) => {
            this._onNewHandle(handle);
        });
    }

    _ensureControls() {
        if (this._sharedIndicator && this._recordingIndicator)
            return;

        this._sharedIndicator = this._addIndicator();
        this._sharedIndicator.icon_name = 'screen-shared-symbolic';
        this._sharedIndicator.add_style_class_name('remote-access-indicator');

        this._sharedItem =
            new PopupMenu.PopupSubMenuMenuItem(_("Screen is Being Shared"),
                                               true);
        this._sharedItem.menu.addAction(_("Turn off"),
            () => {
                for (let handle of this._handles) {
                    if (!handle.is_recording)
                        handle.stop();
                }
            });
        this._sharedItem.icon.icon_name = 'screen-shared-symbolic';
        this.menu.addMenuItem(this._sharedItem);

        this._recordingIndicator = this._addIndicator();
        this._recordingIndicator.icon_name = 'media-record-symbolic';
        this._recordingIndicator.add_style_class_name('screencast-indicator');
    }

    _isScreenShared() {
        return [...this._handles].some(handle => !handle.is_recording);
    }

    _isRecording() {
        return [...this._handles].some(handle => handle.is_recording);
    }

    _sync() {
        if (this._isScreenShared()) {
            this._sharedIndicator.visible = true;
            this._sharedItem.visible = true;
        } else {
            this._sharedIndicator.visible = false;
            this._sharedItem.visible = false;
        }

        this._recordingIndicator.visible = this._isRecording();
    }

    _onStopped(handle) {
        this._handles.delete(handle);
        this._sync();
    }

    _onNewHandle(handle) {
        // We can't possibly know about all types of screen sharing on X11, so
        // showing these controls on X11 might give a false sense of security.
        // Thus, only enable these controls when using Wayland, where we are
        // in control of sharing.
        //
        // We still want to show screen recordings though, to indicate when
        // the built in screen recorder is active, no matter the session type.
        if (!Meta.is_wayland_compositor() && !handle.is_recording)
            return;

        this._handles.add(handle);
        handle.connect('stopped', this._onStopped.bind(this));

        this._ensureControls();
        this._sync();
    }
});
