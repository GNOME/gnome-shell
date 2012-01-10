// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Clutter = imports.gi.Clutter;
const St = imports.gi.St;
const Pango = imports.gi.Pango;
const Gio = imports.gi.Gio;
const GObject = imports.gi.GObject;
const Gcr = imports.gi.Gcr;

const ModalDialog = imports.ui.modalDialog;
const ShellEntry = imports.ui.shellEntry;
const CheckBox = imports.ui.checkBox;

let prompter = null;

const KeyringDialog = new Lang.Class({
    Name: 'KeyringDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function() {
        this.parent({ styleClass: 'prompt-dialog' });

        this.prompt = new Shell.KeyringPrompt();
        this.prompt.connect('show-password', Lang.bind(this, this._onShowPassword));
        this.prompt.connect('show-confirm', Lang.bind(this, this._onShowConfirm));
        this.prompt.connect('hide-prompt', Lang.bind(this, this._onHidePrompt));

        let mainContentBox = new St.BoxLayout({ style_class: 'prompt-dialog-main-layout',
                                                vertical: false });
        this.contentLayout.add(mainContentBox);

        let icon = new St.Icon({ icon_name: 'dialog-password-symbolic' });
        mainContentBox.add(icon,
                           { x_fill:  true,
                             y_fill:  false,
                             x_align: St.Align.END,
                             y_align: St.Align.START });

        this._messageBox = new St.BoxLayout({ style_class: 'prompt-dialog-message-layout',
                                              vertical: true });
        mainContentBox.add(this._messageBox,
                           { y_align: St.Align.START, expand: true, x_fill: true, y_fill: true });

        let subject = new St.Label({ style_class: 'prompt-dialog-headline' });
        this.prompt.bind_property('message', subject, 'text', GObject.BindingFlags.SYNC_CREATE);

        this._messageBox.add(subject,
                             { y_fill:  false,
                               y_align: St.Align.START });

        let description = new St.Label({ style_class: 'prompt-dialog-description' });
        description.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        description.clutter_text.line_wrap = true;
        this.prompt.bind_property('description', description, 'text', GObject.BindingFlags.SYNC_CREATE);
        this._messageBox.add(description,
                            { y_fill:  true,
                              y_align: St.Align.START });

        this._controlTable = null;

        let buttons = [{ label: '',
                         action: Lang.bind(this, this._onCancelButton),
                         key:    Clutter.Escape
                       },
                       { label: '',
                         action: Lang.bind(this, this._onContinueButton)
                       }]

        this.setButtons(buttons);
        this._cancelButton = buttons[0].button;
        this._continueButton = buttons[1].button;

        this.prompt.bind_property('cancel-label', this._cancelButton, 'label', GObject.BindingFlags.SYNC_CREATE);
        this.prompt.bind_property('continue-label', this._continueButton, 'label', GObject.BindingFlags.SYNC_CREATE);
    },

    _buildControlTable: function() {
        let table = new St.Table({ style_class: 'keyring-dialog-control-table' });
        let row = 0;

        if (this.prompt.password_visible) {
            let label = new St.Label(({ style_class: 'prompt-dialog-password-label' }));
            label.set_text(_("Password:"));
            table.add(label, { row: row, col: 0, x_expand: false, x_fill: true, x_align: St.Align.START });
            this._passwordEntry = new St.Entry({ style_class: 'prompt-dialog-password-entry',
                                                 text: '',
                                                 can_focus: true});
            this._passwordEntry.clutter_text.set_password_char('\u25cf'); // ● U+25CF BLACK CIRCLE
            ShellEntry.addContextMenu(this._passwordEntry, { isPassword: true });
            this._passwordEntry.clutter_text.connect('activate', Lang.bind(this, this._onPasswordActivate));
            table.add(this._passwordEntry, { row: row, col: 1, x_expand: true, x_fill: true, x_align: St.Align.START });
            row++;
        } else {
            this._passwordEntry = null;
        }

        if (this.prompt.confirm_visible) {
            var label = new St.Label(({ style_class: 'prompt-dialog-password-label' }));
            label.set_text(_("Type again:"));
            table.add(label, { row: row, col: 0, x_expand: false, x_fill: true, x_align: St.Align.START });
            this._confirmEntry = new St.Entry({ style_class: 'prompt-dialog-password-entry',
                                                text: '',
                                                can_focus: true});
            this._confirmEntry.clutter_text.set_password_char('\u25cf'); // ● U+25CF BLACK CIRCLE
            ShellEntry.addContextMenu(this._confirmEntry, { isPassword: true });
            this._confirmEntry.clutter_text.connect('activate', Lang.bind(this, this._onConfirmActivate));
            table.add(this._confirmEntry, { row: row, col: 1, x_expand: true, x_fill: true, x_align: St.Align.START });
            row++;
        } else {
            this._confirmEntry = null;
        }

        this.prompt.set_password_actor(this._passwordEntry ? this._passwordEntry.clutter_text : null);
        this.prompt.set_confirm_actor(this._confirmEntry ? this._confirmEntry.clutter_text : null);

        if (this.prompt.choice_visible) {
            let choice = new CheckBox.CheckBox();
            this.prompt.bind_property('choice-label', choice.getLabelActor(), 'text', GObject.BindingFlags.SYNC_CREATE);
            this.prompt.bind_property('choice-chosen', choice.actor, 'checked', GObject.BindingFlags.SYNC_CREATE | GObject.BindingFlags.BIDIRECTIONAL);
            table.add(choice.actor, { row: row, col: 1, x_expand: false, x_fill: true, x_align: St.Align.START });
            row++;
        }

        let warning = new St.Label({ style_class: 'prompt-dialog-error-label' });
        warning.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        warning.clutter_text.line_wrap = true;
        table.add(warning, { row: row, col: 1, x_expand: false, x_fill: false, x_align: St.Align.START });
        this.prompt.bind_property('warning-visible', warning, 'visible', GObject.BindingFlags.SYNC_CREATE);
        this.prompt.bind_property('warning', warning, 'text', GObject.BindingFlags.SYNC_CREATE);

        if (this._controlTable) {
            this._controlTable.destroy_all_children();
            this._controlTable.destroy();
        }

        this._controlTable = table;
        this._messageBox.add(table, { x_fill: true, y_fill: true });
    },

    _ensureOpen: function() {
        // NOTE: ModalDialog.open() is safe to call if the dialog is
        // already open - it just returns true without side-effects
        if (this.open())
          return true;

        // The above fail if e.g. unable to get input grab
        //
        // In an ideal world this wouldn't happen (because the
        // Shell is in complete control of the session) but that's
        // just not how things work right now.

        log('keyringPrompt: Failed to show modal dialog.' +
            ' Dismissing prompt request');
        this.prompt.cancel()
        return false;
    },

    _onShowPassword: function(prompt) {
        this._buildControlTable();
        this._ensureOpen();
        this._passwordEntry.grab_key_focus();
    },

    _onShowConfirm: function(prompt) {
        this._buildControlTable();
        this._ensureOpen();
        this._continueButton.grab_key_focus();
    },

    _onHidePrompt: function(prompt) {
        this.close();
    },

    _onPasswordActivate: function() {
        if (this.prompt.confirm_visible)
            this._confirmEntry.grab_key_focus();
        else
            this._onContinueButton();
    },

    _onConfirmActivate: function() {
        this._onContinueButton();
    },

    _onContinueButton: function() {
        this.prompt.complete()
    },

    _onCancelButton: function() {
        this.prompt.cancel()
    },
});

function init() {
    prompter = new Gcr.SystemPrompter();
    prompter.connect('new-prompt', function(prompter) {
        let dialog = new KeyringDialog();
        return dialog.prompt;
    });

    let connection = Gio.DBus.session;
    prompter.register(connection);
    Gio.bus_own_name_on_connection (connection, 'org.gnome.keyring.SystemPrompter',
                                    Gio.BusNameOwnerFlags.REPLACE, null, null);
}
