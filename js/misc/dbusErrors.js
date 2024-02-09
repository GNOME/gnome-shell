import GLib from 'gi://GLib';
import Gio from 'gi://Gio';

function camelcase(str) {
    const words = str.toLowerCase().split('_');
    return words.map(w => `${w.at(0).toUpperCase()}${w.substring(1)}`).join('');
}

function decamelcase(str) {
    return str.replace(/(.)([A-Z])/g, '$1-$2');
}

function registerErrorDomain(domain, errorEnum, prefix = 'org.gnome.Shell') {
    const domainName =
        `shell-${decamelcase(domain).toLowerCase()}-error`;
    const quark = GLib.quark_from_string(domainName);

    for (const [name, code] of Object.entries(errorEnum)) {
        Gio.dbus_error_register_error(quark,
            code, `${prefix}.${domain}.Error.${camelcase(name)}`);
    }
    return quark;
}

export const ModalDialogError = {
    UNKNOWN_TYPE: 0,
    GRAB_FAILED: 1,
};
export const ModalDialogErrors =
    registerErrorDomain('ModalDialog', ModalDialogError);

export const NotificationError = {
    INVALID_APP: 0,
};
export const NotificationErrors =
    registerErrorDomain('Notifications', NotificationError, 'org.gtk');

export const ExtensionError = {
    INFO_DOWNLOAD_FAILED: 0,
    DOWNLOAD_FAILED: 1,
    EXTRACT_FAILED: 2,
    ENABLE_FAILED: 3,
    NOT_ALLOWED: 4,
};
export const ExtensionErrors =
    registerErrorDomain('Extensions', ExtensionError);

export const ScreencastError = {
    ALL_PIPELINES_FAILED: 0,
    PIPELINE_ERROR: 1,
    SAVE_TO_DISK_DISABLED: 2,
    ALREADY_RECORDING: 3,
    RECORDER_ERROR: 4,
    SERVICE_CRASH: 5,
    OUT_OF_DISK_SPACE: 6,
};
export const ScreencastErrors =
    registerErrorDomain('Screencast', ScreencastError);
