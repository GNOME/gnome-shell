const Clutter = imports.gi.Clutter;
const Gio = imports.gi.Gio;
const Gtk = imports.gi.Gtk;
const Meta = imports.gi.Meta;
const Main = imports.ui.main;
const Scripting = imports.ui.scripting;
const Shell = imports.gi.Shell;

let METRICS = {
    timeToDesktop:
    { description: "Time from starting graphical.target to desktop showing",
      units: "us" },

    overviewShowTime:
    { description: "Time to switch to overview view, first time",
      units: "us" },

    applicationsShowTime:
    { description: "Time to switch to applications view, first time",
      units: "us" },

    mainViewRedrawTime:
    { description: "Time to redraw the main view, full screen",
      units: "us" },

    overviewRedrawTime:
    { description: "Time to redraw the overview, full screen, 5 windows",
      units: "us" },

    applicationRedrawTime:
    { description: "Time to redraw frame with a maximized application update",
      units: "us" },

    geditStartTime:
    { description: "Time from gedit launch to window drawn",
      units: "us" },
}

function waitAndDraw(milliseconds) {
    let cb;

    let timeline = new Clutter.Timeline({ duration: milliseconds });
    timeline.start();

    timeline.connect('new-frame',
        function(timeline, frame) {
            global.stage.queue_redraw();
        });

    timeline.connect('completed',
        function() {
            timeline.stop();
            if (cb)
                cb();
        });

    return function(callback) {
        cb = callback;
    };
}

function waitSignal(object, signal) {
    let cb;

    let id = object.connect(signal, function() {
        object.disconnect(id);
        if (cb)
            cb();
    });

    return function(callback) {
        cb = callback;
    };
}

function extractBootTimestamp() {
    let sp = Gio.Subprocess.new(['journalctl', '-b',
                                 'MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5',
                                 'UNIT=graphical.target',
                                 '-o',
                                 'json-pretty'],
                                Gio.SubprocessFlags.STDOUT_PIPE);
    let result = null;

    let datastream = Gio.DataInputStream.new(sp.get_stdout_pipe());
    while (true) {
        let [line, length] = datastream.read_line_utf8(null);
        if (line === null)
            break;

        let m = /.*"__MONOTONIC_TIMESTAMP".*"([0-9]+)"/.exec(line);
        if (m) {
            result = Number(m[1]);
        }
    }
    datastream.close(null);
    return result;
}

function run() {
    Scripting.defineScriptEvent("desktopShown", "Finished initial animation");
    Scripting.defineScriptEvent("overviewShowStart", "Starting to show the overview");
    Scripting.defineScriptEvent("overviewShowDone", "Overview finished showing");
    Scripting.defineScriptEvent("applicationsShowStart", "Starting to switch to applications view");
    Scripting.defineScriptEvent("applicationsShowDone", "Done switching to applications view");
    Scripting.defineScriptEvent("mainViewDrawStart", "Drawing main view");
    Scripting.defineScriptEvent("mainViewDrawDone", "Ending timing main view drawing");
    Scripting.defineScriptEvent("overviewDrawStart", "Drawing overview");
    Scripting.defineScriptEvent("overviewDrawDone", "Ending timing overview drawing");
    Scripting.defineScriptEvent("redrawTestStart", "Drawing application window");
    Scripting.defineScriptEvent("redrawTestDone", "Ending timing application window drawing");
    Scripting.defineScriptEvent("collectTimings", "Accumulate frame timings from redraw tests");
    Scripting.defineScriptEvent("geditLaunch", "gedit application launch");
    Scripting.defineScriptEvent("geditFirstFrame", "first frame of gedit window drawn");

    yield Scripting.waitLeisure();
    Scripting.scriptEvent('desktopShown');

    Gtk.Settings.get_default().gtk_enable_animations = false;

    Scripting.scriptEvent('overviewShowStart');
    Main.overview.show();
    yield Scripting.waitLeisure();
    Scripting.scriptEvent('overviewShowDone');

    yield Scripting.sleep(1000);

    Scripting.scriptEvent('applicationsShowStart');
    Main.overview._dash.showAppsButton.checked = true;

    yield Scripting.waitLeisure();
    Scripting.scriptEvent('applicationsShowDone');

    yield Scripting.sleep(1000);

    Main.overview.hide();
    yield Scripting.waitLeisure();

    ////////////////////////////////////////
    // Tests of redraw speed
    ////////////////////////////////////////

    global.frame_timestamps = true;
    global.frame_finish_timestamp = true;

    for (let k = 0; k < 5; k++)
        yield Scripting.createTestWindow(640, 480,
                                         { maximized: true });
    yield Scripting.waitTestWindows();

    yield Scripting.sleep(1000);

    Scripting.scriptEvent('mainViewDrawStart');
    yield waitAndDraw(1000);
    Scripting.scriptEvent('mainViewDrawDone');

    Main.overview.show();
    Scripting.waitLeisure();

    yield Scripting.sleep(1500);

    Scripting.scriptEvent('overviewDrawStart');
    yield waitAndDraw(1000);
    Scripting.scriptEvent('overviewDrawDone');

    yield Scripting.destroyTestWindows();
    Main.overview.hide();

    yield Scripting.createTestWindow(640, 480,
                                     { maximized: true,
                                       redraws: true});
    yield Scripting.waitTestWindows();

    yield Scripting.sleep(1000);

    Scripting.scriptEvent('redrawTestStart');
    yield Scripting.sleep(1000);
    Scripting.scriptEvent('redrawTestDone');

    yield Scripting.sleep(1000);
    Scripting.scriptEvent('collectTimings');

    yield Scripting.destroyTestWindows();

    global.frame_timestamps = false;
    global.frame_finish_timestamp = false;

    yield Scripting.sleep(1000);

    ////////////////////////////////////////

    let appSys = Shell.AppSystem.get_default();
    let app = appSys.lookup_app('gedit.desktop');

    Scripting.scriptEvent('geditLaunch');
    app.activate();

    let windows = app.get_windows();
    if (windows.length > 0)
        throw new Error('gedit was already running');

    while (windows.length == 0) {
        yield waitSignal(global.display, 'window-created');
        windows = app.get_windows();
    }

    let actor = windows[0].get_compositor_private();
    yield waitSignal(actor, 'first-frame');
    Scripting.scriptEvent('geditFirstFrame');

    yield Scripting.sleep(1000);

    windows[0].delete(global.get_current_time());

    yield Scripting.sleep(1000);

    Gtk.Settings.get_default().gtk_enable_animations = true;
}

let overviewShowStart;
let applicationsShowStart;
let stagePaintStart;
let redrawTiming;
let redrawTimes = {};
let geditLaunchTime;

function script_desktopShown(time) {
    let bootTimestamp = extractBootTimestamp();
    METRICS.timeToDesktop.value = time - bootTimestamp;
}

function script_overviewShowStart(time) {
    overviewShowStart = time;
}

function script_overviewShowDone(time) {
    METRICS.overviewShowTime.value = time - overviewShowStart;
}

function script_applicationsShowStart(time) {
    applicationsShowStart = time;
}

function script_applicationsShowDone(time) {
    METRICS.applicationsShowTime.value = time - applicationsShowStart;
}

function script_mainViewDrawStart(time) {
    redrawTiming = 'mainView';
}

function script_mainViewDrawDone(time) {
    redrawTiming = null;
}

function script_overviewDrawStart(time) {
    redrawTiming = 'overview';
}

function script_overviewDrawDone(time) {
    redrawTiming = null;
}

function script_redrawTestStart(time) {
    redrawTiming = 'application';
}

function script_redrawTestDone(time) {
    redrawTiming = null;
}

function script_collectTimings(time) {
    for (let timing in redrawTimes) {
        let times = redrawTimes[timing];
        times.sort();

        let len = times.length;
        let median;

        if (len == 0)
            median = -1;
        else if (len % 2 == 1)
            median = times[(len - 1)/ 2];
        else
            median = Math.round((times[len / 2 - 1] + times[len / 2]) / 2);

        METRICS[timing + 'RedrawTime'].value = median;
    }
}

function script_geditLaunch(time) {
    geditLaunchTime = time;
}

function script_geditFirstFrame(time) {
    METRICS.geditStartTime.value = time - geditLaunchTime;
}

function clutter_stagePaintStart(time) {
    stagePaintStart = time;
}

function clutter_paintCompletedTimestamp(time) {
    if (redrawTiming != null && stagePaintStart != null) {
        if (!(redrawTiming in redrawTimes))
            redrawTimes[redrawTiming] = [];
        redrawTimes[redrawTiming].push(time - stagePaintStart);
    }
    stagePaintStart = null;
}
