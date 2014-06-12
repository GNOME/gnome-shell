// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Clutter = imports.gi.Clutter;
const Lang = imports.lang;
const Signals = imports.signals;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
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

const REMEMBER_MOUNT_PASSWORD_KEY = 'remember-mount-password';

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
        this.mountOp.connect('show-unmount-progress',
                             Lang.bind(this, this._onShowUnmountProgress));

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

        this._dialogId = this._dialog.connect('response', Lang.bind(this,
            function(object, choice) {
                this.mountOp.set_choice(choice);
                this.mountOp.reply(Gio.MountOperationResult.HANDLED);

                this.close();
            }));

        this._dialog.update(message, choices);
        this._dialog.open();
    },

    _onAskPassword: function(op, message, defaultUser, defaultDomain, flags) {
        if (this._existingDialog) {
            this._dialog = this._existingDialog;
            this._dialog.reaskPassword();
        } else {
            this._dialog = new ShellMountPasswordDialog(message, this._gicon, flags);
        }

        this._dialogId = this._dialog.connect('response', Lang.bind(this,
            function(object, choice, password, remember) {
                if (choice == -1) {
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

        if (this._notifier) {
            this._notifier.done();
            this._notifier = null;
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

            this._dialogId = this._processesDialog.connect('response', Lang.bind(this,
                function(object, choice) {
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

    _onShowUnmountProgress: function(op, message, timeLeft, bytesLeft) {
        if (!this._notifier)
            this._notifier = new ShellUnmountNotifier();
            
        if (bytesLeft == 0)
            this._notifier.done(message);
        else
            this._notifier.show(message);
    },

    borrowDialog: function() {
        if (this._dialogId != 0) {
            this._dialog.disconnect(this._dialogId);
            this._dialogId = 0;
        }

        return this._dialog;
    }
});

const ShellUnmountNotifier = new Lang.Class({
    Name: 'ShellUnmountNotifier',
    Extends: MessageTray.Source,

    _init: function() {
        this.parent('', 'media-removable');

        this._notification = null;
        Main.messageTray.add(this);
    },

    show: function(message) {
        let [header, text] = message.split('\n', 2);

        if (!this._notification) {
            this._notification = new MessageTray.Notification(this, header, text);
            this._notification.setUrgency(MessageTray.Urgency.CRITICAL);
        } else {
            this._notification.update(header, text);
        }

        this.notify(this._notification);
    },

    done: function(message) {
        if (this._notification) {
            this._notification.destroy();
            this._notification = null;
        }

        if (message) {
            let notification = new MessageTray.Notification(this, message, null);

            this.notify(notification);
        }
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

    _init: function(message, gicon, flags) {
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

        this._passwordBox = new St.BoxLayout({ vertical: false, style_class: 'prompt-dialog-password-box' });
        this._messageBox.add(this._passwordBox);

        this._passwordLabel = new St.Label(({ style_class: 'prompt-dialog-password-label',
                                              text: _("Password") }));
        this._passwordBox.add(this._passwordLabel, { y_fill: false, y_align: St.Align.MIDDLE });

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

        if (flags & Gio.AskPasswordFlags.SAVING_SUPPORTED) {
            this._rememberChoice = new CheckBox.CheckBox();
            this._rememberChoice.getLabelActor().text = _("Remember Password");
            this._rememberChoice.actor.checked =
                global.settings.get_boolean(REMEMBER_MOUNT_PASSWORD_KEY);
            this._messageBox.add(this._rememberChoice.actor);
        } else {
            this._rememberChoice = null;
        }

        let buttons = [{ label: _("Cancel"),
                         action: Lang.bind(this, this._onCancelButton),
                         key:    Clutter.Escape
                       },
                       { label: _("Unlock"),
                         action: Lang.bind(this, this._onUnlockButton),
                         default: true
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
        global.settings.set_boolean(REMEMBER_MOUNT_PASSWORD_KEY,
            this._rememberChoice && this._rememberChoice.actor.checked);
        this.emit('response', 1,
            this._passwordEntry.get_text(),
            this._rememberChoice &&
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
        scrollView.add_actor(this._applicationList);

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

const GnomeShellMountOpIface = '<node> \
<interface name="org.Gtk.MountOperationHandler"> \
<method name="AskPassword"> \
    <arg type="s" direction="in" name="object_id"/> \
    <arg type="s" direction="in" name="message"/> \
    <arg type="s" direction="in" name="icon_name"/> \
    <arg type="s" direction="in" name="default_user"/> \
    <arg type="s" direction="in" name="default_domain"/> \
    <arg type="u" direction="in" name="flags"/> \
    <arg type="u" direction="out" name="response"/> \
    <arg type="a{sv}" direction="out" name="response_details"/> \
</method> \
<method name="AskQuestion"> \
    <arg type="s" direction="in" name="object_id"/> \
    <arg type="s" direction="in" name="message"/> \
    <arg type="s" direction="in" name="icon_name"/> \
    <arg type="as" direction="in" name="choices"/> \
    <arg type="u" direction="out" name="response"/> \
    <arg type="a{sv}" direction="out" name="response_details"/> \
</method> \
<method name="ShowProcesses"> \
    <arg type="s" direction="in" name="object_id"/> \
    <arg type="s" direction="in" name="message"/> \
    <arg type="s" direction="in" name="icon_name"/> \
    <arg type="ai" direction="in" name="application_pids"/> \
    <arg type="as" direction="in" name="choices"/> \
    <arg type="u" direction="out" name="response"/> \
    <arg type="a{sv}" direction="out" name="response_details"/> \
</method> \
<method name="Close"/> \
</interface> \
</node>';

const ShellMountOperationType = {
    NONE: 0,
    ASK_PASSWORD: 1,
    ASK_QUESTION: 2,
    SHOW_PROCESSES: 3
};

const GnomeShellMountOpHandler = new Lang.Class({
    Name: 'GnomeShellMountOpHandler',

    _init: function() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(GnomeShellMountOpIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gtk/MountOperationHandler');
        Gio.bus_own_name_on_connection(Gio.DBus.session, 'org.gtk.MountOperationHandler',
                                       Gio.BusNameOwnerFlags.REPLACE, null, null);

        this._dialog = null;
        this._volumeMonitor = Gio.VolumeMonitor.get();

        this._ensureEmptyRequest();
    },

    _ensureEmptyRequest: function() {
        this._currentId = null;
        this._currentInvocation = null;
        this._currentType = ShellMountOperationType.NONE;
    },

    _clearCurrentRequest: function(response, details) {
        if (this._currentInvocation) {
            this._currentInvocation.return_value(
                GLib.Variant.new('(ua{sv})', [response, details]));
        }

        this._ensureEmptyRequest();
    },

    _setCurrentRequest: function(invocation, id, type) {
        let oldId = this._currentId;
        let oldType = this._currentType;
        let requestId = id + '@' + invocation.get_sender();

        this._clearCurrentRequest(Gio.MountOperationResult.UNHANDLED, {});

        this._currentInvocation = invocation;
        this._currentId = requestId;
        this._currentType = type;

        if (this._dialog && (oldId == requestId) && (oldType == type))
            return true;

        return false;
    },

    _closeDialog: function() {
        if (this._dialog) {
            this._dialog.close();
            this._dialog = null;
        }
    },

    _createGIcon: function(iconName) {
        let realIconName = iconName ? iconName : 'drive-harddisk';
        return new Gio.ThemedIcon({ name: realIconName,
                                    use_default_fallbacks: true });
    },

    /**
     * AskPassword:
     * @id: an opaque ID identifying the object for which the operation is requested
     *      The ID must be unique in the context of the calling process.
     * @message: the message to display
     * @icon_name: the name of an icon to display
     * @default_user: the default username for display
     * @default_domain: the default domain for display
     * @flags: a set of GAskPasswordFlags
     * @response: a GMountOperationResult
     * @response_details: a dictionary containing the response details as
     * entered by the user. The dictionary MAY contain the following properties:
     *   - "password" -> (s): a password to be used to complete the mount operation
     *   - "password_save" -> (u): a GPasswordSave
     *
     * The dialog will stay visible until clients call the Close() method, or
     * another dialog becomes visible.
     * Calling AskPassword again for the same id will have the effect to clear
     * the existing dialog and update it with a message indicating the previous
     * attempt went wrong.
     */
    AskPasswordAsync: function(params, invocation) {
        let [id, message, iconName, defaultUser, defaultDomain, flags] = params;

        if (this._setCurrentRequest(invocation, id, ShellMountOperationType.ASK_PASSWORD)) {
            this._dialog.reaskPassword();
            return;
        }

        this._closeDialog();

        this._dialog = new ShellMountPasswordDialog(message, this._createGIcon(iconName), flags);
        this._dialog.connect('response', Lang.bind(this,
            function(object, choice, password, remember) {
                let details = {};
                let response;

                if (choice == -1) {
                    response = Gio.MountOperationResult.ABORTED;
                } else {
                    response = Gio.MountOperationResult.HANDLED;

                    let passSave = remember ? Gio.PasswordSave.PERMANENTLY : Gio.PasswordSave.NEVER;
                    details['password_save'] = GLib.Variant.new('u', passSave);
                    details['password'] = GLib.Variant.new('s', password);
                }

                this._clearCurrentRequest(response, details);
            }));
        this._dialog.open();
    },

    /**
     * AskQuestion:
     * @id: an opaque ID identifying the object for which the operation is requested
     *      The ID must be unique in the context of the calling process.
     * @message: the message to display
     * @icon_name: the name of an icon to display
     * @choices: an array of choice strings
     * GetResponse:
     * @response: a GMountOperationResult
     * @response_details: a dictionary containing the response details as
     * entered by the user. The dictionary MAY contain the following properties:
     *   - "choice" -> (i): the chosen answer among the array of strings passed in
     *
     * The dialog will stay visible until clients call the Close() method, or
     * another dialog becomes visible.
     * Calling AskQuestion again for the same id will have the effect to clear
     * update the dialog with the new question.
     */
    AskQuestionAsync: function(params, invocation) {
        let [id, message, iconName, choices] = params;

        if (this._setCurrentRequest(invocation, id, ShellMountOperationType.ASK_QUESTION)) {
            this._dialog.update(message, choices);
            return;
        }

        this._closeDialog();

        this._dialog = new ShellMountQuestionDialog(this._createGIcon(iconName), message);
        this._dialog.connect('response', Lang.bind(this,
            function(object, choice) {
                this._clearCurrentRequest(Gio.MountOperationResult.HANDLED,
                                          { choice: GLib.Variant.new('i', choice) });
            }));

        this._dialog.update(message, choices);
        this._dialog.open();
    },

    /**
     * ShowProcesses:
     * @id: an opaque ID identifying the object for which the operation is requested
     *      The ID must be unique in the context of the calling process.
     * @message: the message to display
     * @icon_name: the name of an icon to display
     * @application_pids: the PIDs of the applications to display
     * @choices: an array of choice strings
     * @response: a GMountOperationResult
     * @response_details: a dictionary containing the response details as
     * entered by the user. The dictionary MAY contain the following properties:
     *   - "choice" -> (i): the chosen answer among the array of strings passed in
     *
     * The dialog will stay visible until clients call the Close() method, or
     * another dialog becomes visible.
     * Calling ShowProcesses again for the same id will have the effect to clear
     * the existing dialog and update it with the new message and the new list
     * of processes.
     */
    ShowProcessesAsync: function(params, invocation) {
        let [id, message, iconName, applicationPids, choices] = params;

        if (this._setCurrentRequest(invocation, id, ShellMountOperationType.SHOW_PROCESSES)) {
            this._dialog.update(message, applicationPids, choices);
            return;
        }

        this._closeDialog();

        this._dialog = new ShellProcessesDialog(this._createGIcon(iconName));
        this._dialog.connect('response', Lang.bind(this,
            function(object, choice) {
                let response;
                let details = {};

                if (choice == -1) {
                    response = Gio.MountOperationResult.ABORTED;
                } else {
                    response = Gio.MountOperationResult.HANDLED;
                    details['choice'] = GLib.Variant.new('i', choice);
                }

                this._clearCurrentRequest(response, details);
            }));

        this._dialog.update(message, applicationPids, choices);
        this._dialog.open();
    },

    /**
     * Close:
     *
     * Closes a dialog previously opened by AskPassword, AskQuestion or ShowProcesses.
     * If no dialog is open, does nothing.
     */
    Close: function(params, invocation) {
        this._clearCurrentRequest(Gio.MountOperationResult.UNHANDLED, {});
        this._closeDialog();
    }
});
