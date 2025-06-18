/* eslint camelcase: ["error", { properties: "never", allow: ["^script_", "^clutter"] }] */
import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';
import Shell from 'gi://Shell';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as Scripting from 'resource:///org/gnome/shell/ui/scripting.js';

export var METRICS = {
    timeToDesktop: {
        description: 'Time from starting graphical.target to desktop showing',
        units: 'us',
    },

    overviewShowTime: {
        description: 'Time to switch to overview view, first time',
        units: 'us',
    },

    applicationsShowTime: {
        description: 'Time to switch to applications view, first time',
        units: 'us',
    },

    mainViewRedrawTime: {
        description: 'Time to redraw the main view, full screen',
        units: 'us',
    },

    overviewRedrawTime: {
        description: 'Time to redraw the overview, full screen, 5 windows',
        units: 'us',
    },

    applicationRedrawTime: {
        description: 'Time to redraw frame with a maximized application update',
        units: 'us',
    },

    geditStartTime: {
        description: 'Time from gedit launch to window drawn',
        units: 'us',
    },
};

/**
 * @param {number} milliseconds - time to wait
 * @returns {callback}
 */
function waitAndDraw(milliseconds) {
    let cb;

    let timeline = new Clutter.Timeline({duration: milliseconds});
    timeline.start();

    timeline.connect('new-frame', (_timeline, _frame) => {
        global.stage.queue_redraw();
    });

    timeline.connect('completed', () => {
        timeline.stop();
        if (cb)
            cb();
    });

    return callback => (cb = callback);
}

/**
 * @param {object} object - emitter object
 * @param {string} signal - signal name
 * @returns {callback}
 */
function waitSignal(object, signal) {
    let cb;

    let id = object.connect(signal, () => {
        object.disconnect(id);
        if (cb)
            cb();
    });

    return callback => (cb = callback);
}

/**
 * @returns {number}
 */
function extractBootTimestamp() {
    const sp = Gio.Subprocess.new([
        'journalctl', '-b',
        'MESSAGE_ID=7d4958e842da4a758f6c1cdc7b36dcc5',
        'UNIT=graphical.target',
        '-o',
        'json',
    ], Gio.SubprocessFlags.STDOUT_PIPE);
    let result = null;

    let datastream = Gio.DataInputStream.new(sp.get_stdout_pipe());
    while (true) {
        let [line, length_] = datastream.read_line_utf8(null);
        if (line === null)
            break;

        let fields = JSON.parse(line);
        result = Number(fields['__MONOTONIC_TIMESTAMP']);
    }
    datastream.close(null);
    return result;
}

/** @returns {void} */
export async function run() {
    /* eslint-disable no-await-in-loop */
    Scripting.defineScriptEvent('desktopShown', 'Finished initial animation');
    Scripting.defineScriptEvent('overviewShowStart', 'Starting to show the overview');
    Scripting.defineScriptEvent('overviewShowDone', 'Overview finished showing');
    Scripting.defineScriptEvent('applicationsShowStart', 'Starting to switch to applications view');
    Scripting.defineScriptEvent('applicationsShowDone', 'Done switching to applications view');
    Scripting.defineScriptEvent('mainViewDrawStart', 'Drawing main view');
    Scripting.defineScriptEvent('mainViewDrawDone', 'Ending timing main view drawing');
    Scripting.defineScriptEvent('overviewDrawStart', 'Drawing overview');
    Scripting.defineScriptEvent('overviewDrawDone', 'Ending timing overview drawing');
    Scripting.defineScriptEvent('redrawTestStart', 'Drawing application window');
    Scripting.defineScriptEvent('redrawTestDone', 'Ending timing application window drawing');
    Scripting.defineScriptEvent('collectTimings', 'Accumulate frame timings from redraw tests');
    Scripting.defineScriptEvent('geditLaunch', 'gedit application launch');
    Scripting.defineScriptEvent('geditFirstFrame', 'first frame of gedit window drawn');

    await Scripting.waitLeisure();
    Scripting.scriptEvent('desktopShown');

    let interfaceSettings = new Gio.Settings({
        schema_id: 'org.gnome.desktop.interface',
    });
    interfaceSettings.set_boolean('enable-animations', false);

    Scripting.scriptEvent('overviewShowStart');
    Main.overview.show();
    await Scripting.waitLeisure();
    Scripting.scriptEvent('overviewShowDone');

    await Scripting.sleep(1000);

    Scripting.scriptEvent('applicationsShowStart');
    Main.overview.dash.showAppsButton.checked = true;

    await Scripting.waitLeisure();
    Scripting.scriptEvent('applicationsShowDone');

    await Scripting.sleep(1000);

    Main.overview.hide();
    await Scripting.waitLeisure();

    // --------------------- //
    // Tests of redraw speed //
    // --------------------- //

    global.frame_timestamps = true;
    global.frame_finish_timestamp = true;

    for (let k = 0; k < 5; k++)
        await Scripting.createTestWindow({maximized: true});
    await Scripting.waitTestWindows();

    await Scripting.sleep(1000);

    Scripting.scriptEvent('mainViewDrawStart');
    await waitAndDraw(1000);
    Scripting.scriptEvent('mainViewDrawDone');

    Main.overview.show();
    Scripting.waitLeisure();

    await Scripting.sleep(1500);

    Scripting.scriptEvent('overviewDrawStart');
    await waitAndDraw(1000);
    Scripting.scriptEvent('overviewDrawDone');

    await Scripting.destroyTestWindows();
    Main.overview.hide();

    await Scripting.createTestWindow({
        maximized: true,
        redraws: true,
    });
    await Scripting.waitTestWindows();

    await Scripting.sleep(1000);

    Scripting.scriptEvent('redrawTestStart');
    await Scripting.sleep(1000);
    Scripting.scriptEvent('redrawTestDone');

    await Scripting.sleep(1000);
    Scripting.scriptEvent('collectTimings');

    await Scripting.destroyTestWindows();

    global.frame_timestamps = false;
    global.frame_finish_timestamp = false;

    await Scripting.sleep(1000);

    let appSys = Shell.AppSystem.get_default();
    let app = appSys.lookup_app('org.gnome.gedit.desktop');

    Scripting.scriptEvent('geditLaunch');
    app.activate();

    let windows = app.get_windows();
    if (windows.length > 0)
        throw new Error('gedit was already running');

    while (windows.length === 0) {
        await waitSignal(global.display, 'window-created');
        windows = app.get_windows();
    }

    let actor = windows[0].get_compositor_private();
    await waitSignal(actor, 'first-frame');
    Scripting.scriptEvent('geditFirstFrame');

    await Scripting.sleep(1000);

    windows[0].delete(global.get_current_time());

    await Scripting.sleep(1000);

    interfaceSettings.set_boolean('enable-animations', true);
    /* eslint-enable no-await-in-loop */
}

let overviewShowStart;
let applicationsShowStart;
let stagePaintStart;
let redrawTiming;
let redrawTimes = {};
let geditLaunchTime;

/**
 * @param {number} time - event timestamp
 * @returns {void}
 */
export function script_desktopShown(time) {
    let bootTimestamp = extractBootTimestamp();
    METRICS.timeToDesktop.value = time - bootTimestamp;
}

/**
 * @param {number} time - event timestamp
 * @returns {void}
 */
export function script_overviewShowStart(time) {
    overviewShowStart = time;
}

/**
 * @param {number} time - event timestamp
 * @returns {void}
 */
export function script_overviewShowDone(time) {
    METRICS.overviewShowTime.value = time - overviewShowStart;
}

/**
 * @param {number} time - event timestamp
 * @returns {void}
 */
export function script_applicationsShowStart(time) {
    applicationsShowStart = time;
}

/**
 * @param {number} time - event timestamp
 * @returns {void}
 */
export function script_applicationsShowDone(time) {
    METRICS.applicationsShowTime.value = time - applicationsShowStart;
}

/**
 * @param {number} _time - event timestamp
 * @returns {void}
 */
export function script_mainViewDrawStart(_time) {
    redrawTiming = 'mainView';
}

/**
 * @param {number} _time - event timestamp
 * @returns {void}
 */
export function script_mainViewDrawDone(_time) {
    redrawTiming = null;
}

/**
 * @param {number} _time - event timestamp
 * @returns {void}
 */
export function script_overviewDrawStart(_time) {
    redrawTiming = 'overview';
}

/**
 * @param {number} _time - event timestamp
 * @returns {void}
 */
export function script_overviewDrawDone(_time) {
    redrawTiming = null;
}

/**
 * @param {number} _time - event timestamp
 * @returns {void}
 */
export function script_redrawTestStart(_time) {
    redrawTiming = 'application';
}

/**
 * @param {number} _time - event timestamp
 * @returns {void}
 */
export function script_redrawTestDone(_time) {
    redrawTiming = null;
}

/**
 * @param {number} _time - event timestamp
 * @returns {void}
 */
export function script_collectTimings(_time) {
    for (let timing in redrawTimes) {
        let times = redrawTimes[timing];
        times.sort((a, b) => a - b);

        let len = times.length;
        let median;

        if (len === 0)
            median = -1;
        else if (len % 2 === 1)
            median = times[(len - 1) / 2];
        else
            median = Math.round((times[len / 2 - 1] + times[len / 2]) / 2);

        METRICS[`${timing}RedrawTime`].value = median;
    }
}

/**
 * @param {number} time - event timestamp
 * @returns {void}
 */
export function script_geditLaunch(time) {
    geditLaunchTime = time;
}

/**
 * @param {number} time - event timestamp
 * @returns {void}
 */
export function script_geditFirstFrame(time) {
    METRICS.geditStartTime.value = time - geditLaunchTime;
}

/**
 * @param {number} time - event timestamp
 * @returns {void}
 */
export function clutter_stagePaintStart(time) {
    stagePaintStart = time;
}

/**
 * @param {number} time - event timestamp
 * @returns {void}
 */
export function clutter_paintCompletedTimestamp(time) {
    if (redrawTiming != null && stagePaintStart != null) {
        if (!(redrawTiming in redrawTimes))
            redrawTimes[redrawTiming] = [];
        redrawTimes[redrawTiming].push(time - stagePaintStart);
    }
    stagePaintStart = null;
}
