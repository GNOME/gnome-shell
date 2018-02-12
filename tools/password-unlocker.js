#!/usr/bin/gjs

// This script computes the "secret code" to perform a password reset.
// The first argument to the script should be the "verification code"
// displayed by the login screen.

const ByteArray = imports.byteArray;
const Format = imports.format;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Lang = imports.lang;

const RESET_CODE_LENGTH = 7;

String.prototype.format = Format.format;

if (ARGV.length != 1) {
    print('This script should be called with a reset code as the first and only argument');
} else if (ARGV[0].length != RESET_CODE_LENGTH) {
    print('Invalid reset code %s; valid reset codes have length %d'.format(ARGV[0], RESET_CODE_LENGTH));
} else if (ARGV[0].search(/\D/) != -1) {
    print('Invalid reset code %s; code should only contain digits'.format(ARGV[0]));
} else {
    let checksum = new GLib.Checksum(GLib.ChecksumType.MD5);
    checksum.update(ByteArray.fromString(ARGV[0]));

    let unlockCode = checksum.get_string();
    unlockCode = unlockCode.replace(/\D/g, '');
    unlockCode = unlockCode.slice(0, RESET_CODE_LENGTH);

    while (unlockCode.length < RESET_CODE_LENGTH)
        unlockCode += '0';

    print(unlockCode);
}
