/* eslint camelcase: ["error", { properties: "never", allow: ["^script_", "^malloc", "^glx", "^clutter"] }] */

import Clutter from 'gi://Clutter';

import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import * as Scripting from 'resource:///org/gnome/shell/ui/scripting.js';

/** Run test. */
export async function run() {
    /* Make created windows remain visible during exit. */
    await Scripting.disableHelperAutoExit();

    const seat = global.stage.context.get_backend().get_default_seat();
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
}
