const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GObject = imports.gi.GObject;
const Gtk = imports.gi.Gtk;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const Main = imports.ui.main;
const Dialog = imports.ui.dialog;
const ModalDialog = imports.ui.modalDialog;
const CheckBox = imports.ui.checkBox;

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

    _isWhitelisted() {
        return APP_WHITELIST.includes(this._app.get_id());
    }

    _isInList(listName) {
       let list = global.settings.get_strv(listName);
       return list.includes(this._appInfo);
    }

    _addToList(listName) {
       let list = global.settings.get_strv(listName);
       list.push(this._appInfo);
       global.settings.set_strv(listName, list);
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

        let check = new CheckBox.CheckBox();
        check.getLabelActor().text = _("Do not ask again for this application");
        check.actor.checked = false;
        if (this._appInfo)
            content.insertBeforeBody(check.actor);

        this._dialog.contentLayout.add_actor(content);

        this._dialog.addButton({ label: _("Deny"),
                                 action: () => {
                                     if (this._appInfo && check.actor.checked)
                                         this._addToList('inhibit-shortcuts-denied');
                                     this._emitResponse(DialogResponse.DENY);
                                 },
                                 key: Clutter.KEY_Escape });

        this._dialog.addButton({ label: _("Allow"),
                                 action: () => {
                                     if (this._appInfo && check.actor.checked)
                                         this._addToList('inhibit-shortcuts-granted');
                                     this._emitResponse(DialogResponse.ALLOW);
                                 },
                                 default: true });
    }

    _emitResponse(response) {
        this.emit('response', response);
        this._dialog.close();
    }

    vfunc_show() {
        if (this._app) {
            if (this._isWhitelisted() || this._isInList('inhibit-shortcuts-granted')) {
                this._emitResponse(DialogResponse.ALLOW);
                return;
            }
            if (this._isInList('inhibit-shortcuts-denied')) {
                this._emitResponse(DialogResponse.DENY);
                return;
            }
        }
        this._dialog.open();
    }

    vfunc_hide() {
        this._dialog.close();
    }
});
