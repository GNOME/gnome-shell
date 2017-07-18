// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Lang = imports.lang;
const Shell = imports.gi.Shell;
const Clutter = imports.gi.Clutter;
const St = imports.gi.St;
const Pango = imports.gi.Pango;
const Gio = imports.gi.Gio;
const GObject = imports.gi.GObject;
const Gcr = imports.gi.Gcr;

const Animation = imports.ui.animation;
const Dialog = imports.ui.dialog;
const ModalDialog = imports.ui.modalDialog;
const ShellEntry = imports.ui.shellEntry;
const CheckBox = imports.ui.checkBox;
const Tweener = imports.ui.tweener;

var WORK_SPINNER_ICON_SIZE = 16;
var WORK_SPINNER_ANIMATION_DELAY = 1.0;
var WORK_SPINNER_ANIMATION_TIME = 0.3;

var KeyringDialog = new Lang.Class({
    Name: 'KeyringDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function() {
        this.parent({ styleClass: 'prompt-dialog' });

        this.prompt = new Shell.KeyringPrompt();
        this.prompt.connect('show-password', Lang.bind(this, this._onShowPassword));
        this.prompt.connect('show-confirm', Lang.bind(this, this._onShowConfirm));
        this.prompt.connect('prompt-close', Lang.bind(this, this._onHidePrompt));

        let icon = new Gio.ThemedIcon({ name: 'dialog-password-symbolic' });
        this._content = new Dialog.MessageDialogContent({ icon });
        this.contentLayout.add(this._content);

        // FIXME: Why does this break now?
        /*
        this.prompt.bind_property('message', this._content, 'title', GObject.BindingFlags.SYNC_CREATE);
        this.prompt.bind_property('description', this._content, 'body', GObject.BindingFlags.SYNC_CREATE);
        */
        this.prompt.connect('notify::message', () => {
            this._content.title = this.prompt.message;
        });
        this._content.title = this.prompt.message;

        this.prompt.connect('notify::description', () => {
            this._content.body = this.prompt.description;
        });
        this._content.body = this.prompt.description;

        this._workSpinner = null;
        this._controlTable = null;

        this._cancelButton = this.addButton({ label: '',
                                              action: Lang.bind(this, this._onCancelButton),
                                              key: Clutter.Escape });
        this._continueButton = this.addButton({ label: '',
                                                action: Lang.bind(this, this._onContinueButton),
                                                default: true });

        this.prompt.bind_property('cancel-label', this._cancelButton, 'label', GObject.BindingFlags.SYNC_CREATE);
        this.prompt.bind_property('continue-label', this._continueButton, 'label', GObject.BindingFlags.SYNC_CREATE);
    },

    _setWorking: function(working) {
        if (!this._workSpinner)
            return;

        Tweener.removeTweens(this._workSpinner.actor);
        if (working) {
            this._workSpinner.play();
            Tweener.addTween(this._workSpinner.actor,
                             { opacity: 255,
                               delay: WORK_SPINNER_ANIMATION_DELAY,
                               time: WORK_SPINNER_ANIMATION_TIME,
                               transition: 'linear'
                             });
        } else {
            Tweener.addTween(this._workSpinner.actor,
                             { opacity: 0,
                               time: WORK_SPINNER_ANIMATION_TIME,
                               transition: 'linear',
                               onCompleteScope: this,
                               onComplete: function() {
                                   if (this._workSpinner)
                                       this._workSpinner.stop();
                               }
                             });
        }
    },

    _buildControlTable: function() {
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
            this._passwordEntry.clutter_text.connect('activate', Lang.bind(this, this._onPasswordActivate));

            let spinnerIcon = Gio.File.new_for_uri('resource:///org/gnome/shell/theme/process-working.svg');
            this._workSpinner = new Animation.AnimatedIcon(spinnerIcon, WORK_SPINNER_ICON_SIZE);
            this._workSpinner.actor.opacity = 0;

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
            this._confirmEntry.clutter_text.connect('activate', Lang.bind(this, this._onConfirmActivate));
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
        this._setWorking(!sensitive);
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

var KeyringDummyDialog = new Lang.Class({
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

var KeyringPrompter = new Lang.Class({
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

var Component = KeyringPrompter;
