// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const { Clutter, Gio, GLib, Pango, Shell, St } = imports.gi;
const Signals = imports.signals;

const Animation = imports.ui.animation;
const CheckBox = imports.ui.checkBox;
const Dialog = imports.ui.dialog;
const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const ModalDialog = imports.ui.modalDialog;
const Params = imports.misc.params;
const ShellEntry = imports.ui.shellEntry;

const { loadInterfaceXML } = imports.misc.fileUtils;

var LIST_ITEM_ICON_SIZE = 48;
var WORK_SPINNER_ICON_SIZE = 16;

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
                          action: () => { dialog.emit('response', button); }
                        });
    }

    dialog.setButtons(buttons);
}

function _setLabelsForMessage(content, message) {
    let labels = message.split('\n');

    content.title = labels.shift();
    content.body = labels.join('\n');
}

function _createIcon(gicon) {
    return new St.Icon({ gicon: gicon,
                         style_class: 'shell-mount-operation-icon' })
}

/* -------------------------------------------------------- */

var ListItem = class {
    constructor(app) {
        this._app = app;

        let layout = new St.BoxLayout({ vertical: false});

        this.actor = new St.Button({ style_class: 'mount-dialog-app-list-item',
                                     can_focus: true,
                                     child: layout,
                                     reactive: true,
                                     x_align: St.Align.START,
                                     x_fill: true });

        this._icon = this._app.create_icon_texture(LIST_ITEM_ICON_SIZE);

        let iconBin = new St.Bin({ style_class: 'mount-dialog-app-list-item-icon',
                                   child: this._icon });
        layout.add(iconBin);

        this._nameLabel = new St.Label({ text: this._app.get_name(),
                                         style_class: 'mount-dialog-app-list-item-name' });
        let labelBin = new St.Bin({ y_align: St.Align.MIDDLE,
                                    child: this._nameLabel });
        layout.add(labelBin);

        this.actor.connect('clicked', this._onClicked.bind(this));
    }

    _onClicked() {
        this.emit('activate');
        this._app.activate();
    }
};
Signals.addSignalMethods(ListItem.prototype);

var ShellMountOperation = class {
    constructor(source, params) {
        params = Params.parse(params, { existingDialog: null });

        this._dialog = null;
        this._dialogId = 0;
        this._existingDialog = params.existingDialog;
        this._processesDialog = null;

        this.mountOp = new Shell.MountOperation();

        this.mountOp.connect('ask-question',
                             this._onAskQuestion.bind(this));
        this.mountOp.connect('ask-password',
                             this._onAskPassword.bind(this));
        this.mountOp.connect('show-processes-2',
                             this._onShowProcesses2.bind(this));
        this.mountOp.connect('aborted',
                             this.close.bind(this));
        this.mountOp.connect('show-unmount-progress',
                             this._onShowUnmountProgress.bind(this));

        this._gicon = source.get_icon();
    }

    _closeExistingDialog() {
        if (!this._existingDialog)
            return;

        this._existingDialog.close();
        this._existingDialog = null;
    }

    _onAskQuestion(op, message, choices) {
        this._closeExistingDialog();
        this._dialog = new ShellMountQuestionDialog(this._gicon);

        this._dialogId = this._dialog.connect('response',
            (object, choice) => {
                this.mountOp.set_choice(choice);
                this.mountOp.reply(Gio.MountOperationResult.HANDLED);

                this.close();
            });

        this._dialog.update(message, choices);
        this._dialog.open();
    }

    _onAskPassword(op, message, defaultUser, defaultDomain, flags) {
        if (this._existingDialog) {
            this._dialog = this._existingDialog;
            this._dialog.reaskPassword();
        } else {
            this._dialog = new ShellMountPasswordDialog(message, this._gicon, flags);
        }

        this._dialogId = this._dialog.connect('response',
            (object, choice, password, remember, hiddenVolume, systemVolume, pim) => {
                if (choice == -1) {
                    this.mountOp.reply(Gio.MountOperationResult.ABORTED);
                } else {
                    if (remember)
                        this.mountOp.set_password_save(Gio.PasswordSave.PERMANENTLY);
                    else
                        this.mountOp.set_password_save(Gio.PasswordSave.NEVER);

                    this.mountOp.set_password(password);
                    this.mountOp.set_is_tcrypt_hidden_volume(hiddenVolume);
                    this.mountOp.set_is_tcrypt_system_volume(systemVolume);
                    this.mountOp.set_pim(pim);
                    this.mountOp.reply(Gio.MountOperationResult.HANDLED);
                }
            });
        this._dialog.open();
    }

    close(op) {
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
    }

    _onShowProcesses2(op) {
        this._closeExistingDialog();

        let processes = op.get_show_processes_pids();
        let choices = op.get_show_processes_choices();
        let message = op.get_show_processes_message();

        if (!this._processesDialog) {
            this._processesDialog = new ShellProcessesDialog(this._gicon);
            this._dialog = this._processesDialog;

            this._dialogId = this._processesDialog.connect('response',
                (object, choice) => {
                    if (choice == -1) {
                        this.mountOp.reply(Gio.MountOperationResult.ABORTED);
                    } else {
                        this.mountOp.set_choice(choice);
                        this.mountOp.reply(Gio.MountOperationResult.HANDLED);
                    }

                    this.close();
                });
            this._processesDialog.open();
        }

        this._processesDialog.update(message, processes, choices);
    }

    _onShowUnmountProgress(op, message, timeLeft, bytesLeft) {
        if (!this._notifier)
            this._notifier = new ShellUnmountNotifier();
            
        if (bytesLeft == 0)
            this._notifier.done(message);
        else
            this._notifier.show(message);
    }

    borrowDialog() {
        if (this._dialogId != 0) {
            this._dialog.disconnect(this._dialogId);
            this._dialogId = 0;
        }

        return this._dialog;
    }
};

var ShellUnmountNotifier = class extends MessageTray.Source {
    constructor() {
        super('', 'media-removable');

        this._notification = null;
        Main.messageTray.add(this);
    }

    show(message) {
        let [header, text] = message.split('\n', 2);

        if (!this._notification) {
            this._notification = new MessageTray.Notification(this, header, text);
            this._notification.setTransient(true);
            this._notification.setUrgency(MessageTray.Urgency.CRITICAL);
        } else {
            this._notification.update(header, text);
        }

        this.notify(this._notification);
    }

    done(message) {
        if (this._notification) {
            this._notification.destroy();
            this._notification = null;
        }

        if (message) {
            let notification = new MessageTray.Notification(this, message, null);
            notification.setTransient(true);

            this.notify(notification);
        }
    }
};

var ShellMountQuestionDialog = class extends ModalDialog.ModalDialog {
    constructor(icon) {
        super({ styleClass: 'mount-dialog' });

        this._content = new Dialog.MessageDialogContent({ icon });
        this.contentLayout.add(this._content, { x_fill: true, y_fill: false });
    }

    update(message, choices) {
        _setLabelsForMessage(this._content, message);
        _setButtonsForChoices(this, choices);
    }
};
Signals.addSignalMethods(ShellMountQuestionDialog.prototype);

var ShellMountPasswordDialog = class extends ModalDialog.ModalDialog {
    constructor(message, icon, flags) {
        let strings = message.split('\n');
        let title = strings.shift() || null;
        let body = strings.shift() || null;
        super({ styleClass: 'prompt-dialog' });

        let disksApp = Shell.AppSystem.get_default().lookup_app('org.gnome.DiskUtility.desktop');

        let content = new Dialog.MessageDialogContent({ icon, title, body });
        this.contentLayout.add_actor(content);
        content._body.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

        let layout = new Clutter.GridLayout({ orientation: Clutter.Orientation.VERTICAL });
        let grid = new St.Widget({ style_class: 'prompt-dialog-grid',
                                   layout_manager: layout });
        layout.hookup_style(grid);
        let rtl = grid.get_text_direction() === Clutter.TextDirection.RTL;

        if (flags & Gio.AskPasswordFlags.TCRYPT) {
            this._keyfilesLabel = new St.Label(({ style_class: 'prompt-dialog-keyfiles-label',
                                                  visible: false }));

            this._hiddenVolume = new CheckBox.CheckBox(_("Hidden Volume"));
            content.messageBox.add(this._hiddenVolume.actor);

            this._systemVolume = new CheckBox.CheckBox(_("Windows System Volume"));
            content.messageBox.add(this._systemVolume.actor);

            this._keyfilesCheckbox = new CheckBox.CheckBox(_("Uses Keyfiles"));
            this._keyfilesCheckbox.actor.connect("clicked", this._onKeyfilesCheckboxClicked.bind(this));
            content.messageBox.add(this._keyfilesCheckbox.actor);

            this._keyfilesLabel.clutter_text.set_markup(
                /* Translators: %s is the Disks application */
                _("To unlock a volume that uses keyfiles, use the <i>%s</i> utility instead.").format(disksApp.get_name())
            );
            this._keyfilesLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
            this._keyfilesLabel.clutter_text.line_wrap = true;
            content.messageBox.add(this._keyfilesLabel, { y_fill: false, y_align: St.Align.MIDDLE, expand: true });

            this._pimLabel = new St.Label({ style_class: 'prompt-dialog-password-label',
                                            text: _("PIM Number"),
                                            y_align: Clutter.ActorAlign.CENTER });
            this._pimEntry = new St.Entry({ style_class: 'prompt-dialog-password-entry',
                                            can_focus: true,
                                            x_expand: true });
            this._pimEntry.clutter_text.connect('activate', this._onEntryActivate.bind(this));
            this._pimEntry.clutter_text.set_password_char('\u25cf'); // ● U+25CF BLACK CIRCLE
            ShellEntry.addContextMenu(this._pimEntry, { isPassword: true });

            if (rtl) {
                layout.attach(this._pimEntry, 0, 0, 1, 1);
                layout.attach(this._pimLabel, 1, 0, 1, 1);
            } else {
                layout.attach(this._pimLabel, 0, 0, 1, 1);
                layout.attach(this._pimEntry, 1, 0, 1, 1);
            }

            this._pimErrorMessageLabel = new St.Label({ style_class: 'prompt-dialog-password-entry',
                                                        text: _("The PIM must be a number or empty."),
                                                        visible: false });
            layout.attach(this._pimErrorMessageLabel, 0, 2, 2, 1);
        } else {
            this._hiddenVolume = null;
            this._systemVolume = null;
            this._pimEntry = null;
            this._pimErrorMessageLabel = null;
        }

        this._passwordLabel = new St.Label({ style_class: 'prompt-dialog-password-label',
                                             text: _("Password"),
                                             y_align: Clutter.ActorAlign.CENTER });
        this._passwordEntry = new St.Entry({ style_class: 'prompt-dialog-password-entry',
                                             can_focus: true,
                                             x_expand: true });
        this._passwordEntry.clutter_text.connect('activate', this._onEntryActivate.bind(this));
        this._passwordEntry.clutter_text.set_password_char('\u25cf'); // ● U+25CF BLACK CIRCLE
        ShellEntry.addContextMenu(this._passwordEntry, { isPassword: true });
        this.setInitialKeyFocus(this._passwordEntry);
        this._workSpinner = new Animation.Spinner(WORK_SPINNER_ICON_SIZE, true);
        this._passwordEntry.secondary_icon = this._workSpinner.actor;

        if (rtl) {
            layout.attach(this._passwordEntry, 0, 1, 1, 1);
            layout.attach(this._passwordLabel, 1, 1, 1, 1);
        } else {
            layout.attach(this._passwordLabel, 0, 1, 1, 1);
            layout.attach(this._passwordEntry, 1, 1, 1, 1);
        }

        content.messageBox.add(grid);

        this._errorMessageLabel = new St.Label({ style_class: 'prompt-dialog-error-label',
                                                 text: _("Sorry, that didn’t work. Please try again.") });
        this._errorMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._errorMessageLabel.clutter_text.line_wrap = true;
        this._errorMessageLabel.hide();
        content.messageBox.add(this._errorMessageLabel);

        if (flags & Gio.AskPasswordFlags.SAVING_SUPPORTED) {
            this._rememberChoice = new CheckBox.CheckBox(_("Remember Password"));
            this._rememberChoice.actor.checked =
                global.settings.get_boolean(REMEMBER_MOUNT_PASSWORD_KEY);
            content.messageBox.add(this._rememberChoice.actor);
        } else {
            this._rememberChoice = null;
        }

        this._defaultButtons = [{ label: _("Cancel"),
                                  action: this._onCancelButton.bind(this),
                                  key:    Clutter.Escape
                                },
                                { label: _("Unlock"),
                                  action: this._onUnlockButton.bind(this),
                                  default: true
                                }];

        this._usesKeyfilesButtons = [{ label: _("Cancel"),
                                       action: this._onCancelButton.bind(this),
                                       key:    Clutter.Escape
                                     },
                                     { /* Translators: %s is the Disks application */
                                       label: _("Open %s").format(disksApp.get_name()),
                                       action: this._onOpenDisksButton.bind(this),
                                       default: true
                                     }];

        this.setButtons(this._defaultButtons);
    }

    reaskPassword() {
        this._passwordEntry.set_text('');
        this._errorMessageLabel.show();
        this._workSpinner.stop();
    }

    _onCancelButton() {
        this.emit('response', -1, '', false);
    }

    _onUnlockButton() {
        this._onEntryActivate();
    }

    _onEntryActivate() {
        let pim = 0;
        if (this._pimEntry !== null)
            pim = this._pimEntry.get_text();
        if (isNaN(pim)) {
            this._pimEntry.set_text('');
            this._pimErrorMessageLabel.show();
            return;
        } else if (this._pimErrorMessageLabel !== null) {
            this._pimErrorMessageLabel.hide();
        }

        global.settings.set_boolean(REMEMBER_MOUNT_PASSWORD_KEY,
            this._rememberChoice && this._rememberChoice.actor.checked);

        this._workSpinner.play();
        this.emit('response', 1,
            this._passwordEntry.get_text(),
            this._rememberChoice &&
            this._rememberChoice.actor.checked,
            this._hiddenVolume &&
            this._hiddenVolume.actor.checked,
            this._systemVolume &&
            this._systemVolume.actor.checked,
            pim);
    }

    _onKeyfilesCheckboxClicked() {
        let useKeyfiles = this._keyfilesCheckbox.actor.checked;
        this._passwordEntry.reactive = !useKeyfiles;
        this._passwordEntry.can_focus = !useKeyfiles;
        this._passwordEntry.clutter_text.editable = !useKeyfiles;
        this._passwordEntry.clutter_text.selectable = !useKeyfiles;
        this._pimEntry.reactive = !useKeyfiles;
        this._pimEntry.can_focus = !useKeyfiles;
        this._pimEntry.clutter_text.editable = !useKeyfiles;
        this._pimEntry.clutter_text.selectable = !useKeyfiles;
        this._rememberChoice.actor.reactive = !useKeyfiles;
        this._rememberChoice.actor.can_focus = !useKeyfiles;
        this._keyfilesLabel.visible = useKeyfiles;
        this.setButtons(useKeyfiles ? this._usesKeyfilesButtons : this._defaultButtons);
    }

    _onOpenDisksButton() {
        let app = Shell.AppSystem.get_default().lookup_app('org.gnome.DiskUtility.desktop');
        if (app)
            app.activate();
        else
            Main.notifyError(
                /* Translators: %s is the Disks application */
                _("Unable to start %s").format(app.get_name()),
                /* Translators: %s is the Disks application */
                _("Couldn't find the %s application").format(app.get_name())
            );
        this._onCancelButton();
    }
};

var ShellProcessesDialog = class extends ModalDialog.ModalDialog {
    constructor(icon) {
        super({ styleClass: 'mount-dialog' });

        this._content = new Dialog.MessageDialogContent({ icon });
        this.contentLayout.add(this._content, { x_fill: true, y_fill: false });

        let scrollView = new St.ScrollView({ style_class: 'mount-dialog-app-list'});
        scrollView.set_policy(St.PolicyType.NEVER,
                              St.PolicyType.AUTOMATIC);
        this.contentLayout.add(scrollView,
                               { x_fill: true,
                                 y_fill: true });
        scrollView.hide();

        this._applicationList = new St.BoxLayout({ vertical: true });
        scrollView.add_actor(this._applicationList);

        this._applicationList.connect('actor-added', () => {
            if (this._applicationList.get_n_children() == 1)
                scrollView.show();
        });

        this._applicationList.connect('actor-removed', () => {
            if (this._applicationList.get_n_children() == 0)
                scrollView.hide();
        });
    }

    _setAppsForPids(pids) {
        // remove all the items
        this._applicationList.destroy_all_children();

        pids.forEach(pid => {
            let tracker = Shell.WindowTracker.get_default();
            let app = tracker.get_app_from_pid(pid);

            if (!app)
                return;

            let item = new ListItem(app);
            this._applicationList.add(item.actor, { x_fill: true });

            item.connect('activate', () => {
                // use -1 to indicate Cancel
                this.emit('response', -1);
            });
        });
    }

    update(message, processes, choices) {
        this._setAppsForPids(processes);
        _setLabelsForMessage(this._content, message);
        _setButtonsForChoices(this, choices);
    }
};
Signals.addSignalMethods(ShellProcessesDialog.prototype);

const GnomeShellMountOpIface = loadInterfaceXML('org.Gtk.MountOperationHandler');

var ShellMountOperationType = {
    NONE: 0,
    ASK_PASSWORD: 1,
    ASK_QUESTION: 2,
    SHOW_PROCESSES: 3
};

var GnomeShellMountOpHandler = class {
    constructor() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(GnomeShellMountOpIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gtk/MountOperationHandler');
        Gio.bus_own_name_on_connection(Gio.DBus.session, 'org.gtk.MountOperationHandler',
                                       Gio.BusNameOwnerFlags.REPLACE, null, null);

        this._dialog = null;
        this._volumeMonitor = Gio.VolumeMonitor.get();

        this._ensureEmptyRequest();
    }

    _ensureEmptyRequest() {
        this._currentId = null;
        this._currentInvocation = null;
        this._currentType = ShellMountOperationType.NONE;
    }

    _clearCurrentRequest(response, details) {
        if (this._currentInvocation) {
            this._currentInvocation.return_value(
                GLib.Variant.new('(ua{sv})', [response, details]));
        }

        this._ensureEmptyRequest();
    }

    _setCurrentRequest(invocation, id, type) {
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
    }

    _closeDialog() {
        if (this._dialog) {
            this._dialog.close();
            this._dialog = null;
        }
    }

    _createGIcon(iconName) {
        let realIconName = iconName ? iconName : 'drive-harddisk';
        return new Gio.ThemedIcon({ name: realIconName,
                                    use_default_fallbacks: true });
    }

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
    AskPasswordAsync(params, invocation) {
        let [id, message, iconName, defaultUser, defaultDomain, flags] = params;

        if (this._setCurrentRequest(invocation, id, ShellMountOperationType.ASK_PASSWORD)) {
            this._dialog.reaskPassword();
            return;
        }

        this._closeDialog();

        this._dialog = new ShellMountPasswordDialog(message, this._createGIcon(iconName), flags);
        this._dialog.connect('response',
            (object, choice, password, remember, hiddenVolume, systemVolume, pim) => {
                let details = {};
                let response;

                if (choice == -1) {
                    response = Gio.MountOperationResult.ABORTED;
                } else {
                    response = Gio.MountOperationResult.HANDLED;

                    let passSave = remember ? Gio.PasswordSave.PERMANENTLY : Gio.PasswordSave.NEVER;
                    details['password_save'] = GLib.Variant.new('u', passSave);
                    details['password'] = GLib.Variant.new('s', password);
                    details['hidden_volume'] = GLib.Variant.new('b', hiddenVolume);
                    details['system_volume'] = GLib.Variant.new('b', systemVolume);
                    details['pim'] = GLib.Variant.new('u', parseInt(pim));
                }

                this._clearCurrentRequest(response, details);
            });
        this._dialog.open();
    }

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
    AskQuestionAsync(params, invocation) {
        let [id, message, iconName, choices] = params;

        if (this._setCurrentRequest(invocation, id, ShellMountOperationType.ASK_QUESTION)) {
            this._dialog.update(message, choices);
            return;
        }

        this._closeDialog();

        this._dialog = new ShellMountQuestionDialog(this._createGIcon(iconName), message);
        this._dialog.connect('response', (object, choice) => {
            this._clearCurrentRequest(Gio.MountOperationResult.HANDLED,
                                      { choice: GLib.Variant.new('i', choice) });
        });

        this._dialog.update(message, choices);
        this._dialog.open();
    }

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
    ShowProcessesAsync(params, invocation) {
        let [id, message, iconName, applicationPids, choices] = params;

        if (this._setCurrentRequest(invocation, id, ShellMountOperationType.SHOW_PROCESSES)) {
            this._dialog.update(message, applicationPids, choices);
            return;
        }

        this._closeDialog();

        this._dialog = new ShellProcessesDialog(this._createGIcon(iconName));
        this._dialog.connect('response', (object, choice) => {
            let response;
            let details = {};

            if (choice == -1) {
                response = Gio.MountOperationResult.ABORTED;
            } else {
                response = Gio.MountOperationResult.HANDLED;
                details['choice'] = GLib.Variant.new('i', choice);
            }

            this._clearCurrentRequest(response, details);
        });

        this._dialog.update(message, applicationPids, choices);
        this._dialog.open();
    }

    /**
     * Close:
     *
     * Closes a dialog previously opened by AskPassword, AskQuestion or ShowProcesses.
     * If no dialog is open, does nothing.
     */
    Close(params, invocation) {
        this._clearCurrentRequest(Gio.MountOperationResult.UNHANDLED, {});
        this._closeDialog();
    }
};
