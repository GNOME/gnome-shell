const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GObject = imports.gi.GObject;
const Gtk = imports.gi.Gtk;
const Lang = imports.lang;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Dialog = imports.ui.dialog;
const ModalDialog = imports.ui.modalDialog;

const WAYLAND_KEYBINDINGS_SCHEMA = 'org.gnome.mutter.wayland.keybindings';

var DialogResponse = Meta.InhibitShortcutsDialogResponse;

var InhibitShortcutsDialog = new Lang.Class({
    Name: 'InhibitShortcutsDialog',
    Extends: GObject.Object,
    Implements: [Meta.InhibitShortcutsDialog],
    Properties: {
        'window': GObject.ParamSpec.override('window', Meta.InhibitShortcutsDialog)
    },

    _init: function(window) {
        this.parent();
        this._window = window;

        this._dialog = new ModalDialog.ModalDialog();
        this._buildLayout();
    },

    get window() {
        return this._window;
    },

    set window(window) {
        this._window = window;
    },

    _getRestoreAccel: function() {
        let settings = new Gio.Settings({ schema_id: WAYLAND_KEYBINDINGS_SCHEMA });
        let accel = settings.get_strv('restore-shortcuts')[0] || '';
        return Gtk.accelerator_get_label.apply(null,
                                               Gtk.accelerator_parse(accel));
    },

    _buildLayout: function() {
        let windowTracker = Shell.WindowTracker.get_default();
        let app = windowTracker.get_window_app(this._window);
        let name = app ? app.get_name() : this._window.title;

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
    },

    _emitResponse: function(response) {
        this.emit('response', response);
        this._dialog.close();
    },

    vfunc_show: function() {
        this._dialog.open();
    },

    vfunc_hide: function() {
        this._dialog.close();
    }
});
