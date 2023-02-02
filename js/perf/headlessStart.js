// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported init, run, finish, script_monitorsChanged, script_overviewHideDone,
   script_overviewShowDone, METRICS,
*/
/* eslint camelcase: ["error", { properties: "never", allow: ["^script_"] }] */

const {GLib, MetaTest, Shell} = imports.gi;

const Main = imports.ui.main;
const Scripting = imports.ui.scripting;

// This script tests that the shell handles connecting monitors after startup
// is properly handled.

var METRICS = {};

let _testMonitor = null;

/**
 * init:
 */
function init() {
    global.connect('shutdown', () => {
        _testMonitor?.destroy();
    });
    global.context.connect('started',
        () => {
            GLib.timeout_add_seconds(
                GLib.PRIORITY_LOW, 2,
                () => {
                    log('Connecting 1280x720 test monitor');
                    _testMonitor = MetaTest.TestMonitor.new(
                        global.context, 1280, 720, 60.0);
                });
        });
    Scripting.defineScriptEvent('monitorsChanged', 'Monitors changed');

    const display = global.display;
    console.assert(display.get_n_monitors() === 0, 'Not running headless');

    const monitorManager = global.backend.get_monitor_manager();
    const monitorsChangedHandlerId = monitorManager.connect('monitors-changed',
        () => {
            console.assert(display.get_n_monitors() === 1,
                'Expected a single monitor');
            Scripting.scriptEvent('monitorsChanged');
            monitorManager.disconnect(monitorsChangedHandlerId);
        });
    Shell.PerfLog.get_default().set_enabled(true);
}

/**
 * run:
 */
async function run() {
    /* eslint-disable no-await-in-loop */
    Scripting.defineScriptEvent('overviewShowDone', 'Overview finished showing');
    Scripting.defineScriptEvent('overviewHideDone', 'Overview finished hiding');

    Main.overview.connect('shown',
        () => Scripting.scriptEvent('overviewShowDone'));
    Main.overview.connect('hidden',
        () => Scripting.scriptEvent('overviewHideDone'));

    Main.overview.hide();
    await Scripting.waitLeisure();

    Main.overview.show();
    await Scripting.waitLeisure();

    /* eslint-enable no-await-in-loop */
}

let monitorsChanged = false;
let overviewHidden = false;
let overviewShown = false;

/**
 * script_monitorsChanged:
 */
function script_monitorsChanged() {
    monitorsChanged = true;
}

/**
 * script_overviewHideDone:
 */
function script_overviewHideDone() {
    overviewHidden = true;
}

/**
 * script_overviewShowDone:
 */
function script_overviewShowDone() {
    overviewShown = true;
}

/**
 * finish:
 */
function finish() {
    if (!monitorsChanged)
        throw new Error('Monitors never changed');

    if (!overviewHidden)
        throw new Error('Failed to hide overview');

    if (!overviewShown)
        throw new Error('Failed to show overview');
}
