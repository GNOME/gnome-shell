// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GObject = imports.gi.GObject;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;

const Dialog = imports.ui.dialog;
const Main = imports.ui.main;
const Tweener = imports.ui.tweener;

var FROZEN_WINDOW_BRIGHTNESS = -0.3
var DIALOG_TRANSITION_TIME = 0.15

var CloseDialog = new Lang.Class({
    Name: 'CloseDialog',
    Extends: GObject.Object,
    Implements: [ Meta.CloseDialog ],
    Properties: {
        'window': GObject.ParamSpec.override('window', Meta.CloseDialog)
    },

    _init(window) {
        this.parent();
        this._window = window;
        this._dialog = null;
    },

    get window() {
        return this._window;
    },

    set window(window) {
        this._window = window;
    },

    _createDialogContent() {
        let tracker = Shell.WindowTracker.get_default();
        let windowApp = tracker.get_window_app(this._window);

        /* Translators: %s is an application name */
        let title = _("“%s” is not responding.").format(windowApp.get_name());
        let subtitle = _("You may choose to wait a short while for it to " +
                         "continue or force the application to quit entirely.");
        let icon = new Gio.ThemedIcon({ name: 'dialog-warning-symbolic' });
        return new Dialog.MessageDialogContent({ icon, title, subtitle });
    },

    _initDialog() {
        if (this._dialog)
            return;

        let windowActor = this._window.get_compositor_private();
        this._dialog = new Dialog.Dialog(windowActor, 'close-dialog');
        this._dialog.width = windowActor.width;
        this._dialog.height = windowActor.height;

        this._dialog.addContent(this._createDialogContent());
        this._dialog.addButton({ label:   _('Force Quit'),
                                 action:  this._onClose.bind(this),
                                 default: true });
        this._dialog.addButton({ label:  _('Wait'),
                                 action: this._onWait.bind(this),
                                 key:    Clutter.Escape });

        global.focus_manager.add_group(this._dialog);
    },

    _addWindowEffect() {
        // We set the effect on the surface actor, so the dialog itself
        // (which is a child of the MetaWindowActor) does not get the
        // effect applied itself.
        let windowActor = this._window.get_compositor_private();
        let surfaceActor = windowActor.get_first_child();
        let effect = new Clutter.BrightnessContrastEffect();
        effect.set_brightness(FROZEN_WINDOW_BRIGHTNESS);
        surfaceActor.add_effect_with_name("gnome-shell-frozen-window", effect);
    },

    _removeWindowEffect() {
        let windowActor = this._window.get_compositor_private();
        let surfaceActor = windowActor.get_first_child();
        surfaceActor.remove_effect_by_name("gnome-shell-frozen-window");
    },

    _onWait() {
        this.response(Meta.CloseDialogResponse.WAIT);
    },

    _onClose() {
        this.response(Meta.CloseDialogResponse.FORCE_CLOSE);
    },

    vfunc_show() {
        if (this._dialog != null)
            return;

        Meta.disable_unredirect_for_screen(global.screen);

        this._addWindowEffect();
        this._initDialog();

        this._dialog.scale_y = 0;
        this._dialog.set_pivot_point(0.5, 0.5);

        Tweener.addTween(this._dialog,
                         { scale_y: 1,
                           transition: 'linear',
                           time: DIALOG_TRANSITION_TIME,
                           onComplete: () => {
                               Main.layoutManager.trackChrome(this._dialog, { affectsInputRegion: true });
                           }
                         });
    },

    vfunc_hide() {
        if (this._dialog == null)
            return;

        Meta.enable_unredirect_for_screen(global.screen);

        let dialog = this._dialog;
        this._dialog = null;
        this._removeWindowEffect();

        Tweener.addTween(dialog,
                         { scale_y: 0,
                           transition: 'linear',
                           time: DIALOG_TRANSITION_TIME,
                           onComplete: () => {
                               dialog.destroy();
                           }
                         });
    },

    vfunc_focus() {
        if (this._dialog)
            this._dialog.grab_key_focus();
    }
});
