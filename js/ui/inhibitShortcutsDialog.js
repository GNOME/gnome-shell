const { Clutter, Gio, GObject, Gtk, Meta, Shell } = imports.gi;

const Dialog = imports.ui.dialog;
const ModalDialog = imports.ui.modalDialog;

const WAYLAND_KEYBINDINGS_SCHEMA = 'org.gnome.mutter.wayland.keybindings';

const APP_WHITELIST = ['gnome-control-center.desktop'];

var DialogResponse = Meta.InhibitShortcutsDialogResponse;

var InhibitShortcutsDialog = GObject.registerClass({
    Implements: [Meta.InhibitShortcutsDialog],
    Properties: {
        'window': GObject.ParamSpec.override('window', Meta.InhibitShortcutsDialog)
    }
}, class InhibitShortcutsDialog extends GObject.Object {
    _init(window) {
        super._init();
        this._window = window;

        this._dialog = new ModalDialog.ModalDialog();
        this._buildLayout();
    }

    get window() {
        return this._window;
    }

    set window(window) {
        this._window = window;
    }

    get _app() {
        let windowTracker = Shell.WindowTracker.get_default();
        return windowTracker.get_window_app(this._window);
    }

    _getRestoreAccel() {
        let settings = new Gio.Settings({ schema_id: WAYLAND_KEYBINDINGS_SCHEMA });
        let accel = settings.get_strv('restore-shortcuts')[0] || '';
        return Gtk.accelerator_get_label.apply(null,
                                               Gtk.accelerator_parse(accel));
    }

    _buildLayout() {
        let name = this._app ? this._app.get_name() : this._window.title;

        /* Translators: %s is an application name like "Settings" */
        let title = name ? _("%s wants to inhibit shortcuts").format(name)
                         : _("Application wants to inhibit shortcuts");
        let icon = new Gio.ThemedIcon({ name: 'dialog-warning-symbolic' });

        let contentParams = { icon, title };

        let restoreAccel = this._getRestoreAccel();
        if (restoreAccel)
            contentParams.subtitle =
                /* Translators: %s is a keyboard shortcut like "Super+x" */
                _("You can restore shortcuts by pressing %s.").format(restoreAccel);

        let content = new Dialog.MessageDialogContent(contentParams);
        this._dialog.contentLayout.add_actor(content);

        this._dialog.addButton({ label: _("Deny"),
                                 action: () => {
                                     this._emitResponse(DialogResponse.DENY);
                                 },
                                 key: Clutter.KEY_Escape });

        this._dialog.addButton({ label: _("Allow"),
                                 action: () => {
                                     this._emitResponse(DialogResponse.ALLOW);
                                 },
                                 default: true });
    }

    _emitResponse(response) {
        this.emit('response', response);
        this._dialog.close();
    }

    vfunc_show() {
        if (this._app && APP_WHITELIST.indexOf(this._app.get_id()) != -1)
            this._emitResponse(DialogResponse.ALLOW);
        else
            this._dialog.open();
    }

    vfunc_hide() {
        this._dialog.close();
    }
});
