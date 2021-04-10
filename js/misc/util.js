// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported findUrls, spawn, spawnCommandLine, spawnApp, trySpawnCommandLine,
            formatTime, formatTimeSpan, createTimeLabel, insertSorted,
            ensureActorVisibleInScrollView, wiggle, lerp, GNOMEversionCompare */

const { Clutter, Gio, GLib, Shell, St, GnomeDesktop } = imports.gi;
const Gettext = imports.gettext;

const Main = imports.ui.main;
const Params = imports.misc.params;

var SCROLL_TIME = 100;

const WIGGLE_OFFSET = 6;
const WIGGLE_DURATION = 65;
const N_WIGGLES = 3;

// http://daringfireball.net/2010/07/improved_regex_for_matching_urls
const _balancedParens = '\\([^\\s()<>]+\\)';
const _leadingJunk = '[\\s`(\\[{\'\\"<\u00AB\u201C\u2018]';
const _notTrailingJunk = '[^\\s`!()\\[\\]{};:\'\\".,<>?\u00AB\u00BB\u200E\u200F\u201C\u201D\u2018\u2019\u202A\u202C]';

const _urlRegexp = new RegExp(
    '(^|%s)'.format(_leadingJunk) +
    '(' +
        '(?:' +
            '(?:http|https|ftp)://' +             // scheme://
            '|' +
            'www\\d{0,3}[.]' +                    // www.
            '|' +
            '[a-z0-9.\\-]+[.][a-z]{2,4}/' +       // foo.xx/
        ')' +
        '(?:' +                                   // one or more:
            '[^\\s()<>]+' +                       // run of non-space non-()
            '|' +                                 // or
            '%s'.format(_balancedParens) +        // balanced parens
        ')+' +
        '(?:' +                                   // end with:
            '%s'.format(_balancedParens) +        // balanced parens
            '|' +                                 // or
            '%s'.format(_notTrailingJunk) +       // last non-junk char
        ')' +
    ')', 'gi');

let _desktopSettings = null;

// findUrls:
// @str: string to find URLs in
//
// Searches @str for URLs and returns an array of objects with %url
// properties showing the matched URL string, and %pos properties indicating
// the position within @str where the URL was found.
//
// Return value: the list of match objects, as described above
function findUrls(str) {
    let res = [], match;
    while ((match = _urlRegexp.exec(str)))
        res.push({ url: match[2], pos: match.index + match[1].length });
    return res;
}

// spawn:
// @argv: an argv array
//
// Runs @argv in the background, handling any errors that occur
// when trying to start the program.
function spawn(argv) {
    try {
        trySpawn(argv);
    } catch (err) {
        _handleSpawnError(argv[0], err);
    }
}

// spawnCommandLine:
// @commandLine: a command line
//
// Runs @commandLine in the background, handling any errors that
// occur when trying to parse or start the program.
function spawnCommandLine(commandLine) {
    try {
        let [success_, argv] = GLib.shell_parse_argv(commandLine);
        trySpawn(argv);
    } catch (err) {
        _handleSpawnError(commandLine, err);
    }
}

// spawnApp:
// @argv: an argv array
//
// Runs @argv as if it was an application, handling startup notification
function spawnApp(argv) {
    try {
        let app = Gio.AppInfo.create_from_commandline(argv.join(' '), null,
                                                      Gio.AppInfoCreateFlags.SUPPORTS_STARTUP_NOTIFICATION);

        let context = global.create_app_launch_context(0, -1);
        app.launch([], context);
    } catch (err) {
        _handleSpawnError(argv[0], err);
    }
}

// trySpawn:
// @argv: an argv array
//
// Runs @argv in the background. If launching @argv fails,
// this will throw an error.
function trySpawn(argv) {
    var success_, pid;
    try {
        [success_, pid] = GLib.spawn_async(null, argv, null,
                                           GLib.SpawnFlags.SEARCH_PATH | GLib.SpawnFlags.DO_NOT_REAP_CHILD,
                                           null);
    } catch (err) {
        /* Rewrite the error in case of ENOENT */
        if (err.matches(GLib.SpawnError, GLib.SpawnError.NOENT)) {
            throw new GLib.SpawnError({ code: GLib.SpawnError.NOENT,
                                        message: _("Command not found") });
        } else if (err instanceof GLib.Error) {
            // The exception from gjs contains an error string like:
            //   Error invoking GLib.spawn_command_line_async: Failed to
            //   execute child process "foo" (No such file or directory)
            // We are only interested in the part in the parentheses. (And
            // we can't pattern match the text, since it gets localized.)
            let message = err.message.replace(/.*\((.+)\)/, '$1');
            throw new err.constructor({ code: err.code, message });
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

// trySpawnCommandLine:
// @commandLine: a command line
//
// Runs @commandLine in the background. If launching @commandLine
// fails, this will throw an error.
function trySpawnCommandLine(commandLine) {
    let success_, argv;

    try {
        [success_, argv] = GLib.shell_parse_argv(commandLine);
    } catch (err) {
        // Replace "Error invoking GLib.shell_parse_argv: " with
        // something nicer
        err.message = err.message.replace(/[^:]*: /, '%s\n'.format(_('Could not parse command:')));
        throw err;
    }

    trySpawn(argv);
}

function _handleSpawnError(command, err) {
    let title = _("Execution of “%s” failed:").format(command);
    Main.notifyError(title, err.message);
}

function formatTimeSpan(date) {
    let now = GLib.DateTime.new_now_local();

    let timespan = now.difference(date);

    let minutesAgo = timespan / GLib.TIME_SPAN_MINUTE;
    let hoursAgo = timespan / GLib.TIME_SPAN_HOUR;
    let daysAgo = timespan / GLib.TIME_SPAN_DAY;
    let weeksAgo = daysAgo / 7;
    let monthsAgo = daysAgo / 30;
    let yearsAgo = weeksAgo / 52;

    if (minutesAgo < 5)
        return _("Just now");
    if (hoursAgo < 1) {
        return Gettext.ngettext("%d minute ago",
                                "%d minutes ago", minutesAgo).format(minutesAgo);
    }
    if (daysAgo < 1) {
        return Gettext.ngettext("%d hour ago",
                                "%d hours ago", hoursAgo).format(hoursAgo);
    }
    if (daysAgo < 2)
        return _("Yesterday");
    if (daysAgo < 15) {
        return Gettext.ngettext("%d day ago",
                                "%d days ago", daysAgo).format(daysAgo);
    }
    if (weeksAgo < 8) {
        return Gettext.ngettext("%d week ago",
                                "%d weeks ago", weeksAgo).format(weeksAgo);
    }
    if (yearsAgo < 1) {
        return Gettext.ngettext("%d month ago",
                                "%d months ago", monthsAgo).format(monthsAgo);
    }
    return Gettext.ngettext("%d year ago",
                            "%d years ago", yearsAgo).format(yearsAgo);
}

function formatTime(time, params) {
    let date;
    // HACK: The built-in Date type sucks at timezones, which we need for the
    //       world clock; it's often more convenient though, so allow either
    //       Date or GLib.DateTime as parameter
    if (time instanceof Date)
        date = GLib.DateTime.new_from_unix_local(time.getTime() / 1000);
    else
        date = time;

    let now = GLib.DateTime.new_now_local();

    let daysAgo = now.difference(date) / (24 * 60 * 60 * 1000 * 1000);

    let format;

    if (_desktopSettings == null)
        _desktopSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.interface' });
    let clockFormat = _desktopSettings.get_string('clock-format');

    params = Params.parse(params, {
        timeOnly: false,
        ampm: true,
    });

    if (clockFormat == '24h') {
        // Show only the time if date is on today
        if (daysAgo < 1 || params.timeOnly)
            /* Translators: Time in 24h format */
            format = N_("%H\u2236%M");
        // Show the word "Yesterday" and time if date is on yesterday
        else if (daysAgo < 2)
            /* Translators: this is the word "Yesterday" followed by a
             time string in 24h format. i.e. "Yesterday, 14:30" */
            // xgettext:no-c-format
            format = N_("Yesterday, %H\u2236%M");
        // Show a week day and time if date is in the last week
        else if (daysAgo < 7)
            /* Translators: this is the week day name followed by a time
             string in 24h format. i.e. "Monday, 14:30" */
            // xgettext:no-c-format
            format = N_("%A, %H\u2236%M");
        else if (date.get_year() == now.get_year())
            /* Translators: this is the month name and day number
             followed by a time string in 24h format.
             i.e. "May 25, 14:30" */
            // xgettext:no-c-format
            format = N_("%B %-d, %H\u2236%M");
        else
            /* Translators: this is the month name, day number, year
             number followed by a time string in 24h format.
             i.e. "May 25 2012, 14:30" */
            // xgettext:no-c-format
            format = N_("%B %-d %Y, %H\u2236%M");
    } else {
        // Show only the time if date is on today
        if (daysAgo < 1 || params.timeOnly) // eslint-disable-line no-lonely-if
            /* Translators: Time in 12h format */
            format = N_("%l\u2236%M %p");
        // Show the word "Yesterday" and time if date is on yesterday
        else if (daysAgo < 2)
            /* Translators: this is the word "Yesterday" followed by a
             time string in 12h format. i.e. "Yesterday, 2:30 pm" */
            // xgettext:no-c-format
            format = N_("Yesterday, %l\u2236%M %p");
        // Show a week day and time if date is in the last week
        else if (daysAgo < 7)
            /* Translators: this is the week day name followed by a time
             string in 12h format. i.e. "Monday, 2:30 pm" */
            // xgettext:no-c-format
            format = N_("%A, %l\u2236%M %p");
        else if (date.get_year() == now.get_year())
            /* Translators: this is the month name and day number
             followed by a time string in 12h format.
             i.e. "May 25, 2:30 pm" */
            // xgettext:no-c-format
            format = N_("%B %-d, %l\u2236%M %p");
        else
            /* Translators: this is the month name, day number, year
             number followed by a time string in 12h format.
             i.e. "May 25 2012, 2:30 pm"*/
            // xgettext:no-c-format
            format = N_("%B %-d %Y, %l\u2236%M %p");
    }

    // Time in short 12h format, without the equivalent of "AM" or "PM"; used
    // when it is clear from the context
    if (!params.ampm)
        format = format.replace(/\s*%p/g, '');

    let formattedTime = date.format(Shell.util_translate_time_string(format));
    // prepend LTR-mark to colon/ratio to force a text direction on times
    return formattedTime.replace(/([:\u2236])/g, '\u200e$1');
}

function createTimeLabel(date, params) {
    if (_desktopSettings == null)
        _desktopSettings = new Gio.Settings({ schema_id: 'org.gnome.desktop.interface' });

    let label = new St.Label({ text: formatTime(date, params) });
    let id = _desktopSettings.connect('changed::clock-format', () => {
        label.text = formatTime(date, params);
    });
    label.connect('destroy', () => _desktopSettings.disconnect(id));
    return label;
}

// lowerBound:
// @array: an array or array-like object, already sorted
//         according to @cmp
// @val: the value to add
// @cmp: a comparator (or undefined to compare as numbers)
//
// Returns the position of the first element that is not
// lower than @val, according to @cmp.
// That is, returns the first position at which it
// is possible to insert @val without violating the
// order.
// This is quite like an ordinary binary search, except
// that it doesn't stop at first element comparing equal.

function lowerBound(array, val, cmp) {
    let min, max, mid, v;
    cmp = cmp || ((a, b) => a - b);

    if (array.length == 0)
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

    return min == max || cmp(array[min], val) < 0 ? max : min;
}

// insertSorted:
// @array: an array sorted according to @cmp
// @val: a value to insert
// @cmp: the sorting function
//
// Inserts @val into @array, preserving the
// sorting invariants.
// Returns the position at which it was inserted
function insertSorted(array, val, cmp) {
    let pos = lowerBound(array, val, cmp);
    array.splice(pos, 0, val);

    return pos;
}

function ensureActorVisibleInScrollView(scrollView, actor) {
    let adjustment = scrollView.vscroll.adjustment;
    let [value, lower_, upper, stepIncrement_, pageIncrement_, pageSize] = adjustment.get_values();

    let offset = 0;
    let vfade = scrollView.get_effect("fade");
    if (vfade)
        offset = vfade.fade_margins.top;

    let box = actor.get_allocation_box();
    let y1 = box.y1, y2 = box.y2;

    let parent = actor.get_parent();
    while (parent != scrollView) {
        if (!parent)
            throw new Error("actor not in scroll view");

        box = parent.get_allocation_box();
        y1 += box.y1;
        y2 += box.y1;
        parent = parent.get_parent();
    }

    if (y1 < value + offset)
        value = Math.max(0, y1 - offset);
    else if (y2 > value + pageSize - offset)
        value = Math.min(upper, y2 + offset - pageSize);
    else
        return;

    adjustment.ease(value, {
        mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        duration: SCROLL_TIME,
    });
}

function wiggle(actor, params) {
    if (!St.Settings.get().enable_animations)
        return;

    params = Params.parse(params, {
        offset: WIGGLE_OFFSET,
        duration: WIGGLE_DURATION,
        wiggleCount: N_WIGGLES,
    });
    actor.translation_x = 0;

    // Accelerate before wiggling
    actor.ease({
        translation_x: -params.offset,
        duration: params.duration,
        mode: Clutter.AnimationMode.EASE_OUT_QUAD,
        onComplete: () => {
            // Wiggle
            actor.ease({
                translation_x: params.offset,
                duration: params.duration,
                mode: Clutter.AnimationMode.LINEAR,
                repeatCount: params.wiggleCount,
                autoReverse: true,
                onComplete: () => {
                    // Decelerate and return to the original position
                    actor.ease({
                        translation_x: 0,
                        duration: params.duration,
                        mode: Clutter.AnimationMode.EASE_IN_QUAD,
                    });
                },
            });
        },
    });
}

function lerp(start, end, progress) {
    return start + progress * (end - start);
}

// _GNOMEversionToNumber:
// @version: a GNOME version element
//
// Like Number() but returns sortable values for special-cases
// 'alpha' and 'beta'. Returns NaN for unhandled 'versions'.
function _GNOMEversionToNumber(version) {
    let ret = Number(version);
    if (!isNaN(ret))
        return ret;
    if (version === 'alpha')
        return -2;
    if (version === 'beta')
        return -1;
    return ret;
}

// GNOMEversionCompare:
// @version1: a string containing a GNOME version
// @version2: a string containing another GNOME version
//
// Returns an integer less than, equal to, or greater than
// zero, if version1 is older, equal or newer than version2
function GNOMEversionCompare(version1, version2) {
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
