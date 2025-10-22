import GLib from 'gi://GLib';
import Gio from 'gi://Gio';

import {logErrorUnlessCancelled} from 'resource:///org/gnome/shell/misc/errorUtils.js';

describe('logErrorUnlessCancelled()', () => {
    let spy;

    beforeEach(() => {
        spy = spyOn(globalThis, 'logError');
    });

    it('logs regular errors', () => {
        const e = new Error('Test error');
        expect(logErrorUnlessCancelled(e)).toBeTrue();
        expect(spy).toHaveBeenCalled();
    });

    it('logs GErrors', () => {
        const e = GLib.Error.new_literal(
            Gio.IOErrorEnum, Gio.IOErrorEnum.FAILED, 'Failed');
        expect(logErrorUnlessCancelled(e)).toBeTrue();
        expect(spy).toHaveBeenCalled();
    });

    it('does not log CANCELLED errors', () => {
        const e = GLib.Error.new_literal(
            Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED, 'Cancelled');
        expect(logErrorUnlessCancelled(e)).toBeFalse();
        expect(spy).not.toHaveBeenCalled();
    });

    it('handles remote errors', () => {
        function createDBusError(code, message) {
            const e = GLib.Error.new_literal(
                Gio.IOErrorEnum, code, message);
            return Gio.DBusError.new_for_dbus_error(
                Gio.DBusError.encode_gerror(e), message);
        }

        let e = createDBusError(Gio.IOErrorEnum.CANCELLED, 'Cancelled');
        expect(logErrorUnlessCancelled(e)).toBeFalse();
        expect(spy).not.toHaveBeenCalled();

        e = createDBusError(Gio.IOErrorEnum.FAILED, 'Failed');
        expect(logErrorUnlessCancelled(e)).toBeTrue();
        expect(spy).toHaveBeenCalled();
    });
});
