import GUdev from 'gi://GUdev';
import * as Signals from './signals.js';

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
        const isFido = device.get_property_as_int('ID_FIDO_TOKEN') === 1;
        if (this._insertedPasskeys.has(sysfsPath) || !isFido)
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
}
