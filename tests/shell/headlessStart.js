/* eslint camelcase: ["error", { properties: "never", allow: ["^script_"] }] */

import GLib from 'gi://GLib';
import MetaTest from 'gi://MetaTest';
import Shell from 'gi://Shell';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as Scripting from 'resource:///org/gnome/shell/ui/scripting.js';

// This script tests that the shell handles connecting monitors after startup
// is properly handled.

export var METRICS = {};

let _testMonitor = null;

/**
 * init:
 */
export function init() {
    global.connect('shutdown', () => {
        _testMonitor?.destroy();
    });
    GLib.timeout_add_seconds(
        GLib.PRIORITY_LOW, 2,
        () => {
            log('Connecting 1280x720 test monitor');
            _testMonitor = MetaTest.TestMonitor.new(
                global.context, 1280, 720, 60.0);
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
export async function run() {
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
}

let monitorsChanged = false;
let overviewHidden = false;
let overviewShown = false;

/**
 * script_monitorsChanged:
 */
export function script_monitorsChanged() {
    monitorsChanged = true;
}

/**
 * script_overviewHideDone:
 */
export function script_overviewHideDone() {
    overviewHidden = true;
}

/**
 * script_overviewShowDone:
 */
export function script_overviewShowDone() {
    overviewShown = true;
}

/**
 * finish:
 */
export function finish() {
    if (!monitorsChanged)
        throw new Error('Monitors never changed');

    if (!overviewHidden)
        throw new Error('Failed to hide overview');

    if (!overviewShown)
        throw new Error('Failed to show overview');
}
