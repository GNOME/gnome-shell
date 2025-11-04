import Clutter from 'gi://Clutter';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Meta from 'gi://Meta';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Dialog from './dialog.js';

const FROZEN_WINDOW_BRIGHTNESS = -0.3;
const DIALOG_TRANSITION_TIME = 150;
const ALIVE_TIMEOUT = 5000;

export const CloseDialog = GObject.registerClass({
    Implements: [Meta.CloseDialog],
    Properties: {
        'window': GObject.ParamSpec.override('window', Meta.CloseDialog),
    },
}, class CloseDialog extends GObject.Object {
    _init(window) {
        super._init();
        this._window = window;
        this._dialog = null;
        this._timeoutId = 0;
    }

    get window() {
        return this._window;
    }

    set window(window) {
        this._window = window;
    }

    _createDialogContent() {
        const tracker = Shell.WindowTracker.get_default();
        const windowApp = tracker.get_window_app(this._window);

        /* Translators: %s is an application name */
        const title = _('“%s” Is Not Responding').format(windowApp.get_name());
        const description = _('You may choose to wait a short while for it to ' +
                            'continue or force the app to quit entirely');
        return new Dialog.MessageDialogContent({title, description});
    }

    _updateScale() {
        // Since this is a child of MetaWindowActor (which, for Wayland clients,
        // applies the geometry scale factor to its children itself, see
        // meta_window_actor_set_geometry_scale()), make sure we don't apply
        // the factor twice in the end.
        if (this._window.get_client_type() !== Meta.WindowClientType.WAYLAND)
            return;

        const {scaleFactor} = St.ThemeContext.get_for_stage(global.stage);
        this._dialog.set_scale(1 / scaleFactor, 1 / scaleFactor);
    }

    _initDialog() {
        if (this._dialog)
            return;

        const windowActor = this._window.get_compositor_private();
        this._dialog = new Dialog.Dialog(windowActor, 'close-dialog');
        this._dialog.width = windowActor.width;
        this._dialog.height = windowActor.height;

        this._dialog.contentLayout.add_child(this._createDialogContent());
        this._dialog.addButton({
            label: _('Force Quit'),
            action: this._onClose.bind(this),
            default: true,
        });
        this._dialog.addButton({
            label: _('Wait'),
            action: this._onWait.bind(this),
            key: Clutter.KEY_Escape,
        });

        global.focus_manager.add_group(this._dialog);

        const themeContext = St.ThemeContext.get_for_stage(global.stage);
        themeContext.connect('notify::scale-factor', this._updateScale.bind(this));

        this._updateScale();
    }

    _addWindowEffect() {
        // We set the effect on the surface actor, so the dialog itself
        // (which is a child of the MetaWindowActor) does not get the
        // effect applied itself.
        const windowActor = this._window.get_compositor_private();
        const surfaceActor = windowActor.get_first_child();
        const effect = new Clutter.BrightnessContrastEffect();
        effect.set_brightness(FROZEN_WINDOW_BRIGHTNESS);
        surfaceActor.add_effect_with_name('gnome-shell-frozen-window', effect);
    }

    _removeWindowEffect() {
        const windowActor = this._window.get_compositor_private();
        const surfaceActor = windowActor.get_first_child();
        surfaceActor.remove_effect_by_name('gnome-shell-frozen-window');
    }

    _onWait() {
        this.response(Meta.CloseDialogResponse.WAIT);
    }

    _onClose() {
        this.response(Meta.CloseDialogResponse.FORCE_CLOSE);
    }

    vfunc_show() {
        if (this._dialog != null)
            return;

        global.compositor.disable_unredirect();

        this._timeoutId = GLib.timeout_add(GLib.PRIORITY_DEFAULT, ALIVE_TIMEOUT,
            () => {
                this._window.check_alive(global.display.get_current_time_roundtrip());
                return GLib.SOURCE_CONTINUE;
            });

        this._addWindowEffect();
        this._initDialog();

        global.connectObject(
            'shutdown', () => this._onWait(), this._dialog);

        this._dialog._dialog.set_pivot_point(0.5, 0.5);
        this._dialog._dialog.scale_x = 0.8;
        this._dialog._dialog.scale_y = 0.8;

        this._dialog._dialog.ease({
            scale_x: 1,
            scale_y: 1,
            mode: Clutter.AnimationMode.EASE_OUT_BACK,
            duration: DIALOG_TRANSITION_TIME,
        });
    }

    vfunc_hide() {
        if (this._dialog == null)
            return;

        global.compositor.enable_unredirect();

        GLib.source_remove(this._timeoutId);
        this._timeoutId = 0;

        global.display.disconnectObject(this);
        global.stage.disconnectObject(this);

        this._dialog._dialog.remove_all_transitions();

        const dialog = this._dialog;
        this._dialog = null;
        this._removeWindowEffect();

        dialog.makeInactive();
        dialog._dialog.ease({
            opacity: 0,
            scale_x: 0.8,
            scale_y: 0.8,
            mode: Clutter.AnimationMode.EASE_OUT_QUAD,
            duration: DIALOG_TRANSITION_TIME,
            onComplete: () => dialog.destroy(),
        });
    }

    vfunc_focus() {
        if (!this._dialog)
            return;

        const keyFocus = global.stage.key_focus;
        if (!keyFocus || !this._dialog.contains(keyFocus))
            this._dialog.initialKeyFocus.grab_key_focus();
    }
});
