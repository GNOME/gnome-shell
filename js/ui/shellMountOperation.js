import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import GObject from 'gi://GObject';
import Pango from 'gi://Pango';
import Shell from 'gi://Shell';
import St from 'gi://St';

import * as Animation from './animation.js';
import * as CheckBox from './checkBox.js';
import * as Dialog from './dialog.js';
import * as MessageTray from './messageTray.js';
import * as ModalDialog from './modalDialog.js';
import * as Params from '../misc/params.js';
import * as ShellEntry from './shellEntry.js';

import {loadInterfaceXML} from '../misc/fileUtils.js';
import {wiggle} from '../misc/animationUtils.js';

const LIST_ITEM_ICON_SIZE = 48;
const WORK_SPINNER_ICON_SIZE = 16;

const REMEMBER_MOUNT_PASSWORD_KEY = 'remember-mount-password';

/* ------ Common Utils ------- */
function _setButtonsForChoices(dialog, oldChoices, choices) {
    const buttons = [];
    let buttonsChanged = oldChoices.length !== choices.length;

    for (let idx = 0; idx < choices.length; idx++) {
        const button = idx;

        buttonsChanged ||= oldChoices[idx] !== choices[idx];

        buttons.unshift({
            label: choices[idx],
            action: () => dialog.emit('response', button),
        });
    }

    if (buttonsChanged)
        dialog.setButtons(buttons);
}

function _setLabelsForMessage(content, message) {
    const labels = message.split('\n');

    content.title = labels.shift();
    content.description = labels.join('\n');
}

/* -------------------------------------------------------- */

export class ShellMountOperation {
    constructor(source, params) {
        params = Params.parse(params, {existingDialog: null});

        this._dialog = null;
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

        this._drive = source.get_drive();
        this._drive?.connectObject('disconnected',
            this.close.bind(this), this);
    }

    _closeExistingDialog() {
        if (!this._existingDialog)
            return;

        this._existingDialog.close();
        this._existingDialog = null;
    }

    _onAskQuestion(op, message, choices) {
        this._closeExistingDialog();
        this._dialog = new ShellMountQuestionDialog();

        this._dialog.connectObject('response',
            (object, choice) => {
                this.mountOp.set_choice(choice);
                this.mountOp.reply(Gio.MountOperationResult.HANDLED);

                this.close();
            }, this);

        this._dialog.update(message, choices);
        this._dialog.open();
    }

    _onAskPassword(op, message, defaultUser, defaultDomain, flags) {
        if (this._existingDialog) {
            this._dialog = this._existingDialog;
            this._dialog.reaskPassword();
        } else {
            this._dialog = new ShellMountPasswordDialog(message, flags);
        }

        this._dialog.connectObject('response',
            (object, choice, password, remember, hiddenVolume, systemVolume, pim) => {
                if (choice === -1) {
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
            }, this);
        this._dialog.open();
    }

    close(_op) {
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

        if (this._drive) {
            this._drive.disconnectObject(this);
            this._drive = null;
        }
    }

    _onShowProcesses2(op) {
        this._closeExistingDialog();

        const processes = op.get_show_processes_pids();
        const choices = op.get_show_processes_choices();
        const message = op.get_show_processes_message();

        if (!this._processesDialog) {
            this._processesDialog = new ShellProcessesDialog();
            this._dialog = this._processesDialog;

            this._processesDialog.connectObject('response',
                (object, choice) => {
                    if (choice === -1) {
                        this.mountOp.reply(Gio.MountOperationResult.ABORTED);
                    } else {
                        this.mountOp.set_choice(choice);
                        this.mountOp.reply(Gio.MountOperationResult.HANDLED);
                    }

                    this.close();
                }, this);
            this._processesDialog.open();
        }

        this._processesDialog.update(message, processes, choices);
    }

    _onShowUnmountProgress(op, message, timeLeft, bytesLeft) {
        if (bytesLeft === 0)
            this._showUnmountNotificationDone(message);
        else
            this._showUnmountNotification(message);
    }

    borrowDialog() {
        this._dialog?.disconnectObject(this);
        return this._dialog;
    }

    _createNotification(title, body) {
        this._notification?.destroy();

        const source = MessageTray.getSystemSource();
        this._notification = new MessageTray.Notification({
            source,
            title,
            body,
            isTransient: true,
            iconName: 'media-removable-symbolic',
        });

        this._notification.connect('destroy', () => delete this._notification);
        source.addNotification(this._notification);
    }

    _showUnmountNotificationDone(message) {
        if (message)
            this._createNotification(message, null);
    }

    _showUnmountNotification(message) {
        const [title, body] = message.split('\n', 2);

        if (!this._notification)
            this._createNotification(title, body);
        else
            this._notification.set({title, body});
    }
}

const ShellMountQuestionDialog = GObject.registerClass({
    Signals: {'response': {param_types: [GObject.TYPE_INT]}},
}, class ShellMountQuestionDialog extends ModalDialog.ModalDialog {
    _init() {
        super._init({styleClass: 'mount-question-dialog'});

        this._oldChoices = [];

        this._content = new Dialog.MessageDialogContent();
        this.contentLayout.add_child(this._content);
    }

    vfunc_key_release_event(event) {
        if (event.get_key_symbol() === Clutter.KEY_Escape) {
            this.emit('response', -1);
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    }

    update(message, choices) {
        _setLabelsForMessage(this._content, message);
        _setButtonsForChoices(this, this._oldChoices, choices);
        this._oldChoices = choices;
    }
});

const ShellMountPasswordDialog = GObject.registerClass({
    Signals: {
        'response': {
            param_types: [
                GObject.TYPE_INT,
                GObject.TYPE_STRING,
                GObject.TYPE_BOOLEAN,
                GObject.TYPE_BOOLEAN,
                GObject.TYPE_BOOLEAN,
                GObject.TYPE_UINT,
            ],
        },
    },
}, class ShellMountPasswordDialog extends ModalDialog.ModalDialog {
    _init(message, flags) {
        const strings = message.split('\n');
        const title = strings.shift() || null;
        const description = strings.shift() || null;
        super._init({styleClass: 'prompt-dialog'});

        const disksApp = Shell.AppSystem.get_default().lookup_app('org.gnome.DiskUtility.desktop');

        const content = new Dialog.MessageDialogContent({title, description});

        const passwordGridLayout = new Clutter.GridLayout({orientation: Clutter.Orientation.VERTICAL});
        const passwordGrid = new St.Widget({
            style_class: 'prompt-dialog-password-grid',
            layout_manager: passwordGridLayout,
        });
        passwordGridLayout.hookup_style(passwordGrid);

        const rtl = passwordGrid.get_text_direction() === Clutter.TextDirection.RTL;
        let curGridRow = 0;

        if (flags & Gio.AskPasswordFlags.TCRYPT) {
            this._hiddenVolume = new CheckBox.CheckBox(_('Hidden Volume'));
            content.add_child(this._hiddenVolume);

            this._systemVolume = new CheckBox.CheckBox(_('Windows System Volume'));
            content.add_child(this._systemVolume);

            this._keyfilesCheckbox = new CheckBox.CheckBox(_('Uses Keyfiles'));
            this._keyfilesCheckbox.connect('clicked', this._onKeyfilesCheckboxClicked.bind(this));
            content.add_child(this._keyfilesCheckbox);

            this._keyfilesLabel = new St.Label({visible: false});
            if (disksApp) {
                this._keyfilesLabel.clutter_text.set_markup(
                    /* Translators: %s is the Disks application */
                    _('To unlock a volume that uses keyfiles, use the <i>%s</i> utility instead')
                   .format(disksApp.get_name()));
            } else {
                this._keyfilesLabel.clutter_text.set_markup(
                    _('You need an external utility like <i>Disks</i> to unlock a volume that uses keyfiles'));
            }
            this._keyfilesLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
            this._keyfilesLabel.clutter_text.line_wrap = true;
            content.add_child(this._keyfilesLabel);

            this._pimEntry = new St.PasswordEntry({
                style_class: 'prompt-dialog-password-entry',
                hint_text: _('PIM Number'),
                can_focus: true,
                x_expand: true,
            });
            this._pimEntry.clutter_text.connect('activate', this._onEntryActivate.bind(this));
            ShellEntry.addContextMenu(this._pimEntry);

            if (rtl)
                passwordGridLayout.attach(this._pimEntry, 1, curGridRow, 1, 1);
            else
                passwordGridLayout.attach(this._pimEntry, 0, curGridRow, 1, 1);
            curGridRow += 1;
        } else {
            this._hiddenVolume = null;
            this._systemVolume = null;
            this._pimEntry = null;
        }

        this._passwordEntry = new St.PasswordEntry({
            style_class: 'prompt-dialog-password-entry',
            hint_text: _('Password'),
            can_focus: true,
            x_expand: true,
        });
        this._passwordEntry.clutter_text.connect('activate', this._onEntryActivate.bind(this));
        this.setInitialKeyFocus(this._passwordEntry);
        ShellEntry.addContextMenu(this._passwordEntry);

        this._workSpinner = new Animation.Spinner(WORK_SPINNER_ICON_SIZE, {
            animate: true,
        });

        if (rtl) {
            passwordGridLayout.attach(this._workSpinner, 0, curGridRow, 1, 1);
            passwordGridLayout.attach(this._passwordEntry, 1, curGridRow, 1, 1);
        } else {
            passwordGridLayout.attach(this._passwordEntry, 0, curGridRow, 1, 1);
            passwordGridLayout.attach(this._workSpinner, 1, curGridRow, 1, 1);
        }
        curGridRow += 1;

        const warningBox = new St.BoxLayout({orientation: Clutter.Orientation.VERTICAL});

        const capsLockWarning = new ShellEntry.CapsLockWarning();
        warningBox.add_child(capsLockWarning);

        this._errorMessageLabel = new St.Label({
            style_class: 'prompt-dialog-error-label',
            opacity: 0,
        });
        this._errorMessageLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._errorMessageLabel.clutter_text.line_wrap = true;
        warningBox.add_child(this._errorMessageLabel);

        passwordGridLayout.attach(warningBox, 0, curGridRow, 2, 1);

        content.add_child(passwordGrid);

        if (flags & Gio.AskPasswordFlags.SAVING_SUPPORTED) {
            this._rememberChoice = new CheckBox.CheckBox(_('Remember Password'));
            this._rememberChoice.checked =
                global.settings.get_boolean(REMEMBER_MOUNT_PASSWORD_KEY);
            content.add_child(this._rememberChoice);
        } else {
            this._rememberChoice = null;
        }

        this.contentLayout.add_child(content);

        this._defaultButtons = [{
            label: _('Cancel'),
            action: this._onCancelButton.bind(this),
            key: Clutter.KEY_Escape,
        }, {
            label: _('Unlock'),
            action: this._onUnlockButton.bind(this),
            default: true,
        }];

        this._usesKeyfilesButtons = [{
            label: _('Cancel'),
            action: this._onCancelButton.bind(this),
            key: Clutter.KEY_Escape,
        }];

        if (disksApp) {
            this._usesKeyfilesButtons.push({
                /* Translators: %s is the Disks application */
                label: _('Open %s').format(disksApp.get_name()),
                action: () => {
                    disksApp.activate();
                    this._onCancelButton();
                },
                default: true,
            });
        }

        this.setButtons(this._defaultButtons);
    }

    reaskPassword() {
        this._workSpinner.stop();
        this._passwordEntry.set_text('');
        this._errorMessageLabel.text = _('Sorry, that didn’t work. Please try again.');
        this._errorMessageLabel.opacity = 255;

        wiggle(this._passwordEntry);
    }

    _onCancelButton() {
        this.emit('response', -1, '', false, false, false, 0);
    }

    _onUnlockButton() {
        this._onEntryActivate();
    }

    _onEntryActivate() {
        let pim = 0;
        if (this._pimEntry !== null) {
            pim = this._pimEntry.get_text();

            if (isNaN(pim)) {
                this._pimEntry.set_text('');
                this._errorMessageLabel.text = _('The PIM must be a number or empty');
                this._errorMessageLabel.opacity = 255;
                return;
            }

            this._errorMessageLabel.opacity = 0;
        }

        global.settings.set_boolean(REMEMBER_MOUNT_PASSWORD_KEY,
            this._rememberChoice && this._rememberChoice.checked);

        this._workSpinner.play();
        this.emit('response', 1,
            this._passwordEntry.get_text(),
            this._rememberChoice &&
            this._rememberChoice.checked,
            this._hiddenVolume &&
            this._hiddenVolume.checked,
            this._systemVolume &&
            this._systemVolume.checked,
            parseInt(pim));
    }

    _onKeyfilesCheckboxClicked() {
        const useKeyfiles = this._keyfilesCheckbox.checked;
        this._passwordEntry.reactive = !useKeyfiles;
        this._passwordEntry.can_focus = !useKeyfiles;
        this._pimEntry.reactive = !useKeyfiles;
        this._pimEntry.can_focus = !useKeyfiles;
        this._rememberChoice.reactive = !useKeyfiles;
        this._rememberChoice.can_focus = !useKeyfiles;
        this._keyfilesLabel.visible = useKeyfiles;
        this.setButtons(useKeyfiles ? this._usesKeyfilesButtons : this._defaultButtons);
    }
});

const ShellProcessesDialog = GObject.registerClass({
    Signals: {'response': {param_types: [GObject.TYPE_INT]}},
}, class ShellProcessesDialog extends ModalDialog.ModalDialog {
    _init() {
        super._init({styleClass: 'processes-dialog'});

        this._oldChoices = [];

        this._content = new Dialog.MessageDialogContent();
        this.contentLayout.add_child(this._content);

        this._applicationSection = new Dialog.ListSection();
        this._applicationSection.hide();
        this.contentLayout.add_child(this._applicationSection);
    }

    vfunc_key_release_event(event) {
        if (event.get_key_symbol() === Clutter.KEY_Escape) {
            this.emit('response', -1);
            return Clutter.EVENT_STOP;
        }

        return Clutter.EVENT_PROPAGATE;
    }

    _setAppsForPids(pids) {
        // remove all the items
        this._applicationSection.list.destroy_all_children();

        pids.forEach(pid => {
            const tracker = Shell.WindowTracker.get_default();
            const app = tracker.get_app_from_pid(pid);

            if (!app)
                return;

            const listItem = new Dialog.ListSectionItem({
                icon_actor: app.create_icon_texture(LIST_ITEM_ICON_SIZE),
                title: app.get_name(),
            });
            this._applicationSection.list.add_child(listItem);
        });

        this._applicationSection.visible =
            this._applicationSection.list.get_n_children() > 0;
    }

    update(message, processes, choices) {
        this._setAppsForPids(processes);
        _setLabelsForMessage(this._content, message);
        _setButtonsForChoices(this, this._oldChoices, choices);
        this._oldChoices = choices;
    }
});

const GnomeShellMountOpIface = loadInterfaceXML('org.Gtk.MountOperationHandler');

/** @enum {number} */
const ShellMountOperationType = {
    NONE: 0,
    ASK_PASSWORD: 1,
    ASK_QUESTION: 2,
    SHOW_PROCESSES: 3,
};

export class GnomeShellMountOpHandler {
    constructor() {
        this._dbusImpl = Gio.DBusExportedObject.wrapJSObject(GnomeShellMountOpIface, this);
        this._dbusImpl.export(Gio.DBus.session, '/org/gtk/MountOperationHandler');
        Gio.bus_own_name_on_connection(Gio.DBus.session,
            'org.gtk.MountOperationHandler',
            Gio.BusNameOwnerFlags.REPLACE, null, null);

        this._dialog = null;

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
        const oldId = this._currentId;
        const oldType = this._currentType;
        const requestId = `${id}@${invocation.get_sender()}`;

        this._clearCurrentRequest(Gio.MountOperationResult.UNHANDLED, {});

        this._currentInvocation = invocation;
        this._currentId = requestId;
        this._currentType = type;

        if (this._dialog && (oldId === requestId) && (oldType === type))
            return true;

        return false;
    }

    _closeDialog() {
        if (this._dialog) {
            this._dialog.close();
            this._dialog = null;
        }
    }

    /**
     * The dialog will stay visible until clients call the Close() method, or
     * another dialog becomes visible.
     * Calling AskPassword again for the same id will have the effect to clear
     * the existing dialog and update it with a message indicating the previous
     * attempt went wrong.
     *
     * @param {Array} params
     *   {string} id: an opaque ID identifying the object for which
     *       the operation is requested
     *   {string} message: the message to display
     *   {string} icon_name: the name of an icon to display
     *   {string} default_user: the default username for display
     *   {string} default_domain: the default domain for display
     *   {Gio.AskPasswordFlags} flags: a set of GAskPasswordFlags
     *   {Gio.MountOperationResults} response: a GMountOperationResult
     *   {Object} response_details: a dictionary containing response details as
     *       entered by the user. The dictionary MAY contain the following
     *       properties:
     *   - "password" -> (s): a password to be used to complete the mount operation
     *   - "password_save" -> (u): a GPasswordSave
     * @param {Gio.DBusMethodInvocation} invocation
     *      The ID must be unique in the context of the calling process.
     */
    AskPasswordAsync(params, invocation) {
        const [id, message, iconName_, defaultUser_, defaultDomain_, flags] = params;

        if (this._setCurrentRequest(invocation, id, ShellMountOperationType.ASK_PASSWORD)) {
            this._dialog.reaskPassword();
            return;
        }

        this._closeDialog();

        this._dialog = new ShellMountPasswordDialog(message, flags);
        this._dialog.connect('response',
            (object, choice, password, remember, hiddenVolume, systemVolume, pim) => {
                const details = {};
                let response;

                if (choice === -1) {
                    response = Gio.MountOperationResult.ABORTED;
                } else {
                    response = Gio.MountOperationResult.HANDLED;

                    const passSave = remember ? Gio.PasswordSave.PERMANENTLY : Gio.PasswordSave.NEVER;
                    details['password_save'] = GLib.Variant.new('u', passSave);
                    details['password'] = GLib.Variant.new('s', password);
                    details['hidden_volume'] = GLib.Variant.new('b', hiddenVolume);
                    details['system_volume'] = GLib.Variant.new('b', systemVolume);
                    details['pim'] = GLib.Variant.new('u', pim);
                }

                this._clearCurrentRequest(response, details);
            });
        this._dialog.open();
    }

    /**
     * The dialog will stay visible until clients call the Close() method, or
     * another dialog becomes visible.
     * Calling AskQuestion again for the same id will have the effect to clear
     * update the dialog with the new question.
     *
     * @param {Array} params - params
     *   {string} id: an opaque ID identifying the object for which
     *       the operation is requested
     *      The ID must be unique in the context of the calling process.
     *   {string} message: the message to display
     *   {string} icon_name: the name of an icon to display
     *   {string[]} choices: an array of choice strings
     * @param {Gio.DBusMethodInvocation} invocation - invocation
     */
    AskQuestionAsync(params, invocation) {
        const [id, message, iconName_, choices] = params;

        if (this._setCurrentRequest(invocation, id, ShellMountOperationType.ASK_QUESTION)) {
            this._dialog.update(message, choices);
            return;
        }

        this._closeDialog();

        this._dialog = new ShellMountQuestionDialog(message);
        this._dialog.connect('response', (object, choice) => {
            let response;
            const details = {};

            if (choice === -1) {
                response = Gio.MountOperationResult.ABORTED;
            } else {
                response = Gio.MountOperationResult.HANDLED;
                details['choice'] = GLib.Variant.new('i', choice);
            }

            this._clearCurrentRequest(response, details);
        });

        this._dialog.update(message, choices);
        this._dialog.open();
    }

    /**
     * The dialog will stay visible until clients call the Close() method, or
     * another dialog becomes visible.
     * Calling ShowProcesses again for the same id will have the effect to clear
     * the existing dialog and update it with the new message and the new list
     * of processes.
     *
     * @param {Array} params - params
     *   {string} id: an opaque ID identifying the object for which
     *       the operation is requested
     *      The ID must be unique in the context of the calling process.
     *   {string} message: the message to display
     *   {string} icon_name: the name of an icon to display
     *   {number[]} application_pids: the PIDs of the applications to display
     *   {string[]} choices: an array of choice strings
     * @param {Gio.DBusMethodInvocation} invocation - invocation
     */
    ShowProcessesAsync(params, invocation) {
        const [id, message, iconName_, applicationPids, choices] = params;

        if (this._setCurrentRequest(invocation, id, ShellMountOperationType.SHOW_PROCESSES)) {
            this._dialog.update(message, applicationPids, choices);
            return;
        }

        this._closeDialog();

        this._dialog = new ShellProcessesDialog();
        this._dialog.connect('response', (object, choice) => {
            let response;
            const details = {};

            if (choice === -1) {
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
     * Closes a dialog previously opened by AskPassword, AskQuestion or ShowProcesses.
     * If no dialog is open, does nothing.
     *
     * @param {Array} _params - params
     * @param {Gio.DBusMethodInvocation} _invocation - invocation
     */
    Close(_params, _invocation) {
        this._clearCurrentRequest(Gio.MountOperationResult.UNHANDLED, {});
        this._closeDialog();
    }
}
