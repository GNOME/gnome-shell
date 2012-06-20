// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Signals = imports.signals;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Pango = imports.gi.Pango;
const St = imports.gi.St;
const Shell = imports.gi.Shell;

const CheckBox = imports.ui.checkBox;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const ModalDialog = imports.ui.modalDialog;
const Params = imports.misc.params;
const ShellEntry = imports.ui.shellEntry;

const LIST_ITEM_ICON_SIZE = 48;

/* ------ Common Utils ------- */
function _setLabelText(label, text) {
    if (text) {
        label.set_text(text);
        label.show();
    } else {
        label.set_text('');
        label.hide();
    }
}

function _setButtonsForChoices(dialog, choices) {
    let buttons = [];

    for (let idx = 0; idx < choices.length; idx++) {
        let button = idx;
        buttons.unshift({ label: choices[idx],
                          action: Lang.bind(dialog, function() {
                              dialog.emit('response', button);
                          })});
    }

    dialog.setButtons(buttons);
}

function _setLabelsForMessage(dialog, message) {
    let labels = message.split('\n');

    _setLabelText(dialog.subjectLabel, labels[0]);
    if (labels.length > 1)
        _setLabelText(dialog.descriptionLabel, labels[1]);
}

function _createIcon(gicon) {
    return new St.Icon({ gicon: gicon,
                         style_class: 'shell-mount-operation-icon' })
}

/* -------------------------------------------------------- */

const ListItem = new Lang.Class({
    Name: 'ListItem',

    _init: function(app) {
        this._app = app;

        let layout = new St.BoxLayout({ vertical: false});

        this.actor = new St.Button({ style_class: 'show-processes-dialog-app-list-item',
                                     can_focus: true,
                                     child: layout,
                                     reactive: true,
                                     x_align: St.Align.START,
                                     x_fill: true });

        this._icon = this._app.create_icon_texture(LIST_ITEM_ICON_SIZE);

        let iconBin = new St.Bin({ style_class: 'show-processes-dialog-app-list-item-icon',
                                   child: this._icon });
        layout.add(iconBin);

        this._nameLabel = new St.Label({ text: this._app.get_name(),
                                         style_class: 'show-processes-dialog-app-list-item-name' });
        let labelBin = new St.Bin({ y_align: St.Align.MIDDLE,
                                    child: this._nameLabel });
        layout.add(labelBin);

        this.actor.connect('clicked', Lang.bind(this, this._onClicked));
    },

    _onClicked: function() {
        this.emit('activate');
        this._app.activate();
    }
});
Signals.addSignalMethods(ListItem.prototype);

const ShellMountOperation = new Lang.Class({
    Name: 'ShellMountOperation',

    _init: function(source, params) {
        params = Params.parse(params, { existingDialog: null });

        this._dialog = null;
        this._dialogId = 0;
        this._existingDialog = params.existingDialog;
        this._processesDialog = null;

        this.mountOp = new Shell.MountOperation();

        this.mountOp.connect('ask-question',
                             Lang.bind(this, this._onAskQuestion));
        this.mountOp.connect('ask-password',
                             Lang.bind(this, this._onAskPassword));
        this.mountOp.connect('show-processes-2',
                             Lang.bind(this, this._onShowProcesses2));
        this.mountOp.connect('aborted',
                             Lang.bind(this, this.close));

        this._gicon = source.get_icon();
    },

    _closeExistingDialog: function() {
        if (!this._existingDialog)
            return;

        this._existingDialog.close();
        this._existingDialog = null;
    },

    _onAskQuestion: function(op, message, choices) {
        this._closeExistingDialog();
        this._dialog = new ShellMountQuestionDialog(this._gicon);

        this._dialogId = this._dialog.connect('response',
                               Lang.bind(this, function(object, choice) {
                                   this.mountOp.set_choice(choice);
                                   this.mountOp.reply(Gio.MountOperationResult.HANDLED);

                                   this.close();
                               }));

        this._dialog.update(message, choices);
        this._dialog.open();
    },

    _onAskPassword: function(op, message) {
        if (this._existingDialog) {
            this._dialog = this._existingDialog;
            this._dialog.reaskPassword();
        } else {
            this._dialog = new ShellMountPasswordDialog(message, this._gicon);
        }

        this._dialogId = this._dialog.connect('response', Lang.bind(this,
            function(object, choice, password, remember) {
                if (choice == '-1') {
                    this.mountOp.reply(Gio.MountOperationResult.ABORTED);
                } else {
                    if (remember)
                        this.mountOp.set_password_save(Gio.PasswordSave.PERMANENTLY);
                    else
                        this.mountOp.set_password_save(Gio.PasswordSave.NEVER);

                    this.mountOp.set_password(password);
                    this.mountOp.reply(Gio.MountOperationResult.HANDLED);
                }
            }));
        this._dialog.open();
    },

    close: function(op) {
        this._closeExistingDialog();
        this._processesDialog = null;

        if (this._dialog) {
            this._dialog.close();
            this._dialog = null;
        }
    },

    _onShowProcesses2: function(op) {
        this._closeExistingDialog();

        let processes = op.get_show_processes_pids();
        let choices = op.get_show_processes_choices();
        let message = op.get_show_processes_message();

        if (!this._processesDialog) {
            this._processesDialog = new ShellProcessesDialog(this._gicon);
            this._dialog = this._processesDialog;

            this._dialogId = this._processesDialog.connect('response',
                                          Lang.bind(this, function(object, choice) {
                                              if (choice == -1) {
                                                  this.mountOp.reply(Gio.MountOperationResult.ABORTED);
                                              } else {
                                                  this.mountOp.set_choice(choice);
                                                  this.mountOp.reply(Gio.MountOperationResult.HANDLED);
                                              }

                                              this.close();
                                          }));
            this._processesDialog.open();
        }

        this._processesDialog.update(message, processes, choices);
    },

    borrowDialog: function() {
        if (this._dialogId != 0) {
            this._dialog.disconnect(this._dialogId);
            this._dialogId = 0;
        }

        return this._dialog;
    }
});

const ShellMountQuestionDialog = new Lang.Class({
    Name: 'ShellMountQuestionDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function(gicon) {
        this.parent({ styleClass: 'mount-question-dialog' });

        let mainContentLayout = new St.BoxLayout();
        this.contentLayout.add(mainContentLayout, { x_fill: true,
                                                    y_fill: false });

        this._iconBin = new St.Bin({ child: _createIcon(gicon) });
        mainContentLayout.add(this._iconBin,
                              { x_fill:  true,
                                y_fill:  false,
                                x_align: St.Align.END,
                                y_align: St.Align.MIDDLE });

        let messageLayout = new St.BoxLayout({ vertical: true });
        mainContentLayout.add(messageLayout,
                              { y_align: St.Align.START });

        this.subjectLabel = new St.Label({ style_class: 'mount-question-dialog-subject' });
        this.subjectLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this.subjectLabel.clutter_text.line_wrap = true;

        messageLayout.add(this.subjectLabel,
                          { y_fill:  false,
                            y_align: St.Align.START });

        this.descriptionLabel = new St.Label({ style_class: 'mount-question-dialog-description' });
        this.descriptionLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this.descriptionLabel.clutter_text.line_wrap = true;

        messageLayout.add(this.descriptionLabel,
                          { y_fill:  true,
                            y_align: St.Align.START });
    },

    update: function(message, choices) {
        _setLabelsForMessage(this, message);
        _setButtonsForChoices(this, choices);
    }
});
Signals.addSignalMethods(ShellMountQuestionDialog.prototype);

const ShellMountPasswordDialog = new Lang.Class({
    Name: 'ShellMountPasswordDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function(message, gicon) {
        let strings = message.split('\n');
        this.parent({ styleClass: 'prompt-dialog' });

        let mainContentBox = new St.BoxLayout({ style_class: 'prompt-dialog-main-layout',
                                                vertical: false });
        this.contentLayout.add(mainContentBox);

        let icon = _createIcon(gicon);
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
        this._messageBox.add(subject,
                             { y_fill:  false,
                               y_align: St.Align.START });
        if (strings[0])
            subject.set_text(strings[0]);

        let description = new St.Label({ style_class: 'prompt-dialog-description' });
        description.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        description.clutter_text.line_wrap = true;
        this._messageBox.add(description,
                            { y_fill:  true,
                              y_align: St.Align.START });
        if (strings[1])
            description.set_text(strings[1]);

        this._passwordBox = new St.BoxLayout({ vertical: false });
        this._messageBox.add(this._passwordBox);

        this._passwordLabel = new St.Label(({ style_class: 'prompt-dialog-password-label',
                                              text: _("Passphrase") }));
        this._passwordBox.add(this._passwordLabel);

        this._passwordEntry = new St.Entry({ style_class: 'prompt-dialog-password-entry',
                                             text: "",
                                             can_focus: true});
        ShellEntry.addContextMenu(this._passwordEntry, { isPassword: true });
        this._passwordEntry.clutter_text.connect('activate', Lang.bind(this, this._onEntryActivate));
        this._passwordEntry.clutter_text.set_password_char('\u25cf'); // ● U+25CF BLACK CIRCLE
        this._passwordBox.add(this._passwordEntry, {expand: true });
        this.setInitialKeyFocus(this._passwordEntry);

        this._errorMessageLabel = new St.Label({ style_class: 'prompt-dialog-error-label',
                                                 text: _("Sorry, that didn\'t work. Please try again.") });
        this._errorMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._errorMessageLabel.clutter_text.line_wrap = true;
        this._errorMessageLabel.hide();
        this._messageBox.add(this._errorMessageLabel);

        this._rememberChoice = new CheckBox.CheckBox();
        this._rememberChoice.getLabelActor().text = _("Remember Passphrase");
        this._rememberChoice.actor.checked = true;
        this._messageBox.add(this._rememberChoice.actor);

        let buttons = [{ label: _("Cancel"),
                         action: Lang.bind(this, this._onCancelButton),
                         key:    Clutter.Escape
                       },
                       { label: _("Unlock"),
                         action: Lang.bind(this, this._onUnlockButton)
                       }];

        this.setButtons(buttons);
    },

    reaskPassword: function() {
        this._passwordEntry.set_text('');
        this._errorMessageLabel.show();
    },

    _onCancelButton: function() {
        this.emit('response', -1, '', false);
    },

    _onUnlockButton: function() {
        this._onEntryActivate();
    },

    _onEntryActivate: function() {
        this.emit('response', 1,
            this._passwordEntry.get_text(),
            this._rememberChoice.actor.checked);
    }
});

const ShellProcessesDialog = new Lang.Class({
    Name: 'ShellProcessesDialog',
    Extends: ModalDialog.ModalDialog,

    _init: function(gicon) {
        this.parent({ styleClass: 'show-processes-dialog' });

        let mainContentLayout = new St.BoxLayout();
        this.contentLayout.add(mainContentLayout, { x_fill: true,
                                                    y_fill: false });

        this._iconBin = new St.Bin({ child: _createIcon(gicon) });
        mainContentLayout.add(this._iconBin,
                              { x_fill:  true,
                                y_fill:  false,
                                x_align: St.Align.END,
                                y_align: St.Align.MIDDLE });

        let messageLayout = new St.BoxLayout({ vertical: true });
        mainContentLayout.add(messageLayout,
                              { y_align: St.Align.START });

        this.subjectLabel = new St.Label({ style_class: 'show-processes-dialog-subject' });

        messageLayout.add(this.subjectLabel,
                          { y_fill:  false,
                            y_align: St.Align.START });

        this.descriptionLabel = new St.Label({ style_class: 'show-processes-dialog-description' });
        this.descriptionLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this.descriptionLabel.clutter_text.line_wrap = true;

        messageLayout.add(this.descriptionLabel,
                          { y_fill:  true,
                            y_align: St.Align.START });

        let scrollView = new St.ScrollView({ style_class: 'show-processes-dialog-app-list'});
        scrollView.set_policy(Gtk.PolicyType.NEVER,
                              Gtk.PolicyType.AUTOMATIC);
        this.contentLayout.add(scrollView,
                               { x_fill: true,
                                 y_fill: true });
        scrollView.hide();

        this._applicationList = new St.BoxLayout({ vertical: true });
        scrollView.add_actor(this._applicationList,
                             { x_fill:  true,
                               y_fill:  true,
                               x_align: St.Align.START,
                               y_align: St.Align.MIDDLE });

        this._applicationList.connect('actor-added',
                                      Lang.bind(this, function() {
                                          if (this._applicationList.get_n_children() == 1)
                                              scrollView.show();
                                      }));

        this._applicationList.connect('actor-removed',
                                      Lang.bind(this, function() {
                                          if (this._applicationList.get_n_children() == 0)
                                              scrollView.hide();
                                      }));
    },

    _setAppsForPids: function(pids) {
        // remove all the items
        this._applicationList.destroy_all_children();

        pids.forEach(Lang.bind(this, function(pid) {
            let tracker = Shell.WindowTracker.get_default();
            let app = tracker.get_app_from_pid(pid);

            if (!app)
                return;

            let item = new ListItem(app);
            this._applicationList.add(item.actor, { x_fill: true });

            item.connect('activate',
                         Lang.bind(this, function() {
                             // use -1 to indicate Cancel
                             this.emit('response', -1);
                         }));
        }));
    },

    update: function(message, processes, choices) {
        this._setAppsForPids(processes);
        _setLabelsForMessage(this, message);
        _setButtonsForChoices(this, choices);
    }
});
Signals.addSignalMethods(ShellProcessesDialog.prototype);
