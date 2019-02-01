const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const Gtk = imports.gi.Gtk;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Dialog = imports.ui.dialog;
const ModalDialog = imports.ui.modalDialog;
const PermissionStore = imports.misc.permissionStore;

const WAYLAND_KEYBINDINGS_SCHEMA = 'org.gnome.mutter.wayland.keybindings';

const APP_WHITELIST = ['gnome-control-center.desktop'];
const APP_PERMISSIONS_TABLE = 'gnome';
const APP_PERMISSIONS_ID = 'shortcuts-inhibitor';

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

    get _appInfo() {
        let app = this._app;
        if (!app)
            return null;

        let appInfo = app.get_app_info();
        if (!appInfo)
            return null;

        return appInfo.get_id();
    }

    _getRestoreAccel() {
        let settings = new Gio.Settings({ schema_id: WAYLAND_KEYBINDINGS_SCHEMA });
        let accel = settings.get_strv('restore-shortcuts')[0] || '';
        return Gtk.accelerator_get_label.apply(null,
                                               Gtk.accelerator_parse(accel));
    }

    _saveToPermissionStore(grant) {
        if (this._appInfo == null || this._permStore == null)
            return;

        this._permissions[this._appInfo] = [grant.toString()];
        let data = GLib.Variant.new('av', {});

        this._permStore.SetRemote(APP_PERMISSIONS_TABLE,
                                  true,
                                  APP_PERMISSIONS_ID,
                                  this._permissions,
                                  data,
                                  (result, error) => {
            if (error != null)
                log(error.message);
        });
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
                                     this._saveToPermissionStore(false);
                                     this._emitResponse(DialogResponse.DENY);
                                 },
                                 key: Clutter.KEY_Escape });

        this._dialog.addButton({ label: _("Allow"),
                                 action: () => {
                                     this._saveToPermissionStore(true);
                                     this._emitResponse(DialogResponse.ALLOW);
                                 },
                                 default: true });
    }

    _emitResponse(response) {
        this.emit('response', response);
        this._dialog.close();
    }

    vfunc_show() {
        if (this._app && APP_WHITELIST.indexOf(this._app.get_id()) != -1) {
            this._emitResponse(DialogResponse.ALLOW);
            return;
        }

        /* Ask the permission-store */
        this._permStore = new PermissionStore.PermissionStore((proxy, error) => {
            if (error) {
                log(error.message);
                this._dialog.open();
                return;
            }

            if (this._appInfo) {
                this._permStore.LookupRemote(APP_PERMISSIONS_TABLE,
                                             APP_PERMISSIONS_ID,
                                             (res, error) => {
                    if (error) {
                        this._dialog.open();
                        log(error.message);
                        return;
                    }

                    let [permissions, data] = res;
                    if (permissions[this._appInfo] === undefined) // Not found
                        this._dialog.open();
                    else if (permissions[this._appInfo] == "true")
                        this._emitResponse(DialogResponse.ALLOW);
                    else
                        this._emitResponse(DialogResponse.DENY);
               });
            }
        });
    }

    vfunc_hide() {
        this._dialog.close();
    }
});
