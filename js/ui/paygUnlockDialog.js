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

const { Atk, Clutter, Gio, GLib,
    GObject, Meta, Pango, Shell, St }  = imports.gi;
const Gettext = imports.gettext;

const PaygManager = imports.misc.paygManager;

const Animation = imports.ui.animation;
const Main = imports.ui.main;
const LayoutManager = imports.ui.layout;

const MSEC_PER_SEC = 1000;

// The timeout before going back automatically to the lock screen
const IDLE_TIMEOUT_SECS = 2 * 60;

const SPINNER_ICON_SIZE_PIXELS = 16;

var UnlockStatus = {
    NOT_VERIFYING: 0,
    VERIFYING: 1,
    FAILED: 2,
    TOO_MANY_ATTEMPTS: 3,
    SUCCEEDED: 4,
};

var PaygUnlockCodeEntry = GObject.registerClass({
    Signals: {
        'code-changed': { param_types: [GObject.TYPE_STRING] },
    },
}, class PaygUnlockCodeEntry extends St.Entry {

    _init() {
        super._init({
            style_class: 'unlock-dialog-payg-entry',
            reactive: true,
            can_focus: true,
            x_align: Clutter.ActorAlign.FILL,
        });

        this._code = '';
        this.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        this.clutter_text.x_align = Clutter.ActorAlign.CENTER;

        this._enabled = false;
        this._buttonPressEventId = this.connect('button-press-event', this._onButtonPressEvent.bind(this));
        this._capturedEventId = this.clutter_text.connect('captured-event', this._onCapturedEvent.bind(this));
        this._textChangedId = this.clutter_text.connect('text-changed', this._onTextChanged.bind(this));

        this.connect('destroy', this._onDestroy.bind(this));
    }

    _onDestroy() {
        if (this._buttonPressEventId > 0) {
            this.disconnect(this._buttonPressEventId);
            this._buttonPressEventId = 0;
        }

        if (this._capturedEventId > 0) {
            this.clutter_text.disconnect(this._capturedEventId);
            this._capturedEventId = 0;
        }

        if (this._textChangedId > 0) {
            this.clutter_text.disconnect(this._textChangedId);
            this._textChangedId = 0;
        }
    }

    _onCapturedEvent(textActor, event) {
        if (event.type() !== Clutter.EventType.KEY_PRESS)
            return Clutter.EVENT_PROPAGATE;

        let keysym = event.get_key_symbol();
        let isDeleteKey =
            keysym === Clutter.KEY_Delete ||
            keysym === Clutter.KEY_KP_Delete ||
            keysym === Clutter.KEY_BackSpace;
        let isEnterKey =
            keysym === Clutter.KEY_Return ||
            keysym === Clutter.KEY_KP_Enter ||
            keysym === Clutter.KEY_ISO_Enter;
        let isExitKey =
            keysym === Clutter.KEY_Escape ||
            keysym === Clutter.KEY_Tab;
        let isMovementKey =
            keysym === Clutter.KEY_Left ||
            keysym === Clutter.KEY_Right ||
            keysym === Clutter.KEY_Home ||
            keysym === Clutter.KEY_KP_Home ||
            keysym === Clutter.KEY_End ||
            keysym === Clutter.KEY_KP_End;

        // Make sure we can leave the entry and delete and
        // navigate numbers with the keyboard.
        if (isExitKey || isEnterKey || isDeleteKey || isMovementKey)
            return Clutter.EVENT_PROPAGATE;

        let character = event.get_key_unicode();
        this.addCharacter(character);

        return Clutter.EVENT_STOP;
    }

    _onTextChanged(textActor) {
        this._code = textActor.text;
        this.emit('code-changed', this._code);
    }

    _onButtonPressEvent() {
        if (!this._enabled)
            return;

        this.grab_key_focus();
        return false;
    }

    addCharacter(character) {
        if (!this._enabled || !GLib.unichar_isprint(character))
            return;

        let pos = this.clutter_text.get_cursor_position();
        let before = pos === -1 ? this._code : this._code.slice(0, pos);
        let after = pos === -1 ? '' : this._code.slice(pos);
        let newCode = before + character + after;

        if (!Main.paygManager.validateCode(newCode, true))
            return;

        this.clutter_text.insert_unichar(character);
    }

    setEnabled(value) {
        if (this._enabled === value)
            return;

        this._enabled = value;
        this.reactive = value;
        this.can_focus = value;
        this.clutter_text.reactive = value;
        this.clutter_text.editable = value;
        this.clutter_text.cursor_visible = value;
    }

    reset() {
        this.text = '';
    }

    get code() {
        return this._code;
    }

    get length() {
        return this._code.length;
    }
});

var PaygUnlockDialog = GObject.registerClass({
    Signals: {
        'failed': {},
        'wake-up-screen': {},
    },
}, class PaygUnlockDialog extends St.Widget {

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

        this._verificationStatus = UnlockStatus.NOT_VERIFYING;

        // Clear the clipboard to make sure nothing can be copied into the entry.
        St.Clipboard.get_default().set_text(St.ClipboardType.CLIPBOARD, '');
        St.Clipboard.get_default().set_text(St.ClipboardType.PRIMARY, '');

        this.add_constraint(new LayoutManager.MonitorConstraint({ primary: true }));

        this._parentActor.add_child(this);

        let mainBox = new St.BoxLayout({
            vertical: true,
            x_align: Clutter.ActorAlign.FILL,
            y_align: Clutter.ActorAlign.CENTER,
            x_expand: true,
            y_expand: true,
            style_class: 'unlock-dialog-payg-layout',
        });
        this.add_child(mainBox);

        let titleLabel = new St.Label({
            style_class: 'unlock-dialog-payg-title',
            text: _('Your Endless pay-as-you-go usage credit has expired.'),
            x_align: Clutter.ActorAlign.CENTER,
        });
        mainBox.add_child(titleLabel);

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
        mainBox.add_child(promptBox);

        let promptLabel = new St.Label({
            style_class: 'unlock-dialog-payg-label',
            text: _('Enter a new code to unlock your computer:'),
            x_align: Clutter.ActorAlign.START,
        });
        promptLabel.clutter_text.line_wrap = true;
        promptLabel.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        promptBox.add_child(promptLabel);

        this._entry = new PaygUnlockCodeEntry();
        promptBox.add_child(this._entry);

        this._errorMessage = new St.Label({
            opacity: 0,
            styleClass: 'unlock-dialog-payg-message',
        });
        this._errorMessage.clutter_text.line_wrap = true;
        this._errorMessage.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;
        promptBox.add_child(this._errorMessage);

        this._buttonBox = this._createButtonsArea();
        promptBox.add_child(this._buttonBox);

        let helpLineMain = new St.Label({
            style_class: 'unlock-dialog-payg-help-main',
            text: _('Don’t have an unlock code? That’s OK!'),
            x_align: Clutter.ActorAlign.START,
        });
        helpLineMain.clutter_text.line_wrap = true;
        helpLineMain.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

        promptBox.add_child(helpLineMain);

        let helpLineSub = new St.Label({
            style_class: 'unlock-dialog-payg-help-sub',
            text: _('Talk to your sales representative to purchase a new code.'),
            x_align: Clutter.ActorAlign.START,
        });
        helpLineSub.clutter_text.line_wrap = true;
        helpLineSub.clutter_text.ellipsize = Pango.EllipsizeMode.NONE;

        promptBox.add_child(helpLineSub);

        Main.ctrlAltTabManager.addGroup(promptBox, _('Unlock Machine'), 'dialog-password-symbolic');

        this._cancelButton.connect('clicked', () => {
            this._onCancelled();
        });
        this._nextButton.connect('clicked', () => {
            this._startVerifyingCode();
        });

        this._entry.connect('code-changed', () => {
            this._updateNextButtonSensitivity();
        });

        this._entry.clutter_text.connect('activate', () => {
            this._startVerifyingCode();
        });

        this._clearTooManyAttemptsId = 0;
        this.connect('destroy', this._onDestroy.bind(this));

        this._idleMonitor = Meta.IdleMonitor.get_core();
        this._idleWatchId = this._idleMonitor.add_idle_watch(IDLE_TIMEOUT_SECS * MSEC_PER_SEC, this._onCancelled.bind(this));

        this._updateSensitivity();
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
        this._spinner = new Animation.Spinner(SPINNER_ICON_SIZE_PIXELS, {
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

    _onCancelled() {
        this._cancelled = true;
        this._reset();

        // The ScreenShield will connect to the 'failed' signal
        // to know when to cancel the unlock dialog.
        if (this._verificationStatus !== UnlockStatus.SUCCEEDED)
            this.emit('failed');
    }

    _validateCurrentCode() {
        return Main.paygManager.validateCode(this._entry.code);
    }

    _updateNextButtonSensitivity() {
        let sensitive = this._validateCurrentCode() &&
            this._verificationStatus !== UnlockStatus.VERIFYING &&
            this._verificationStatus !== UnlockStatus.TOO_MANY_ATTEMPTS;

        this._nextButton.reactive = sensitive;
        this._nextButton.can_focus = sensitive;
    }

    _updateSensitivity() {
        let shouldEnableEntry = this._verificationStatus !== UnlockStatus.VERIFYING &&
            this._verificationStatus !== UnlockStatus.TOO_MANY_ATTEMPTS;

        this._updateNextButtonSensitivity();
        this._entry.setEnabled(shouldEnableEntry);
    }

    _setErrorMessage(message) {
        if (message) {
            this._errorMessage.text = message;
            this._errorMessage.opacity = 255;
        } else {
            this._errorMessage.text = '';
            this._errorMessage.opacity = 0;
        }
    }

    _reset() {
        this._spinner.stop();
        this._entry.reset();
        this._updateSensitivity();
    }

    _processError(error) {
        logError(error, 'Error adding PAYG code');

        // The 'too many errors' case is a bit special, and sets a different state.
        if (error.matches(PaygManager.PaygErrorDomain, PaygManager.PaygError.TOO_MANY_ATTEMPTS)) {
            let currentTime = GLib.get_real_time() / GLib.USEC_PER_SEC;
            let secondsLeft = Main.paygManager.rateLimitEndTime - currentTime;
            if (secondsLeft > 30) {
                let minutesLeft = Math.max(0, Math.ceil(secondsLeft / 60));
                this._setErrorMessage(
                    Gettext.ngettext(
                        'Too many attempts. Try again in %s minute.',
                        'Too many attempts. Try again in %s minutes.', minutesLeft).format(minutesLeft));
            } else {
                this._setErrorMessage(_('Too many attempts. Try again in a few seconds.'));
            }

            // Make sure to clean the status once the time is up (if this dialog is still alive)
            // and make sure that we install this callback at some point in the future (+1 sec).
            this._clearTooManyAttemptsId = GLib.timeout_add_seconds(
                GLib.PRIORITY_DEFAULT,
                Math.max(1, secondsLeft),
                () => {
                    this._verificationStatus = UnlockStatus.NOT_VERIFYING;
                    this._clearError();
                    this._updateSensitivity();
                    this._entry.grab_key_focus();
                    return GLib.SOURCE_REMOVE;
                });

            this._verificationStatus = UnlockStatus.TOO_MANY_ATTEMPTS;
            return;
        }

        // Common errors after this point.
        if (error.matches(PaygManager.PaygErrorDomain, PaygManager.PaygError.INVALID_CODE)) {
            this._setErrorMessage(_('Invalid code. Please try again.'));
        } else if (error.matches(PaygManager.PaygErrorDomain, PaygManager.PaygError.CODE_ALREADY_USED)) {
            this._setErrorMessage(_('Code already used. Please enter a new code.'));
        } else if (error.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.TIMED_OUT)) {
            this._setErrorMessage(_('Time exceeded while verifying the code'));
        } else {
            // We don't consider any other error here (and we don't consider DISABLED explicitly,
            // since that should not happen), but still we need to show something to the user.
            this._setErrorMessage(_('Unknown error'));
        }

        this._verificationStatus = UnlockStatus.FAILED;
    }

    _clearError() {
        this._setErrorMessage(null);
    }

    _addCodeCallback(error) {
        // We don't care about the result if we're closing the dialog.
        if (this._cancelled) {
            this._verificationStatus = UnlockStatus.NOT_VERIFYING;
            return;
        }

        if (error) {
            this._processError(error);
        } else {
            this._verificationStatus = UnlockStatus.SUCCEEDED;
            this._clearError();
        }

        this._reset();
    }

    _startVerifyingCode() {
        if (!this._validateCurrentCode())
            return;

        this._verificationStatus = UnlockStatus.VERIFYING;
        this._spinner.play();
        this._updateSensitivity();
        this._cancelled = false;

        Main.paygManager.addCode(this._entry.code, this._addCodeCallback.bind(this));
    }

    addCharacter(unichar) {
        this._entry.addCharacter(unichar);
    }

    cancel() {
        this._reset();
        this.destroy();
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
