// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
//
// Copyright (C) 2017, 2018 Endless Mobile, Inc.
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

const { GLib, GObject } = imports.gi;

const Config = imports.misc.config;

const CUSTOMER_SUPPORT_FILENAME = 'vendor-customer-support.ini';
const CUSTOMER_SUPPORT_LOCATIONS = [
    Config.LOCALSTATEDIR + '/lib/eos-image-defaults',
    Config.PKGDATADIR,
];

const CUSTOMER_SUPPORT_GROUP_NAME = 'Customer Support';
const CUSTOMER_SUPPORT_KEY_EMAIL = 'Email';

const PASSWORD_RESET_GROUP_NAME = 'Password Reset';
const PASSWORD_RESET_KEY_SALT = 'Salt';

const PAYG_GROUP_NAME = 'Pay As You Go';
const PAYG_KEY_CONTACT_NAME = 'ContactName';
const PAYG_KEY_CONTACT_NUMBER = 'ContactPhoneNumber';
const PAYG_KEY_INSTRUCTIONS_LINE1 = 'InstructionsLine1';
const PAYG_KEY_INSTRUCTIONS_LINE2 = 'InstructionsLine2';

var CustomerSupport = GObject.registerClass(
class CustomerSupport extends GObject.Object {

    _init() {
        super._init();
        this._customerSupportKeyFile = null;
    }

    _ensureCustomerSupportFile() {
        if (this._customerSupportKeyFile)
            return this._customerSupportKeyFile;

        this._customerSupportKeyFile = new GLib.KeyFile();
        try {
            this._customerSupportKeyFile.load_from_dirs(CUSTOMER_SUPPORT_FILENAME,
                                                        CUSTOMER_SUPPORT_LOCATIONS,
                                                        GLib.KeyFileFlags.NONE);
        } catch (e) {
            logError(e, "Failed to read customer support data");
        }
    }

    _getLocaleString(groupName, keyName) {
        this._ensureCustomerSupportFile();

        try {
            return this._customerSupportKeyFile.get_locale_string(groupName, keyName, null);
        } catch (e) {
            if (e.matches(GLib.KeyFileError, GLib.KeyFileError.KEY_NOT_FOUND))
                log("Key '" + keyName + "' from group '" + groupName + "' does not exist");
            else if (e.matches(GLib.KeyFileError, GLib.KeyFileError.GROUP_NOT_FOUND))
                log("Group '" + groupName + "' does not exist");
            else
                logError(e, "Failed to read key '" + keyName + "' from group '" + groupName + "'");

            return null;
        }
    }

    get customerSupportEmail() {
        if (this._supportEmail === undefined) {
            this._supportEmail = this._getLocaleString(CUSTOMER_SUPPORT_GROUP_NAME,
                                                       CUSTOMER_SUPPORT_KEY_EMAIL);
        }

        return this._supportEmail;
    }

    get passwordResetSalt() {
        if (this._passwordResetSalt === undefined) {
            this._passwordResetSalt = this._getLocaleString(PASSWORD_RESET_GROUP_NAME,
                                                            PASSWORD_RESET_KEY_SALT);
        }

        return this._passwordResetSalt;
    }

    get paygContactName() {
        if (this._paygContactName === undefined) {
            this._paygContactName = this._getLocaleString(PAYG_GROUP_NAME,
                                                          PAYG_KEY_CONTACT_NAME);
        }

        return this._paygContactName;
    }

    get paygContactNumber() {
        if (this._paygContactNumber === undefined) {
            this._paygContactNumber = this._getLocaleString(PAYG_GROUP_NAME,
                                                            PAYG_KEY_CONTACT_NUMBER);
        }

        return this._paygContactNumber;
    }

    get paygInstructionsLine1() {
        if (this._paygInstructionsLine1 === undefined) {
            this._paygInstructionsLine1 = this._getLocaleString(PAYG_GROUP_NAME,
                                                                PAYG_KEY_INSTRUCTIONS_LINE1);
        }

        return this._paygInstructionsLine1;
    }

    get paygInstructionsLine2() {
        if (this._paygInstructionsLine2 === undefined) {
            this._paygInstructionsLine2 = this._getLocaleString(PAYG_GROUP_NAME,
                                                                PAYG_KEY_INSTRUCTIONS_LINE2);
        }

        return this._paygInstructionsLine2;
    }
});
