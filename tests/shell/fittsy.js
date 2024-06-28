/* eslint camelcase: ["error", { properties: "never", allow: ["^script_"] }] */

import Clutter from 'gi://Clutter';
import Shell from 'gi://Shell';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as Scripting from 'resource:///org/gnome/shell/ui/scripting.js';
import {sleep} from 'resource:///org/gnome/shell/ui/scripting.js';

const {Orientation, PickMode, TextDirection} = Clutter;
const EPSILON = 0.1;

function defineScriptEvent(eventName, description) {
    Shell.PerfLog.get_default().define_event(
        `script.${eventName}`, description, 'i');
}

function scriptEvent(eventName, bool) {
    Shell.PerfLog.get_default().event_i(`script.${eventName}`,
        Number(bool));
}

function checkFittsiness(eventName, actor, orientation, edge) {
    const [x, y] = actor.get_transformed_position();
    const pickCoords = orientation === Orientation.HORIZONTAL
        ? [edge, y + Math.floor(actor.height / 2)]
        : [x + Math.floor(actor.width / 2), edge];
    const pickedActor = global.stage.get_actor_at_pos(
        PickMode.REACTIVE, ...pickCoords);

    scriptEvent(eventName, pickedActor === actor);
}

/**
 * run:
 */
export async function run() {
    defineScriptEvent('fittsyDash', 'Dash extends to screen edge');
    defineScriptEvent('fittsyTopBarVert', 'Top bar extends to top screen edge');
    defineScriptEvent('fittsyTopBarHorzLTR', 'Top bar extends to left screen edge');
    defineScriptEvent('fittsyTopBarHorzRTL', 'Top bar extends to right screen edge');

    const {primaryMonitor} = Main.layoutManager;
    const topEdge = primaryMonitor.y + EPSILON;
    const bottomEdge = primaryMonitor.y + primaryMonitor.height - EPSILON;
    const leftEdge = primaryMonitor.x + EPSILON;
    const rightEdge = primaryMonitor.x + primaryMonitor.width - EPSILON;

    await Scripting.waitLeisure();

    const showApps = Main.overview.dash.showAppsButton;
    console.debug('Checking that dash extends to bottom screen edge');
    checkFittsiness('fittsyDash',
        showApps, Orientation.VERTICAL, bottomEdge);

    const activitiesButton = Main.panel.statusArea.activities;
    console.debug('Checking that top bar extends to top screen edge');
    checkFittsiness('fittsyTopBarVert',
        activitiesButton, Orientation.VERTICAL, topEdge);

    Main.panel.text_direction = TextDirection.LTR;
    await sleep(50);

    console.debug('Checking that top bar extends to left screen edge for LTR');
    checkFittsiness('fittsyTopBarHorzLTR',
        activitiesButton, Orientation.HORIZONTAL, leftEdge);

    Main.panel.text_direction = TextDirection.RTL;
    await sleep(50);

    console.debug('Checking that top bar extends to right screen edge for RTL');
    checkFittsiness('fittsyTopBarHorzRTL',
        activitiesButton, Orientation.HORIZONTAL, rightEdge);
}

let fittsyDash = false;
let fittsyTopBarVert = false;
let fittsyTopBarHorzLTR = false;
let fittsyTopBarHorzRTL = false;

/**
 * @param {number} time
 * @param {number} num
 */
export function script_fittsyDash(time, num) {
    fittsyDash = num !== 0;
}

/**
 * @param {number} time
 * @param {number} num
 */
export function script_fittsyTopBarVert(time, num) {
    fittsyTopBarVert = num !== 0;
}

/**
 * @param {number} time
 * @param {number} num
 */
export function script_fittsyTopBarHorzLTR(time, num) {
    fittsyTopBarHorzLTR = num !== 0;
}

/**
 * @param {number} time
 * @param {number} num
 */
export function script_fittsyTopBarHorzRTL(time, num) {
    fittsyTopBarHorzRTL = num !== 0;
}

export function finish() {
    if (!fittsyDash)
        throw new Error('Dash does not extend to screen edge');

    if (!fittsyTopBarVert)
        throw new Error('Top bar does not extend to top screen edge');

    if (!fittsyTopBarHorzLTR)
        throw new Error('Top bar does not extend to left screen edge');

    if (!fittsyTopBarHorzRTL)
        throw new Error('Top bar does not extend to right screen edge');
}
