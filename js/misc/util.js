import Gio from 'gi://Gio';
import GLib from 'gi://GLib';
import Pango from 'gi://Pango';
import Shell from 'gi://Shell';
import St from 'gi://St';
import GnomeDesktop from 'gi://GnomeDesktop';

import {formatTime} from './dateUtils.js';

// http://daringfireball.net/2010/07/improved_regex_for_matching_urls
const _balancedParens = '\\([^\\s()<>]+\\)';
const _leadingJunk = '[\\s`(\\[{\'\\"<\u00AB\u201C\u2018]';
const _notTrailingJunk = '[^\\s`!()\\[\\]{};:\'\\".,<>?\u00AB\u00BB\u200E\u200F\u201C\u201D\u2018\u2019\u202A\u202C]';

const _urlRegexp = new RegExp(
    `(^|${_leadingJunk})` +
    '(' +
        '(?:' +
            '(?:http|https|ftp)://' +             // scheme://
            '|' +
            'www\\d{0,3}[.]' +                    // www.
            '|' +
            '([a-z0-9\\-]+[.])+[a-z]{2,4}/' +     // foo.xx/
        ')' +
        '(?:' +                                   // one or more:
            '[^\\s()<>]+' +                       // run of non-space non-()
            '|' +                                 // or
            `${_balancedParens}` +                // balanced parens
        ')+' +
        '(?:' +                                   // end with:
            `${_balancedParens}` +                // balanced parens
            '|' +                                 // or
            `${_notTrailingJunk}` +               // last non-junk char
        ')' +
    ')', 'gi');

let _desktopSettings = null;

/**
 * findUrls:
 *
 * @param {string} str string to find URLs in
 *
 * Searches `str` for URLs and returns an array of objects with %url
 * properties showing the matched URL string, and %pos properties indicating
 * the position within `str` where the URL was found.
 *
 * @returns {{url: string, pos: number}[]} the list of match objects, as described above
 */
export function findUrls(str) {
    let res = [], match;
    while ((match = _urlRegexp.exec(str)))
        res.push({url: match[2], pos: match.index + match[1].length});
    return res;
}

/**
 * spawn:
 *
 * Runs `argv` in the background, handling any errors that occur
 * when trying to start the program.
 *
 * @param {readonly string[]} argv an argv array
 */
export function spawn(argv) {
    try {
        trySpawn(argv);
    } catch (err) {
        _handleSpawnError(argv[0], err);
    }
}

/**
 * spawnCommandLine:
 *
 * @param {readonly string[]} commandLine a command line
 *
 * Runs commandLine in the background, handling any errors that
 * occur when trying to parse or start the program.
 */
export function spawnCommandLine(commandLine) {
    try {
        let [success_, argv] = GLib.shell_parse_argv(commandLine);
        trySpawn(argv);
    } catch (err) {
        _handleSpawnError(commandLine, err);
    }
}

/**
 * spawnApp:
 *
 * @param {readonly string[]} argv an argv array
 *
 * Runs argv as if it was an application, handling startup notification
 */
export function spawnApp(argv) {
    try {
        const app = Gio.AppInfo.create_from_commandline(argv.join(' '),
            null,
            Gio.AppInfoCreateFlags.SUPPORTS_STARTUP_NOTIFICATION);

        let context = global.create_app_launch_context(0, -1);
        app.launch([], context);
    } catch (err) {
        _handleSpawnError(argv[0], err);
    }
}

/**
 * trySpawn:
 *
 * @param {readonly string[]} argv an argv array
 *
 * Runs argv in the background. If launching argv fails,
 * this will throw an error.
 */
export function trySpawn(argv) {
    let pid;
    try {
        const launchContext = global.create_app_launch_context(0, -1);
        pid = Shell.util_spawn_async(
            null, argv, launchContext.get_environment(),
            GLib.SpawnFlags.SEARCH_PATH | GLib.SpawnFlags.DO_NOT_REAP_CHILD);
    } catch (err) {
        /* Rewrite the error in case of ENOENT */
        if (err.matches(GLib.SpawnError, GLib.SpawnError.NOENT)) {
            throw new GLib.SpawnError({
                code: GLib.SpawnError.NOENT,
                message: _('Command not found'),
            });
        } else if (err instanceof GLib.Error) {
            // The exception from gjs contains an error string like:
            //   Error invoking GLib.spawn_command_line_async: Failed to
            //   execute child process "foo" (No such file or directory)
            // We are only interested in the part in the parentheses. (And
            // we can't pattern match the text, since it gets localized.)
            let message = err.message.replace(/.*\((.+)\)/, '$1');
            throw new err.constructor({code: err.code, message});
        } else {
            throw err;
        }
    }

    // Async call, we don't need the reply though
    GnomeDesktop.start_systemd_scope(argv[0], pid, null, null, null, () => {});

    // Dummy child watch; we don't want to double-fork internally
    // because then we lose the parent-child relationship, which
    // can break polkit.  See https://bugzilla.redhat.com//show_bug.cgi?id=819275
    GLib.child_watch_add(GLib.PRIORITY_DEFAULT, pid, () => {});
}

/**
 * trySpawnCommandLine:
 *
 * @param {readonly string[]} commandLine a command line
 *
 * Runs commandLine in the background. If launching commandLine
 * fails, this will throw an error.
 */
export function trySpawnCommandLine(commandLine) {
    const [, argv] = GLib.shell_parse_argv(commandLine);
    trySpawn(argv);
}

function _handleSpawnError(command, err) {
    const title = _('Execution of “%s” failed:').format(command);
    // Use dynamic import to not pull in UI related code in unit tests
    import('../ui/main.js').then(
        ({notifyError}) => notifyError(title, err.message));
}

/**
 * Fix up embedded markup so that it can be displayed correctly in
 * UI elements such as the message list. In some cases, we might want to
 * keep some of the embedded markup, so specify allowMarkup for that case
 *
 * @param {string} text containing markup to escape and parse
 * @param {boolean} allowMarkup to allow embedded markup or just escape it all
 * @returns the escaped string
 */
export function fixMarkup(text, allowMarkup) {
    if (allowMarkup) {
        // Support &amp;, &quot;, &apos;, &lt; and &gt;, escape all other
        // occurrences of '&'.
        let _text = text.replace(/&(?!amp;|quot;|apos;|lt;|gt;)/g, '&amp;');

        // Support <b>, <i>, and <u>, escape anything else
        // so it displays as raw markup.
        _text = _text.replace(/<(?!\/?[biu]>)/g, '&lt;');

        try {
            Pango.parse_markup(_text, -1, '');
            return _text;
        } catch {}
    }

    // !allowMarkup, or invalid markup
    return GLib.markup_escape_text(text, -1);
}

/**
 * Returns an {@link St.Label} with the date passed formatted
 * using {@link formatTime}
 *
 * @param {Date} date the date to format for the label
 * @param {object} params params for {@link formatTime}
 * @returns {St.Label}
 */
export function createTimeLabel(date, params) {
    if (_desktopSettings == null)
        _desktopSettings = new Gio.Settings({schema_id: 'org.gnome.desktop.interface'});

    let label = new St.Label({text: formatTime(date, params)});
    _desktopSettings.connectObject(
        'changed::clock-format', () => (label.text = formatTime(date, params)),
        label);
    return label;
}


/**
 * lowerBound:
 *
 * @template T, [K=T]
 * @param {readonly T[]} array an array or array-like object, already sorted
 *         according to `cmp`
 * @param {K} val the value to add
 * @param {(a: T, val: K) => number} cmp a comparator (or undefined to compare as numbers)
 * @returns {number}
 *
 * Returns the position of the first element that is not
 * lower than `val`, according to `cmp`.
 * That is, returns the first position at which it
 * is possible to insert val without violating the
 * order.
 *
 * This is quite like an ordinary binary search, except
 * that it doesn't stop at first element comparing equal.
 */
function lowerBound(array, val, cmp) {
    let min, max, mid, v;
    cmp ||= (a, b) => a - b;

    if (array.length === 0)
        return 0;

    min = 0;
    max = array.length;
    while (min < (max - 1)) {
        mid = Math.floor((min + max) / 2);
        v = cmp(array[mid], val);

        if (v < 0)
            min = mid + 1;
        else
            max = mid;
    }

    return min === max || cmp(array[min], val) < 0 ? max : min;
}

/**
 * insertSorted:
 *
 * @template T, [K=T]
 * @param {T[]} array an array sorted according to `cmp`
 * @param {K} val a value to insert
 * @param {(a: T, val: K) => number} cmp the sorting function
 * @returns {number}
 *
 * Inserts `val` into `array`, preserving the
 * sorting invariants.
 *
 * Returns the position at which it was inserted
 */
export function insertSorted(array, val, cmp) {
    let pos = lowerBound(array, val, cmp);
    array.splice(pos, 0, val);

    return pos;
}

/**
 * @param {number} start
 * @param {number} end
 * @param {number} progress
 * @returns {number}
 */
export function lerp(start, end, progress) {
    return start + progress * (end - start);
}

/**
 * _GNOMEversionToNumber:
 *
 * @param {string} version a GNOME version element
 * @returns {number}
 *
 * Like Number() but returns sortable values for special-cases
 * 'alpha' and 'beta'. Returns NaN for unhandled 'versions'.
 */
function _GNOMEversionToNumber(version) {
    let ret = Number(version);
    if (!isNaN(ret))
        return ret;
    if (version === 'alpha')
        return -3;
    if (version === 'beta')
        return -2;
    if (version === 'rc')
        return -1;
    return ret;
}

/**
 * GNOMEversionCompare:
 *
 * @param {string} version1 a string containing a GNOME version
 * @param {string} version2 a string containing another GNOME version
 * @returns {number}
 *
 * Returns an integer less than, equal to, or greater than
 * zero, if `version1` is older, equal or newer than `version2`
 */
export function GNOMEversionCompare(version1, version2) {
    const v1Array = version1.split('.');
    const v2Array = version2.split('.');

    for (let i = 0; i < Math.max(v1Array.length, v2Array.length); i++) {
        let elemV1 = _GNOMEversionToNumber(v1Array[i] || '0');
        let elemV2 = _GNOMEversionToNumber(v2Array[i] || '0');
        if (elemV1 < elemV2)
            return -1;
        if (elemV1 > elemV2)
            return 1;
    }

    return 0;
}

export class DBusSenderChecker {
    /**
     * @param {string[]} allowList - list of allowed well-known names
     */
    constructor(allowList) {
        this._allowlistMap = new Map();

        this._uninitializedNames = new Set(allowList);
        this._initializedPromise = new Promise(resolve => {
            this._resolveInitialized = resolve;
        });

        this._watchList = allowList.map(name => {
            return Gio.DBus.watch_name(Gio.BusType.SESSION,
                name,
                Gio.BusNameWatcherFlags.NONE,
                (conn_, name_, owner) => {
                    this._allowlistMap.set(name, owner);
                    this._checkAndResolveInitialized(name);
                },
                () => {
                    this._allowlistMap.delete(name);
                    this._checkAndResolveInitialized(name);
                });
        });
    }

    /**
     * @param {string} name - bus name for which the watcher got initialized
     */
    _checkAndResolveInitialized(name) {
        if (this._uninitializedNames.delete(name) &&
            this._uninitializedNames.size === 0)
            this._resolveInitialized();
    }

    /**
     * @async
     * @param {string} sender - the bus name that invoked the checked method
     * @returns {bool}
     */
    async _isSenderAllowed(sender) {
        await this._initializedPromise;
        return [...this._allowlistMap.values()].includes(sender);
    }

    /**
     * Check whether the bus name that invoked @invocation maps
     * to an entry in the allow list.
     *
     * @async
     * @throws
     * @param {Gio.DBusMethodInvocation} invocation - the invocation
     * @returns {void}
     */
    async checkInvocation(invocation) {
        if (global.context.unsafe_mode)
            return;

        if (await this._isSenderAllowed(invocation.get_sender()))
            return;

        throw new GLib.Error(Gio.DBusError,
            Gio.DBusError.ACCESS_DENIED,
            `${invocation.get_method_name()} is not allowed`);
    }

    /**
     * @returns {void}
     */
    destroy() {
        for (const id in this._watchList)
            Gio.DBus.unwatch_name(id);
        this._watchList = [];
    }
}

/* @class Highlighter Highlight given terms in text using markup. */
export class Highlighter {
    /**
     * @param {?string[]} terms - list of terms to highlight
     */
    constructor(terms) {
        if (!terms)
            return;

        const escapedTerms = terms
            .map(term => Shell.util_regex_escape(term))
            .filter(term => term.length > 0);

        if (escapedTerms.length === 0)
            return;

        this._highlightRegex = new RegExp(
            `(${escapedTerms.join('|')})`, 'gi');
    }

    /**
     * Highlight all occurences of the terms defined for this
     * highlighter in the provided text using markup.
     *
     * @param {string} text - text to highlight the defined terms in
     * @returns {string}
     */
    highlight(text) {
        if (!this._highlightRegex)
            return GLib.markup_escape_text(text, -1);

        let escaped = [];
        let lastMatchEnd = 0;
        let match;
        while ((match = this._highlightRegex.exec(text))) {
            if (match.index > lastMatchEnd) {
                let unmatched = GLib.markup_escape_text(
                    text.slice(lastMatchEnd, match.index), -1);
                escaped.push(unmatched);
            }
            let matched = GLib.markup_escape_text(match[0], -1);
            escaped.push(`<b>${matched}</b>`);
            lastMatchEnd = match.index + match[0].length;
        }
        let unmatched = GLib.markup_escape_text(
            text.slice(lastMatchEnd), -1);
        escaped.push(unmatched);

        return escaped.join('');
    }
}
