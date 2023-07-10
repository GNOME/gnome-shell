// -*- mode: js; js-indent-level: 4; indent-tabs-mode: nil -*-
/* eslint camelcase: ["error", { properties: "never", allow: ["^script_", "^malloc", "^glx", "^clutter"] }] */

import Clutter from 'gi://Clutter';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as Scripting from 'resource:///org/gnome/shell/ui/scripting.js';

/** Run test. */
export async function run() {
    /* eslint-disable no-await-in-loop */

    /* Make created windows remain visible during exit. */
    await Scripting.disableHelperAutoExit();

    const seat = Clutter.get_default_backend().get_default_seat();
    const virtualDevice_ =
        seat.create_virtual_device(Clutter.InputDeviceType.KEYBOARD_DEVICE);

    Main.overview.hide();
    await Scripting.waitLeisure();
    await Scripting.sleep(1000);

    await Scripting.createTestWindow({
        width: 640,
        height: 480,
        textInput: true,
    });

    await Scripting.waitTestWindows();
    await Scripting.waitLeisure();
    await Scripting.sleep(1000);

    /* eslint-enable no-await-in-loop */
}
