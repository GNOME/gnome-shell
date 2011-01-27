/* -*- mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil -*- */

const Gdk = imports.gi.Gdk;
const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Shell = imports.gi.Shell;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;

const Gettext = imports.gettext.domain('gnome-shell');
const _ = Gettext.gettext;

/* http://daringfireball.net/2010/07/improved_regex_for_matching_urls */
const _urlRegexp = new RegExp('\\b(([a-z][\\w-]+:(/{1,3}|[a-z0-9%])|www\\d{0,3}[.]|[a-z0-9.\\-]+[.][a-z]{2,4}/)([^\\s()<>]+|\\(([^\\s()<>]+|(\\([^\\s()<>]+\\)))*\\))+(\\(([^\\s()<>]+|(\\([^\\s()<>]+\\)))*\\)|[^\\s`!()\\[\\]{};:\'\\".,<>?«»“”‘’]))', 'gi');

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
        res.push({ url: match[0], pos: match.index });
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
// @command_line: a command line
//
// Runs @command_line in the background, handling any errors that
// occur when trying to parse or start the program.
function spawnCommandLine(command_line) {
    try {
        let [success, argc, argv] = GLib.shell_parse_argv(command_line);
        trySpawn(argv);
    } catch (err) {
        _handleSpawnError(command_line, err);
    }
}

// spawnDesktop:
// @id: a desktop file ID
//
// Spawns the desktop file identified by @id using startup notification,
// etc, handling any errors that occur when trying to find or start
// the program.
function spawnDesktop(id) {
    try {
        trySpawnDesktop(id);
    } catch (err) {
        _handleSpawnError(id, err);
    }
}

// trySpawn:
// @argv: an argv array
//
// Runs @argv in the background. If launching @argv fails,
// this will throw an error.
function trySpawn(argv)
{
    try {
        GLib.spawn_async(null, argv, null,
                         GLib.SpawnFlags.SEARCH_PATH,
                         null, null);
    } catch (err) {
        if (err.code == GLib.SpawnError.G_SPAWN_ERROR_NOENT) {
            err.message = _("Command not found");
        } else {
            // The exception from gjs contains an error string like:
            //   Error invoking GLib.spawn_command_line_async: Failed to
            //   execute child process "foo" (No such file or directory)
            // We are only interested in the part in the parentheses. (And
            // we can't pattern match the text, since it gets localized.)
            err.message = err.message.replace(/.*\((.+)\)/, '$1');
        }

        throw err;
    }
}

// trySpawnCommandLine:
// @command_line: a command line
//
// Runs @command_line in the background. If launching @command_line
// fails, this will throw an error.
function trySpawnCommandLine(command_line) {
    let success, argc, argv;

    try {
        [success, argc, argv] = GLib.shell_parse_argv(command_line);
    } catch (err) {
        // Replace "Error invoking GLib.shell_parse_argv: " with
        // something nicer
        err.message = err.message.replace(/[^:]*: /, _("Could not parse command:") + "\n");
        throw err;
    }

    trySpawn(argv);
}

// trySpawnDesktop:
// @id: a desktop file ID
//
// Spawns the desktop file identified by @id using startup notification.
// On error, throws an exception.
function trySpawnDesktop(id) {
    let app;

    // shell_app_system_load_from_desktop_file() will end up returning
    // a stupid error message if the desktop file doesn't exist, but
    // that's the only case it returns an error for, so we just
    // substitute our own error in instead
    try {
        app = Shell.AppSystem.get_default().load_from_desktop_file(id + '.desktop');
    } catch (err) {
        throw new Error(_("No such application"));
    }

    try {
        app.launch();
    } catch(err) {
        // see trySpawn
        err.message = err.message.replace(/.*\((.+)\)/, '$1');
        throw err;
    }
}

function _handleSpawnError(command, err) {
    let title = _("Execution of '%s' failed:").format(command);

    let source = new MessageTray.SystemNotificationSource();
    Main.messageTray.add(source);
    let notification = new MessageTray.Notification(source, title, err.message);
    notification.setTransient(true);
    source.notify(notification);
}

// killall:
// @processName: a process name
//
// Kills @processName. If no process with the given name is found,
// this will fail silently.
function killall(processName) {
    try {
        // pkill is more portable than killall, but on Linux at least
        // it won't match if you pass more than 15 characters of the
        // process name... However, if you use the '-f' flag to match
        // the entire command line, it will work, but we have to be
        // careful in that case that we can match
        // '/usr/bin/processName' but not 'gedit processName.c' or
        // whatever...

        let argv = ['pkill', '-f', '^([^ ]*/)?' + processName + '($| )'];
        GLib.spawn_sync(null, argv, null, GLib.SpawnFlags.SEARCH_PATH, null, null);
        // It might be useful to return success/failure, but we'd need
        // a wrapper around WIFEXITED and WEXITSTATUS. Since none of
        // the current callers care, we don't bother.
    } catch (e) {
        logError(e, 'Failed to kill ' + processName);
    }
}
