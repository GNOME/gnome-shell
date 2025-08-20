import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

import * as Signals from './signals.js';

import {loadInterfaceXML} from './fileUtils.js';

const FprintManagerInfo = Gio.DBusInterfaceInfo.new_for_xml(
    loadInterfaceXML('net.reactivated.Fprint.Manager'));
const FprintDeviceInfo = Gio.DBusInterfaceInfo.new_for_xml(
    loadInterfaceXML('net.reactivated.Fprint.Device'));

export const FingerprintReaderType = {
    NONE: 0,
    PRESS: 1,
    SWIPE: 2,
};

export class FingerprintManager extends Signals.EventEmitter {
    constructor(cancellable) {
        super();

        this._fingerprintManagerProxy = new Gio.DBusProxy({
            g_connection: Gio.DBus.system,
            g_name: 'net.reactivated.Fprint',
            g_object_path: '/net/reactivated/Fprint/Manager',
            g_interface_name: FprintManagerInfo.name,
            g_interface_info: FprintManagerInfo,
            g_flags: Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES |
                Gio.DBusProxyFlags.DO_NOT_AUTO_START_AT_CONSTRUCTION |
                Gio.DBusProxyFlags.DO_NOT_CONNECT_SIGNALS,
        });

        this._fingerprintReaderType = FingerprintReaderType.NONE;

        this._initFingerprintManagerProxy(cancellable);
    }

    get readerType() {
        return this._fingerprintReaderType;
    }

    get readerFound() {
        return this._fingerprintReaderFound;
    }

    setDefaultTimeout(timeout) {
        this._fingerprintManagerProxy.set_default_timeout(timeout);
    }

    async checkReaderType(cancellable) {
        try {
            // Wrappers don't support null cancellable, so let's ignore it in case
            const args = cancellable ? [cancellable] : [];
            const [devicePath] =
                await this._fingerprintManagerProxy.GetDefaultDeviceAsync(...args);

            const fprintDeviceProxy = this._getFprintDeviceProxy(devicePath);
            await fprintDeviceProxy.init_async(GLib.PRIORITY_DEFAULT, cancellable ?? null);

            const fingerprintReaderKey = fprintDeviceProxy['scan-type'].toUpperCase();
            const fingerprintReaderType = FingerprintReaderType[fingerprintReaderKey];
            this._setFingerprintReaderType(fingerprintReaderType);
        } catch (e) {
            this._handleFingerprintError(e);
        }
    }

    async _initFingerprintManagerProxy(cancellable) {
        try {
            await this._fingerprintManagerProxy.init_async(
                GLib.PRIORITY_DEFAULT, cancellable ?? null);
            await this.checkReaderType(cancellable);
        } catch (e) {
            this._handleFingerprintError(e);
        }
    }

    _getFprintDeviceProxy(devicePath) {
        return new Gio.DBusProxy({
            g_connection: Gio.DBus.system,
            g_name: 'net.reactivated.Fprint',
            g_object_path: devicePath,
            g_interface_name: FprintDeviceInfo.name,
            g_interface_info: FprintDeviceInfo,
            g_flags: Gio.DBusProxyFlags.DO_NOT_CONNECT_SIGNALS,
        });
    }

    _setFingerprintReaderType(fingerprintReaderType) {
        if (this._fingerprintReaderType === fingerprintReaderType)
            return;

        this._fingerprintReaderType = fingerprintReaderType;

        this._fingerprintReaderFound =
            !!this._fingerprintReaderType &&
            this._fingerprintReaderType !== FingerprintReaderType.NONE;

        if (this._fingerprintReaderType === undefined)
            throw new Error(`Unexpected fingerprint device type '${fingerprintReaderType}'`);

        this.emit('reader-type-changed');
    }

    _handleFingerprintError(e) {
        this._setFingerprintReaderType(FingerprintReaderType.NONE);

        if (e instanceof GLib.Error) {
            if (e.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
                return;
            if (e.matches(Gio.DBusError, Gio.DBusError.SERVICE_UNKNOWN))
                return;
            if (Gio.DBusError.is_remote_error(e) &&
                Gio.DBusError.get_remote_error(e) ===
                    'net.reactivated.Fprint.Error.NoSuchDevice')
                return;
        }

        logError(e, 'Failed to interact with fprintd service');
    }
}
