const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;
const Pango = imports.gi.Pango;
const Shell = imports.gi.Shell;
const St = imports.gi.St;

const CheckBox = imports.ui.checkBox;
const Dialog = imports.ui.dialog;
const ModalDialog = imports.ui.modalDialog;

const RequestIface = '<node> \
<interface name="org.freedesktop.impl.portal.Request"> \
<method name="Close"/> \
</interface> \
</node>';

const AccessIface = '<node> \
<interface name="org.freedesktop.impl.portal.Access"> \
<method name="AccessDialog"> \
  <arg type="o" name="handle" direction="in"/> \
  <arg type="s" name="app_id" direction="in"/> \
  <arg type="s" name="parent_window" direction="in"/> \
  <arg type="s" name="title" direction="in"/> \
  <arg type="s" name="subtitle" direction="in"/> \
  <arg type="s" name="body" direction="in"/> \
  <arg type="a{sv}" name="options" direction="in"/> \
  <arg type="u" name="response" direction="out"/> \
  <arg type="a{sv}" name="results" direction="out"/> \
</method> \
</interface> \
</node>';

var DialogResponse = {
    OK: 0,
    CANCEL: 1,
    CLOSED: 2
};

var AccessDialog = new Lang.Class({
    Name: 'AccessDialog',
    Extends: ModalDialog.ModalDialog,

    _init(invocation, handle, title, subtitle, body, options) {
        this.parent({ styleClass: 'access-dialog' });

        this._invocation = invocation;
        this._handle = handle;

        this._requestExported = false;
        this._request = Gio.DBusExportedObject.wrapJSObject(RequestIface, this);

        for (let option in options)
            options[option] = options[option].deep_unpack();

        this._buildLayout(title, subtitle, body, options);
    },

    _buildLayout(title, subtitle, body, options) {
        // No support for non-modal system dialogs, so ignore the option
        //let modal = options['modal'] || true;
        let denyLabel = options['deny_label'] || _("Deny Access");
        let grantLabel = options['grant_label'] || _("Grant Access");
        let iconName = options['icon'] || null;
        let choices = options['choices'] || [];

        let contentParams = { title, subtitle, body };
        if (iconName)
            contentParams.icon = new Gio.ThemedIcon({ name: iconName });
        let content = new Dialog.MessageDialogContent(contentParams);
        this.contentLayout.add_actor(content);

        this._choices = new Map();

        for (let i = 0; i < choices.length; i++) {
            let [id, name, opts, selected] = choices[i];
            if (opts.length > 0)
                continue; // radio buttons, not implemented

            let check = new CheckBox.CheckBox();
            check.getLabelActor().text = name;
            check.actor.checked = selected == "true";
            content.insertBeforeBody(check.actor);

            this._choices.set(id, check);
        }

        this.addButton({ label: denyLabel,
                         action: () => {
                             this._sendResponse(DialogResponse.CANCEL);
                         },
                         key: Clutter.KEY_Escape });
        this.addButton({ label: grantLabel,
                         action: () => {
                             this._sendResponse(DialogResponse.OK);
                         }});
    },

    open() {
        this.parent();

        let connection = this._invocation.get_connection();
        this._requestExported = this._request.export(connection, this._handle);
    },

    CloseAsync(invocation, params) {
        if (this._invocation.get_sender() != invocation.get_sender()) {
            invocation.return_error_literal(Gio.DBusError,
                                            Gio.DBusError.ACCESS_DENIED,
                                            '');
            return;
        }

        this._sendResponse(DialogResponse.CLOSED);
    },

    _sendResponse(response) {
        if (this._requestExported)
            this._request.unexport();
        this._requestExported = false;

        let results = {};
        if (response == DialogResponse.OK) {
            for (let [id, check] of this._choices) {
                let checked = check.actor.checked ? 'true' : 'false';
                results[id] = new GLib.Variant('s', checked);
            }
        }

        // Delay actual response until the end of the close animation (if any)
        this.connect('closed', () => {
            this._invocation.return_value(new GLib.Variant('(ua{sv})',
                                                           [response, results]));
        });
        this.close();
    }
});

var AccessDialogDBus = new Lang.Class({
    Name: 'AccessDialogDBus',

    _init() {
        this._accessDialog = null;

        this._windowTracker = Shell.WindowTracker.get_default();

        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(AccessIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/freedesktop/portal/desktop');

        Gio.DBus.session.own_name('org.freedesktop.impl.portal.desktop.gnome', Gio.BusNameOwnerFlags.REPLACE, null, null);
    },

    AccessDialogAsync(params, invocation) {
        if (this._accessDialog) {
            invocation.return_error_literal(Gio.DBusError,
                                            Gio.DBusError.LIMITS_EXCEEDED,
                                            'Already showing a system access dialog');
            return;
        }

        let [handle, appId, parentWindow, title, subtitle, body, options] = params;
        // We probably want to use parentWindow and global.display.focus_window
        // for this check in the future
        if (appId && appId + '.desktop' != this._windowTracker.focus_app.id) {
            invocation.return_error_literal(Gio.DBusError,
                                            Gio.DBusError.ACCESS_DENIED,
                                            'Only the focused app is allowed to show a system access dialog');
            return;
        }

        let dialog = new AccessDialog(invocation, handle, title,
                                      subtitle, body, options);
        dialog.open();

        dialog.connect('closed', () => { this._accessDialog = null; });

        this._accessDialog = dialog;
    }
});
