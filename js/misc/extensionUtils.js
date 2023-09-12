// Common utils for the extension system, the extensions D-Bus service
// and the Extensions app

import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

export const ExtensionType = {
    SYSTEM: 1,
    PER_USER: 2,
};

/**
 * @enum {number}
 */
export const ExtensionState = {
    ACTIVE: 1,
    INACTIVE: 2,
    ERROR: 3,
    OUT_OF_DATE: 4,
    DOWNLOADING: 5,
    INITIALIZED: 6,
    DEACTIVATING: 7,
    ACTIVATING: 8,

    // Used as an error state for operations on unknown extensions,
    // should never be in a real extensionMeta object.
    UNINSTALLED: 99,
};

const SERIALIZED_PROPERTIES = [
    'type',
    'state',
    'enabled',
    'path',
    'error',
    'hasPrefs',
    'hasUpdate',
    'canChange',
];

/**
 * Serialize extension into an object that can be used
 * in a vardict {GLib.Variant}
 *
 * @param {object} extension - an extension object
 * @returns {object}
 */
export function serializeExtension(extension) {
    let obj = {...extension.metadata};

    SERIALIZED_PROPERTIES.forEach(prop => {
        obj[prop] = extension[prop];
    });

    let res = {};
    for (let key in obj) {
        let val = obj[key];
        let type;
        switch (typeof val) {
        case 'string':
            type = 's';
            break;
        case 'number':
            type = 'd';
            break;
        case 'boolean':
            type = 'b';
            break;
        default:
            continue;
        }
        res[key] = GLib.Variant.new(type, val);
    }

    return res;
}

/**
 * Deserialize an unpacked variant into an extension object
 *
 * @param {object} variant - an unpacked {GLib.Variant}
 * @returns {object}
 */
export function deserializeExtension(variant) {
    let res = {metadata: {}};
    for (let prop in variant) {
        let val = variant[prop].unpack();
        if (SERIALIZED_PROPERTIES.includes(prop))
            res[prop] = val;
        else
            res.metadata[prop] = val;
    }
    // add the 2 additional properties to create a valid extension object, as createExtensionObject()
    res.uuid = res.metadata.uuid;
    res.dir = Gio.File.new_for_path(res.path);
    return res;
}
