/* exported InhibitShortcutsDialog */
const {Clutter, GObject, Meta, Pango, Shell, St} = imports.gi;

const Dialog = imports.ui.dialog;
const ModalDialog = imports.ui.modalDialog;
const PermissionStore = imports.misc.permissionStore;

const APP_ALLOWLIST = ['org.gnome.Settings.desktop'];
const APP_PERMISSIONS_TABLE = 'gnome';
const APP_PERMISSIONS_ID = 'shortcuts-inhibitor';
const GRANTED = 'GRANTED';
const DENIED = 'DENIED';

var DialogResponse = Meta.InhibitShortcutsDialogResponse;

var InhibitShortcutsDialog = GObject.registerClass({
    Implements: [Meta.InhibitShortcutsDialog],
    Properties: {
        'window': GObject.ParamSpec.override('window', Meta.InhibitShortcutsDialog),
    },
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

    _shouldUsePermStore() {
        return this._app && !this._app.is_window_backed();
    }

    async _saveToPermissionStore(grant) {
        if (!this._shouldUsePermStore() || this._permStore == null)
            return;

        try {
            await this._permStore.SetPermissionAsync(APP_PERMISSIONS_TABLE,
                true,
                APP_PERMISSIONS_ID,
                this._app.get_id(),
                [grant]);
        } catch (error) {
            log(error.message);
        }
    }

    _buildLayout() {
        const name = this._app?.get_name() ?? this._window.title;

        let content = new Dialog.MessageDialogContent({
            title: _('Allow inhibiting shortcuts'),
            description: name
                /* Translators: %s is an application name like "Settings" */
                ? _('The app %s wants to inhibit shortcuts').format(name)
                : _('An app wants to inhibit shortcuts'),
        });

        const restoreAccel = Meta.prefs_get_keybinding_label('restore-shortcuts');
        if (restoreAccel) {
            let restoreLabel = new St.Label({
                /* Translators: %s is a keyboard shortcut like "Super+x" */
                text: _('You can restore shortcuts by pressing %s.').format(restoreAccel),
                style_class: 'message-dialog-description',
            });
            restoreLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
            restoreLabel.clutter_text.line_wrap = true;
            content.add_child(restoreLabel);
        }

        this._dialog.contentLayout.add_child(content);

        this._dialog.addButton({
            label: _('Deny'),
            action: () => {
                this._saveToPermissionStore(DENIED);
                this._emitResponse(DialogResponse.DENY);
            },
            key: Clutter.KEY_Escape,
        });

        this._dialog.addButton({
            label: _('Allow'),
            action: () => {
                this._saveToPermissionStore(GRANTED);
                this._emitResponse(DialogResponse.ALLOW);
            },
            default: true,
        });
    }

    _emitResponse(response) {
        this.emit('response', response);
        this._dialog.close();
    }

    vfunc_show() {
        if (this._app && APP_ALLOWLIST.includes(this._app.get_id())) {
            this._emitResponse(DialogResponse.ALLOW);
            return;
        }

        if (!this._shouldUsePermStore()) {
            this._dialog.open();
            return;
        }

        /* Check with the permission store */
        let appId = this._app.get_id();
        this._permStore = new PermissionStore.PermissionStore(async (proxy, error) => {
            if (error) {
                log(error.message);
                this._dialog.open();
                return;
            }

            try {
                const [permissions] = await this._permStore.LookupAsync(
                    APP_PERMISSIONS_TABLE, APP_PERMISSIONS_ID);

                if (permissions[appId] === undefined) // Not found
                    this._dialog.open();
                else if (permissions[appId][0] === GRANTED)
                    this._emitResponse(DialogResponse.ALLOW);
                else
                    this._emitResponse(DialogResponse.DENY);
            } catch (err) {
                this._dialog.open();
                log(err.message);
            }
        });
    }

    vfunc_hide() {
        this._dialog.close();
    }
});
