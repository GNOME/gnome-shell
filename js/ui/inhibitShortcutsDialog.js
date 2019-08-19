/* exported InhibitShortcutsDialog */
const { Clutter, Gio, GLib, GObject, Gtk, Meta, Shell } = imports.gi;

const Dialog = imports.ui.dialog;
const ModalDialog = imports.ui.modalDialog;
const PermissionStore = imports.misc.permissionStore;

const WAYLAND_KEYBINDINGS_SCHEMA = 'org.gnome.mutter.wayland.keybindings';

const APP_WHITELIST = ['gnome-control-center.desktop'];
const APP_PERMISSIONS_TABLE = 'gnome';
const APP_PERMISSIONS_ID = 'shortcuts-inhibitor';
const GRANTED = 'GRANTED';
const DENIED = 'DENIED';

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

    _shouldUsePermStore() {
        return this._app && !this._app.is_window_backed();
    }

    _saveToPermissionStore(grant) {
        if (!this._shouldUsePermStore() || this._permStore == null)
            return;

        let permissions = {};
        permissions[this._app.get_id()] = [grant];
        let data = GLib.Variant.new('av', {});

        this._permStore.SetRemote(APP_PERMISSIONS_TABLE,
                                  true,
                                  APP_PERMISSIONS_ID,
                                  permissions,
                                  data,
            (result, error) => {
                if (error != null)
                    log(error.message);
            });
    }

    _buildLayout() {
        let name = this._app ? this._app.get_name() : this._window.title;

        /* Translators: %s is an application name like "Settings" */
        let title = name
            ? _("%s wants to inhibit shortcuts").format(name)
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
                                     this._saveToPermissionStore(DENIED);
                                     this._emitResponse(DialogResponse.DENY);
                                 },
                                 key: Clutter.KEY_Escape });

        this._dialog.addButton({ label: _("Allow"),
                                 action: () => {
                                     this._saveToPermissionStore(GRANTED);
                                     this._emitResponse(DialogResponse.ALLOW);
                                 },
                                 default: true });
    }

    _emitResponse(response) {
        this.emit('response', response);
        this._dialog.close();
    }

    vfunc_show() {
        if (this._app && APP_WHITELIST.includes(this._app.get_id())) {
            this._emitResponse(DialogResponse.ALLOW);
            return;
        }

        if (!this._shouldUsePermStore()) {
            this._dialog.open();
            return;
        }

        /* Check with the permission store */
        let appId = this._app.get_id();
        this._permStore = new PermissionStore.PermissionStore((proxy, error) => {
            if (error) {
                log(error.message);
                this._dialog.open();
                return;
            }

            this._permStore.LookupRemote(APP_PERMISSIONS_TABLE,
                                         APP_PERMISSIONS_ID,
                (res, error) => {
                    if (error) {
                        this._dialog.open();
                        log(error.message);
                        return;
                    }

                    let [permissions] = res;
                    if (permissions[appId] === undefined) // Not found
                        this._dialog.open();
                    else if (permissions[appId] == GRANTED)
                        this._emitResponse(DialogResponse.ALLOW);
                    else
                        this._emitResponse(DialogResponse.DENY);
                });
        });
    }

    vfunc_hide() {
        this._dialog.close();
    }
});
