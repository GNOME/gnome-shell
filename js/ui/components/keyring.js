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

const KeyringDialog = new Lang.Class({
    Name: 'KeyringDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function() {
        this.parent({ styleClass: 'prompt-dialog' });

        this.prompt = new Shell.KeyringPrompt();
        this.prompt.connect('show-password', Lang.bind(this, this._onShowPassword));
        this.prompt.connect('show-confirm', Lang.bind(this, this._onShowConfirm));
        this.prompt.connect('prompt-close', Lang.bind(this, this._onHidePrompt));

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


        this._cancelButton = this.addButton({ label: '',
                                              action: Lang.bind(this, this._onCancelButton),
                                              key: Clutter.Escape },
                                            { expand: true, x_fill: false, x_align: St.Align.START });
        this.placeSpinner({ expand: false,
                            x_fill: false,
                            y_fill: false,
                            x_align: St.Align.END,
                            y_align: St.Align.MIDDLE });
        this._continueButton = this.addButton({ label: '',
                                                action: Lang.bind(this, this._onContinueButton),
                                                default: true },
                                              { expand: false, x_fill: false, x_align: St.Align.END });

        this.prompt.bind_property('cancel-label', this._cancelButton, 'label', GObject.BindingFlags.SYNC_CREATE);
        this.prompt.bind_property('continue-label', this._continueButton, 'label', GObject.BindingFlags.SYNC_CREATE);
    },

    _buildControlTable: function() {
        let layout = new Clutter.TableLayout();
        let table = new St.Widget({ style_class: 'keyring-dialog-control-table',
                                    layout_manager: layout });
        layout.hookup_style(table);
        let row = 0;

        if (this.prompt.password_visible) {
            let label = new St.Label({ style_class: 'prompt-dialog-password-label' });
            label.set_text(_("Password:"));
            label.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
            layout.pack(label, 0, row);
            layout.child_set(label, { x_expand: false, y_fill: false,
                                      x_align: Clutter.TableAlignment.START });
            this._passwordEntry = new St.Entry({ style_class: 'prompt-dialog-password-entry',
                                                 text: '',
                                                 can_focus: true });
            this._passwordEntry.clutter_text.set_password_char('\u25cf'); // ● U+25CF BLACK CIRCLE
            ShellEntry.addContextMenu(this._passwordEntry, { isPassword: true });
            this._passwordEntry.clutter_text.connect('activate', Lang.bind(this, this._onPasswordActivate));
            layout.pack(this._passwordEntry, 1, row);
            row++;
        } else {
            this._passwordEntry = null;
        }

        if (this.prompt.confirm_visible) {
            var label = new St.Label(({ style_class: 'prompt-dialog-password-label' }));
            label.set_text(_("Type again:"));
            layout.pack(label, 0, row);
            layout.child_set(label, { x_expand: false, y_fill: false,
                                      x_align: Clutter.TableAlignment.START });
            this._confirmEntry = new St.Entry({ style_class: 'prompt-dialog-password-entry',
                                                text: '',
                                                can_focus: true });
            this._confirmEntry.clutter_text.set_password_char('\u25cf'); // ● U+25CF BLACK CIRCLE
            ShellEntry.addContextMenu(this._confirmEntry, { isPassword: true });
            this._confirmEntry.clutter_text.connect('activate', Lang.bind(this, this._onConfirmActivate));
            layout.pack(this._confirmEntry, 1, row);
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
            layout.pack(choice.actor, 1, row);
            row++;
        }

        let warning = new St.Label({ style_class: 'prompt-dialog-error-label' });
        warning.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        warning.clutter_text.line_wrap = true;
        layout.pack(warning, 1, row);
        this.prompt.bind_property('warning-visible', warning, 'visible', GObject.BindingFlags.SYNC_CREATE);
        this.prompt.bind_property('warning', warning, 'text', GObject.BindingFlags.SYNC_CREATE);

        if (this._controlTable) {
            this._controlTable.destroy_all_children();
            this._controlTable.destroy();
        }

        this._controlTable = table;
        this._messageBox.add(table, { x_fill: true, y_fill: true });
    },

    _updateSensitivity: function(sensitive) {
        if (this._passwordEntry) {
            this._passwordEntry.reactive = sensitive;
            this._passwordEntry.clutter_text.editable = sensitive;
        }

        if (this._confirmEntry) {
            this._confirmEntry.reactive = sensitive;
            this._confirmEntry.clutter_text.editable = sensitive;
        }

        this._continueButton.can_focus = sensitive;
        this._continueButton.reactive = sensitive;
        this.setWorking(!sensitive);
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
        this._updateSensitivity(true);
        this._passwordEntry.grab_key_focus();
    },

    _onShowConfirm: function(prompt) {
        this._buildControlTable();
        this._ensureOpen();
        this._updateSensitivity(true);
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
        this._updateSensitivity(false);
        this.prompt.complete();
    },

    _onCancelButton: function() {
        this.prompt.cancel();
    },
});

const KeyringDummyDialog = new Lang.Class({
    Name: 'KeyringDummyDialog',

    _init: function() {
        this.prompt = new Shell.KeyringPrompt();
        this.prompt.connect('show-password',
                            Lang.bind(this, this._cancelPrompt));
        this.prompt.connect('show-confirm', Lang.bind(this,
                            this._cancelPrompt));
    },

    _cancelPrompt: function() {
        this.prompt.cancel();
    }
});

const KeyringPrompter = new Lang.Class({
    Name: 'KeyringPrompter',

    _init: function() {
        this._prompter = new Gcr.SystemPrompter();
        this._prompter.connect('new-prompt', Lang.bind(this,
            function() {
                let dialog = this._enabled ? new KeyringDialog()
                                           : new KeyringDummyDialog();
                this._currentPrompt = dialog.prompt;
                return this._currentPrompt;
            }));
        this._dbusId = null;
        this._registered = false;
        this._enabled = false;
        this._currentPrompt = null;
    },

    enable: function() {
        if (!this._registered) {
            this._prompter.register(Gio.DBus.session);
            this._dbusId = Gio.DBus.session.own_name('org.gnome.keyring.SystemPrompter',
                                                     Gio.BusNameOwnerFlags.ALLOW_REPLACEMENT, null, null);
            this._registered = true;
        }
        this._enabled = true;
    },

    disable: function() {
        this._enabled = false;

        if (this._prompter.prompting)
            this._currentPrompt.cancel();
        this._currentPrompt = null;
    }
});

const Component = KeyringPrompter;
