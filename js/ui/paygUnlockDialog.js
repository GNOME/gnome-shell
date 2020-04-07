// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// Copyright (C) 2018 Endless Mobile, Inc.
//
// Licensed under the GNU General Public License Version 2
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

/* exported PaygUnlockDialog */

const { Atk, Clutter, GLib,
    GObject, Meta, Pango, Shell, St }  = imports.gi;

const Payg = imports.ui.payg;

const Animation = imports.ui.animation;
const Main = imports.ui.main;
const LayoutManager = imports.ui.layout;

const MSEC_PER_SEC = 1000;

// The timeout before going back automatically to the lock screen
const IDLE_TIMEOUT_SECS = 2 * 60;

var PaygUnlockDialog = GObject.registerClass({
    Signals: {
        'code-added': {},
        'failed': {},
        'success-message-shown': {},
        'wake-up-screen': {},
    },
}, class PaygUnlockDialog extends Payg.PaygUnlockUi {

    _init(parentActor) {
        super._init({
            accessible_role: Atk.Role.WINDOW,
            style_class: 'unlock-dialog-payg',
            layout_manager: new Clutter.BoxLayout(),
            visible: false,
        });

        this._parentActor = parentActor;
        this._entry = null;
        this._errorMessage = null;
        this._cancelButton = null;
        this._nextButton = null;
        this._spinner = null;
        this._cancelled = false;

        this._verificationStatus = Payg.UnlockStatus.NOT_VERIFYING;

        // Clear the clipboard to make sure nothing can be copied into the entry.
        St.Clipboard.get_default().set_text(St.ClipboardType.CLIPBOARD, '');
        St.Clipboard.get_default().set_text(St.ClipboardType.PRIMARY, '');

        this.add_constraint(new LayoutManager.MonitorConstraint({ primary: true }));

        this._parentActor.add_child(this);

        let mainBox = new St.BoxLayout({
            vertical: true,
            x_align: Clutter.ActorAlign.FILL,
            y_align: Clutter.ActorAlign.FILL,
            x_expand: true,
            y_expand: true,
        });
        this.add_child(mainBox);

        let paygEnterCodeBox = new St.BoxLayout({
            vertical: true,
            x_align: Clutter.ActorAlign.FILL,
            y_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
            y_expand: true,
            style_class: 'unlock-dialog-payg-layout',
        });
        mainBox.add_child(paygEnterCodeBox);

        let titleLabel = new St.Label({
            style_class: 'unlock-dialog-payg-title',
            text: _('Your Pay As You Go usage credit has expired.'),
            x_align: Clutter.ActorAlign.CENTER,
        });
        paygEnterCodeBox.add_child(titleLabel);

        let promptBox = new St.BoxLayout({
            vertical: true,
            x_align: Clutter.ActorAlign.CENTER,
            y_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
            y_expand: true,
            style_class: 'unlock-dialog-payg-promptbox',
        });
        promptBox.connect('key-press-event', (actor, event) => {
            if (event.get_key_symbol() === Clutter.KEY_Escape)
                this._onCancelled();

            return Clutter.EVENT_PROPAGATE;
        });
        paygEnterCodeBox.add_child(promptBox);

        let promptLabel = new St.Label({
            style_class: 'unlock-dialog-payg-label',
            text: _('Enter a new code to unlock your computer:'),
            x_align: Clutter.ActorAlign.START,
        });
        promptLabel.clutter_text.line_wrap = true;
        promptLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        promptBox.add_child(promptLabel);

        let entryBox = this._createEntryArea();
        promptBox.add_child(entryBox);

        this._errorMessage = new St.Label({
            opacity: 0,
            styleClass: 'unlock-dialog-payg-message',
        });
        this._errorMessage.clutter_text.line_wrap = true;
        this._errorMessage.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        promptBox.add_child(this._errorMessage);

        this._buttonBox = this._createButtonsArea();
        promptBox.add_child(this._buttonBox);

        // Use image-specific instructions if present, or the fallback text otherwise.
        let instructionsLine1 = Main.customerSupport.paygInstructionsLine1
            ? Main.customerSupport.paygInstructionsLine1 : _('Don’t have an unlock code? That’s OK!');

        let helpLineMain = new St.Label({
            style_class: 'unlock-dialog-payg-help-main',
            text: instructionsLine1,
            x_align: Clutter.ActorAlign.START,
        });
        helpLineMain.clutter_text.line_wrap = true;
        helpLineMain.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

        promptBox.add_child(helpLineMain);

        // Default to the fallback text, before figuring out whether
        // we can show something more image-specific to the user.
        let instructionsLine2;
        if (Main.customerSupport.paygInstructionsLine2) {
            // Overrides for the entire line take priority over everything else.
            instructionsLine2 = Main.customerSupport.paygInstructionsLine2;
        } else if (Main.customerSupport.paygContactName && Main.customerSupport.paygContactNumber) {
            // The second possible override is to use the template text below
            // with the contact's name and phone number, if BOTH are present.
            instructionsLine2 = _('Talk to your sales representative to purchase a new code. Call or text %s at %s')
                .format(Main.customerSupport.paygContactName, Main.customerSupport.paygContactNumber);
        } else {
            // No overrides present, default to fallback text.
            instructionsLine2 = _('Talk to your sales representative to purchase a new code.');
        }

        let helpLineSub = new St.Label({
            style_class: 'unlock-dialog-payg-help-sub',
            text: instructionsLine2,
            x_align: Clutter.ActorAlign.START,
        });
        helpLineSub.clutter_text.line_wrap = true;
        helpLineSub.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

        promptBox.add_child(helpLineSub);

        // The standard value for an empty ACCOUNT_ID is 0, which implies the backend provider doesn't
        // support the feature.
        if (Main.paygManager.accountID !== '0') {
            let infoBox = new St.BoxLayout({
                vertical: true,
                x_align: Clutter.ActorAlign.START,
                y_align: Clutter.ActorAlign.END,
                x_expand: true,
                style_class: 'unlock-dialog-payg-layout',
            });

            let accountIDText = _('Pay As You Go Account ID: %s').format(Main.paygManager.accountID);
            let accountIDInfo = new St.Label({
                style_class: 'unlock-dialog-payg-account-id',
                text: accountIDText,
            });

            infoBox.add_child(accountIDInfo);
            mainBox.add_child(infoBox);
        }

        Main.ctrlAltTabManager.addGroup(promptBox, _('Unlock Machine'), 'dialog-password-symbolic');

        this._cancelButton.connect('clicked', () => {
            this._onCancelled();
        });
        this._nextButton.connect('clicked', () => {
            this.startVerifyingCode();
        });

        this._entry.connect('code-changed', () => {
            this.updateApplyButtonSensitivity();
        });

        this._entry.clutter_text.connect('activate', () => {
            this.startVerifyingCode();
        });

        this.connect('code-added', () => {
            this.remove_child(mainBox);
            this._successScreen();
        });

        this.connect('code-reset', () => {
            this._paygUnlockDialog = mainBox;
            this.remove_child(mainBox);
            this._resetScreen();
        });

        this._idleMonitor = Meta.IdleMonitor.get_core();
        this._idleWatchId = this._idleMonitor.add_idle_watch(IDLE_TIMEOUT_SECS * MSEC_PER_SEC, this._onCancelled.bind(this));

        this.updateSensitivity();
        this._entry.grab_key_focus();
    }

    _onDestroy() {
        this.popModal();

        if (this._idleWatchId) {
            this._idleMonitor.remove_watch(this._idleWatchId);
            this._idleWatchId = 0;
        }

        if (this._clearTooManyAttemptsId > 0) {
            GLib.source_remove(this._clearTooManyAttemptsId);
            this._clearTooManyAttemptsId = 0;
        }
    }

    _createButtonsArea() {
        let buttonsBox = new St.BoxLayout({
            style_class: 'unlock-dialog-payg-button-box',
            vertical: false,
            x_expand: true,
            x_align: Clutter.ActorAlign.FILL,
            y_expand: true,
            y_align: Clutter.ActorAlign.END,
        });

        this._cancelButton = new St.Button({
            style_class: 'modal-dialog-button button',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: true,
            can_focus: true,
            label: _('Cancel'),
            x_align: St.Align.START,
            y_align: St.Align.END,
        });
        buttonsBox.add_child(this._cancelButton);

        let buttonSpacer = new St.Widget({
            layout_manager: new Clutter.BinLayout(),
            x_expand: true,
            x_align: Clutter.ActorAlign.END,
        });
        buttonsBox.add_child(buttonSpacer);

        // We make the most of the spacer to show the spinner while verifying the code.
        this._spinner = new Animation.Spinner(Payg.SPINNER_ICON_SIZE_PIXELS, {
            animate: true,
            hideOnStop: true,
        });
        buttonSpacer.add_child(this._spinner);

        this._nextButton = new St.Button({
            style_class: 'modal-dialog-button button',
            button_mask: St.ButtonMask.ONE | St.ButtonMask.THREE,
            reactive: true,
            can_focus: true,
            label: _('Unlock'),
            x_align: St.Align.END,
            y_align: St.Align.END,
        });
        this._nextButton.add_style_pseudo_class('default');
        buttonsBox.add_child(this._nextButton);

        return buttonsBox;
    }

    _createEntryArea() {
        let entryBox = new St.BoxLayout({
            vertical: false,
            x_expand: true,
            x_align: Clutter.ActorAlign.FILL,
        });

        if (Main.paygManager.codeFormatPrefix !== '') {
            let prefix = new St.Label({
                style_class: 'unlock-dialog-payg-code-entry',
                text: Main.paygManager.codeFormatPrefix,
                x_align: Clutter.ActorAlign.CENTER,
            });

            entryBox.add_child(prefix);
        }

        this._entry = new Payg.PaygUnlockCodeEntry({
            style_class: 'unlock-dialog-payg-entry',
            reactive: true,
            can_focus: true,
            x_align: Clutter.ActorAlign.FILL,
            x_expand: true,
            y_expand: false,
        });
        this._entry.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this._entry.clutter_text.x_align = Clutter.ActorAlign.CENTER;

        entryBox.add_child(this._entry);

        if (Main.paygManager.codeFormatSuffix !== '') {
            let suffix = new St.Label({
                style_class: 'unlock-dialog-payg-code-entry',
                text: Main.paygManager.codeFormatSuffix,
                x_align: Clutter.ActorAlign.CENTER,
            });
            entryBox.add_child(suffix);
        }

        return entryBox;
    }

    _createMessageBox() {
        let messageBox = new St.BoxLayout({
            vertical: true,
            x_align: Clutter.ActorAlign.FILL,
            y_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
            y_expand: true,
            style_class: 'unlock-dialog-payg-layout',
        });

        return messageBox;
    }

    _createMessageButtonArea(buttonLabel) {
        let messageButtonBox = new St.BoxLayout({
            style_class: 'unlock-dialog-payg-button-box',
            vertical: false,
            x_expand: true,
            x_align: Clutter.ActorAlign.CENTER,
            y_expand: true,
            y_align: Clutter.ActorAlign.END,
        });

        this._messageButton = new St.Button({
            style_class: 'modal-dialog-button button',
            button_mask: St.ButtonMask.THREE,
            reactive: true,
            can_focus: true,
            label: buttonLabel,
            x_align: St.Align.MIDDLE,
            y_align: St.Align.END,
        });

        this._messageButton.add_style_pseudo_class('default');
        messageButtonBox.add_child(this._messageButton);

        return messageButtonBox;
    }

    _createMessageString(string) {
        let messageString = new St.Label({
            style_class: 'unlock-dialog-payg-success',
            text: string,
            x_align: Clutter.ActorAlign.CENTER,
        });

        return messageString;
    }

    _successScreen() {
        let messageBox = this._createMessageBox();
        this.add_child(messageBox);

        // successMessage will handle the formatting of the string
        messageBox.add_child(this._createMessageString(Payg.successMessage()));
        messageBox.add_child(this._createMessageButtonArea(_('Success!')));
        this._messageButton.grab_key_focus();

        this._timeoutId = GLib.timeout_add_seconds(
            GLib.PRIORITY_DEFAULT,
            3,
            () => {
                this.emit('success-message-shown');
                return GLib.SOURCE_REMOVE;
            });
        this._messageButton.connect('button-press-event', () => {
            GLib.source_remove(this._timeoutId);
            this.emit('success-message-shown');
        });
        this._messageButton.connect('key-press-event', () => {
            GLib.source_remove(this._timeoutId);
            this.emit('success-message-shown');
        });
    }

    _resetScreen() {
        let messageBox = this._createMessageBox();
        this.add_child(messageBox);

        messageBox.add_child(this._createMessageString(_('Remaining time cleared!')));
        messageBox.add_child(this._createMessageButtonArea(_('OK!')));
        this._messageButton.grab_key_focus();

        this._timeoutId = GLib.timeout_add_seconds(
            GLib.PRIORITY_DEFAULT,
            3,
            () => {
                this.add_child(this._paygUnlockDialog);
                this.remove_child(messageBox);
                return GLib.SOURCE_REMOVE;
            });
        this._messageButton.connect('button-press-event', () => {
            this._restorePaygUnlockCodeEntry();
            this.remove_child(messageBox);
        });
        this._messageButton.connect('key-press-event', () => {
            this._restorePaygUnlockCodeEntry();
            this.remove_child(messageBox);
        });
    }

    _restorePaygUnlockCodeEntry() {
        GLib.source_remove(this._timeoutId);
        this.add_child(this._paygUnlockDialog);
        this.updateSensitivity();
        this._entry.grab_key_focus();
    }

    _onCancelled() {
        this._cancelled = true;
        this.reset();

        // The ScreenShield will connect to the 'failed' signal
        // to know when to cancel the unlock dialog.
        if (this._verificationStatus !== Payg.UnlockStatus.SUCCEEDED)
            this.emit('failed');
    }

    entrySetEnabled(enabled) {
        this._entry.setEnabled(enabled);
    }

    entryReset() {
        this._entry.reset();
    }

    onCodeAdded() {
        this.emit('code-added');
        this.clearError();
    }

    get entryCode() {
        return this._entry.code;
    }

    get verificationStatus() {
        return this._verificationStatus;
    }

    set verificationStatus(value) {
        this._verificationStatus = value;
    }

    get cancelled() {
        return this._cancelled;
    }

    set cancelled(value) {
        this._cancelled = value;
    }

    get errorLabel() {
        return this._errorMessage;
    }

    get spinner() {
        return this._spinner;
    }

    get applyButton() {
        return this._nextButton;
    }

    addCharacter(unichar) {
        this._entry.addCharacter(unichar);
    }

    cancel() {
        this.entryReset();
    }

    finish(onComplete) {
        // Nothing to do other than calling the callback.
        if (onComplete)
            onComplete();
    }

    open(timestamp) {
        this.show();

        if (this._isModal)
            return true;

        if (!Main.pushModal(this, {
            timestamp,
            actionMode: Shell.ActionMode.UNLOCK_SCREEN,
        }))
            return false;

        this._isModal = true;

        return true;
    }

    popModal(timestamp) {
        if (this._isModal) {
            Main.popModal(this, timestamp);
            this._isModal = false;
        }
    }
});
