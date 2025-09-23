import GObject from 'gi://GObject';
import GUdev from 'gi://GUdev';

let _passkeyDeviceManager = null;

/**
 * @returns {PasskeyDeviceManager}
 */
export function getPasskeyDeviceManager() {
    if (_passkeyDeviceManager == null)
        _passkeyDeviceManager = new PasskeyDeviceManager();

    return _passkeyDeviceManager;
}

class PasskeyDeviceManager extends GObject.Object {
    static [GObject.GTypeName] = 'PasskeyDeviceManager';

    static [GObject.signals] = {
        'passkey-inserted': {param_types: [GObject.TYPE_JSOBJECT]},
        'passkey-removed': {param_types: [GObject.TYPE_JSOBJECT]},
    };

    static {
        GObject.registerClass(this);
    }

    constructor() {
        super();

        this._insertedPasskeys = new Map();
        this._udevClient = new GUdev.Client({subsystems: ['hidraw']});

        this._onLoaded();
    }

    get hasInsertedPasskeys() {
        return this._insertedPasskeys.size > 0;
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
        if (!isFido || this._insertedPasskeys.has(sysfsPath))
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
