// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* exported run */
/* eslint camelcase: ["error", { properties: "never", allow: ["^script_", "^malloc", "^glx", "^clutter"] }] */

const Main = imports.ui.main;
const Scripting = imports.ui.scripting;

/** Run test. */
async function run() {
    /* eslint-disable no-await-in-loop */

    /* Make created windows remain visible during exit. */
    Scripting.disableHelperAutoExit();

    Main.overview.hide();
    await Scripting.waitLeisure();
    await Scripting.sleep(1000);

    await Scripting.createTestWindow({
        width: 640,
        height: 480,
    });

    await Scripting.waitTestWindows();
    await Scripting.waitLeisure();
    await Scripting.sleep(1000);

    /* eslint-enable no-await-in-loop */
}
