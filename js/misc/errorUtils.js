// Common code for displaying errors to the user in various dialogs
import Gio from 'gi://Gio';
import GLib from 'gi://GLib';

function formatSyntaxErrorLocation(error) {
    const {fileName = '<unknown>', lineNumber = 0, columnNumber = 0} = error;
    return ` @ ${fileName}:${lineNumber}:${columnNumber}`;
}

function formatExceptionStack(error) {
    const {stack} = error;
    if (!stack)
        return '\n\n(No stack trace)';

    const indentedStack = stack.split('\n').map(line => `  ${line}`).join('\n');
    return `\n\nStack trace:\n${indentedStack}`;
}

function formatExceptionWithCause(error, seenCauses, showStack) {
    let fmt = showStack ? formatExceptionStack(error) : '';

    const {cause} = error;
    if (!cause)
        return fmt;

    fmt += `\nCaused by: ${cause}`;

    if (cause !== null && typeof cause === 'object') {
        if (seenCauses.has(cause))
            return fmt;  // avoid recursion
        seenCauses.add(cause);

        fmt += formatExceptionWithCause(cause, seenCauses);
    }

    return fmt;
}

/**
 * Formats a thrown exception into a string, including the stack, taking the
 * location where a SyntaxError was thrown into account.
 *
 * @param {Error} error The error to format
 * @param {object} options Formatting options
 * @param {boolean} options.showStack Whether to show the stack trace (default
 *   true)
 * @returns {string} The formatted string
 */
export function formatError(error, {showStack = true} = {}) {
    try {
        let fmt = `${error}`;
        if (error === null || typeof error !== 'object')
            return fmt;

        if (error instanceof SyntaxError) {
            fmt += formatSyntaxErrorLocation(error);
            if (showStack)
                fmt += formatExceptionStack(error);
            return fmt;
        }

        const seenCauses = new Set([error]);
        fmt += formatExceptionWithCause(error, seenCauses, showStack);
        return fmt;
    } catch (e) {
        return `(could not display error: ${e})`;
    }
}

/**
 * Logs a stack trace for `error` with an optional prefix, unless the error
 * matches `Gio.IOErrorEnum.CANCELLED`.
 *
 * @param {Error} error - the error to log
 * @param {string?} prefix - optional prefix for the message
 *
 * @returns {boolean} whether the error was logged
 */
export function logErrorUnlessCancelled(error, prefix) {
    if (error instanceof GLib.Error && Gio.DBusError.is_remote_error(error)) {
        const errorName = Gio.DBusError.get_remote_error(error);
        Gio.DBusError.strip_remote_error(error);
        error = Gio.DBusError.new_for_dbus_error(errorName, error.message);
    }

    if (error.matches(Gio.IOErrorEnum, Gio.IOErrorEnum.CANCELLED))
        return false;

    logError(error, prefix);
    return true;
}
