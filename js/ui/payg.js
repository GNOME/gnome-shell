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

/* exported PaygUnlockUi, SPINNER_ICON_SIZE_PIXELS, SUCCESS_DELAY_SECONDS */

const { Clutter, Gio, GLib, GObject, Shell, St } = imports.gi;

const PaygManager = imports.misc.paygManager;

const Gettext = imports.gettext;
const Main = imports.ui.main;

var SUCCESS_DELAY_SECONDS = 3;

var SPINNER_ICON_SIZE_PIXELS = 16;
var SPINNER_ANIMATION_DELAY_MSECS = 1000;
var SPINNER_ANIMATION_TIME_MSECS = 300;

var UnlockStatus = {
    NOT_VERIFYING: 0,
    VERIFYING: 1,
    FAILED: 2,
    TOO_MANY_ATTEMPTS: 3,
    SUCCEEDED: 4,
};

var PaygUnlockUi = GObject.registerClass(
class PaygUnlockUi extends St.Widget {

    // the following properties and functions are required for any subclasses of
    // this class

    // properties
    // -----------
    // applyButton
    // entryCode
    // spinner
    // verificationStatus

    // functions
    // ----------
    // entryReset
    // entrySetEnabled
    // onCodeAdded
    // reset

    _init(params = {}) {
        super._init(params);
        this._clearTooManyAttemptsId = 0;
        this.connect('destroy', this._onDestroy.bind(this));
    }

    updateApplyButtonSensitivity() {
        let sensitive = this.validateCurrentCode() &&
            this.verificationStatus !== UnlockStatus.VERIFYING &&
            this.verificationStatus !== UnlockStatus.SUCCEEDED &&
            this.verificationStatus !== UnlockStatus.TOO_MANY_ATTEMPTS;

        this.applyButton.reactive = sensitive;
        this.applyButton.can_focus = sensitive;
    }

    updateSensitivity() {
        let shouldEnableEntry =
            this.verificationStatus !== UnlockStatus.VERIFYING &&
            this.verificationStatus !== UnlockStatus.SUCCEEDED &&
            this.verificationStatus !== UnlockStatus.TOO_MANY_ATTEMPTS;

        this.updateApplyButtonSensitivity();
        this.entrySetEnabled(shouldEnableEntry);
    }

    processError(error) {
        logError(error, 'Error adding PAYG code');

        // The 'too many errors' case is a bit special, and sets a different state.
        if (error.matches(PaygManager.PaygErrorDomain, PaygManager.PaygError.TOO_MANY_ATTEMPTS)) {
            let currentTime = Shell.util_get_boottime() / GLib.USEC_PER_SEC;
            let secondsLeft = Main.paygManager.rateLimitEndTime - currentTime;
            if (secondsLeft > 30) {
                let minutesLeft = Math.max(0, Math.ceil(secondsLeft / 60));
                this.setErrorMessage(
                    Gettext.ngettext(
                        'Too many attempts. Try again in %s minute.',
                        'Too many attempts. Try again in %s minutes.', minutesLeft)
                        .format(minutesLeft));
            } else {
                this.setErrorMessage(_('Too many attempts. Try again in a few seconds.'));
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
                    return GLib.SOURCE_REMOVE;
                });

            this.verificationStatus = UnlockStatus.TOO_MANY_ATTEMPTS;
            return;
        }

        // Common errors after this point.
        if (error.matches(PaygManager.PaygErrorDomain, PaygManager.PaygError.INVALID_CODE)) {
            this.setErrorMessage(_('Invalid code. Please try again.'));
        } else if (error.matches(PaygManager.PaygErrorDomain, PaygManager.PaygError.CODE_ALREADY_USED)) {
            this.setErrorMessage(_('Code already used. Please enter a new code.'));
        } else if (error.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.TIMED_OUT)) {
            this.setErrorMessage(_('Time exceeded while verifying the code'));
        } else {
            // We don't consider any other error here (and we don't consider DISABLED explicitly,
            // since that should not happen), but still we need to show something to the user.
            this.setErrorMessage(_('Unknown error'));
        }

        this.verificationStatus = UnlockStatus.FAILED;
    }

    _onDestroy() {
        if (this._clearTooManyAttemptsId > 0) {
            GLib.source_remove(this._clearTooManyAttemptsId);
            this._clearTooManyAttemptsId = 0;
        }
    }

    setErrorMessage(message) {
        if (message) {
            this.errorLabel.text = message;
            this.errorLabel.opacity = 255;
        } else {
            this.errorLabel.text = '';
            this.errorLabel.opacity = 0;
        }
    }

    clearError() {
        this.setErrorMessage(null);
    }

    startSpinning() {
        this.spinner.play();
        this.spinner.actor.show();
        this.spinner.actor.ease({
            opacity: 255,
            delay: SPINNER_ANIMATION_DELAY_MSECS,
            duration: SPINNER_ANIMATION_TIME_MSECS,
            mode: Clutter.AnimationMode.LINEAR,
        });
    }

    stopSpinning() {
        this.spinner.actor.hide();
        this.spinner.actor.opacity = 0;
        this.spinner.stop();
    }

    reset() {
        this.stopSpinning();
        this.entryReset();
        this.updateSensitivity();
    }

    validateCurrentCode() {
        return Main.paygManager.validateCode(this.entryCode);
    }

    startVerifyingCode() {
        if (!this.validateCurrentCode())
            return;

        this.verificationStatus = UnlockStatus.VERIFYING;
        this.startSpinning();
        this.updateSensitivity();
        this.cancelled = false;

        Main.paygManager.addCode(this.entryCode, error => {
            // We don't care about the result if we're closing the dialog.
            if (this.cancelled) {
                this.verificationStatus = UnlockStatus.NOT_VERIFYING;
                return;
            }

            if (error) {
                this.processError(error);
            } else {
                this.verificationStatus = UnlockStatus.SUCCEEDED;
                this.onCodeAdded();
            }

            this.reset();
        });
    }
});
