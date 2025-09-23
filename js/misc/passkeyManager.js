import Gio from 'gi://Gio';
import GUdev from 'gi://GUdev';
import * as Signals from './signals.js';

const HID_RPTDESC_FIRST_BYTE_LONG_ITEM = 0xfe;
const HID_RPTDESC_TYPE_GLOBAL = 0x1;
const HID_RPTDESC_TYPE_LOCAL = 0x2;
const HID_RPTDESC_TAG_USAGE_PAGE = 0x0;
const HID_RPTDESC_TAG_USAGE = 0x0;
const FIDO_FULL_USAGE_CTAPHID = 0xf1d00001;

let _passkeyManager = null;

/**
 * @returns {PasskeyManager}
 */
export function getPasskeyManager() {
    if (_passkeyManager == null)
        _passkeyManager = new PasskeyManager();

    return _passkeyManager;
}

class PasskeyManager extends Signals.EventEmitter {
    constructor() {
        super();

        this._insertedPasskeys = new Map();
        this._udevClient = new GUdev.Client({subsystems: ['hidraw']});

        this._onLoaded();
    }

    hasInsertedPasskeys() {
        return Object.keys(this._insertedPasskeys).length > 0;
    }

    _onLoaded() {
        this._udevClient.query_by_subsystem('hidraw')
            .forEach(d => this._addPasskey(d));

        this._udevClient.connect('uevent', (_, action, device) => {
            if (action === 'add')
                this._addPasskey(device);
            else if (action === 'remove')
                this._removePasskey(device);
        });
    }

    _addPasskey(device) {
        const sysfsPath = device.get_sysfs_path();
        if (this._insertedPasskeys.has(sysfsPath) ||
            !this._isFidoDevice(sysfsPath))
            return;

        this._insertedPasskeys.set(sysfsPath, device);
        this.emit('passkey-inserted', device);
    }

    _removePasskey(device) {
        const sysfsPath = device.get_sysfs_path();
        if (!this._insertedPasskeys.has(sysfsPath))
            return;

        this._insertedPasskeys.delete(sysfsPath);
        this.emit('passkey-removed', device);
    }

    _isFidoDevice(sysfsPath) {
        try {
            const reportDescPath = `${sysfsPath}/device/report_descriptor`;
            const reportDescFile = Gio.File.new_for_path(reportDescPath);
            if (!reportDescFile.query_exists(null))
                return false;

            const [success, descriptor] = reportDescFile.load_contents(null);
            if (!success || descriptor.length < 0)
                return false;

            let usage = 0;
            let pos = 0;
            while (pos < descriptor.length) {
                // Handle long items (not defined in spec, skip them)
                if (descriptor[pos] === HID_RPTDESC_FIRST_BYTE_LONG_ITEM) {
                    if (pos + 1 >= descriptor.length)
                        return false;
                    pos += descriptor[pos + 1] + 3;
                    continue;
                }

                // Header byte
                const tag = descriptor[pos] >> 4;
                const type = (descriptor[pos] >> 2) & 0x3;
                const sizeCode = descriptor[pos] & 0x3;
                const size = sizeCode < 3 ? sizeCode : 4;
                pos++;

                // Value bytes
                if (pos + size > descriptor.length)
                    return false;

                let value = 0;
                for (let i = 0; i < size; i++)
                    value |= descriptor[pos + i] << (8 * i);
                pos += size;

                // A usage page is a 16 bit value coded on at most 16 bit
                if (type === HID_RPTDESC_TYPE_GLOBAL && tag === HID_RPTDESC_TAG_USAGE_PAGE) {
                    if (size > 2)
                        return false;

                    usage = ((value & 0x0000ffff) << 16) >>> 0;
                }

                // A usage is a 32 bit value, but is prepended with the current usage page if
                // coded on less than 4 bytes (that is, at most 2 bytes)
                if (type === HID_RPTDESC_TYPE_LOCAL && tag === HID_RPTDESC_TAG_USAGE) {
                    if (size === 4)
                        usage = value >>> 0;
                    else
                        usage = ((usage & 0xffff0000) | (value & 0x0000ffff)) >>> 0;

                    if (usage === FIDO_FULL_USAGE_CTAPHID)
                        return true;
                }
            }
        } catch (e) {
            logError(e);
        }
        return false;
    }
}
