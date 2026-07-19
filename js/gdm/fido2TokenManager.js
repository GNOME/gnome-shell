import GObject from 'gi://GObject';
import GUdev from 'gi://GUdev';

let _fido2TokenManager = null;

/**
 * @returns {Fido2TokenManager}
 */
export function getFido2TokenManager() {
    if (_fido2TokenManager == null)
        _fido2TokenManager = new Fido2TokenManager();

    return _fido2TokenManager;
}

class Fido2TokenManager extends GObject.Object {
    static [GObject.signals] = {
        'fido2-token-inserted': {param_types: [GObject.TYPE_JSOBJECT]},
        'fido2-token-removed': {param_types: [GObject.TYPE_JSOBJECT]},
    };

    static {
        GObject.registerClass(this);
    }

    constructor() {
        super();

        this._insertedTokens = new Map();
        this._udevClient = new GUdev.Client({subsystems: ['hidraw']});

        this._onLoaded();
    }

    get hasInsertedTokens() {
        return this._insertedTokens.size > 0;
    }

    _onLoaded() {
        this._udevClient.query_by_subsystem('hidraw')
            .forEach(d => this._addToken(d));

        this._udevClient.connect('uevent', (_, action, device) => {
            if (action === 'add')
                this._addToken(device);
            else if (action === 'remove')
                this._removeToken(device);
        });
    }

    _addToken(device) {
        const sysfsPath = device.get_sysfs_path();
        const isFido = device.get_property_as_int('ID_FIDO_TOKEN') === 1;
        if (!isFido || this._insertedTokens.has(sysfsPath))
            return;

        this._insertedTokens.set(sysfsPath, device);
        this.emit('fido2-token-inserted', device);
    }

    _removeToken(device) {
        const sysfsPath = device.get_sysfs_path();
        if (!this._insertedTokens.has(sysfsPath))
            return;

        this._insertedTokens.delete(sysfsPath);
        this.emit('fido2-token-removed', device);
    }
}
