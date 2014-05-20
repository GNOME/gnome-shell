// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-

const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const Mainloop = imports.mainloop;
const Meta = imports.gi.Meta;
const Shell = imports.gi.Shell;

const Main = imports.ui.main;
const Params = imports.misc.params;

// This module provides functionality for driving the shell user interface
// in an automated fashion. The primary current use case for this is
// automated performance testing (see runPerfScript()), but it could
// be applied to other forms of automation, such as testing for
// correctness as well.
//
// When scripting an automated test we want to make a series of calls
// in a linear fashion, but we also want to be able to let the main
// loop run so actions can finish. For this reason we write the script
// as a generator function that yields when it want to let the main
// loop run.
//
//    yield Scripting.sleep(1000);
//    main.overview.show();
//    yield Scripting.waitLeisure();
//
// While it isn't important to the person writing the script, the actual
// yielded result is a function that the caller uses to provide the
// callback for resuming the script.

/**
 * sleep:
 * @milliseconds: number of milliseconds to wait
 *
 * Used within an automation script to pause the the execution of the
 * current script for the specified amount of time. Use as
 * 'yield Scripting.sleep(500);'
 */
function sleep(milliseconds) {
    let cb;

    let id = Mainloop.timeout_add(milliseconds, function() {
                             if (cb)
                                 cb();
                             return GLib.SOURCE_REMOVE;
                         });
    GLib.Source.set_name_by_id(id, '[gnome-shell] sleep');

    return function(callback) {
        cb = callback;
    };
}

/**
 * waitLeisure:
 *
 * Used within an automation script to pause the the execution of the
 * current script until the shell is completely idle. Use as
 * 'yield Scripting.waitLeisure();'
 */
function waitLeisure() {
    let cb;

    global.run_at_leisure(function() {
                             if (cb)
                                 cb();
                          });

    return function(callback) {
        cb = callback;
    };
}

const PerfHelperIface = '<node> \
<interface name="org.gnome.Shell.PerfHelper"> \
<method name="CreateWindow"> \
    <arg type="i" direction="in" /> \
    <arg type="i" direction="in" /> \
    <arg type="b" direction="in" /> \
    <arg type="b" direction="in" /> \
</method> \
<method name="WaitWindows" /> \
<method name="DestroyWindows" /> \
</interface> \
</node>';

var PerfHelperProxy = Gio.DBusProxy.makeProxyWrapper(PerfHelperIface);
function PerfHelper() {
    return new PerfHelperProxy(Gio.DBus.session, 'org.gnome.Shell.PerfHelper', '/org/gnome/Shell/PerfHelper');
}

let _perfHelper = null;
function _getPerfHelper() {
    if (_perfHelper == null)
        _perfHelper = new PerfHelper();

    return _perfHelper;
}

function _callRemote(obj, method, ...args) {
    let cb;
    let errcb;

    args.push(function(result, excp) {
                  if (excp) {
                      if (errcb)
                          errcb(excp);
                  } else {
                      if (cb)
                          cb();
                  }
             });

    method.apply(obj, args);

    return function(callback, error_callback) {
        cb = callback;
        errcb = error_callback;
    };
}

/**
 * createTestWindow:
 * @params: options for window creation.
 *   width - width of window, in pixels (default 640)
 *   height - height of window, in pixels (default 480)
 *   alpha - whether the window should have an alpha channel (default false)
 *   maximized - whether the window should be created maximized (default false)
 * @maximized: whethe the window should be created maximized
 *
 * Creates a window using gnome-shell-perf-helper for testing purposes.
 * While this function can be used with yield in an automation
 * script to pause until the D-Bus call to the helper process returns,
 * because of the normal X asynchronous mapping process, to actually wait
 * until the window has been mapped and exposed, use waitTestWindows().
 */
function createTestWindow(width, height, params) {
    params = Params.parse(params, { width: 640,
                                    height: 480,
                                    alpha: false,
                                    maximized: false });

    let perfHelper = _getPerfHelper();
    return _callRemote(perfHelper, perfHelper.CreateWindowRemote,
                       params.width, params.height,
                       params.alpha, params.maximized);
}

/**
 * waitTestWindows:
 *
 * Used within an automation script to pause until all windows previously
 * created with createTestWindow have been mapped and exposed.
 */
function waitTestWindows() {
    let perfHelper = _getPerfHelper();
    return _callRemote(perfHelper, perfHelper.WaitWindowsRemote);
}

/**
 * destroyTestWindows:
 *
 * Destroys all windows previously created with createTestWindow().
 * While this function can be used with yield in an automation
 * script to pause until the D-Bus call to the helper process returns,
 * this doesn't guarantee that Mutter has actually finished the destroy
 * process because of normal X asynchronicity.
 */
function destroyTestWindows() {
    let perfHelper = _getPerfHelper();
    return _callRemote(perfHelper, perfHelper.DestroyWindowsRemote);
}

/**
 * defineScriptEvent
 * @name: The event will be called script.<name>
 * @description: Short human-readable description of the event
 *
 * Convenience function to define a zero-argument performance event
 * within the 'script' namespace that is reserved for events defined locally
 * within a performance automation script
 */
function defineScriptEvent(name, description) {
    Shell.PerfLog.get_default().define_event("script." + name,
                                             description,
                                             "");
}

/**
 * scriptEvent
 * @name: Name registered with defineScriptEvent()
 *
 * Convenience function to record a script-local performance event
 * previously defined with defineScriptEvent
 */
function scriptEvent(name) {
    Shell.PerfLog.get_default().event("script." + name);
}

/**
 * collectStatistics
 *
 * Convenience function to trigger statistics collection
 */
function collectStatistics() {
    Shell.PerfLog.get_default().collect_statistics();
}

function _step(g, finish, onError) {
    try {
        let waitFunction = g.next();
        waitFunction(function() {
                         _step(g, finish, onError);
                     },
                     function(err) {
                         if (onError)
                             onError(err);
                     });
    } catch (err if err instanceof StopIteration) {
        if (finish)
            finish();
    } catch (err) {
        if (onError)
            onError(err);
    }
}

function _collect(scriptModule, outputFile) {
    let eventHandlers = {};

    for (let f in scriptModule) {
        let m = /([A-Za-z]+)_([A-Za-z]+)/.exec(f);
        if (m)
            eventHandlers[m[1] + "." + m[2]] = scriptModule[f];
    }

    Shell.PerfLog.get_default().replay(
        function(time, eventName, signature, arg) {
            if (eventName in eventHandlers)
                eventHandlers[eventName](time, arg);
        });

    if ('finish' in scriptModule)
        scriptModule.finish();

    if (outputFile) {
        let f = Gio.file_new_for_path(outputFile);
        let raw = f.replace(null, false,
                            Gio.FileCreateFlags.NONE,
                            null);
        let out = Gio.BufferedOutputStream.new_sized (raw, 4096);
        Shell.write_string_to_stream (out, "{\n");

        Shell.write_string_to_stream(out, '"events":\n');
        Shell.PerfLog.get_default().dump_events(out);

        let monitors = Main.layoutManager.monitors;
        let primary = Main.layoutManager.primaryIndex;
        Shell.write_string_to_stream(out, ',\n"monitors":\n[');
        for (let i = 0; i < monitors.length; i++) {
            let monitor = monitors[i];
            if (i != 0)
                Shell.write_string_to_stream(out, ', ');
            Shell.write_string_to_stream(out, '"%s%dx%d+%d+%d"'.format(i == primary ? "*" : "",
                                                                       monitor.width, monitor.height,
                                                                       monitor.x, monitor.y));
        }
        Shell.write_string_to_stream(out, ' ]');

        Shell.write_string_to_stream(out, ',\n"metrics":\n[ ');
        let first = true;
        for (let name in scriptModule.METRICS) {
            let metric = scriptModule.METRICS[name];
            // Extra checks here because JSON.stringify generates
            // invalid JSON for undefined values
            if (metric.description == null) {
                log("Error: No description found for metric " + name);
                continue;
            }
            if (metric.units == null) {
                log("Error: No units found for metric " + name);
                continue;
            }
            if (metric.value == null) {
                log("Error: No value found for metric " + name);
                continue;
            }

            if (!first)
                Shell.write_string_to_stream(out, ',\n  ');
            first = false;

            Shell.write_string_to_stream(out,
                                         '{ "name": ' + JSON.stringify(name) + ',\n' +
                                         '    "description": ' + JSON.stringify(metric.description) + ',\n' +
                                         '    "units": ' + JSON.stringify(metric.units) + ',\n' +
                                         '    "value": ' + JSON.stringify(metric.value) + ' }');
        }
        Shell.write_string_to_stream(out, ' ]');

        Shell.write_string_to_stream (out, ',\n"log":\n');
        Shell.PerfLog.get_default().dump_log(out);

        Shell.write_string_to_stream (out, '\n}\n');
        out.close(null);
    } else {
        let metrics = [];
        for (let metric in scriptModule.METRICS)
            metrics.push(metric);

        metrics.sort();

        print ('------------------------------------------------------------');
        for (let i = 0; i < metrics.length; i++) {
            let metric = metrics[i];
            print ('# ' + scriptModule.METRICS[metric].description);
            print (metric + ': ' +  scriptModule.METRICS[metric].value + scriptModule.METRICS[metric].units);
        }
        print ('------------------------------------------------------------');
    }
}

/**
 * runPerfScript
 * @scriptModule: module object with run and finish functions
 *    and event handlers
 *
 * Runs a script for automated collection of performance data. The
 * script is defined as a Javascript module with specified contents.
 *
 * First the run() function within the module will be called as a
 * generator to automate a series of actions. These actions will
 * trigger performance events and the script can also record its
 * own performance events.
 *
 * Then the recorded event log is replayed using handler functions
 * within the module. The handler for the event 'foo.bar' is called
 * foo_bar().
 *
 * Finally if the module has a function called finish(), that will
 * be called.
 *
 * The event handler and finish functions are expected to fill in
 * metrics to an object within the module called METRICS. Each
 * property of this object represents an individual metric. The
 * name of the property is the name of the metric, the value
 * of the property is an object with the following properties:
 *
 *  description: human readable description of the metric
 *  units: a string representing the units of the metric. It has
 *   the form '<unit> <unit> ... / <unit> / <unit> ...'. Certain
 *   unit values are recognized: s, ms, us, B, KiB, MiB. Other
 *   values can appear but are uninterpreted. Examples 's',
 *   '/ s', 'frames', 'frames / s', 'MiB / s / frame'
 *  value: computed value of the metric
 *
 * The resulting metrics will be written to @outputFile as JSON, or,
 * if @outputFile is not provided, logged.
 *
 * After running the script and collecting statistics from the
 * event log, GNOME Shell will exit.
 **/
function runPerfScript(scriptModule, outputFile) {
    Shell.PerfLog.get_default().set_enabled(true);

    let g = scriptModule.run();

    _step(g,
          function() {
              try {
                  _collect(scriptModule, outputFile);
              } catch (err) {
                  log("Script failed: " + err + "\n" + err.stack);
                  Meta.exit(Meta.ExitCode.ERROR);
              }
              Meta.exit(Meta.ExitCode.SUCCESS);
          },
         function(err) {
             log("Script failed: " + err + "\n" + err.stack);
             Meta.exit(Meta.ExitCode.ERROR);
         });
}
