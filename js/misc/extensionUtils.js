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
    'sessionModes',
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

    function packValue(val) {
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
        case 'object':
            if (Array.isArray(val)) {
                type = 'av';
                val = val.map(v => packValue(v));
            } else {
                type = 'a{sv}';
                let res = {};
                for (let key in val) {
                    let packed = packValue(val[key]);
                    if (packed)
                        res[key] = packed;
                }
                val = res;
            }
            break;
        default:
            return null;
        }
        return GLib.Variant.new(type, val);
    }

    return packValue(obj).deepUnpack();
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
        let val = variant[prop].recursiveUnpack();
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

/**
 * Load extension metadata from directory
 *
 * @param {string} uuid of the extension
 * @param {GioFile} dir to load metadata from
 * @returns {object}
 */
export function loadExtensionMetadata(uuid, dir) {
    const dirName = dir.get_basename();
    if (dirName !== uuid)
        throw new Error(`Directory name "${dirName}" does not match UUID "${uuid}"`);

    const metadataFile = dir.get_child('metadata.json');
    if (!metadataFile.query_exists(null))
        throw new Error('Missing metadata.json');

    let metadataContents, success_;
    try {
        [success_, metadataContents] = metadataFile.load_contents(null);
        metadataContents = new TextDecoder().decode(metadataContents);
    } catch (e) {
        throw new Error(`Failed to load metadata.json: ${e}`);
    }
    let meta;
    try {
        meta = JSON.parse(metadataContents);
    } catch (e) {
        throw new Error(`Failed to parse metadata.json: ${e}`);
    }

    const requiredProperties = [{
        prop: 'uuid',
        typeName: 'string',
    }, {
        prop: 'name',
        typeName: 'string',
    }, {
        prop: 'description',
        typeName: 'string',
    }, {
        prop: 'shell-version',
        typeName: 'string array',
        typeCheck: v => Array.isArray(v) && v.length > 0 && v.every(e => typeof e === 'string'),
    }];
    for (let i = 0; i < requiredProperties.length; i++) {
        const {
            prop, typeName, typeCheck = v => typeof v === typeName,
        } = requiredProperties[i];

        if (!meta[prop])
            throw new Error(`missing "${prop}" property in metadata.json`);
        if (!typeCheck(meta[prop]))
            throw new Error(`property "${prop}" is not of type ${typeName}`);
    }

    if (uuid !== meta.uuid)
        throw new Error(`UUID "${meta.uuid}" from metadata.json does not match directory name "${uuid}"`);

    return meta;
}
