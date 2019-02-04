// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Gcr, Gio, GObject, Pango, Shell, St } = imports.gi;

const Animation = imports.ui.animation;
const Dialog = imports.ui.dialog;
const ModalDialog = imports.ui.modalDialog;
const ShellEntry = imports.ui.shellEntry;
const CheckBox = imports.ui.checkBox;

var WORK_SPINNER_ICON_SIZE = 16;

var KeyringDialog = GObject.registerClass(
class KeyringDialog extends ModalDialog.ModalDialog {
    _init() {
        super._init({ styleClass: 'prompt-dialog' });

        this.prompt = new Shell.KeyringPrompt();
        this.prompt.connect('show-password', this._onShowPassword.bind(this));
        this.prompt.connect('show-confirm', this._onShowConfirm.bind(this));
        this.prompt.connect('prompt-close', this._onHidePrompt.bind(this));

        let icon = new Gio.ThemedIcon({ name: 'dialog-password-symbolic' });
        this._content = new Dialog.MessageDialogContent({ icon });
        this.contentLayout.add(this._content);

        this.prompt.bind_property('message', this._content, 'title', GObject.BindingFlags.SYNC_CREATE);
        this.prompt.bind_property('description', this._content, 'body', GObject.BindingFlags.SYNC_CREATE);

        this._workSpinner = null;
        this._controlTable = null;

        this._cancelButton = this.addButton({ label: '',
                                              action: this._onCancelButton.bind(this),
                                              key: Clutter.Escape });
        this._continueButton = this.addButton({ label: '',
                                                action: this._onContinueButton.bind(this),
                                                default: true });

        this.prompt.bind_property('cancel-label', this._cancelButton, 'label', GObject.BindingFlags.SYNC_CREATE);
        this.prompt.bind_property('continue-label', this._continueButton, 'label', GObject.BindingFlags.SYNC_CREATE);
    }

    _setWorking(working) {
        if (!this._workSpinner)
            return;

        if (working)
            this._workSpinner.play();
        else
            this._workSpinner.stop();
    }

    _buildControlTable() {
        let layout = new Clutter.GridLayout({ orientation: Clutter.Orientation.VERTICAL });
        let table = new St.Widget({ style_class: 'keyring-dialog-control-table',
                                    layout_manager: layout });
        layout.hookup_style(table);
        let rtl = table.get_text_direction() == Clutter.TextDirection.RTL;
        let row = 0;

        if (this.prompt.password_visible) {
            let label = new St.Label({ style_class: 'prompt-dialog-password-label',
                                       x_align: Clutter.ActorAlign.START,
                                       y_align: Clutter.ActorAlign.CENTER });
            label.set_text(_("Password:"));
            label.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
            this._passwordEntry = new St.Entry({ style_class: 'prompt-dialog-password-entry',
                                                 text: '',
                                                 can_focus: true,
                                                 x_expand: true });
            this._passwordEntry.clutter_text.set_password_char('\u25cf'); // ● U+25CF BLACK CIRCLE
            ShellEntry.addContextMenu(this._passwordEntry, { isPassword: true });
            this._passwordEntry.clutter_text.connect('activate', this._onPasswordActivate.bind(this));

            this._workSpinner = new Animation.Spinner(WORK_SPINNER_ICON_SIZE, true);

            if (rtl) {
                layout.attach(this._workSpinner.actor, 0, row, 1, 1);
                layout.attach(this._passwordEntry, 1, row, 1, 1);
                layout.attach(label, 2, row, 1, 1);
            } else {
                layout.attach(label, 0, row, 1, 1);
                layout.attach(this._passwordEntry, 1, row, 1, 1);
                layout.attach(this._workSpinner.actor, 2, row, 1, 1);
            }
            row++;
        } else {
            this._workSpinner = null;
            this._passwordEntry = null;
        }

        if (this.prompt.confirm_visible) {
            var label = new St.Label(({ style_class: 'prompt-dialog-password-label',
                                        x_align: Clutter.ActorAlign.START,
                                        y_align: Clutter.ActorAlign.CENTER }));
            label.set_text(_("Type again:"));
            this._confirmEntry = new St.Entry({ style_class: 'prompt-dialog-password-entry',
                                                text: '',
                                                can_focus: true,
                                                x_expand: true });
            this._confirmEntry.clutter_text.set_password_char('\u25cf'); // ● U+25CF BLACK CIRCLE
            ShellEntry.addContextMenu(this._confirmEntry, { isPassword: true });
            this._confirmEntry.clutter_text.connect('activate', this._onConfirmActivate.bind(this));
            if (rtl) {
                layout.attach(this._confirmEntry, 0, row, 1, 1);
                layout.attach(label, 1, row, 1, 1);
            } else {
                layout.attach(label, 0, row, 1, 1);
                layout.attach(this._confirmEntry, 1, row, 1, 1);
            }
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
            layout.attach(choice.actor, rtl ? 0 : 1, row, 1, 1);
            row++;
        }

        let warning = new St.Label({ style_class: 'prompt-dialog-error-label',
                                     x_align: Clutter.ActorAlign.START });
        warning.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        warning.clutter_text.line_wrap = true;
        layout.attach(warning, rtl ? 0 : 1, row, 1, 1);
        this.prompt.bind_property('warning-visible', warning, 'visible', GObject.BindingFlags.SYNC_CREATE);
        this.prompt.bind_property('warning', warning, 'text', GObject.BindingFlags.SYNC_CREATE);

        if (this._controlTable) {
            this._controlTable.destroy_all_children();
            this._controlTable.destroy();
        }

        this._controlTable = table;
        this._content.messageBox.add(table, { x_fill: true, y_fill: true });
    }

    _updateSensitivity(sensitive) {
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
        this._setWorking(!sensitive);
    }

    _ensureOpen() {
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
        this.prompt.cancel();
        return false;
    }

    _onShowPassword() {
        this._buildControlTable();
        this._ensureOpen();
        this._updateSensitivity(true);
        this._passwordEntry.grab_key_focus();
    }

    _onShowConfirm() {
        this._buildControlTable();
        this._ensureOpen();
        this._updateSensitivity(true);
        this._continueButton.grab_key_focus();
    }

    _onHidePrompt() {
        this.close();
    }

    _onPasswordActivate() {
        if (this.prompt.confirm_visible)
            this._confirmEntry.grab_key_focus();
        else
            this._onContinueButton();
    }

    _onConfirmActivate() {
        this._onContinueButton();
    }

    _onContinueButton() {
        this._updateSensitivity(false);
        this.prompt.complete();
    }

    _onCancelButton() {
        this.prompt.cancel();
    }
});

var KeyringDummyDialog = class {
    constructor() {
        this.prompt = new Shell.KeyringPrompt();
        this.prompt.connect('show-password', this._cancelPrompt.bind(this));
        this.prompt.connect('show-confirm', this._cancelPrompt.bind(this));
    }

    _cancelPrompt() {
        this.prompt.cancel();
    }
};

var KeyringPrompter = class {
    constructor() {
        this._prompter = new Gcr.SystemPrompter();
        this._prompter.connect('new-prompt', () => {
            let dialog = this._enabled ? new KeyringDialog()
                                       : new KeyringDummyDialog();
            this._currentPrompt = dialog.prompt;
            return this._currentPrompt;
        });
        this._dbusId = null;
        this._registered = false;
        this._enabled = false;
        this._currentPrompt = null;
    }

    enable() {
        if (!this._registered) {
            this._prompter.register(Gio.DBus.session);
            this._dbusId = Gio.DBus.session.own_name('org.gnome.keyring.SystemPrompter',
                                                     Gio.BusNameOwnerFlags.ALLOW_REPLACEMENT, null, null);
            this._registered = true;
        }
        this._enabled = true;
    }

    disable() {
        this._enabled = false;

        if (this._prompter.prompting)
            this._currentPrompt.cancel();
        this._currentPrompt = null;
    }
};

var Component = KeyringPrompter;
