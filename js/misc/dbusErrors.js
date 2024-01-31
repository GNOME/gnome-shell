import GLib from 'gi://GLib';
import Gio from 'gi://Gio';

function decamelcase(str, sep) {
    return str.replace(/(.)([A-Z])/g, `$1${sep}$2`);
}

function registerErrorDomain(domain, errorNames, prefix = 'org.gnome.Shell') {
    const domainName =
        `shell-${decamelcase(domain, '-').toLowerCase()}-error`;
    const quark = GLib.quark_from_string(domainName);

    for (const [code, name] of errorNames.entries()) {
        Gio.dbus_error_register_error(quark,
            code, `${prefix}.${domain}.Error.${name}`);
    }
    return quark;
}

function createErrorEnum(errorNames) {
    const obj = {};

    for (const [code, name] of errorNames.entries()) {
        const propName = decamelcase(name, '_').toUpperCase();
        obj[propName] = code;
    }
    return obj;
}

const modalDialogErrorNames = [
    'UnknownType',
    'GrabFailed',
];
export const ModalDialogErrors =
    registerErrorDomain('ModalDialog', modalDialogErrorNames);
export const ModalDialogError = createErrorEnum(modalDialogErrorNames);
