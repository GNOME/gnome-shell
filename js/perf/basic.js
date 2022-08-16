// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported run, finish, script_topBarNavDone, script_notificationShowDone,
   script_notificationCloseDone, script_overviewShowDone,
   script_applicationsShowStart, script_applicationsShowDone, METRICS,
*/
/* eslint camelcase: ["error", { properties: "never", allow: ["^script_"] }] */

const { St } = imports.gi;

const Main = imports.ui.main;
const MessageTray = imports.ui.messageTray;
const Scripting = imports.ui.scripting;

// This script tests the most important (basic) functionality of the shell.

var METRICS = {};

async function run() {
    console.debug('Running basic perf test');

    /* eslint-disable no-await-in-loop */
    Scripting.defineScriptEvent('topBarNavStart', 'Starting to navigate the top bar');
    Scripting.defineScriptEvent('topBarNavDone', 'Done navigating the top bar');
    Scripting.defineScriptEvent('notificationShowStart', 'Showing a notification');
    Scripting.defineScriptEvent('notificationShowDone', 'Done showing a notification');
    Scripting.defineScriptEvent('notificationCloseStart', 'Closing a notification');
    Scripting.defineScriptEvent('notificationCloseDone', 'Done closing a notification');
    Scripting.defineScriptEvent('overviewShowStart', 'Starting to show the overview');
    Scripting.defineScriptEvent('overviewShowDone', 'Overview finished showing');
    Scripting.defineScriptEvent('applicationsShowStart', 'Starting to switch to applications view');
    Scripting.defineScriptEvent('applicationsShowDone', 'Done switching to applications view');

    Main.overview.connect('shown',
        () => Scripting.scriptEvent('overviewShowDone'));

    await Scripting.sleep(1000);

    // navigate through top bar

    console.debug('Navigate through top bar');
    Scripting.scriptEvent('topBarNavStart');
    Main.panel.statusArea.quickSettings.menu.open();
    await Scripting.sleep(400);

    const { menuManager } = Main.panel;
    while (menuManager.activeMenu &&
        Main.panel.navigate_focus(menuManager.activeMenu.sourceActor,
            St.DirectionType.TAB_BACKWARD, false))
        await Scripting.sleep(400);
    Scripting.scriptEvent('topBarNavDone');

    await Scripting.sleep(1000);

    // notification
    console.debug('Show notification message');
    const source = new MessageTray.SystemNotificationSource();
    Main.messageTray.add(source);

    Scripting.scriptEvent('notificationShowStart');
    source.connect('notification-show',
        () => Scripting.scriptEvent('notificationShowDone'));

    const notification = new MessageTray.Notification(source,
        'A test notification');
    source.showNotification(notification);
    await Scripting.sleep(400);

    console.debug('Show date menu');
    Main.panel.statusArea.dateMenu.menu.open();
    await Scripting.sleep(400);

    Scripting.scriptEvent('notificationCloseStart');
    notification.connect('destroy',
        () => Scripting.scriptEvent('notificationCloseDone'));

    console.debug('Destroy notification message');
    notification.destroy();
    await Scripting.sleep(400);

    console.debug('Close date menu');
    Main.panel.statusArea.dateMenu.menu.close();
    await Scripting.waitLeisure();

    await Scripting.sleep(1000);

    // overview (window picker)
    Scripting.scriptEvent('overviewShowStart');
    Main.overview.show();
    await Scripting.waitLeisure();
    Main.overview.hide();
    await Scripting.waitLeisure();

    await Scripting.sleep(1000);

    // overview (app picker)
    console.debug('Showing overview');
    Main.overview.show();
    await Scripting.waitLeisure();

    Scripting.scriptEvent('applicationsShowStart');
    console.debug('Showing applications');
    // eslint-disable-next-line require-atomic-updates
    Main.overview.dash.showAppsButton.checked = true;
    await Scripting.waitLeisure();
    Scripting.scriptEvent('applicationsShowDone');
    console.debug('Hiding applications');
    // eslint-disable-next-line require-atomic-updates
    Main.overview.dash.showAppsButton.checked = false;
    await Scripting.waitLeisure();

    console.debug('Hiding overview');
    Main.overview.hide();
    await Scripting.waitLeisure();
    /* eslint-enable no-await-in-loop */

    console.debug('Finished basic perf test');
}

let topBarNav = false;
let notificationShown = false;
let notificationClosed = false;
let windowPickerShown = false;
let appPickerShown = false;

function script_topBarNavDone() {
    topBarNav = true;
}

function script_notificationShowDone() {
    notificationShown = true;
}

function script_notificationCloseDone() {
    notificationClosed = true;
}

function script_overviewShowDone() {
    windowPickerShown = true;
}

function script_applicationsShowDone() {
    appPickerShown = true;
}

function finish() {
    if (!topBarNav)
        throw new Error('Failed to navigate top bar');

    if (!notificationShown)
        throw new Error('Failed to show notification');

    if (!notificationClosed)
        throw new Error('Failed to close notification');

    if (!windowPickerShown)
        throw new Error('Failed to show window picker');

    if (!appPickerShown)
        throw new Error('Failed to show app picker');
}
