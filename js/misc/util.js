// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported findUrls, spawn, spawnCommandLine, spawnApp, trySpawnCommandLine,
            formatTime, formatTimeSpan, createTimeLabel, insertSorted,
            makeCloseButton, ensureActorVisibleInScrollView, wiggle,
            getSearchEngineName, findSearchUrls, getBrowserApp */

const { Clutter, Gio, GLib, GObject, Shell, St, GnomeDesktop } = imports.gi;
const Gettext = imports.gettext;

const Json = imports.gi.Json;
const Main = imports.ui.main;
const Params = imports.misc.params;

var SCROLL_TIME = 100;

const WIGGLE_OFFSET = 6;
const WIGGLE_DURATION = 65;
const N_WIGGLES = 3;

const FALLBACK_BROWSER_ID = 'chromium-browser.desktop';
const GOOGLE_CHROME_ID = 'google-chrome.desktop';

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

var CloseButton = GObject.registerClass(
class CloseButton extends St.Button {
    _init(boxpointer) {
        super._init({
            style_class: 'notification-close',
            x_expand: true,
            y_expand: true,
            x_align: Clutter.ActorAlign.END,
            y_align: Clutter.ActorAlign.START,
        });

        this._boxPointer = boxpointer;
        if (boxpointer)
            this._boxPointer.connect('arrow-side-changed', this._sync.bind(this));
    }

    _computeBoxPointerOffset() {
        if (!this._boxPointer || !this._boxPointer.get_stage())
            return 0;

        let side = this._boxPointer.arrowSide;
        if (side == St.Side.TOP)
            return this._boxPointer.getArrowHeight();
        else
            return 0;
    }

    _sync() {
        let themeNode = this.get_theme_node();

        let offY = this._computeBoxPointerOffset();
        this.translation_x = themeNode.get_length('-shell-close-overlap-x');
        this.translation_y = themeNode.get_length('-shell-close-overlap-y') + offY;
    }

    vfunc_style_changed() {
        this._sync();
        super.vfunc_style_changed();
    }
});

function makeCloseButton(boxpointer) {
    return new CloseButton(boxpointer);
}

function ensureActorVisibleInScrollView(scrollView, actor) {
    let adjustment = scrollView.vscroll.adjustment;
    let [value, lower_, upper, stepIncrement_, pageIncrement_, pageSize] = adjustment.get_values();

    let offset = 0;
    let vfade = scrollView.get_effect("fade");
    if (vfade)
        offset = vfade.vfade_offset;

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

// http://stackoverflow.com/questions/4691070/validate-url-without-www-or-http
const _searchUrlRegexp = new RegExp(
    '^([a-zA-Z0-9]+(\.[a-zA-Z0-9]+)+.*)\\.+[A-Za-z0-9\.\/%&=\?\-_]+$',
    'gi');

const supportedSearchSchemes = ['http', 'https', 'ftp'];

// findSearchUrls:
// @terms: list of searchbar terms to find URLs in
// @maxLength: maximum number of characters in each non-URI term to match against, defaults
//             to 32 characters to prevent hogging the CPU with too long generic strings.
//
// Similar to "findUrls", but adapted for use only with terms from the searchbar.
//
// In order not to be too CPU-expensive, this function is implemented in the following way:
//   1. If the term is a valid URI in that it's possible to parse at least
//      its scheme and host fields, it's considered a valid URL "as-is".
//   2. Else, if the term is a generic string exceeding the maximum length
//      specified then we simply ignore it and move onto the next term.
//   3. In any other case (non-URI term, valid length) we match the term
//      passed against the regular expression to determine if it's a URL.
//
// Note that the regex for these URLs matches strings such as "google.com" (no need to the
// specify a preceding scheme), which is why we have to limit its execution to a certain
// maximum length, as the term can be pretty free-form. By default, this maximum length
// is 32 characters, which should be a good compromise considering that "many of the world's
// most visited web sites have domain names of between 6 - 10 characters" (see [1][2]).
//
// [1] https://www.domainregistration.com.au/news/2013/1301-domain-length.php
// [2] https://www.domainregistration.com.au/infocentre/info-domain-length.php
//
// Return value: the list of URLs found in the string
function findSearchUrls(terms, maxLength = 32) {
    let res = [], match;
    for (let term of terms) {
        if (GLib.uri_parse_scheme(term)) {
            let supportedScheme = false;
            for (let scheme of supportedSearchSchemes) {
                if (term.startsWith('%s://'.format(scheme))) {
                    supportedScheme = true;
                    break;
                }
            }

            // Check that there's a valid host after the scheme part.
            if (supportedScheme && term.split('://')[1]) {
                res.push(term);
                continue;
            }
        }

        // Try to save CPU cycles from regexp-matching too long strings.
        if (term.length > maxLength)
            continue;

        while ((match = _searchUrlRegexp.exec(term)))
            res.push(match[0]);
    }
    return res;
}

function getBrowserId() {
    let id = FALLBACK_BROWSER_ID;
    let app = Gio.app_info_get_default_for_type('x-scheme-handler/http', true);
    if (app)
        id = app.get_id();
    return id;
}

function getBrowserApp() {
    let id = getBrowserId();
    let appSystem = Shell.AppSystem.get_default();
    let browserApp = appSystem.lookup_app(id);
    return browserApp;
}

function _getJsonSearchEngine(folder) {
    let path = GLib.build_filenamev([GLib.get_user_config_dir(), folder, 'Default', 'Preferences']);
    let parser = new Json.Parser();

    /*
     * Translators: this is the name of the search engine that shows in the
     * Shell's desktop search entry.
     */
    let defaultString = _('Google');

    try {
        parser.load_from_file(path);
    } catch (e) {
        if (e.matches(GLib.FileError, GLib.FileError.NOENT))
            return defaultString;

        logError(e, 'error while parsing %s'.format(path));
        return null;
    }

    let root = parser.get_root().get_object();

    let searchProviderDataNode = root.get_member('default_search_provider_data');
    if (!searchProviderDataNode || searchProviderDataNode.get_node_type() !== Json.NodeType.OBJECT)
        return defaultString;

    let searchProviderData = searchProviderDataNode.get_object();
    if (!searchProviderData)
        return defaultString;

    let templateUrlDataNode = searchProviderData.get_member('template_url_data');
    if (!templateUrlDataNode || templateUrlDataNode.get_node_type() !== Json.NodeType.OBJECT)
        return defaultString;

    let templateUrlData = templateUrlDataNode.get_object();
    if (!templateUrlData)
        return defaultString;

    let shortNameNode = templateUrlData.get_member('short_name');
    if (!shortNameNode || shortNameNode.get_node_type() !== Json.NodeType.VALUE)
        return defaultString;

    return shortNameNode.get_string();
}

// getSearchEngineName:
//
// Retrieves the current search engine from
// the default browser.
function getSearchEngineName() {
    let browser = getBrowserId();

    if (browser === FALLBACK_BROWSER_ID)
        return _getJsonSearchEngine('chromium');

    if (browser === GOOGLE_CHROME_ID)
        return _getJsonSearchEngine('google-chrome');

    return null;
}
